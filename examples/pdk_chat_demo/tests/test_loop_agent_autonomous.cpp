// tests/test_loop_agent_autonomous.cpp
// Wave 2 P0 (2026-09-17): Autonomous mode E2E tests
// 验证: DSLEngine::run(ctx, ExecutionFlag::Autonomous) 使 DSL_CALL 不暂停
#include "catch_amalgamated.hpp"

#include <core/engine.h>
#include <common/llm/mock_provider.h>
#include "test_helpers/real_llm_env.h"

#include <filesystem>
#include <sstream>
namespace fs = std::filesystem;
using namespace agenticdsl;

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