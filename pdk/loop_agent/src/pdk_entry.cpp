// PDK Plugin entry — AgenticDSL v1 API (hydraforge namespace for PluginInfo, agenticdsl for IToolRegistry)
// 关联: openspec/changes/2026-07-17-pdk-chat-demo-buildable/

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

            nlohmann::json output;
            output["response"] =
                "[loop_agent/" + loop_type + "] Processed: \"" + user_prompt + "\"\n\n"
                "This is a mock response from the Loop Agent plugin. In production, "
                "this would invoke lib/loop/" + loop_type + ".agent.md via DSLEngine::from_markdown. "
                "For the demo --mock mode, we return a canned response to avoid the architectural "
                "limitation that DSLEngine::from_markdown creates an isolated sub-engine whose LLM "
                "provider cannot inherit configuration from the parent engine (ADR-0019 follow-up).";
            output["steps"] = 1;
            output["tokens_used"] = 42;
            output["cost_usd"] = 0.001;
            output["success"] = true;
            return output;
        }
    );
}