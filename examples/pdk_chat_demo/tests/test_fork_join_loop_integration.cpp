// examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp
// 文件头注释
// 功能描述：PDK ForkJoinLoop 集成测试 (Phase 6a U1, pdk-chat-demo-plan-execute-fork-join)。
//          验证 end-to-end: Mock provider → ForkJoinLoop.run() → LoopResult
//          1 个 TEST_CASE: 3 branches 全成功 → Done
// 设计依据：openspec/changes/pdk-chat-demo-plan-execute-fork-join
//          + include/agenticdsl/pdk/agent_loops/fork_join_loop.h
// 作者：HydraForge Phase 6a
// 最后修改日期：2026-08-14

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "agenticdsl/pdk/agent_loops/fork_join_loop.h"
#include "agenticdsl/types/layered_context.h"
#include "core/engine.h"
#include "core/types/tool_result.h"

#include <memory>
#include <string>
#include <vector>

using namespace hydraforge::pdk;

namespace {

// 最小 DSL 模板 (ForkJoinLoop 不直接执行 DSL, 但需构造 engine)
const std::string kMinimalDsl = R"(
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

std::unique_ptr<agenticdsl::DSLEngine> make_minimal_engine() {
  return agenticdsl::DSLEngine::from_markdown(kMinimalDsl);
}

}  // namespace

// =====================================================================
// Test 1: ForkJoinLoop end-to-end — 3 branches all succeed → Done
// 验证:
//   - ForkJoinLoop.run() 返回 LoopResult
//   - success == true
//   - total_steps == 3
//   - final_context.working["data"] 包含 3 个 branch 输出
//   - state() == Done
// =====================================================================
TEST_CASE("ForkJoinLoop: 3 branches all succeed → Done",
          "[pdk_chat_demo][fork_join][integration]") {
  auto bus = std::make_shared<agenticdsl::InMemoryBus>();
  auto engine = make_minimal_engine();

  ForkJoinLoop loop(std::move(engine), bus, /*num_threads=*/4);

  // 3 个并发分支
  std::vector<std::string> branches = {"search_web", "search_docs", "search_code"};

  agenticdsl::LayeredContext ctx;
  LoopResult result = loop.run(branches, ctx);

  REQUIRE(result.success == true);
  REQUIRE(result.message == "ForkJoinLoop: completed");
  REQUIRE(result.total_steps == 3);
  REQUIRE_FALSE(result.failed_phase.has_value());
  REQUIRE(loop.state() == ForkJoinLoop::State::Done);

  // 每个 branch 输出都合并到 final_context.working.data
  REQUIRE(result.final_context.working["data"].is_object());
  REQUIRE(result.final_context.working["data"].contains("search_web"));
  REQUIRE(result.final_context.working["data"].contains("search_docs"));
  REQUIRE(result.final_context.working["data"].contains("search_code"));

  // 验证每个 branch 的输出结构 (ForkJoinLoop 的默认 handler 返回 {branch_id, data: args})
  REQUIRE(result.final_context.working["data"]["search_web"]["branch_id"] == "search_web");
  REQUIRE(result.final_context.working["data"]["search_docs"]["branch_id"] == "search_docs");
  REQUIRE(result.final_context.working["data"]["search_code"]["branch_id"] == "search_code");
}
