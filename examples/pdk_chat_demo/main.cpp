// main.cpp - PDK Chat Demo 入口
// 关联: docs/examples/pdk_chat_demo/DESIGN.md
//      docs/adr/adr-0052 ~ adr-0065

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>

#include <nlohmann/json.hpp>

#include "chat_session.h"
#include "event_handler.h"

// HydraForge AgenticOS 核心
#include <core/engine.h>
#include <agenticdsl/types/layered_context.h>
#include <core/types/tool_result.h>
#include <core/types/budget.h>
#include <common/llm/llm_types.h>
#include <common/llm/mock_provider.h>
#include <common/llm/llm_config.h>
#include <common/llm/llm_provider_factory.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <agenticdsl/contract/inmemory_bus.h>
#include <agenticdsl/plugin/plugin_loader.h>
#include <modules/budget/budget_controller.h>

#include <agenticdsl/skill/skill_interpreter.h>

namespace fs = std::filesystem;

namespace {

hydraforge::PluginLoader* g_loader = nullptr;
agenticdsl::IInteractionBus* g_bus = nullptr;

void unload_all_plugins(hydraforge::PluginLoader& loader) {
    for (const auto& info : loader.list_loaded()) {
        loader.unload_plugin(std::string(info.name));
    }
}

void signal_handler(int sig) {
    std::cerr << "\n[main] Caught signal " << sig << ", shutting down..." << std::endl;
    if (g_bus) {
        g_bus->emit(agenticdsl::BusEvent{"app.shutdown", agenticdsl::ToolResult{
            .ok = true,
            .meta = {{"signal", sig}}
        }, std::chrono::steady_clock::now()});
    }
    if (g_loader) unload_all_plugins(*g_loader);
    std::exit(0);
}

}  // namespace

