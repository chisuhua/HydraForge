// tests/test_t1_t7_integration_chain.cpp
// Sprint 24 审计补全：B6 集成闭环验证 — 串行 Sprint 24 change 真实接口
// 当前覆盖 T1C append_graphs_callback (核心冒烟); T4/T3/T5 各自已有独立测试, 不重复。

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

TEST_CASE("T1C append_graphs_callback chain smoke test",
          "[executor][integration][stage0]") {
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