# Spec: dag-scheduler-pipeline

> **关联 proposal**: `2026-07-30-sprint-8-scheduler-pipeline-followup/proposal.md`
> **关联 design**: `2026-07-30-sprint-8-scheduler-pipeline-followup/design.md`
> **关联 tasks**: `2026-07-30-sprint-8-scheduler-pipeline-followup/tasks.md`
> **关联 spec 历史**: 关联 Sprint 7 推进 (`a7a2edc` / `6a63518` / `b45d049` / `69670ec` / `75ded94` / `a00734f`, merge `b44b486`)

## Purpose

定义 `TopoScheduler::execute()` DAG 拓扑调度管线的纯函数式契约 + 性能约束:
- DagState 结构体作为 3 子函数 (prepare_dag_state / dispatch_ready_nodes / handle_node_completion) 通信唯一接口
- execute() 编排层 ≤ 60 行
- 3 子函数 out_degree < 25 + execute() out_degree < 30
- 修 JOIN 死代码 + 死循环 (Sprint 8 Day 1 Blocker)
- handle_node_completion 函数体实施 + NodeResult 类型定义 (Sprint 8 Day 6-7)

## Requirements

### REQ-1: DagState 7 字段契约 (Sprint 7 Day 5 ✅)

#### Scenario: DagState 字段定义

- **WHEN** 定义 `struct DagState` 在 `topo_scheduler.h` private 段
- **THEN** 字段必须为: `unordered_map<NodePath, Node*> nodes` (非拥有视图) + `unordered_map<NodePath, vector<NodePath>> reverse_edges` + `unordered_map<NodePath, vector<NodePath>> wait_for_dependents` + `unordered_map<NodePath, int> in_degree` + `queue<NodePath> ready_queue` + `unordered_set<NodePath> executed` + `vector<ParsedGraph> dynamic_graphs`
- **AND** `nodes` 必须为 `Node*` 非拥有 (非 `unique_ptr<Node>`, 避免与 `all_nodes_` 双重所有权)

### REQ-2: 3 子函数签名 (Sprint 7 Day 6 ✅)

#### Scenario: 3 子函数接受 DagState& 参数

- **WHEN** 声明 3 子函数
- **THEN** 签名必须为: `std::optional<ExecutionResult> prepare_dag_state(DagState& state);` + `std::variant<std::monostate, NodeLookupResult, ExecutionResult> dispatch_ready_nodes(DagState& state, const Context& context);` + `ExecutionResult finalize_execution(DagState& state, const Context& context);`
- **AND** `dispatch_next_node` 必须重命名为 `dispatch_ready_nodes` (spec 命名要求)
- **AND** `handle_node_completion` 声明: `std::optional<ExecutionResult> handle_node_completion(DagState& state, const NodeResult& result);` (Sprint 8 Day 6 实施)

### REQ-3: 3 子函数纯函数式 (Sprint 7 Day 7+8 部分 ✅, Sprint 8 Day 6-7 完整)

#### Scenario: prepare_dag_state 纯函数式

- **WHEN** `prepare_dag_state(state)` 被调用
- **THEN** 仅读 TopoScheduler 不可变数据 (full_graphs_ / node_map_ / reverse_edges_ / wait_for_dependents_ / in_degree_)
- **AND** 仅写 `state.ready_queue` + 校验 entry_point 存在性
- **AND** 不修改 this->ready_queue_ (Day 7 双写临时已 Sprint 8 清理)

#### Scenario: dispatch_ready_nodes 纯函数式

- **WHEN** `dispatch_ready_nodes(state, context)` 被调用
- **THEN** 读 `state.ready_queue` 派发节点
- **AND** 不直接调用 `session_.check_and_requeue_dynamic_deps()` (mutating call 移到 execute() 编排层)

#### Scenario: handle_node_completion 纯函数式

- **WHEN** `handle_node_completion(state, result)` 被调用
- **THEN** 写 `state.executed.insert(path)` + 遍历 `state.wait_for_dependents[path]` 减 `state.in_degree[dependent]`
- **AND** 0 时 `state.ready_queue.push(dependent)`
- **AND** 失败时 (`result.success == false`) downstream 标 Skipped (`state.in_degree[dependent] = -1`)

