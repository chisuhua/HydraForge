// chat_session.cpp - Chat Session 实现
// 关联: chat_session.h, docs/adr/adr-0060-agent-composition.md

#include "chat_session.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <core/engine.h>
#include <core/types/tool_result.h>
#include <agenticdsl/types/layered_context.h>
#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/contract/iinteraction_bus.h>

namespace pdk_chat_demo {

// --- ChatConfig ---

ChatConfig ChatConfig::from_json(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open config: " + path);
    }
    nlohmann::json j;
    f >> j;
    ChatConfig cfg;

    cfg.schema_version = j.value("schema_version", "1.0");
    cfg.app_id = j.value("app_id", "pdk_chat_demo");

    if (j.contains("providers")) cfg.providers = j["providers"];
    if (j.contains("orchestration")) cfg.orchestration = j["orchestration"];
    if (j.contains("safety")) cfg.safety = j["safety"];

    if (j.contains("agent")) {
        auto& a = j["agent"];
        cfg.agent.loop_type = a.value("loop_type", "react");
        cfg.agent.provider = a.value("provider", "mock");
        cfg.agent.model = a.value("model", "test");
        cfg.agent.system_prompt = a.value("system_prompt", "");
        if (a.contains("tools")) {
            for (auto& t : a["tools"]) cfg.agent.tools.push_back(t.get<std::string>());
        }
        cfg.agent.max_steps = a.value("max_steps", 50);
        cfg.agent.timeout_ms = a.value("timeout_ms", 300000);
        cfg.agent.budget_limit_usd = a.value("budget_limit_usd", 1.0);
    }

    if (j.contains("plugins")) {
        for (auto& p : j["plugins"]) {
            PluginConfig pc;
            pc.id = p.value("id", "");
            pc.path = p.value("path", "");
            pc.type = p.value("type", "so");
            pc.lifecycle = p.value("lifecycle", "eager");
            pc.requires_isolation = p.value("requires_isolation", false);
            if (p.contains("activation_events")) {
                for (auto& e : p["activation_events"]) {
                    pc.activation_events.push_back(e.get<std::string>());
                }
            }
            cfg.plugins.push_back(std::move(pc));
        }
    }

    if (j.contains("observability")) {
        auto& o = j["observability"];
        cfg.observability.otel_enabled = o.value("otel_enabled", false);
        cfg.observability.endpoint = o.value("endpoint", "http://localhost:4318");
        cfg.observability.sample_rate = o.value("sample_rate", 1.0);
        cfg.observability.export_format = o.value("export_format", "otlp+http");
    }

    if (j.contains("session")) {
        auto& s = j["session"];
        cfg.session.persist_dir = s.value("persist_dir", "~/.hydraforge/sessions/");
        cfg.session.compact_threshold_tokens = s.value("compact_threshold_tokens", 8000);
        cfg.session.branch_on_user_request = s.value("branch_on_user_request", true);
    }

    return cfg;
}

void ChatConfig::override_provider(const std::string& provider, const std::string& model) {
    this->agent.provider = provider;
    this->agent.model = model;
}

void ChatConfig::validate() const {
    if (schema_version != "1.0") {
        throw std::runtime_error("Unsupported schema_version: " + schema_version);
    }
    if (app_id.empty()) {
        throw std::runtime_error("app_id is required");
    }
    if (agent.provider.empty() || agent.model.empty()) {
        throw std::runtime_error("agent.provider and agent.model are required");
    }
    if (agent.max_steps <= 0) {
        throw std::runtime_error("agent.max_steps must be > 0");
    }
    if (agent.timeout_ms <= 0) {
        throw std::runtime_error("agent.timeout_ms must be > 0");
    }
}

// --- ChatSession::Impl ---

class ChatSession::Impl {
public:
    agenticdsl::DSLEngine* engine;
    std::shared_ptr<agenticdsl::IInteractionBus> bus;
    agenticdsl::IToolRegistry* registry;
    AgentConfig agent_cfg;
    SessionConfig session_cfg;
    std::vector<nlohmann::json> messages;  // 当前分支的历史

