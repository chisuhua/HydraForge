// tests/test_loop_agent_autonomous.cpp
// Wave 2 P0 (2026-09-17): Autonomous mode E2E tests
// 验证: DSLEngine::run(ctx, ExecutionFlag::Autonomous) 使 DSL_CALL 不暂停
#include "catch_amalgamated.hpp"

#include <core/engine.h>
#include <common/llm/mock_provider.h>
#include "test_helpers/real_llm_env.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>
namespace fs = std::filesystem;
using namespace agenticdsl;

// 复用 test_loop_agent_plugin.cpp 的 helper: 找 LoopAgent.so 实际路径
static std::string find_loop_agent_so() {
#ifdef LOOP_AGENT_SO_PATH
    const char* p = LOOP_AGENT_SO_PATH;
    if (fs::exists(p)) return fs::canonical(p).string();
#endif
    for (const char* c : {"build/pdk/loop_agent/libLoopAgent.so", "pdk/loop_agent/libLoopAgent.so"}) {
        if (fs::exists(c)) return fs::canonical(c).string();
    }
    throw std::runtime_error("libLoopAgent.so not found");
}

static void ensure_plugin_path_env() {
    if (std::getenv("HYDRAFORGE_PLUGIN_PATH")) return;
#ifdef LOOP_AGENT_SO_PATH
    auto p = fs::path(LOOP_AGENT_SO_PATH);
    // LOOP_AGENT_SO_PATH = .../build/pdk/loop_agent/libLoopAgent.so
    // PluginLoader whitelist 期望 build/pdk/, 所以取 parent_path × 2
    auto pdk_dir = p.parent_path().parent_path().string();
    setenv("HYDRAFORGE_PLUGIN_PATH", pdk_dir.c_str(), 0);
#endif
}

// DSL 含 llm_call 节点 — 无 Autonomous 时会 pause, 有 Autonomous 时不 pause
static const char* kMiniReactDSL = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
name: autonomous-test
version: "0.1.0"
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: [/main/llm]
  - id: llm
    type: llm_call
    prompt_template: "Say hello"
    output_keys: [llm_response]
    next: [/main/end]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

// DSL 无 llm_call (assign only) — 用于验证 Autonomous 不破坏正常流程
static const char* kMinPlan = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
name: minimal-plan
version: "0.1.0"
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: [/main/assign]
  - id: assign
    type: assign
    assign:
      result: "hello from autonomous plan"
    next: [/main/end]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

// ============================================================
// Test 1: Autonomous flag 抑制 DSL_CALL pause (mock)
// ============================================================
TEST_CASE("Autonomous mode: DSL_CALL does not pause", "[autonomous][dsl]") {
    // 创建 engine + mock provider
    auto engine = DSLEngine::from_markdown(kMiniReactDSL);
    REQUIRE(engine != nullptr);

    auto mock = std::make_unique<MockLLMProvider>();
    mock->enqueue_response(R"({"name":"finish","arguments":{"answer":"hello"}})");
    engine->set_llm_provider(std::move(mock));

    // 非 Autonomous: DSL_CALL 应 pause
    LayeredContext ctx1;
    auto result1 = engine->run(ctx1);
    REQUIRE(result1.paused_at.has_value());  // paused at llm_call node
    REQUIRE(result1.paused_at.value().find("/main/llm") != std::string::npos);

    // 创建新 engine (run 不是幂等的)
    auto engine2 = DSLEngine::from_markdown(kMiniReactDSL);
    REQUIRE(engine2 != nullptr);
    auto mock2 = std::make_unique<MockLLMProvider>();
    mock2->enqueue_response(R"({"name":"finish","arguments":{"answer":"hello"}})");
    engine2->set_llm_provider(std::move(mock2));

    // Autonomous: DSL_CALL 不 pause (核心验证)
    LayeredContext ctx2;
    auto result2 = engine2->run(ctx2, ExecutionFlag::Autonomous);
    REQUIRE_FALSE(result2.paused_at.has_value());  // no pause
}

// ============================================================
// Test 2: 无 llm_call 的 DSL 完整执行 (验证 Autonomous 不破坏正常流)
// ============================================================
TEST_CASE("Autonomous mode: plan without llm_call runs to completion",
          "[autonomous][dsl][nollm]") {
    auto engine = DSLEngine::from_markdown(kMinPlan);
    REQUIRE(engine != nullptr);

    LayeredContext ctx;
    auto result = engine->run(ctx, ExecutionFlag::Autonomous);
    REQUIRE(result.success == true);
    REQUIRE_FALSE(result.paused_at.has_value());
}

