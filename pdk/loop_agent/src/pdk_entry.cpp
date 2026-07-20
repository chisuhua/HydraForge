// PDK Plugin entry — AgenticDSL v1 API (hydraforge namespace for PluginInfo, agenticdsl for IToolRegistry)
// 关联: openspec/changes/2026-07-17-pdk-chat-demo-buildable/

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>
#include <agenticdsl/types/layered_context.h>
#include <core/engine.h>

namespace fs = std::filesystem;

namespace {

inline nlohmann::json json_arg(const std::unordered_map<std::string, std::string>& args,
                                const std::string& key) {
    auto it = args.find(key);
    if (it == args.end()) return nlohmann::json();
    try {
        return nlohmann::json::parse(it->second);
    } catch (...) {
        return it->second;
    }
}

inline std::string str_arg(const std::unordered_map<std::string, std::string>& args,
                           const std::string& key, const std::string& default_val = "") {
    auto it = args.find(key);
    return (it != args.end()) ? it->second : default_val;
}

inline int int_arg(const std::unordered_map<std::string, std::string>& args,
                   const std::string& key, int default_val = 0) {
    auto it = args.find(key);
    if (it == args.end()) return default_val;
    try { return std::stoi(it->second); } catch (...) { return default_val; }
}

// 找到 lib/loop/ 目录路径
fs::path find_loop_dir() {
    // 1. 环境变量
    const char* env_path = std::getenv("HYDRAFORGE_LOOP_DIR");
    if (env_path) return env_path;

    // 2. 当前工作目录相对路径
    return fs::current_path() / "lib" / "loop";
}

std::string load_agent_file(const std::string& loop_type) {
    fs::path loop_dir = find_loop_dir();
    fs::path agent_file = loop_dir / (loop_type + ".agent.md");

    if (!fs::exists(agent_file)) {
        throw std::runtime_error(
            "Loop Agent file not found: " + agent_file.string()
        );
    }

    std::ifstream f(agent_file);
    if (!f.is_open()) {
        throw std::runtime_error(
            "Cannot open loop file: " + agent_file.string()
        );
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

// loop-agent-dsl-execution: thread_local storage for parent engine's LLM provider
// Uses thread_local for per-thread isolation (multi-engine scenarios).
// nullptr = not set, mock fallback path.
static thread_local ::agenticdsl::ILLMProvider* tls_parent_provider = nullptr;

// --- pdk_plugin_info ---
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    hydraforge::CURRENT_ABI_VERSION,                                  // abi_version = 2
    "chat.loop",                                                      // name[64]
    0, 1, 0,                                                          // semver major.minor.patch
    "Loop Agent - React/PlanExecute/ForkJoin DSL executor",           // description[256]
    "react_loop,plan_execute_loop,fork_join_loop",                    // capabilities[512]
    ""                                                                // dependencies[256]
};

// --- pdk_register_tools ---
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    // loop/set_parent_provider — 配置父引擎 LLM provider 引用
    // force_approval_always=true, allowed_layers={Workflow} (仅 hand-written DSL 可调用)
    registry.register_tool_function(
        "loop/set_parent_provider",
        ::agenticdsl::ToolMetadata{
            .name = "loop/set_parent_provider",
            .description = "Set parent engine LLM provider for loop agent DSL execution",
            .domain = "loop",
            .category = ::agenticdsl::ToolCategory::StateModify,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = false,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = true
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            auto it = args.find("provider_ptr");
            if (it == args.end() || it->second.empty()) {
                return {{"success", false}, {"error", "Missing provider_ptr argument"}};
            }
            try {
                auto* new_provider = reinterpret_cast<::agenticdsl::ILLMProvider*>(
                    std::stoull(it->second));
                if (!new_provider) {
                    return {{"success", false}, {"error", "Null provider_ptr"}};
                }
                if (tls_parent_provider && tls_parent_provider != new_provider) {
                    std::cerr << "[loop_agent] WARNING: overwriting parent provider "
                              << tls_parent_provider << " → " << new_provider << std::endl;
                }
                tls_parent_provider = new_provider;
                return {{"success", true}};
            } catch (const std::exception& e) {
                return {{"success", false}, {"error", std::string("Invalid provider_ptr: ") + e.what()}};
            }
        }
    );

    // 注册 loop/run 工具
    registry.register_tool_function(
        "loop/run",
        ::agenticdsl::ToolMetadata{
            .name = "loop/run",
            .description = "Run an agent loop from a .agent.md file",
            .domain = "loop",
            .category = ::agenticdsl::ToolCategory::Execute,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = false,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = false
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            std::string loop_type = str_arg(args, "loop_type", "react");
            std::string user_prompt = str_arg(args, "prompt");

            // loop_type 合法性校验 (Q7: 仅 react/plan_execute/fork_join)
            if (loop_type != "react" && loop_type != "plan_execute" && loop_type != "fork_join") {
                return {{"success", false},
                        {"error", "Invalid loop_type: '" + loop_type +
                         "'. Must be one of: react, plan_execute, fork_join"}};
            }

            // Mock fallback when parent provider not set (Q3/Q7)
            if (!tls_parent_provider) {
                nlohmann::json output;
                output["response"] =
                    "[loop_agent/" + loop_type + "] Processed: \"" + user_prompt + "\"\n\n"
                    "Mock fallback: parent LLM provider not set. Call loop/set_parent_provider first, "
                    "or this will return the legacy mock response.";
                output["steps"] = 1;
                output["tokens_used"] = 42;
                output["cost_usd"] = 0.001;
                output["success"] = true;
                return output;
            }

            // Real DSL execution path (Q4: errors propagate via return)
            try {
                auto agent_content = load_agent_file(loop_type);

                auto child = ::agenticdsl::DSLEngine::from_markdown(
                    agent_content, *tls_parent_provider);

                ::agenticdsl::LayeredContext ctx;
                ctx.working["user_input"] = user_prompt;

                auto result = child->run(ctx);

                nlohmann::json output;
                output["success"] = result.success;
                output["error"]   = result.success ? "" : result.message;
                output["response"] = result.final_context.value("response",
                    result.final_context.value("output", result.message));
                output["steps"]  = 1;
                output["tokens_used"] = 0;
                output["cost_usd"]    = 0.0;
                return output;
            } catch (const std::exception& e) {
                return {{"success", false}, {"error", e.what()},
                        {"response", ""}, {"steps", 0}, {"tokens_used", 0}, {"cost_usd", 0.0}};
            }
        }
    );
}