### REQ-4: execute() ≤ 60 行 (Sprint 8 Day 2-5 必达)

#### Scenario: execute 行数约束

- **WHEN** 测量 `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l`
- **THEN** 必须 ≤ 60 行
- **AND** execute() 必须为纯编排层, 调用 4 子函数 (resolve_dynamic_waits / process_fork_join / rebuild_dynamic_graph) + build_dag + 3 main subfunction

### REQ-5: Hub out_degree 测量 (Sprint 8 Day 4 必达)

#### Scenario: execute out_degree 约束

- **WHEN** 运行 `mcp__code-review-graph__get_hub_nodes --top_n 5`
- **THEN** `topo_scheduler::execute` out_degree 必须 < 30
- **AND** 3 子函数 (prepare_dag_state / dispatch_ready_nodes / handle_node_completion) out_degree 必须 < 25

### REQ-6: JOIN 死代码 + 死循环修复 (Sprint 8 Day 1 Blocker 必达)

#### Scenario: JOIN 派发即处理模式

- **WHEN** `current_node->type == NodeType::JOIN` 在 execute() 循环中触发
- **THEN** 必须直接调用 `start_join_simulation + finish_join_simulation + finish_fork_simulation`
- **AND** 不重新入队 `ready_queue_.push(current_path)` (避免死循环)
- **AND** 不依赖 `is_executing_fork_branches_` flag (Day 1 移走 finish_fork_simulation 调用)

#### Scenario: 2 fork+join TDD 测试覆盖

- **WHEN** 跑 `ctest -R test_scheduler --output-on-failure`
- **THEN** 必须有 9 cases PASS (7 Day 2 + 2 Sprint 8 fork+join tests)
- **AND** `prepare_dag_state_fork_branch` 验证 `is_executing_fork_branches_` 切换
- **AND** `handle_node_completion_fork_join` 验证修复后不重新入队

### REQ-7: pending_dynamic_deps_ 访问一致 (Sprint 7 Day 8 ✅)

#### Scenario: accessor 使用

- **WHEN** 访问 `session_.pending_dynamic_deps_`
- **THEN** 必须用 `session_.get_pending_dynamic_deps()` accessor (execution_session.h:70)
- **AND** `grep "session_\.pending_dynamic_deps_" src/modules/scheduler/topo_scheduler.cpp` 必须 0 命中

### REQ-8: NodeResult 类型定义 (Sprint 8 Day 6 必达)

#### Scenario: NodeResult 字段定义

- **WHEN** 定义 `struct NodeResult` 在 `core/types/node.h`
- **THEN** 字段必须为: `bool success = true;` + `nlohmann::json output;` + `std::string error_message;`
- **AND** `handle_node_completion` 函数体接受 `const NodeResult&` 参数

### REQ-9: 访问一致 (Sprint 7 Day 8 ✅)

#### Scenario: get_pending_dynamic_deps accessor

- **WHEN** `dispatch_ready_nodes` 内检查 pending dynamic deps
- **THEN** 必须用 `session_.get_pending_dynamic_deps()` 而非 friend 访问

## Verification

| 项 | 命令 | 期望 |
|---|---|---|
| ctest | `cd build && ctest --output-on-failure` | 36/36 PASS |
| TSan | `cmake --preset tsan && ctest --output-on-failure` | 0 race |
| ASan | `cmake --preset asan && ctest --output-on-failure` | 0 leak |
| execute 行数 | `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp \| wc -l` | ≤ 60 |
| Hub out_degree | `mcp__code-review-graph__get_hub_nodes --top_n 5` | execute < 30, 3 子函数 < 25 |
| 死代码 grep | `grep "session_\.pending_dynamic_deps_" src/modules/scheduler/topo_scheduler.cpp` | 0 命中 |
| OpenSpec 归档 | `openspec archive 2026-07-30-sprint-8-scheduler-pipeline-followup` | 成功 |
