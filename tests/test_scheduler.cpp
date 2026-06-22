// tests/test_scheduler.cpp
// Sprint 7 Day 2: 7 state-based TEST_CASE 覆盖 TopoScheduler::execute 拆分的 3 子函数:
//   prepare_dag_state / dispatch_next_node / finalize_execution (handle_node_completion 间接)
//   Day 1 I-2 fork regression test 推迟到 Day 5+ scheduler-pipeline-tightened (NodeExecutor::execute_fork
//   当前未实现, 无法通过 DSL 触发 is_executing_fork_branches_=true; Day 5+ 直接 unit test
//   私有 fork 处理方法锁定)
// 作者: Sprint 7 tech-debt-followup Task 2
// 最后修改日期: 2026-06-22

#include "catch_amalgamated.hpp"
#include "core/engine.h"
#include <iostream>
#include <string>

agenticdsl::Context run_dsl(const std::string& markdown) {
    auto engine = agenticdsl::DSLEngine::from_markdown(markdown);
    auto result = engine->run();
    REQUIRE(result.success);
    return result.final_context;
}

agenticdsl::ExecutionResult run_dsl_raw(const std::string& markdown) {
    auto engine = agenticdsl::DSLEngine::from_markdown(markdown);
    return engine->run();
}

// Test 1: Basic DAG with linear flow (sanity check)
TEST_CASE("Linear DAG Execution", "[stage1][scheduler]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: assign
    assign:
      step1: "done"
    next: /main/step2
  - id: step2
    type: assign
    assign:
      step2: "done"
    next: /main/end
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    REQUIRE(ctx.contains("step1"));
    REQUIRE(ctx.contains("step2"));
}

// Test 2: Concurrent branches with wait_for (all_of)
TEST_CASE("Concurrent Branches with wait_for", "[stage1][scheduler]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: assign
    assign:
      trigger: "go"
    next: ["/task/a", "/task/b"]
  - id: join
    type: assign
    assign:
      final: "{{ result_a }}+{{ result_b }}"
    wait_for:
      all_of: ["/task/a", "/task/b"]
    next: /main/end
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```

### AgenticDSL `/task/a`
```yaml
# --- BEGIN AgenticDSL ---
type: assign
assign:
  result_a: "A"
next: "/main/join"
# --- END AgenticDSL ---
```

### AgenticDSL `/task/b`
```yaml
# --- BEGIN AgenticDSL ---
type: assign
assign:
  result_b: "B"
next: "/main/join"
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    REQUIRE(ctx["final"] == "A+B");
}

// Test 3: soft termination allows parent flow to continue
TEST_CASE("Soft Termination Continues Parent Flow", "[stage1][scheduler]") {
    std::string markdown_with_dep = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: assign
    assign:
      before_lib: "yes"
    next: "/__system__/noop"
  - id: after_lib
    type: assign
    assign:
      after_lib: "executed"
    wait_for: ["/__system__/noop"]
    next: /main/end
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown_with_dep);
    REQUIRE(ctx["before_lib"] == "yes");
    REQUIRE(ctx["after_lib"] == "executed");
}

// Test 4: System node /__system__/budget_exceeded is registered and reachable
TEST_CASE("System Node Budget Exceeded is Registered", "[stage1][scheduler]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: assign
    assign:
      test: "value"
    next: "/__system__/budget_exceeded"
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    // Should terminate cleanly (hard end)
    REQUIRE(ctx["test"] == "value");
}

// Test 5: /lib/utils/noop executes without error (soft end)
TEST_CASE("Lib Utils Noop Executes as Soft End", "[stage1][scheduler]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: test_noop
nodes:
  - id: test_noop
    type: assign
    assign:
      before: "start"
    next: "/__system__/noop"
  - id: continue
    type: assign
    assign:
      after: "continued"
    wait_for: ["/__system__/noop"]
    next: /main/end
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    REQUIRE(ctx["before"] == "start");
    REQUIRE(ctx["after"] == "continued");
}