int main(int argc, char* argv[]) {
    // === SkillInterpreter 子进程早期分支（在 DSLEngine 初始化之前） ===
    if (argc > 1 && std::string(argv[1]) == "--skill-child") {
        return agenticdsl::skill_child_main(argc, argv);
    }

    bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");

    // T1.3: --session <id> CLI flag
    std::string session_id_to_load;
    {
        std::vector<std::string> args(argv + 1, argv + argc);
        for (size_t i = 0; i + 1 < args.size(); ++i) {
            if (args[i] == "--session") {
                session_id_to_load = args[i + 1];
            }
        }
    }

    // ============================================================
    // 1. 解析配置
    // ============================================================
    pdk_chat_demo::ChatConfig config;
    try {
        config = pdk_chat_demo::ChatConfig::from_json("config.json");
        config.validate();
        if (mock_mode) {
            config.override_provider("mock", "test");
            std::cout << "[main] Mock mode: provider=mock, model=test" << std::endl;
        } else {
            std::cout << "[main] Live mode: provider=" << config.agent.provider
                      << ", model=" << config.agent.model << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[main] Failed to load config: " << e.what() << std::endl;
        return 1;
    }

    // ============================================================
    // 2. 初始化 AgenticOS (Option 2: 使用 engine 内部 ToolRegistry)
    // ============================================================
    auto engine = std::make_unique<agenticdsl::DSLEngine>(
        std::vector<agenticdsl::ParsedGraph>{});
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();

    // 使用 ExecutionBudget 配置预算
    agenticdsl::ExecutionBudget budget_cfg;
    budget_cfg.max_llm_calls = static_cast<int>(config.agent.budget_limit_usd * 1000);
    budget_cfg.max_duration_sec = config.agent.timeout_ms / 1000;
    engine->get_budget_controller().set_budget(std::move(budget_cfg));

    engine->set_interaction_bus(bus);
    g_bus = bus.get();

    // ============================================================
    // 3. 加载所有 Plugin
    // ============================================================

    static constexpr std::string_view kPluginPathPrefix = "/pdk/";
    {
        std::string plugin_root;
        for (const auto& plugin_cfg : config.plugins) {
            if (plugin_cfg.type == "so") {
                auto pos = plugin_cfg.path.find(kPluginPathPrefix);
                if (pos != std::string::npos) {
                    plugin_root = plugin_cfg.path.substr(0, pos + kPluginPathPrefix.size());
                    break;
                }
            }
        }
        if (!plugin_root.empty()) {
            setenv("HYDRAFORGE_PLUGIN_PATH", plugin_root.c_str(), 1);
            std::cout << "[main] HYDRAFORGE_PLUGIN_PATH=" << plugin_root << std::endl;
        }
    }

    hydraforge::PluginLoader loader;
    g_loader = &loader;

    for (const auto& plugin_cfg : config.plugins) {
        if (plugin_cfg.type == "so") {
            try {
                if (!fs::exists(plugin_cfg.path)) {
                    std::cerr << "[main] Plugin not found: " << plugin_cfg.path
                              << " (skipping, demo mode)" << std::endl;
                    continue;
                }
                if (!loader.load_so(plugin_cfg.path, engine->get_tool_registry())) {
                    std::cerr << "[main] FAILED to load plugin: " << plugin_cfg.id
                              << " from " << plugin_cfg.path
                              << " (see PluginLoader WARN/ERROR above)" << std::endl;
                    continue;
                }
                std::cout << "[main] Loaded plugin: " << plugin_cfg.id
                          << " from " << plugin_cfg.path << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[main] Failed to load plugin " << plugin_cfg.id
                          << ": " << e.what() << std::endl;
            }
        } else if (plugin_cfg.type == "skill") {
            // 检查文件后缀决定加载方式
            if (plugin_cfg.path.size() > 9 &&
                plugin_cfg.path.substr(plugin_cfg.path.size() - 9) == ".skill.md") {
                // 命令式 .skill.md — 使用 SkillInterpreter 真实加载
                static bool skill_interpreter_initialized = false;
                static std::unique_ptr<agenticdsl::SkillInterpreter> si;
                if (!skill_interpreter_initialized) {
                    si = std::make_unique<agenticdsl::SkillInterpreter>(
                        engine->get_tool_registry(),
                        *bus,
                        engine->get_llm_provider(),
                        nullptr);
                    skill_interpreter_initialized = true;
                }
                auto cap = agenticdsl::default_skill_capability();
                auto result = si->run(plugin_cfg.path, cap);
                if (result.success) {
                    std::cout << "[main] Skill executed: " << plugin_cfg.id
                              << " (output: " << result.output.dump() << ")"
                              << std::endl;
                } else {
                    std::cerr << "[main] Skill FAILED: " << plugin_cfg.id
                              << " (error=" << static_cast<int>(result.error_code)
                              << ", stderr=" << result.stderr_content << ")"
                              << std::endl;
                }
            } else {
                // 旧式 SKILL.md LLM prompt template — 保持 mock-only
                std::cout << "[main] Skill registered (mock-only, requires SkillInterpreter ADR-0055): "
                          << plugin_cfg.id << std::endl;
            }
        }
    }

    // ============================================================
    // 4. 注册 provider configs
    // ============================================================
    try {
        // provider/register handler 解析 args["args"] 为 JSON
        // 见 pdk/provider_agent/src/pdk_entry.cpp:70
        std::unordered_map<std::string, std::string> provider_map;
        provider_map["args"] = config.providers.dump();
        engine->get_tool_registry().call_tool("provider/register", provider_map);
    } catch (const std::exception& e) {
        std::cerr << "[main] provider/register failed: " << e.what() << std::endl;
    }

    // ============================================================
    // 5. 解析默认 provider → 设置 LLM
    // ============================================================
    std::unique_ptr<agenticdsl::ILLMProvider> llm_provider;

    if (mock_mode) {
        // 直接使用 MockLLMProvider
        llm_provider = std::make_unique<agenticdsl::MockLLMProvider>();
        std::cout << "[main] Using MockLLMProvider" << std::endl;
    } else {
        // 通过 provider/resolve 工具获取 LLMConfig
        try {
            std::unordered_map<std::string, std::string> resolve_args;
            resolve_args["provider_id"] = config.agent.provider;
            resolve_args["model_id"] = config.agent.model;
            auto llm_cfg_json = engine->get_tool_registry().call_tool("provider/resolve", resolve_args);

            // 手动构造 LLMConfig（LLMConfig::from_json 不存在）
            agenticdsl::LLMConfig llm_cfg;
            llm_cfg.provider = llm_cfg_json.value("provider", config.agent.provider);
            llm_cfg.model = llm_cfg_json.value("model", config.agent.model);
            if (llm_cfg_json.contains("api_url"))
                llm_cfg.api_url = llm_cfg_json["api_url"].get<std::string>();
            if (llm_cfg_json.contains("api_key"))
                llm_cfg.api_key = llm_cfg_json["api_key"].get<std::string>();
            // 某些 provider (如百度千帆) 使用非标准 endpoint，需手动指定
            if (llm_cfg_json.contains("api_endpoint"))
                llm_cfg.api_endpoint = llm_cfg_json["api_endpoint"].get<std::string>();

            agenticdsl::LLMProviderFactory factory;
            llm_provider = factory.create(llm_cfg);
        } catch (const std::exception& e) {
            std::cerr << "[main] LLM setup failed: " << e.what() << std::endl;
            std::cerr << "[main] Falling back to MockLLMProvider" << std::endl;
            llm_provider = std::make_unique<agenticdsl::MockLLMProvider>();
        }
    }

    engine->set_llm_provider(std::move(llm_provider));

    if (mock_mode) {
        auto* mock = dynamic_cast<agenticdsl::MockLLMProvider*>(
            engine->get_llm_provider());
        if (mock) {
            mock->enqueue_response(
                R"({"content":"I'll write a hello world in C++ for you.","tool_calls":[]})");
            mock->enqueue_response(
                R"({"content":"Here's the C++ code:\n\n```cpp\n#include <iostream>\nint main(){ std::cout << \"Hello, World!\" << std::endl; return 0; }\n```","tool_calls":[]})");
        }
    }

    // ============================================================
    // 5.5. 传递 LLM provider 给 Loop Agent (loop-agent-dsl-execution)
    // ============================================================
    {
        auto* provider = engine->get_llm_provider();
        if (provider) {
            std::stringstream ss;
            ss << reinterpret_cast<uintptr_t>(provider);
            std::unordered_map<std::string, std::string> set_provider_args;
            set_provider_args["provider_ptr"] = ss.str();
            auto result = engine->get_tool_registry().call_tool(
                "loop/set_parent_provider", set_provider_args);
            if (result.value("success", false)) {
                std::cout << "[main] Loop Agent provider configured" << std::endl;
            }
        }
    }

    // ============================================================
    // 6. 订阅事件 → 终端输出
    // ============================================================
    pdk_chat_demo::EventHandler handler(bus);

    // ============================================================
    // 7. ChatSession 初始化（使用 engine 内部 registry）
    // ============================================================
    // T1.9: 启动时清理 >24h 的 stale session 文件
    pdk_chat_demo::ChatSession::cleanup_stale(config.session.persist_dir);

    pdk_chat_demo::ChatSession session(
        engine.get(), bus, &engine->get_tool_registry(),
        config.agent, config.session
    );

    // T1.4: --session <id> 从磁盘恢复
    if (!session_id_to_load.empty()) {
        if (session.load_from_disk(session_id_to_load)) {
            std::cout << "[main] Session restored: " << session.session_id()
                      << " (" << session.history().size() << " messages)" << std::endl;
        } else {
            std::cout << "[main] Session not found/corrupted, starting new: "
                      << session.session_id() << std::endl;
        }
    }

    std::cout << "[main] Session started: " << session.session_id() << std::endl;
    std::cout << "[main] Type 'exit' or Ctrl-D to quit" << std::endl;
    std::cout << std::endl << "User> " << std::flush;

    // ============================================================
    // 8. 信号处理 + 交互循环
    // ============================================================
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "exit" || input == "quit") {
            break;
        }
        if (input.empty()) {
            std::cout << std::endl << "User> " << std::flush;
            continue;
        }

        auto result = session.chat(input);

        // T1 线程安全: 检查 budget alert atomic flag (dispatch 线程置位)
        if (session.consume_budget_alert()) {
            std::cerr << std::endl << "[⚠ Budget exceeded] cost limit reached" << std::endl;
        }

        if (result.success) {
            std::cout << std::endl << "Assistant: " << result.response << std::endl;
            std::cout << "  [steps=" << result.total_steps
                      << ", tokens=" << result.total_tokens
                      << ", cost=$" << result.cost_usd << "]" << std::endl;
        } else {
            std::cerr << std::endl << "[error] " << result.error_message << std::endl;
        }

        std::cout << std::endl << "User> " << std::flush;
    }

    // T1.3.3: shutdown 前显式检查 budget alert flag (防止 dispatch 未完成)
    if (session.consume_budget_alert()) {
        std::cerr << std::endl << "[⚠ Budget exceeded] final check on shutdown" << std::endl;
    }

    // T1: 落盘当前 session
    session.save_to_disk();

    // ============================================================
    // 9. 优雅退出 — 先销毁 ChatSession + DSLEngine（释放 ToolRegistry
    //    中的 plugin function ptr），再 unload plugin .so，避免
    //    dangling function pointer → SIGSEGV
    // ============================================================
    bus->emit(agenticdsl::BusEvent{"app.shutdown", agenticdsl::ToolResult{.ok = true, .meta = nullptr}, std::chrono::steady_clock::now()});
    // 跳出局部 scope 以销毁 ChatSession 和 DSLEngine
    //（它们的析构函数会清理 ToolRegistry 中的 plugin 引用）
    {
        pdk_chat_demo::ChatSession discard(nullptr, nullptr, nullptr, {}, {});
        engine.reset();
    }
    unload_all_plugins(loader);
    std::cout << std::endl << "[main] Goodbye!" << std::endl;

    return 0;
}
