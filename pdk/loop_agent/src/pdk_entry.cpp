// PDK Plugin entry — AgenticDSL v1 API (hydraforge namespace for PluginInfo, agenticdsl for IToolRegistry)
// 关联: openspec/changes/2026-07-17-pdk-chat-demo-buildable/
// C1 (loop-agent-tools): +4 工具 + ProviderLLMTool 提取为文件级 static class

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <atomic>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/plugin/plugin_info.h>
#include <agenticdsl/types/layered_context.h>
#include <core/engine.h>

// §4.0.4 chat-async-io-consumer-loop: use SHARED CancellationRegistry from hydraforge::pdk
// (was: file-static g_loop_registry; now: g_cancellation_registry global, same identity as ChatSession)
// pdk-chat-session-shim-cleanup: cancellation_globals now lives in PDK
#include <agenticdsl/pdk/cancellation_registry.h>
#include <agenticdsl/pdk/cancellation_globals.h>

namespace fs = std::filesystem;

namespace {

// ============================================================
// ProviderLLMTool — 桥接父引擎 LLM provider 到子引擎
// 提取自 loop/run lambda 内部类（C1 重构为文件级 static，供 4 工具复用）
// ============================================================
class ProviderLLMTool : public ::agenticdsl::ILLMTool {
 public:
    ProviderLLMTool(::agenticdsl::ILLMProvider& p, std::stop_token tok)
        : provider_(p), cancellation_token_(std::move(tok)) {}
    ::agenticdsl::LLMResult generate(
        const std::string& prompt,
        const ::agenticdsl::LLMParams& params) override {
        ::agenticdsl::LLMResult out;
        ::agenticdsl::GenerationRequest req(prompt);
        req.params = params;
        auto avail = provider_.available_models();
        if (!avail.empty()) {
            req.params.model = avail.front().name;
        }
        auto res = provider_.generate(req, cancellation_token_);
        if (res.has_value()) {
            out.success = true;
            out.text = std::move(res).value().text;
            out.tokens_generated = res.value().completion_tokens;
        } else {
            out.success = false;
            out.error = res.error().message;
        }
        return out;
    }
    bool is_available() const override { return true; }
    std::string name() const override { return "loop-agent-provider-bridge"; }
 private:
    ::agenticdsl::ILLMProvider& provider_;
    std::stop_token cancellation_token_;
};

// ============================================================
// Helper: C0 契约三字段 (ok, success, error_code) 统一构造
// ============================================================
inline nlohmann::json ok_result(bool ok, const char* ec = nullptr,
                                const std::string& err = "") {
    nlohmann::json j;
    j["ok"] = ok;
    j["success"] = ok;
    j["error_code"] = ec ? nlohmann::json(std::string(ec)) : nlohmann::json(nullptr);
    if (!err.empty()) j["error"] = err;
    return j;
}

inline nlohmann::json error_result(const std::string& ec, const std::string& err) {
    return ok_result(false, ec.c_str(), err);
}

// ============================================================
// Helper: 三级 fallback ReAct 决策解析器 (design D1)
// ============================================================
nlohmann::json parse_react_decision(const std::string& response) {
    // --- L1: 整体 JSON ---
    // 尝试两种常见格式:
    //   {"name": "...", "arguments": {...}}  (OpenAI function_call)
    //   {"tool": "...", "args": {...}}
    try {
        auto j = nlohmann::json::parse(response);
        if (j.is_object()) {
            // 检测 name+arguments 风格 (OpenAI function_call)
            if (j.contains("name") && j.contains("arguments")) {
                std::string tool = j["name"].get<std::string>();
                nlohmann::json args = j["arguments"];
                // arguments 可能是 JSON 字符串 (二次 parse)
                if (args.is_string()) {
                    try { args = nlohmann::json::parse(args.get<std::string>()); }
                    catch (...) { args = {{"input", j["arguments"].get<std::string>()}}; }
                }
                nlohmann::json out = ok_result(true);
                out["final"] = false;
                out["action_tool"] = tool;
                out["action_args"] = args;
                out["response"] = response;
                return out;
            }
            // 检测 tool+args 风格
            if (j.contains("tool") && j.contains("args")) {
                nlohmann::json out = ok_result(true);
                out["final"] = false;
                out["action_tool"] = j["tool"].get<std::string>();
                nlohmann::json args = j["args"];
                out["action_args"] = args.is_string()
                    ? nlohmann::json{{"input", args.get<std::string>()}}
                    : args;
                out["response"] = response;
                return out;
            }
        }
    } catch (const nlohmann::json::parse_error&) {
        // L1 失败 → fall through to L2
    }

    // --- L2: XML 标签 ---
    {
        static const std::regex xml_re(R"(<tool>([^<]+)</tool>\s*<args>(\{[\s\S]*?\}|[^<]+)</args>)");
        std::smatch m;
        if (std::regex_search(response, m, xml_re)) {
            std::string tool = m[1].str();
            std::string args_str = m[2].str();
            nlohmann::json args;
            try {
                args = nlohmann::json::parse(args_str);
            } catch (...) {
                args = {{"input", args_str}};
            }
            nlohmann::json out = ok_result(true);
            out["final"] = false;
            out["action_tool"] = tool;
            out["action_args"] = args;
            out["response"] = response;
            return out;
        }
    }

    // --- L3: 自然语言 → final fallback ---
    // 含 "Final Answer:" → 取后续文本；否则原响应文本
    std::string final_text = response;
    {
        static const std::regex final_re(R"(Final Answer:\s*(.*))",
                                         std::regex::icase | std::regex::ECMAScript);
        std::smatch m;
        if (std::regex_search(response, m, final_re)) {
            final_text = m[1].str();
        }
    }

    nlohmann::json out = ok_result(true);
    out["final"] = true;
    out["action_tool"] = "";
    out["action_args"] = nullptr;
    out["response"] = final_text;
    return out;
}

// ============================================================
// Helper: 提取 ```yaml fenced block 中的 DSL 内容
// 返回完整 markdown（含 ### AgenticDSL 前缀）
// ============================================================
std::string extract_agenticdsl_block(const std::string& plan) {
    // 查找 ```yaml ... ```
    static const std::regex fence_re(
        R"(```yaml\s*\n([\s\S]*?)\n```)",
        std::regex::ECMAScript);
    std::smatch m;
    if (!std::regex_search(plan, m, fence_re)) {
        return "";
    }
    // 重新包装为完整 markdown: 需要 ### AgenticDSL header + ```yaml fence
    std::string yaml_content = m[1].str();
    std::string markdown = "### AgenticDSL `/main`\n```yaml\n" + yaml_content + "\n```\n";
    return markdown;
}

// loop-agent-dsl-execution: thread_local storage for parent engine's LLM provider
// Uses thread_local for per-thread isolation (multi-engine scenarios).
// nullptr = not set, mock fallback path.
static thread_local ::agenticdsl::ILLMProvider* tls_parent_provider = nullptr;

// C1: thread_local capture mode (0=None, 1=Training)
static thread_local std::atomic<int> tls_capture_mode{0};

// ADR-0068 附录 A 事件发射 helper (Decision 4/5): 未注入 bus 时静默跳过
inline void emit_loop_event(::agenticdsl::IInteractionBus* bus,
                            const std::string& session_id,
                            const std::string& topic,
                            nlohmann::json payload) {
    if (!bus) return;
    payload["session_id"] = session_id;
    bus->emit(::agenticdsl::BusEvent{
        topic, ::agenticdsl::ToolResult{.ok = true, .meta = std::move(payload)}});
}

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
// (declared in anonymous namespace above for all tool lambdas to access)

// §4.0.4 chat-async-io-consumer-loop: REMOVED file-static g_loop_registry
// Now uses hydraforge::pdk::g_cancellation_registry (same identity as ChatSession)

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
                return {{"success", false}, {"ok", false},
                        {"error_code", "InvalidParams"}, {"error", "Missing provider_ptr argument"}};
            }
            try {
                auto* new_provider = reinterpret_cast<::agenticdsl::ILLMProvider*>(
                    std::stoull(it->second));
                if (!new_provider) {
                    return {{"success", false}, {"ok", false},
                            {"error_code", "InvalidParams"}, {"error", "Null provider_ptr"}};
                }
                if (tls_parent_provider && tls_parent_provider != new_provider) {
                    std::cerr << "[loop_agent] WARNING: overwriting parent provider "
                              << tls_parent_provider << " → " << new_provider << std::endl;
                }
                tls_parent_provider = new_provider;
                return {{"success", true}, {"ok", true}, {"error_code", nullptr}};
            } catch (const std::exception& e) {
                return {{"success", false}, {"ok", false},
                        {"error_code", "Unknown"},
                        {"error", std::string("Invalid provider_ptr: ") + e.what()}};
            }
        }
    );

    // ============================================================
    // loop/decide_react — 三级 ReAct 决策解析器 (C1)
    // ============================================================
    registry.register_tool_function(
        "loop/decide_react",
        ::agenticdsl::ToolMetadata{
            .name = "loop/decide_react",
            .description = "Parse LLM response into ReAct decision using 3-level fallback",
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
            // 获取 response 参数
            auto it = args.find("response");
            if (it == args.end() || it->second.empty()) {
                return error_result("InvalidParams", "Missing 'response' argument");
            }
            const std::string& response = it->second;
            return parse_react_decision(response);
        }
    );

    // ============================================================
    // loop/execute_plan — 子图执行器 (C1, design D2)
    // ============================================================
    registry.register_tool_function(
        "loop/execute_plan",
        ::agenticdsl::ToolMetadata{
            .name = "loop/execute_plan",
            .description = "Execute an AgenticDSL plan as a child engine synchronously",
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
            // 获取 plan 参数
            std::string plan = str_arg(args, "plan");
            if (plan.empty()) {
                return error_result("InvalidParams", "Missing 'plan' argument");
            }

            // 需要 parent provider
            if (!tls_parent_provider) {
                return error_result("Unknown", "parent provider not set. Call loop/set_parent_provider first.");
            }

            // 提取 AgenticDSL fenced block
            std::string agenticdsl_md = extract_agenticdsl_block(plan);
            if (agenticdsl_md.empty()) {
                return error_result("InvalidParams",
                    "plan is not AgenticDSL markdown: no ```yaml AgenticDSL block found");
            }

            try {
                // 创建子引擎 (借用 parent provider)
                auto child = ::agenticdsl::DSLEngine::from_markdown(
                    agenticdsl_md, *tls_parent_provider);

                // 注册 ProviderLLMTool (子图内 llm_call 节点用)
                child->register_llm_tool(
                    "llama-default",
                    std::make_unique<ProviderLLMTool>(
                        *tls_parent_provider, std::stop_token{}));

                // 构造 context
                ::agenticdsl::LayeredContext ctx;
                std::string context_str = str_arg(args, "context");
                if (!context_str.empty()) {
                    try {
                        auto context_json = nlohmann::json::parse(context_str);
                        // RFC 7396 merge_patch: context 覆盖 ctx.working 同名 key
                        ctx.working.merge_patch(context_json);
                    } catch (const nlohmann::json::parse_error&) {
                        return error_result("InvalidParams",
                            "context argument is not valid JSON");
                    }
                }

                // 同步执行 (lock-step)
                auto result = child->run(ctx);

                // 构造输出
                nlohmann::json output;
                output["ok"] = result.success;
                output["success"] = result.success;
                output["error_code"] = result.success
                    ? nlohmann::json(nullptr)
                    : nlohmann::json("Unknown");
                if (!result.success && !result.message.empty()) {
                    output["error"] = result.message;
                }
                output["results"] = result.final_context;
                output["total_steps"] = 1;
                return output;
            } catch (const std::exception& e) {
                return error_result("Unknown", e.what());
            }
        }
    );

    // ============================================================
    // loop/process_task — 单分支执行器 (C1, design D3)
    // ============================================================
    registry.register_tool_function(
        "loop/process_task",
        ::agenticdsl::ToolMetadata{
            .name = "loop/process_task",
            .description = "Execute a single task via one bridged LLM call",
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
            // 获取参数
            std::string task_id = str_arg(args, "task_id");
            if (task_id.empty()) {
                return error_result("InvalidParams", "Missing 'task_id' argument");
            }
            std::string input = str_arg(args, "input");
            if (input.empty()) {
                return error_result("InvalidParams", "Missing 'input' argument");
            }

            // 需要 parent provider
            if (!tls_parent_provider) {
                return error_result("Unknown",
                    "parent provider not set. Call loop/set_parent_provider first.");
            }

            try {
                // 构造 prompt: 单线程单次 LLM 调用
                std::string prompt = "Task " + task_id + ": " + input;

                ::agenticdsl::GenerationRequest req(prompt);
                auto avail = tls_parent_provider->available_models();
                if (!avail.empty()) {
                    req.params.model = avail.front().name;
                }

                auto res = tls_parent_provider->generate(req, std::stop_token{});
                if (res.has_value()) {
                    nlohmann::json out = ok_result(true);
                    out["branch_id"] = task_id;
                    out["result"] = std::move(res).value().text;
                    return out;
                } else {
                    return error_result("Unknown", res.error().message);
                }
            } catch (const std::exception& e) {
                return error_result("Unknown", e.what());
            }
        }
    );

    // ============================================================
    // loop/set_capture_mode — Data-RSI 采集开关 MVP stub (C1, design D4)
    // ============================================================
    registry.register_tool_function(
        "loop/set_capture_mode",
        ::agenticdsl::ToolMetadata{
            .name = "loop/set_capture_mode",
            .description = "Switch thread-local capture mode (None/Training)",
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
            std::string mode = str_arg(args, "mode");
            if (mode == "None") {
                tls_capture_mode.store(0, std::memory_order_relaxed);
                return ok_result(true);
            } else if (mode == "Training") {
                tls_capture_mode.store(1, std::memory_order_relaxed);
                return ok_result(true);
            } else {
                return error_result("InvalidParams",
                    "Invalid capture mode: '" + mode + "'. Must be 'None' or 'Training'.");
            }
        }
    );

    // ============================================================
    // loop/run — 注册 loop/run 工具 (C0 ship, C1 重构 ProviderLLMTool)
    // ============================================================
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

            // loop_type 合法性校验
            if (loop_type != "react" && loop_type != "plan_execute" && loop_type != "fork_join") {
                return {{"ok", false},
                        {"success", false},
                        {"error_code", "InvalidParams"},
                        {"error", "Invalid loop_type: '" + loop_type +
                         "'. Must be one of: react, plan_execute, fork_join"}};
            }

            // 可选 bus 注入
            ::agenticdsl::IInteractionBus* bus = nullptr;
            {
                auto bus_it = args.find("bus_ptr");
                if (bus_it != args.end() && !bus_it->second.empty()) {
                    bus = reinterpret_cast<::agenticdsl::IInteractionBus*>(
                        std::stoull(bus_it->second));
                }
            }
            std::string session_id = str_arg(args, "session_id");

            // §4.0.10 chat-async-io-consumer-loop: resolve from shared g_cancellation_registry
            std::string cancellation_id = str_arg(args, "cancellation_id");
            std::stop_token cancellation_token;
            if (!cancellation_id.empty() && hydraforge::pdk::g_cancellation_registry) {
                cancellation_token =
                    hydraforge::pdk::g_cancellation_registry->resolve_token(cancellation_id);
            }

            // Mock fallback when parent provider not set
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
                output["ok"] = true;
                output["error_code"] = nullptr;
                return output;
            }

            // 收到取消请求时提前返回
            if (cancellation_token.stop_requested()) {
                nlohmann::json cancelled_result;
                cancelled_result["success"] = false;
                cancelled_result["ok"] = false;
                cancelled_result["error_code"] = "Cancelled";
                cancelled_result["error"] = "cancelled";
                cancelled_result["response"] = "";
                cancelled_result["steps"] = 0;
                cancelled_result["tokens_used"] = 0;
                cancelled_result["cost_usd"] = 0.0;
                return cancelled_result;
            }

            // Real DSL execution path
            try {
                auto agent_content = load_agent_file(loop_type);

                auto child = ::agenticdsl::DSLEngine::from_markdown(
                    agent_content, *tls_parent_provider);

                // 使用文件级 ProviderLLMTool (C1 重构)
                child->register_llm_tool(
                    "llama-default",
                    std::make_unique<ProviderLLMTool>(*tls_parent_provider, cancellation_token));

                ::agenticdsl::LayeredContext ctx;
                ctx.working["user_input"] = user_prompt;
                ctx.working["system_prompt"] = str_arg(args, "system_prompt");
                ctx.working["history"] = str_arg(args, "history");

                // ADR-0068 附录 A: loop.turn.start
                emit_loop_event(bus, session_id, "loop.turn.start",
                                {{"turn", 1}, {"step", 1}});

                // ADR-0068 附录 A: loop.decision
                emit_loop_event(bus, session_id, "loop.decision",
                                {{"decision", "tool_call"}, {"tool", "loop/run"}});

                auto result = child->run(ctx);

                // ADR-0068 附录 A: loop.turn.end
                emit_loop_event(bus, session_id, "loop.turn.end",
                                {{"turn", 1},
                                 {"decision", result.success ? "respond" : "give_up"}});

                nlohmann::json output;
                output["success"] = result.success;
                output["ok"] = result.success;
                output["error_code"] = result.success ? nullptr : nlohmann::json("Unknown");
                output["error"]   = result.success ? "" : result.message;
                std::string response_text;
                for (const char* k : {"response", "output",
                                      "llm_response", "plan_response",
                                      "final_result"}) {
                    auto v = result.final_context.find(k);
                    if (v != result.final_context.end() && v->is_string() &&
                        !v->get<std::string>().empty()) {
                        response_text = v->get<std::string>();
                        break;
                    }
                }
                if (response_text.empty()) response_text = result.message;
                output["response"] = response_text;
                output["steps"]  = 1;
                output["tokens_used"] = 0;
                output["cost_usd"]    = 0.0;
                return output;
            } catch (const std::exception& e) {
                return {{"ok", false}, {"success", false}, {"error_code", "Unknown"},
                        {"error", e.what()},
                        {"response", ""}, {"steps", 0}, {"tokens_used", 0}, {"cost_usd", 0.0}};
            }
        }
    );
}