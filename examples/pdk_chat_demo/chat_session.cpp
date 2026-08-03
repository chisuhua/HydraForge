// chat_session.cpp - Chat Session 实现
// 关联: chat_session.h, docs/adr/adr-0060-agent-composition.md
//      openspec/changes/pdk-chat-demo-v1-recap/design.md (T1: 持久化 + Budget 告警)

#include "chat_session.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <utility>

#include <core/engine.h>
#include <core/types/tool_result.h>
#include <agenticdsl/types/layered_context.h>
#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <modules/budget/budget_controller.h>


namespace pdk_chat_demo {

namespace {

static std::string ptr_to_str(void* p) {
    std::ostringstream ss;
    ss << reinterpret_cast<uintptr_t>(p);
    return ss.str();
}

// T1.2: 展开 ~ 为 HOME 目录 (优先 HOME env, fallback getpwuid)
std::string expand_home(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + path.substr(1);
    }
    return path;
}

// T1.10: 创建目录权限 0700
bool ensure_dir_0700(const std::filesystem::path& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        std::cerr << "[session] create_directories failed: " << dir
                  << " (" << ec.message() << ")" << std::endl;
        return false;
    }
    if (chmod(dir.c_str(), 0700) != 0) {
        std::cerr << "[session] chmod 0700 failed: " << dir << std::endl;
    }
    return true;
}

constexpr int kSessionSchemaVersion = 1;

}  // namespace

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
    std::vector<nlohmann::json> messages;
    std::string provider_mode;
    std::string persist_dir_expanded;

    Impl(
        agenticdsl::DSLEngine* e,
        std::shared_ptr<agenticdsl::IInteractionBus> b,
        agenticdsl::IToolRegistry* r,
        const AgentConfig& a,
        const SessionConfig& s
    ) : engine(e), bus(std::move(b)), registry(r), agent_cfg(a), session_cfg(s),
        provider_mode(a.provider),
        persist_dir_expanded(expand_home(s.persist_dir)) {
        if (!persist_dir_expanded.empty()) {
            ensure_dir_0700(persist_dir_expanded);
        }
    }
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

    // T1 线程安全: budget.checked 回调仅置 atomic flag, 不触 TUI
    if (impl_->bus) {
        impl_->bus->subscribe("budget.checked",
            [this](const agenticdsl::BusEvent&) {
                budget_alert_flag_.store(true, std::memory_order_release);
            });
    }
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

    // 2. emit "user.input" 事件 (ADR-0068 §4 — args=业务字段, meta=trace context)
    impl_->bus->emit(agenticdsl::EventBuilder("user.input")
        .args(nlohmann::json{{"input", user_input}})
        .meta(nlohmann::json{{"session_id", session_id_}})
        .build());

    // 3. 获取 LLM 响应 — 统一经 Loop Agent 工具 (loop/run) 执行
    //    loop_agent 内部决定 mock fallback (parent provider 未设置) 或真实 DSL 执行
    try {
        // Loop Agent 工具 — 唯一 ReAct 执行路径
        std::unordered_map<std::string, std::string> loop_args;
        loop_args["loop_type"] = impl_->agent_cfg.loop_type;
        loop_args["prompt"] = user_input;
        loop_args["system_prompt"] = impl_->agent_cfg.system_prompt;
        loop_args["history"] = nlohmann::json(impl_->messages).dump();
        loop_args["tools"] = nlohmann::json(impl_->agent_cfg.tools).dump();
        loop_args["max_steps"] = std::to_string(impl_->agent_cfg.max_steps);
        // 将 bus 与会话 ID 透传给 loop_agent, 用于真实事件发射
        loop_args["bus_ptr"] = ptr_to_str(impl_->bus.get());
        loop_args["session_id"] = session_id_;

        nlohmann::json loop_result = impl_->registry->call_tool("loop/run", loop_args);

        result.response = loop_result.value("response", "");
        result.total_steps = loop_result.value("steps", 0);
        result.total_tokens = loop_result.value("tokens_used", 0);
        result.cost_usd = loop_result.value("cost_usd", 0.0);
        result.success = true;

        if (result.success) {
            // 4. 追加 assistant 消息到历史
            nlohmann::json assistant_msg = {
                {"role", "assistant"},
                {"content", result.response},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
                {"steps", result.total_steps},
                {"tokens", result.total_tokens}
            };
            impl_->messages.push_back(assistant_msg);

            // 5. emit "loop.done" (ADR-0068 §4 — args=业务字段, meta=trace context)
            impl_->bus->emit(agenticdsl::EventBuilder("loop.done")
                .args(nlohmann::json{
                    {"response", result.response},
                    {"total_steps", result.total_steps},
                    {"total_tokens", result.total_tokens}
                })
                .meta(nlohmann::json{{"session_id", session_id_}})
                .build());

            // T1 Budget 告警: 每轮后轮询 engine budget controller
            if (impl_->engine) {
                const auto& bc = impl_->engine->get_budget_controller();
                if (bc.exceeded()) {
                    double used = bc.get_total_cost_usd();
                    double limit = impl_->agent_cfg.budget_limit_usd;
                    impl_->bus->emit(agenticdsl::EventBuilder("budget.checked")
                        .args(nlohmann::json{
                            {"limit", limit},
                            {"used", used},
                            {"unit", "llm_calls"},
                            {"reason", "cost_limit"},
                            {"ok", false}
                        })
                        .meta(nlohmann::json{{"session_id", session_id_}})
                        .build());
                    result.success = false;
                    result.error_message = "[budget] cost_limit exceeded (used="
                        + std::to_string(used) + ", limit="
                        + std::to_string(limit) + ")";
                }
            }

            // 6. 持久化 (异步, 简化版 fire-and-forget)
            if (impl_->session_cfg.persist_dir != "") {
                impl_->bus->emit(agenticdsl::EventBuilder("session.persist_request")
                    .args(nlohmann::json{{"messages", nlohmann::json(impl_->messages)}})
                    .meta(nlohmann::json{{"session_id", session_id_}})
                    .build());
                // T1: 同步落盘 (原子写入) - 确保跨进程可恢复
                save_to_disk();
            }
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        impl_->bus->emit(agenticdsl::EventBuilder("loop.error")
            .args(nlohmann::json{{"error", result.error_message}})
            .meta(nlohmann::json{{"session_id", session_id_}})
            .build());
    }

    return result;
}