// Test 6: Cross-graph execution - node in /side branch connects to /main branch
// This test verifies nodes can connect across different graphs
TEST_CASE("Cross-Graph Edge Execution", "[scheduler][cross-graph][bug]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: start
    next: "/side/work"
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```

### AgenticDSL `/side/work`
```yaml
# --- BEGIN AgenticDSL ---
type: assign
assign:
  side_done: "yes"
next: "/main/end"
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    REQUIRE(ctx["side_done"] == "yes");
}

// ============================================================================
// Sprint 7 Day 2: 7 state-based TEST_CASE 覆盖 TopoScheduler::execute 拆分的 3 子函数
//   - prepare_dag_state  (测试 #1, #2, #3)
//   - dispatch_next_node (测试 #4, #5)
//   - handle_node_completion / finalize_execution (测试 #6, #3)
//   - I-2 fork regression (测试 #7, 锁定 Day 1 fork dedup)
// 注: 3 个 split subfunctions 都是 TopoScheduler 私有, 通过 DSLEngine.execute 公开 API 间接测试。
// ============================================================================

// Test 1: prepare_dag_state 验证 3 节点线性 DAG (A→B→C) 入口点入队
TEST_CASE("prepare_dag_state_simple_linear", "[scheduler][stage2][dag_state][linear]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: step_a
nodes:
  - id: step_a
    type: assign
    assign:
      a: "A"
    next: ["/main/step_b"]
  - id: step_b
    type: assign
    assign:
      b: "B"
    next: ["/main/step_c"]
  - id: step_c
    type: assign
    assign:
      c: "C"
    next: ["/main/end"]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    REQUIRE(ctx.contains("a"));
    REQUIRE(ctx.contains("b"));
    REQUIRE(ctx.contains("c"));
    REQUIRE(ctx["a"] == "A");
    REQUIRE(ctx["b"] == "B");
    REQUIRE(ctx["c"] == "C");
}

// Test 2: prepare_dag_state 验证 4 节点菱形 DAG (A→{B,C}→D) 拓扑正确
TEST_CASE("prepare_dag_state_diamond", "[scheduler][stage2][dag_state][diamond]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: top
nodes:
  - id: top
    type: assign
    assign:
      top: "root"
    next: ["/main/left", "/main/right"]
  - id: left
    type: assign
    assign:
      left: "L"
    next: ["/main/bottom"]
  - id: right
    type: assign
    assign:
      right: "R"
    next: ["/main/bottom"]
  - id: bottom
    type: assign
    assign:
      bottom: "done"
    next: ["/main/end"]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    REQUIRE(ctx["top"] == "root");
    REQUIRE(ctx["left"] == "L");
    REQUIRE(ctx["right"] == "R");
    REQUIRE(ctx["bottom"] == "done");
}