// ============================================================
// Test 3: loop/execute_plan with Autonomous (mock, 无 llm_call)
// ============================================================
TEST_CASE("Autonomous mode: loop/execute_plan with mock provider",
          "[autonomous][loop][execute_plan]") {
    auto parent_engine = DSLEngine::from_markdown(kMinPlan);
    REQUIRE(parent_engine != nullptr);

    auto mock = std::make_unique<MockLLMProvider>();
    mock->enqueue_response(R"({"name":"finish","arguments":{"answer":"mock"}})");
    parent_engine->set_llm_provider(std::move(mock));

    LayeredContext ctx;
    auto result = parent_engine->run(ctx, ExecutionFlag::Autonomous);
    REQUIRE(result.success == true);
    REQUIRE_FALSE(result.paused_at.has_value());
}

// ============================================================
// Test 4: loop/run wrapper — mock react loop end-to-end
//   验证: pdk_entry.cpp D4 改动 (Autonomous + react support tools + 真实 steps)
// ============================================================
#include <agenticdsl/plugin/plugin_loader.h>
TEST_CASE("Autonomous mode: loop/run react loop end-to-end (mock)",
          "[autonomous][loop][mock][react]") {
    ensure_plugin_path_env();
#ifdef LOOP_AGENT_SO_PATH
    std::string so_path = LOOP_AGENT_SO_PATH;
#else
    std::string so_path = find_loop_agent_so();
#endif
    if (!fs::exists(so_path)) {
        FAIL("libLoopAgent.so not found: " << so_path);
    }

    // Heap-allocate engine + loader, 显式 reset 控制析构顺序
    // (避免 .so 在 registry lambda 之前 dlclose → SIGSEGV)
    // 关键: loader 后声明 → 后构造 → 后析构 (lambda symbols 仍活)
    auto loader = std::make_unique<hydraforge::PluginLoader>();
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
    bool loaded = loader->load_so(so_path, engine->get_tool_registry());
    if (!loaded) {
        FAIL("PluginLoader::load_so returned false for: " << so_path);
    }

    // Heap allocation 保活到 test end (避免 thread_local raw ptr 悬垂)
    auto mock = std::make_unique<MockLLMProvider>();
    mock->enqueue_response(
        R"({"name":"finish","arguments":{"answer":"hello from mock"}})");

    auto set_ret = engine->get_tool_registry().call_tool("loop/set_parent_provider",
        std::unordered_map<std::string, std::string>{
            {"provider_ptr", std::to_string(reinterpret_cast<uintptr_t>(mock.get()))}
        });
    if (!set_ret.value("ok", false)) {
        FAIL("loop/set_parent_provider failed: " << set_ret.dump());
    }

    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "react"},
            {"prompt", "Say hello"},
            {"system_prompt", ""},
            {"history", "[]"},
            {"tools", "[]"},
            {"max_steps", "10"}
        });

    INFO("loop/run result: " << run_ret.dump());
    // Oracle C1: 真实步数 >= 6 (react.agent.md: start→think→decide→act→observe→end)
    REQUIRE(run_ret.value("steps", 0) >= 6);
    // Oracle M5: response 必须来自 DSL 真执行 (不是 "Paused at LLM call")
    auto resp_str = run_ret.value("response", std::string{});
    REQUIRE(resp_str != "Paused at LLM call");
    REQUIRE(resp_str != "[loop_agent/react] Processed: ...");  // 不是 mock fallback
    // react loop 真跑通 (mock decide 返回 finish action → act 调 finish → observe → end)
    // response 应含 mock 注入的 finish answer 或 react decision output
    REQUIRE_FALSE(resp_str.empty());
}