std::vector<nlohmann::json> ChatSession::history() const {
    return impl_->messages;
}

// === T1: Session 持久化实现 ===

namespace {

std::filesystem::path session_file_path(const std::string& dir, const std::string& id) {
    return std::filesystem::path(dir) / (id + ".json");
}

}  // namespace

bool ChatSession::load_from_disk(const std::string& session_id) {
    namespace fs = std::filesystem;
    auto path = session_file_path(impl_->persist_dir_expanded, session_id);

    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return false;
    }

    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[session/load] cannot open: " << path << std::endl;
        return false;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[session/load] invalid JSON: " << path << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[session/load] error: " << path << ": " << e.what() << std::endl;
        return false;
    }

    // schema 版本校验
    int sv = j.value("schema_version", 0);
    if (sv != kSessionSchemaVersion) {
        std::cerr << "[session/load] unsupported schema_version " << sv
                  << " (expected " << kSessionSchemaVersion << "): " << path << std::endl;
        return false;
    }

    // T1.7 provider_mode reconcile: 恢复 history 但用本次 provider
    session_id_ = j.value("session_id", session_id);
    impl_->messages.clear();
    if (j.contains("history") && j["history"].is_array()) {
        for (const auto& msg : j["history"]) {
            impl_->messages.push_back(msg);
        }
    }
    // provider_mode 不恢复 - 保持本次构造时的 provider
    return true;
}

bool ChatSession::save_to_disk() {
    namespace fs = std::filesystem;
    if (impl_->persist_dir_expanded.empty()) return false;

    if (!ensure_dir_0700(impl_->persist_dir_expanded)) return false;

    auto path = session_file_path(impl_->persist_dir_expanded, session_id_);
    auto tmp = path;
    tmp += ".tmp";

    nlohmann::json j;
    j["schema_version"] = kSessionSchemaVersion;
    j["session_id"] = session_id_;
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch().count();
    j["created_at"] = epoch;
    j["updated_at"] = epoch;
    j["provider_mode"] = impl_->provider_mode;
    j["budget"] = {
        {"total", impl_->agent_cfg.budget_limit_usd},
        {"used", impl_->engine ? impl_->engine->get_budget_controller().get_total_cost_usd() : 0.0}
    };
    j["history"] = nlohmann::json(impl_->messages);

    {
        std::ofstream f(tmp);
        if (!f.is_open()) {
            std::cerr << "[session/save] cannot write tmp: " << tmp << std::endl;
            return false;
        }
        f << j.dump(2);
    }

    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        std::cerr << "[session/save] rename failed: " << ec.message() << std::endl;
        std::error_code rm_ec;
        fs::remove(tmp, rm_ec);
        return false;
    }

    // ADR-0068 §4 — 仅在原子 rename 成功后 emit session.persisted
    if (impl_->bus) {
        impl_->bus->emit(agenticdsl::EventBuilder("session.persisted")
            .args(nlohmann::json{
                {"session_id", session_id_},
                {"path", path.string()}
            })
            .meta(nlohmann::json{{"session_id", session_id_}})
            .build());
    }
    return true;
}

std::vector<std::string> ChatSession::list_sessions(const std::string& persist_dir) {
    namespace fs = std::filesystem;
    std::vector<std::string> ids;
    auto dir = expand_home(persist_dir);
    if (dir.empty()) return ids;

    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return ids;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        if (p.extension() == ".json") {
            ids.push_back(p.stem().string());
        }
    }
    return ids;
}

void ChatSession::cleanup_stale(const std::string& persist_dir, long long max_age_seconds) {
    namespace fs = std::filesystem;
    auto dir = expand_home(persist_dir);
    if (dir.empty()) return;

    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;

    auto now = fs::file_time_type::clock::now();
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        auto lwt = entry.last_write_time(ec);
        if (ec) {
            std::cerr << "[session/cleanup] stat failed: " << entry.path() << std::endl;
            continue;
        }
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - lwt).count();
        if (age > max_age_seconds) {
            std::error_code rm_ec;
            fs::remove(entry.path(), rm_ec);
            if (rm_ec) {
                std::cerr << "[session/cleanup] remove failed: " << entry.path()
                          << ": " << rm_ec.message() << std::endl;
            }
        }
    }
}

bool ChatSession::consume_budget_alert() {
    return budget_alert_flag_.exchange(false, std::memory_order_acq_rel);
}

}  // namespace pdk_chat_demo