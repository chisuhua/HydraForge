// tests/test_generate_subgraph_callback.cpp
// P0 断链修复测试 — GenerateSubGraphNode 调用 append_graphs_callback_
// 验证 execute_generate_subgraph() 在解析 LLM 输出后调用 set_append_graphs_callback 注入的回调。

#include "catch_amalgamated.hpp"
#include "modules/executor/node_executor.h"
#include "core/types/node.h"
#include "common/tools/registry.h"
#include "common/llm/mock_provider.h"
#include "core/types/context.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace agenticdsl;

namespace {

const std::string kValidDynamicDsl = R"(
### AgenticDSL `/dynamic/callback_test_001`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/dynamic/callback_test_001/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

TEST_CASE("GenerateSubgraph append_graphs_callback fires when set",
          "[executor][generate_subgraph][p0_fix][stage0]") {
  ToolRegistry registry;
  MockLLMProvider mock;
  mock.set_fixed_response(kValidDynamicDsl);

  GenerateSubgraphNode node("/main/gen",
                            "Generate a simple subgraph",
                            {"generated_path"},
                            {});

  int callback_count = 0;
  std::vector<std::string> captured_graph_paths;

  NodeExecutor executor(registry, &mock, nullptr);
  executor.set_append_graphs_callback(
      [&callback_count, &captured_graph_paths](std::vector<ParsedGraph> graphs) {
        callback_count++;
        for (auto& g : graphs) captured_graph_paths.push_back(g.path);
      });

  Context ctx;
  ctx["__rendered_prompt__"] = std::string("rendered");

  REQUIRE_NOTHROW(executor.execute_node(&node, ctx));

  CHECK(callback_count == 1);
  CHECK(captured_graph_paths.size() == 1);
  CHECK(captured_graph_paths[0] == "/dynamic/callback_test_001");
}

TEST_CASE("GenerateSubgraph callback wiring is bound to NodeExecutor instance",
          "[executor][generate_subgraph][p0_fix][stage0]") {
  ToolRegistry registry;
  MockLLMProvider mock;
  mock.set_fixed_response(kValidDynamicDsl);

  GenerateSubgraphNode node("/main/gen",
                            "Generate",
                            {"generated_path"},
                            {});

  NodeExecutor executor_a(registry, &mock, nullptr);
  NodeExecutor executor_b(registry, &mock, nullptr);

  int a_calls = 0;
  executor_a.set_append_graphs_callback(
      [&a_calls](std::vector<ParsedGraph>) { a_calls++; });

  Context ctx;
  ctx["__rendered_prompt__"] = std::string("rendered");
  REQUIRE_NOTHROW(executor_a.execute_node(&node, ctx));
  CHECK(a_calls == 1);

  int b_calls = 0;
  executor_b.set_append_graphs_callback(
      [&b_calls](std::vector<ParsedGraph>) { b_calls++; });
  REQUIRE_NOTHROW(executor_b.execute_node(&node, ctx));
  CHECK(b_calls == 1);
  CHECK(a_calls == 1);
}

TEST_CASE("GenerateSubgraph append_graphs_callback fires for multiple dynamic graphs",
          "[executor][generate_subgraph][p0_fix][stage0][multi]") {
  const std::string kMultiDynamicDsl = R"(
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
### AgenticDSL `/dynamic/multi_alpha`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/dynamic/multi_alpha/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
### AgenticDSL `/dynamic/multi_beta`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/dynamic/multi_beta/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

  ToolRegistry registry;
  MockLLMProvider mock;
  mock.set_fixed_response(kMultiDynamicDsl);

  GenerateSubgraphNode node("/main/gen",
                            "Multi-graph test",
                            {"generated_paths"},
                            {});

  std::vector<std::string> registered_paths;
  std::vector<std::string> registered_paths_call_count;

  NodeExecutor executor(registry, &mock, nullptr);
  executor.set_append_graphs_callback(
      [&registered_paths, &registered_paths_call_count](std::vector<ParsedGraph> graphs) {
        registered_paths_call_count.push_back(std::to_string(graphs.size()));
        for (auto& g : graphs) registered_paths.push_back(g.path);
      });

  Context ctx;
  ctx["__rendered_prompt__"] = std::string("rendered");
  REQUIRE_NOTHROW(executor.execute_node(&node, ctx));

  REQUIRE(registered_paths_call_count.size() >= 2);
  CHECK(registered_paths_call_count[0] == "1");
  CHECK(registered_paths_call_count[1] == "1");
  std::set<std::string> unique_paths(registered_paths.begin(), registered_paths.end());
  CHECK(unique_paths.size() == 2);
  CHECK(unique_paths.count("/dynamic/multi_alpha") == 1);
  CHECK(unique_paths.count("/dynamic/multi_beta") == 1);
}

}  // namespace