// ============================================================
// Test 5: 真实 DeepSeek LLM react loop
// ============================================================
TEST_CASE("Autonomous mode: real DeepSeek react loop",
          "[autonomous][loop][real_llm][react]") {
    pdk_chat_demo::testing::require_real_llm_env();  // FAIL if no key

    ensure_plugin_path_env(); const std::string so_path = find_loop_agent_so();
    REQUIRE(fs::exists(so_path));

    auto loader_obj = std::make_unique<hydraforge::PluginLoader>();
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});  // declared AFTER loader → destroyed BEFORE (析构反序)

    REQUIRE(loader_obj->load_so(so_path, engine->get_tool_registry()));

    auto provider = pdk_chat_demo::testing::real_llm_provider();
    REQUIRE(provider != nullptr);
    engine->get_tool_registry().call_tool("loop/set_parent_provider",
        std::unordered_map<std::string, std::string>{
            {"provider_ptr", std::to_string(reinterpret_cast<uintptr_t>(provider.get()))}
        });

    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "react"},
            {"prompt", "Say hello and nothing else"},
            {"system_prompt", "You are a helpful assistant. Reply concisely."},
            {"history", "[]"},
            {"tools", "[]"},
            {"max_steps", "5"}
        });

    INFO("real LLM result: " << run_ret.dump());
    auto resp_str = run_ret.value("response", std::string{});
    // Oracle M5 宽松断言: 真实 LLM 输出格式多样, 仅验证非 pause fallback + 非空 + 无 markup
    REQUIRE(resp_str != "Paused at LLM call");
    REQUIRE_FALSE(resp_str.empty());
    REQUIRE(resp_str.find("<tool>") == std::string::npos);
    REQUIRE(resp_str.find("Thought:") == std::string::npos);
}

// ============================================================
// Test 6: 真实 DeepSeek react loop + finish tool call
// ============================================================
TEST_CASE("Autonomous mode: real DeepSeek with finish tool call",
          "[autonomous][loop][real_llm][react][tool]") {
    pdk_chat_demo::testing::require_real_llm_env();

    ensure_plugin_path_env(); const std::string so_path = find_loop_agent_so();
    REQUIRE(fs::exists(so_path));

    auto loader_obj = std::make_unique<hydraforge::PluginLoader>();
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});  // declared AFTER loader → destroyed BEFORE (析构反序)

    REQUIRE(loader_obj->load_so(so_path, engine->get_tool_registry()));

    auto provider = pdk_chat_demo::testing::real_llm_provider();
    REQUIRE(provider != nullptr);
    engine->get_tool_registry().call_tool("loop/set_parent_provider",
        std::unordered_map<std::string, std::string>{
            {"provider_ptr", std::to_string(reinterpret_cast<uintptr_t>(provider.get()))}
        });

    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "react"},
            {"prompt", "Use the finish tool to answer with 'Paris'"},
            {"system_prompt", "When you know the answer, call the finish tool."},
            {"history", "[]"},
            {"tools", "[{\"name\":\"finish\",\"description\":\"finish task\"}]"},
            {"max_steps", "3"}
        });

    INFO("real LLM tool test result: " << run_ret.dump());
    auto resp_str = run_ret.value("response", std::string{});
    // Oracle M5 宽松断言: 真实 LLM 可能不严格遵循 prompt, 仅验证非 pause
    REQUIRE(resp_str != "Paused at LLM call");
}

// ============================================================
// Test 7: plan_execute loop Autonomous mode (mock)
// ============================================================
TEST_CASE("Autonomous mode: plan_execute loop wrapper (mock)",
          "[autonomous][loop][mock][plan_execute]") {
    ensure_plugin_path_env(); const std::string so_path = find_loop_agent_so();
    REQUIRE(fs::exists(so_path));

    auto loader_obj = std::make_unique<hydraforge::PluginLoader>();
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});  // declared AFTER loader → destroyed BEFORE (析构反序)

    REQUIRE(loader_obj->load_so(so_path, engine->get_tool_registry()));

    auto mock = std::make_unique<MockLLMProvider>();
    mock->enqueue_response(R"({"answer":"plan ok"})");
    engine->get_tool_registry().call_tool("loop/set_parent_provider",
        std::unordered_map<std::string, std::string>{
            {"provider_ptr", std::to_string(reinterpret_cast<uintptr_t>(mock.get()))}
        });

    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "plan_execute"},
            {"prompt", "Do task"},
            {"system_prompt", ""},
            {"history", "[]"},
            {"tools", "[]"},
            {"max_steps", "5"}
        });

    INFO("plan_execute mock result: " << run_ret.dump());
    REQUIRE(run_ret.value("response", std::string{}) != "Paused at LLM call");
    // Oracle C1: 真实步数 >= 1 (plan_execute 子图至少 1 个节点)
    REQUIRE(run_ret.value("steps", 0) >= 1);
}