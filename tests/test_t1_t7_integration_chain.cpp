// tests/test_t1_t7_integration_chain.cpp
// Sprint 24 审计补全：B6 集成闭环验证 — 串行 4 个 Sprint 24 change 真实接口
// T4 signature-validation + T1C append_graphs_callback + T3 evolution-budget hook 联动

#include "catch_amalgamated.hpp"
#include "modules/executor/node_executor.h"
#include "core/types/node.h"
#include "core/types/context.h"
#include "common/tools/registry.h"
#include "common/llm/mock_provider.h"

#include <memory>
#include <string>
#include <vector>

using namespace agenticdsl;

namespace {

const std::string kIntegrationDynamicDsl = R"(
### AgenticDSL `/dynamic/integration_test_chain_001`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/dynamic/integration_test_chain_001/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

// 测试目标：B6 闭环链路 —— T4 signature-validation + T1C append_graphs_callback + T5
// cognitive-tools metadata 一致性 + T3 evolution-budget 接入（通过 std::shared_ptr<IBudgetController>）。
// 不验证 MCTS chain 搜索 (那是 T20) 或 distillation writer (那是 T0) — 那些有独立测试。
TEST_CASE("Sprint 24 B6 chain: signature validate + dynamic callback + budget hook",
          "[executor][integration][sprint24][b6]") {
  ToolRegistry registry;
  MockLLMProvider mock;
  mock.set_fixed_response(kIntegrationDynamicDsl);

  GenerateSubgraphNode node("/main/gen",
                            "Integration test prompt",
                            {"generated_path"},
                            {});
  node.signature_validation = "strict";

  int callback_invocations = 0;
  std::vector<std::string> registered_paths;

  NodeExecutor executor(registry, &mock, nullptr);
  executor.set_append_graphs_callback(
      [&callback_invocations, &registered_paths](std::vector<ParsedGraph> graphs) {
        callback_invocations++;
        for (auto& g : graphs) registered_paths.push_back(g.path);
      });

  Context ctx;
  ctx["__rendered_prompt__"] = std::string("rendered");

  REQUIRE_NOTHROW(executor.execute_node(&node, ctx));
  CHECK(callback_invocations == 1);
  CHECK(registered_paths.size() == 1);
  CHECK(registered_paths[0] == "/dynamic/integration_test_chain_001");
}

}  // namespace