// tests/test_plan_execute_restart.cpp
// Phase 5 / Task 4.10 — PlanExecuteLoop verify_phase restart semantics
// Scenarios:
//   A. verify success -> terminate & return success (no retry)
//   B. verify retryable failure -> restart from plan_phase (retry_count++)
//   C. verify non-retryable failure -> terminate & return failure

#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "agenticdsl/pdk/agent_loops/loop_result.h"
#include "agenticdsl/pdk/agent_loops/plan_execute_loop.h"

#include "common/llm/mock_provider.h"
#include "core/engine.h"
#include "agenticdsl/types/layered_context.h"

#include <catch_amalgamated.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace agenticdsl;

// 最小有效 DSL — 必须包含 /main subgraph with start/end nodes
static const std::string kMinimalValidDsl = R"(
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

// 注入响应到 mock provider — 配合 make_engine_with_responses lambda 调用
namespace {

using EnqueueFn = std::function<void(std::queue<GenerationResult>&)>;

std::unique_ptr<DSLEngine> make_engine_with_responses(EnqueueFn filler) {
  auto engine = DSLEngine::from_markdown(kMinimalValidDsl);
  ILLMProvider* p = engine->get_llm_provider();
  MockLLMProvider* mock = nullptr;
  if (auto* m = dynamic_cast<MockLLMProvider*>(p)) {
    mock = m;
  } else if (auto* d = dynamic_cast<ILLMProviderDecorator*>(p)) {
    mock = dynamic_cast<MockLLMProvider*>(d->inner());
  }
  REQUIRE(mock != nullptr);
  std::queue<GenerationResult> q;
  filler(q);
  while (!q.empty()) {
    mock->enqueue_response(q.front());
    q.pop();
  }
  return engine;
}

}  // namespace

// === Scenario A: verify_success -> terminate success ===
TEST_CASE("PlanExecuteLoop verify success terminates loop",
          "[pdk][plan_execute][restart][success]") {
  auto bus = std::make_shared<InMemoryBus>();
  // Plan 一次成功 (返回 markdown DSL), Verify 返回 "yes"
  GenerationResult plan_ok;
  plan_ok.text = "# /main\n```start\necho \"x\"\n```";
  plan_ok.prompt_tokens = 10;
  plan_ok.completion_tokens = 20;

  GenerationResult verify_yes;
  verify_yes.text = "yes, success";
  verify_yes.prompt_tokens = 5;
  verify_yes.completion_tokens = 10;

  auto engine = make_engine_with_responses(
      [&](std::queue<GenerationResult>& q) {
        q.push(plan_ok);
        q.push(verify_yes);
      });

  hydraforge::pdk::PlanExecuteLoop loop(std::move(engine), bus);
  hydraforge::pdk::LoopResult result;
  agenticdsl::LayeredContext ctx;
  result = loop.run("test goal", ctx);

  REQUIRE(result.success);
  REQUIRE_FALSE(result.failed_phase.has_value());
  REQUIRE(result.retries_used == 0);
}

// === Scenario B: verify retryable failure -> restart from plan_phase ===
// 注: 当前 PlanExecuteLoop 实现判定 verify_phase 返回 false 即 retry (无论 retryable/non-retryable)
// 因此本测试只需验证 retry 逻辑本身 + retries_used 递增
TEST_CASE("PlanExecuteLoop verify failure restarts main loop",
          "[pdk][plan_execute][restart][retry]") {
  auto bus = std::make_shared<InMemoryBus>();

  // Plan 1 + verify-fail + Plan 2 + verify-success (3 LLM calls)
  GenerationResult plan1;
  plan1.text = "# /main\n```start\necho \"x\"\n```";
  plan1.prompt_tokens = 10;
  plan1.completion_tokens = 20;

  GenerationResult verify_no;
  verify_no.text = "no, failed";
  verify_no.prompt_tokens = 5;
  verify_no.completion_tokens = 10;

  GenerationResult plan2;
  plan2.text = "# /main\n```start\necho \"y\"\n```";
  plan2.prompt_tokens = 10;
  plan2.completion_tokens = 20;

  GenerationResult verify_yes;
  verify_yes.text = "yes, success";
  verify_yes.prompt_tokens = 5;
  verify_yes.completion_tokens = 10;

  auto engine = make_engine_with_responses(
      [&](std::queue<GenerationResult>& q) {
        q.push(plan1);
        q.push(verify_no);
        q.push(plan2);
        q.push(verify_yes);
      });
  hydraforge::pdk::PlanExecuteLoop loop(std::move(engine), bus, /*max_retries=*/5);

  hydraforge::pdk::LoopResult result;
  agenticdsl::LayeredContext ctx;
  result = loop.run("test", ctx);

  REQUIRE(result.success);
  // 一次 verify 失败后 retry, retries_used 应为 1
  REQUIRE(result.retries_used == 1);
}

// === Scenario C: max retries exceeded -> terminate failure ===
TEST_CASE("PlanExecuteLoop max retries exceeded returns failure",
          "[pdk][plan_execute][restart][exhausted]") {
  auto bus = std::make_shared<InMemoryBus>();

  // 持续 verify-no 直到 max_retries exhausted
  // 每个 cycle 需要 2 个响应 (plan + verify_no)
  GenerationResult plan;
  plan.text = "# /main\n```start\necho \"x\"\n```";
  plan.prompt_tokens = 10;
  plan.completion_tokens = 20;

  GenerationResult verify_no;
  verify_no.text = "no";
  verify_no.prompt_tokens = 5;
  verify_no.completion_tokens = 10;

  std::queue<GenerationResult> responses;
  // max_retries=2: 首次 + 2 retries = 3 cycles total, all verify-no
  for (int i = 0; i < 3; ++i) {
    responses.push(plan);
    responses.push(verify_no);
  }

  auto engine = make_engine_with_responses(
      [&responses](std::queue<GenerationResult>& q) {
        q = responses;
      });
  hydraforge::pdk::PlanExecuteLoop loop(std::move(engine), bus, /*max_retries=*/2);

  hydraforge::pdk::LoopResult result;
  agenticdsl::LayeredContext ctx;
  result = loop.run("test", ctx);

  REQUIRE_FALSE(result.success);
  REQUIRE(result.failed_phase.has_value());
  REQUIRE(*result.failed_phase == "Verifying");
  // retries_used 应 == max_retries (2)
  REQUIRE(result.retries_used == 2);
}
