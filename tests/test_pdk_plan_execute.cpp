// tests/test_pdk_plan_execute.cpp
// 文件头注释
// 功能描述：PDK PlanExecuteLoop 单元测试 (Phase 1 Sprint 20, ADR-0021 §3.2)。
//          5 个 TEST_CASE 覆盖:
//            1. 规划成功 + 验证成功 → Done (一次通过)
//            2. 规划成功 + 验证失败 + 重试成功 → Done (Retry 路径)
//            3. 规划失败 → 整体失败 (Planning 阶段)
//            4. MockLLMProvider 空响应 → 失败
//            5. Retry 3 次后仍失败 → 整体失败
// 设计依据：openspec/changes/pdk-plan-execute-fork-join (Sprint 20)
//          + ADR-0021 §3.2 + ADR-0020 SimpleCognitiveOrchestrator
// 作者：AgenticDSL Phase 1 Sprint 20
// 最后修改日期：2026-08-01

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/agent_loops/plan_execute_loop.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "agenticdsl/types/layered_context.h"
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"
#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "core/engine.h"

#include <memory>
#include <stdexcept>
#include <string>

using namespace hydraforge::pdk;

namespace {

// 最小 DSL 模板 (Plan 阶段生成后, engine 能解析为有效 start/end 子图)
const std::string kMinimalValidDsl = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

// 辅助: 创建配置 MockLLM 的 DSLEngine
namespace {
agenticdsl::MockLLMProvider* unwrap_to_mock(agenticdsl::ILLMProvider* p) {
  if (!p) return nullptr;
  if (auto* m = dynamic_cast<agenticdsl::MockLLMProvider*>(p)) return m;
  if (auto* d = dynamic_cast<agenticdsl::ILLMProviderDecorator*>(p))
    return dynamic_cast<agenticdsl::MockLLMProvider*>(d->inner());
  return nullptr;
}
}  // namespace
std::unique_ptr<agenticdsl::DSLEngine> make_engine_with_mock_responses(
    std::queue<agenticdsl::GenerationResult> responses) {
  auto engine = agenticdsl::DSLEngine::from_markdown(kMinimalValidDsl);
  if (auto* mock = unwrap_to_mock(engine->get_llm_provider())) {
    while (!responses.empty()) {
      mock->enqueue_response(responses.front());
      responses.pop();
    }
  }
  return engine;
}

} // namespace

// =====================================================================
// Test 1: 规划成功 + 验证成功 → Done (一次通过, retries_used=0)
// =====================================================================
TEST_CASE("PDK PlanExecuteLoop: plan success + verify success → Done",
          "[pdk][sprint20][plan_execute][success]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();

  std::queue<agenticdsl::GenerationResult> responses;
  responses.push(agenticdsl::GenerationResult{kMinimalValidDsl, 0, 0, ""});
  responses.push(agenticdsl::GenerationResult{"yes, succeeded", 0, 0, ""});

  auto engine = make_engine_with_mock_responses(responses);
  PlanExecuteLoop loop(std::move(engine), bus);

  agenticdsl::LayeredContext ctx;
  LoopResult result = loop.run("achieve goal", ctx);

  INFO("result.message: " << result.message);
  INFO("result.failed_phase: "
       << (result.failed_phase.has_value() ? *result.failed_phase : "none"));
  REQUIRE(result.success);
  REQUIRE(result.message == "PlanExecuteLoop: completed successfully");
  REQUIRE(result.retries_used == 0);
  REQUIRE(result.total_steps == 1);
  REQUIRE_FALSE(result.failed_phase.has_value());
  REQUIRE(loop.state() == PlanExecuteLoop::State::Done);
}

