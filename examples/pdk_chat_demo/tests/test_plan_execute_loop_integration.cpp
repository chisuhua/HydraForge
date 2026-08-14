// examples/pdk_chat_demo/tests/test_plan_execute_loop_integration.cpp
// 文件头注释
// 功能描述：PDK PlanExecuteLoop 集成测试 (Phase 6a U1, pdk-chat-demo-plan-execute-fork-join)。
//          验证 end-to-end: MockLLMProvider → PlanExecuteLoop.run() → LoopResult
//          1 个 TEST_CASE: plan success + verify success → Done
// 设计依据：openspec/changes/pdk-chat-demo-plan-execute-fork-join
//          + include/agenticdsl/pdk/agent_loops/plan_execute_loop.h
// 作者：HydraForge Phase 6a
// 最后修改日期：2026-08-14

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
#include <string>
#include <queue>

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

// 辅助: 从 provider 解包 MockLLMProvider
agenticdsl::MockLLMProvider* unwrap_to_mock(agenticdsl::ILLMProvider* p) {
  if (!p) return nullptr;
  if (auto* m = dynamic_cast<agenticdsl::MockLLMProvider*>(p)) return m;
  if (auto* d = dynamic_cast<agenticdsl::ILLMProviderDecorator*>(p))
    return dynamic_cast<agenticdsl::MockLLMProvider*>(d->inner());
  return nullptr;
}

// 创建配置 MockLLM 的 DSLEngine
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

}  // namespace

// =====================================================================
// Test 1: PlanExecuteLoop end-to-end — plan success + verify success → Done
// 验证:
//   - PlanExecuteLoop.run() 返回 LoopResult
//   - success == true
//   - retries_used == 0
//   - total_steps == 1
//   - state() == Done
// =====================================================================
TEST_CASE("PlanExecuteLoop: plan success + verify success → Done",
          "[pdk_chat_demo][plan_execute][integration]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();

  // 预设 MockLLM 响应队列:
  // 1. plan_phase: 返回 DSL 片段
  // 2. verify_phase: 返回 "yes" (验证成功)
  std::queue<agenticdsl::GenerationResult> responses;
  responses.push(agenticdsl::GenerationResult{kMinimalValidDsl, 0, 0, ""});
  responses.push(agenticdsl::GenerationResult{"yes, the task succeeded", 0, 0, ""});

  auto engine = make_engine_with_mock_responses(std::move(responses));
  PlanExecuteLoop loop(std::move(engine), bus);

  agenticdsl::LayeredContext ctx;
  LoopResult result = loop.run("研究量子计算", ctx);

  INFO("result.message: " << result.message);
  INFO("result.failed_phase: "
       << (result.failed_phase.has_value() ? *result.failed_phase : "none"));
  REQUIRE(result.success == true);
  REQUIRE(result.message == "PlanExecuteLoop: completed successfully");
  REQUIRE(result.retries_used == 0);
  REQUIRE(result.total_steps == 1);
  REQUIRE_FALSE(result.failed_phase.has_value());
  REQUIRE(loop.state() == PlanExecuteLoop::State::Done);
}