    Impl(
        agenticdsl::DSLEngine* e,
        std::shared_ptr<agenticdsl::IInteractionBus> b,
        agenticdsl::IToolRegistry* r,
        const AgentConfig& a,
        const SessionConfig& s
    ) : engine(e), bus(std::move(b)), registry(r), agent_cfg(a), session_cfg(s) {}
};

// --- ChatSession ---

ChatSession::ChatSession(
    agenticdsl::DSLEngine* engine,
    std::shared_ptr<agenticdsl::IInteractionBus> bus,
    agenticdsl::IToolRegistry* registry,
    const AgentConfig& agent_cfg,
    const SessionConfig& session_cfg
) : impl_(std::make_unique<Impl>(engine, std::move(bus), registry, agent_cfg, session_cfg)) {
    // 生成 session ID (UUID 简化版)
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream oss;
    oss << "sess_" << std::hex << dis(gen) << dis(gen);
    session_id_ = oss.str();
}

ChatSession::~ChatSession() = default;

ChatResult ChatSession::chat(const std::string& user_input) {
    ChatResult result;

    // 1. 追加用户消息到历史
    nlohmann::json user_msg = {
        {"role", "user"},
        {"content", user_input},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    impl_->messages.push_back(user_msg);

    // 2. emit "user.input" 事件
    impl_->bus->emit("user.input", agenticdsl::ToolResult{
        .ok = true,
        .meta = {{"session_id", session_id_}, {"input", user_input}}
    });

    // 3. 调用 Loop Agent
    std::unordered_map<std::string, std::string> loop_args;
    loop_args["loop_type"] = impl_->agent_cfg.loop_type;
    loop_args["prompt"] = user_input;
    loop_args["system_prompt"] = impl_->agent_cfg.system_prompt;
    loop_args["history"] = nlohmann::json(impl_->messages).dump();
    loop_args["tools"] = nlohmann::json(impl_->agent_cfg.tools).dump();
    loop_args["max_steps"] = std::to_string(impl_->agent_cfg.max_steps);
    loop_args["timeout_ms"] = std::to_string(impl_->agent_cfg.timeout_ms);

    try {
        nlohmann::json loop_result = impl_->registry->call_tool("loop/run", loop_args);

        result.response = loop_result.value("response", "");
        result.total_steps = loop_result.value("steps", 0);
        result.total_tokens = loop_result.value("tokens_used", 0);
        result.cost_usd = loop_result.value("cost_usd", 0.0);
        result.success = true;

        // 4. 追加 assistant 消息到历史
        nlohmann::json assistant_msg = {
            {"role", "assistant"},
            {"content", result.response},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"steps", result.total_steps},
            {"tokens", result.total_tokens}
        };
        impl_->messages.push_back(assistant_msg);

        // 5. emit "loop.done"
        impl_->bus->emit("loop.done", agenticdsl::ToolResult{
            .ok = true,
            .meta = {
                {"session_id", session_id_},
                {"response", result.response},
                {"total_steps", result.total_steps},
                {"total_tokens", result.total_tokens}
            }
        });

        // 6. 持久化 (异步, 简化版 fire-and-forget)
        if (impl_->session_cfg.persist_dir != "") {
            impl_->bus->emit("session.persist_request", agenticdsl::ToolResult{
                .ok = true,
                .meta = {
                    {"session_id", session_id_},
                    {"messages", nlohmann::json(impl_->messages)}
                }
            });
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        impl_->bus->emit("loop.error", agenticdsl::ToolResult{
            .ok = false,
            .meta = {
                {"session_id", session_id_},
                {"error", result.error_message}
            }
        });
    }

    return result;
}

std::vector<nlohmann::json> ChatSession::history() const {
    return impl_->messages;
}

}  // namespace pdk_chat_demo