// =====================================================================
// Test 2: 规划成功 + 验证失败 + 重试成功 → Done (Retry 路径)
// =====================================================================
TEST_CASE("PDK PlanExecuteLoop: verify fail + retry success → Done",
          "[pdk][sprint20][plan_execute][retry]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();

  // Plan ×2 + Verify ×2: 第1次 verify=no, 第2次 verify=yes
  std::queue<agenticdsl::GenerationResult> responses;
  responses.push(agenticdsl::GenerationResult{kMinimalValidDsl, 0, 0, ""});
  responses.push(agenticdsl::GenerationResult{"no, not yet", 0, 0, ""});
  responses.push(agenticdsl::GenerationResult{kMinimalValidDsl, 0, 0, ""});
  responses.push(agenticdsl::GenerationResult{"yes, succeeded", 0, 0, ""});

  auto engine = make_engine_with_mock_responses(responses);
  PlanExecuteLoop loop(std::move(engine), bus, /*max_retries=*/3);

  agenticdsl::LayeredContext ctx;
  LoopResult result = loop.run("achieve goal", ctx);

  REQUIRE(result.success);
  REQUIRE(result.retries_used == 1);
  REQUIRE(result.total_steps == 2);  // 2 cycles of Plan→Execute→Verify
  REQUIRE(loop.state() == PlanExecuteLoop::State::Done);
}

// =====================================================================
// Test 3: 规划失败 (LLM 返回空) → 整体失败
// =====================================================================
TEST_CASE("PDK PlanExecuteLoop: plan failure (empty response) → fail",
          "[pdk][sprint20][plan_execute][plan_fail]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();

  std::queue<agenticdsl::GenerationResult> responses;
  responses.push(agenticdsl::GenerationResult{"", 0, 0, ""});  // 空响应

  auto engine = make_engine_with_mock_responses(responses);
  PlanExecuteLoop loop(std::move(engine), bus);

  agenticdsl::LayeredContext ctx;
  LoopResult result = loop.run("achieve goal", ctx);

  REQUIRE_FALSE(result.success);
  REQUIRE(result.failed_phase.has_value());
  REQUIRE(result.failed_phase.value() == "Planning");
  REQUIRE(loop.state() == PlanExecuteLoop::State::Done);
}

// =====================================================================
// Test 4: MockLLMProvider 模拟 error (LLM 调用失败)
// =====================================================================
TEST_CASE("PDK PlanExecuteLoop: LLM simulated error → fail",
          "[pdk][sprint20][plan_execute][llm_error]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();

  auto engine = make_engine_with_mock_responses({});
  if (auto* mock = unwrap_to_mock(engine->get_llm_provider())) {
    mock->set_simulate_error(agenticdsl::LLMError::Code::NetworkError,
                              "connection refused");
  }

  PlanExecuteLoop loop(std::move(engine), bus);

  agenticdsl::LayeredContext ctx;
  LoopResult result = loop.run("achieve goal", ctx);

  REQUIRE_FALSE(result.success);
  REQUIRE(result.failed_phase.has_value());
  REQUIRE(result.failed_phase.value() == "Planning");
  REQUIRE(loop.state() == PlanExecuteLoop::State::Done);
}

// =====================================================================
// Test 5: Retry 3 次后仍失败 → 整体失败 (Verify 持续失败)
// =====================================================================
TEST_CASE("PDK PlanExecuteLoop: 3 retries exhausted → fail",
          "[pdk][sprint20][plan_execute][retry_exhausted]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();

  // Plan ×4 (initial + 3 retries) + Verify ×4 (全 no)
  std::queue<agenticdsl::GenerationResult> responses;
  for (int i = 0; i < 4; ++i) {
    responses.push(agenticdsl::GenerationResult{kMinimalValidDsl, 0, 0, ""});
    responses.push(agenticdsl::GenerationResult{"no, failed", 0, 0, ""});
  }

  auto engine = make_engine_with_mock_responses(responses);
  PlanExecuteLoop loop(std::move(engine), bus, /*max_retries=*/3);

  agenticdsl::LayeredContext ctx;
  LoopResult result = loop.run("achieve goal", ctx);

  REQUIRE_FALSE(result.success);
  REQUIRE(result.retries_used == 3);  // 首次 + 3 retries 都失败
  REQUIRE(result.total_steps == 4);   // 4 cycles
  REQUIRE(result.failed_phase.has_value());
  REQUIRE(result.failed_phase.value() == "Verifying");
  REQUIRE(loop.state() == PlanExecuteLoop::State::Done);
}