# Spec: dag-scheduler-pipeline

> **关联 proposal**: `2026-07-30-sprint-8-scheduler-pipeline-followup/proposal.md`
> **关联 design**: `2026-07-30-sprint-8-scheduler-pipeline-followup/design.md`
> **关联 tasks**: `2026-07-30-sprint-8-scheduler-pipeline-followup/tasks.md`

## ADDED Requirements

### Requirement: DagState 7 字段契约

The system MUST define `struct DagState` with 7 fields: `unordered_map<NodePath, Node*> nodes` (non-owning view), `unordered_map<NodePath, vector<NodePath>> reverse_edges`, `unordered_map<NodePath, vector<NodePath>> wait_for_dependents`, `unordered_map<NodePath, int> in_degree`, `queue<NodePath> ready_queue`, `unordered_set<NodePath> executed`, `vector<ParsedGraph> dynamic_graphs`. The `nodes` field MUST be `Node*` (non-owning), not `unique_ptr<Node>`, to avoid double ownership with `all_nodes_`.

#### Scenario: DagState 字段定义

- **WHEN** 定义 `struct DagState` 在 `topo_scheduler.h` private 段
- **THEN** 字段必须为: `nodes` (Node* 非拥有) + `reverse_edges` + `wait_for_dependents` + `in_degree` + `ready_queue` + `executed` + `dynamic_graphs`
- **AND** `nodes` 必须为 `Node*` 非拥有

### Requirement: 3 子函数签名

The system MUST declare 3 subfunctions with `DagState&` parameter. `dispatch_next_node` MUST be renamed to `dispatch_ready_nodes`. `handle_node_completion` MUST accept `(DagState& state, const NodeResult& result)`.

#### Scenario: 3 子函数接受 DagState& 参数

- **WHEN** 声明 3 子函数
- **THEN** 签名必须为: `prepare_dag_state(DagState& state)` + `dispatch_ready_nodes(DagState& state, const Context&)` + `finalize_execution(DagState& state, const Context&)`
- **AND** `handle_node_completion` 声明: `std::optional<ExecutionResult> handle_node_completion(DagState& state, const NodeResult& result);`

### Requirement: 3 子函数纯函数式

The system MUST make `prepare_dag_state`, `dispatch_ready_nodes`, and `handle_node_completion` PURE functional, reading only TopoScheduler immutable data and writing only to `DagState&`. They MUST NOT mutate `this->` members (except via `build_dag(DagState& state)` for state migration).

#### Scenario: handle_node_completion 纯函数式

- **WHEN** `handle_node_completion(state, result)` 被调用
- **THEN** 写 `state.executed.insert(path)` + 遍历 `state.wait_for_dependents[path]` 减 `state.in_degree[dependent]`
- **AND** 0 时 `state.ready_queue.push(dependent)`
- **AND** 失败时 (`result.success == false`) downstream 标 Skipped

### Requirement: execute() ≤ 60 行

The `execute()` function MUST be at most 60 lines. The function MUST be a pure orchestration layer calling subfunctions: `handle_fork_branches_block` + `dispatch_ready_nodes` + `resolve_dynamic_waits` + `process_jump` + `handle_fork_node` + `process_fork_join` + `update_successors` + `check_end_termination` + `rebuild_dynamic_graph` + `build_dag(state)` + 3 main subfunction (`prepare_dag_state` / `dispatch_ready_nodes` / `finalize_execution`).

#### Scenario: execute 行数约束

- **WHEN** 测量 `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l`
- **THEN** MUST be ≤ 60 lines
- **AND** MUST be a pure orchestration layer

### Requirement: Hub out_degree

The system MUST have `topo_scheduler::execute` out_degree < 30 and 3 subfunctions out_degree < 25, as measured by `mcp__code-review-graph__get_hub_nodes --top_n 5`.

#### Scenario: hub out_degree 约束

- **WHEN** 运行 hub out_degree 测量
- **THEN** `topo_scheduler::execute` out_degree MUST be < 30
- **AND** 3 子函数 out_degree MUST be < 25

### Requirement: JOIN 死代码 + 死循环修复

The system MUST repair the JOIN dead code + dead loop bug (Oracle Day 8 confirmed). JOIN dispatch MUST be processed directly via `start_join_simulation + finish_join_simulation + finish_fork_simulation` (派发即处理模式) without re-queueing and without depending on `is_executing_fork_branches_` flag.

#### Scenario: JOIN 派发即处理模式

- **WHEN** `current_node->type == NodeType::JOIN` 在 execute() 循环中触发
- **THEN** MUST directly call `start_join_simulation + finish_join_simulation + finish_fork_simulation` (via `process_fork_join` subfunction)
- **AND** MUST NOT re-queue `ready_queue_.push(current_path)` (avoid dead loop)
- **AND** MUST NOT depend on `is_executing_fork_branches_` flag

### Requirement: pending_dynamic_deps_ 访问一致

The system MUST use `session_.get_pending_dynamic_deps()` accessor instead of direct friend access to `session_.pending_dynamic_deps_`.

#### Scenario: accessor 使用

- **WHEN** 访问 `session_.pending_dynamic_deps_`
- **THEN** MUST use `session_.get_pending_dynamic_deps()` accessor
- **AND** `grep "session_\.pending_dynamic_deps_" src/modules/scheduler/topo_scheduler.cpp` MUST return 0 hits

### Requirement: NodeResult 类型定义

The system MUST define `struct NodeResult` in `core/types/node.h` with 3 fields: `bool success = true;` + `nlohmann::json output;` + `std::string error_message;`. `handle_node_completion` function body MUST accept `const NodeResult&` parameter.

#### Scenario: NodeResult 字段定义

- **WHEN** 定义 `struct NodeResult` 在 `core/types/node.h`
- **THEN** 字段 MUST be: `bool success = true;` + `nlohmann::json output;` + `std::string error_message;`
- **AND** `handle_node_completion` function body MUST accept `const NodeResult&` parameter

### Requirement: build_dag → state 迁移

The system MUST provide a private `void build_dag(DagState& state);` overload that mirrors `this->` fields to `state` (state.nodes, state.reverse_edges, state.wait_for_dependents, state.in_degree, state.executed.clear) and recomputes `state.ready_queue` from `state.in_degree`. `execute()` MUST call `build_dag(state)` after `prepare_dag_state`.

#### Scenario: 私有 build_dag(DagState&) 重载

- **WHEN** 声明 `void build_dag(DagState& state);` 私有成员
- **THEN** 实现 MUST mirror this-> 字段到 state + 重算 state.ready_queue
- **AND** execute() MUST call `build_dag(state);` after `prepare_dag_state`