// Test 3: prepare_dag_state / finalize_execution 验证 cycle 检测
//         A→B→A 循环 + 无 entry_point → finalize_execution 报 "Unmet dependencies or cycles"
TEST_CASE("prepare_dag_state_cycle_detection", "[scheduler][stage2][dag_state][cycle]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: cyc_a
    type: assign
    assign:
      a: "A"
    next: ["/main/cyc_b"]
  - id: cyc_b
    type: assign
    assign:
      b: "B"
    next: ["/main/cyc_a"]
# --- END AgenticDSL ---
```
)";

    auto result = run_dsl_raw(markdown);
    REQUIRE_FALSE(result.success);
    REQUIRE((result.message.find("cycles") != std::string::npos
             || result.message.find("Unexecuted") != std::string::npos
             || result.message.find("Unmet") != std::string::npos));
}

// Test 4: dispatch_next_node 验证初始 ready_queue 单节点派发
TEST_CASE("dispatch_ready_nodes_initial", "[scheduler][stage2][dispatch][initial]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: first
nodes:
  - id: first
    type: assign
    assign:
      first: "1"
    next: ["/main/second"]
  - id: second
    type: assign
    assign:
      second: "2"
    next: ["/main/end"]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    REQUIRE(ctx["first"] == "1");
    REQUIRE(ctx["second"] == "2");
}

// Test 5: dispatch_next_node 验证 3 个独立节点全部派发 (并发就绪)
TEST_CASE("dispatch_ready_nodes_parallel", "[scheduler][stage2][dispatch][parallel]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: fanout
nodes:
  - id: fanout
    type: assign
    assign:
      fanout: "go"
    next: ["/main/branch_a", "/main/branch_b", "/main/branch_c"]
  - id: branch_a
    type: assign
    assign:
      a: "A"
    next: ["/main/end"]
  - id: branch_b
    type: assign
    assign:
      b: "B"
    next: ["/main/end"]
  - id: branch_c
    type: assign
    assign:
      c: "C"
    next: ["/main/end"]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    REQUIRE(ctx["a"] == "A");
    REQUIRE(ctx["b"] == "B");
    REQUIRE(ctx["c"] == "C");
}

// Test 6: handle_node_completion 验证下游在完成后进入 ready_queue
//         B 有 in_degree==1 来自 A, 必须等 A 完成 (执行 + 减 in_degree==0) 才能入队
TEST_CASE("handle_node_completion_success", "[scheduler][stage2][completion][downstream]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: producer
nodes:
  - id: producer
    type: assign
    assign:
      upstream: "PRODUCED"
    next: ["/main/consumer"]
  - id: consumer
    type: assign
    assign:
      downstream: "{{ upstream }}_CONSUMED"
    next: ["/main/end"]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

    auto ctx = run_dsl(markdown);
    REQUIRE(ctx["upstream"] == "PRODUCED");
    REQUIRE(ctx["downstream"] == "PRODUCED_CONSUMED");
}

// Test 7: handle_node_completion_failure — 失败传播: A 失败时, 2 个下游 (B, C) 不执行
//         当前 scheduler 实现: NodeExecutor.execute_tool_call 对 Abort error_code 抛 [ABORT],
//         ExecutionSession.execute_node 捕获后 result.success=false, TopoScheduler.execute()
//         立即 return false。后续 B/C never dispatched, 也就不在 executed_ 集合中。
//         验证契约: result.success=false + B/C output_keys 不出现在 final_context。
//
// 注: Day 1 I-2 fork dedup regression test 推迟到 Sprint 7 scheduler-pipeline-tightened
// (Day 5+) 实施, 因为 NodeExecutor::execute_fork 当前未实现 (throws runtime_error),
// 无法通过 DSL 触发 is_executing_fork_branches_=true 路径。Day 5+ 将通过直接 unit test
// 私有 fork 处理方法锁定。
TEST_CASE("handle_node_completion_failure", "[scheduler][stage2][completion][failure]") {
    int downstream_counter = 0;

    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: fail_node
nodes:
  - id: fail_node
    type: tool_call
    tool: always_fail
    arguments: {}
    output_keys: ["fail_result"]
    next: ["/main/down_a", "/main/down_b"]
  - id: down_a
    type: tool_call
    tool: downstream_marker
    arguments: {}
    output_keys: ["a_marker"]
    next: ["/main/end"]
  - id: down_b
    type: tool_call
    tool: downstream_marker
    arguments: {}
    output_keys: ["b_marker"]
    next: ["/main/end"]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

    auto engine = agenticdsl::DSLEngine::from_markdown(markdown);
    engine->register_tool("always_fail", [](const nlohmann::json&)
                                          -> nlohmann::json {
        return nlohmann::json{
            {"ok", false},
            {"data", nlohmann::json::object()},
            {"meta", {{"error_message", "intentional Abort for failure propagation test"}}},
            {"error_code", "Abort"}
        };
    });
    engine->register_tool("downstream_marker",
                          [&downstream_counter](const nlohmann::json&)
                              -> nlohmann::json {
        downstream_counter++;
        return {{"invocation", downstream_counter}};
    });

    auto result = engine->run();
    REQUIRE_FALSE(result.success);
    REQUIRE(result.message.find("always_fail") != std::string::npos);
    REQUIRE(downstream_counter == 0);
    REQUIRE_FALSE(result.final_context.contains("a_marker"));
    REQUIRE_FALSE(result.final_context.contains("b_marker"));
}
