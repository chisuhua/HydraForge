# dag-scheduler-pipeline Specification

## Purpose
Sprint 8 收尾 Sprint 7 `scheduler-pipeline-tightened` 推迟项 — JOIN 死循环/死代码修复(`finish_fork_simulation()` 调用时序 + `current_fork_branches_.clear()` size=0 检测)、`execute()` 拆分为 ≤60 行(SPEC 5.1 验证)、`build_dag()` state 迁移(`DagState&` 参数, push 到 `state.ready_queue` 替代 `this->ready_queue_`)、`HardEndException` 替换为 status enum。
## Requirements
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

### Requirement: build-dag-function-size-limit

`TopoScheduler::build_dag()` MUST 限制在 ≤40 行。复杂逻辑 (wait_for 依赖解析 + ready_queue 填充) MUST 拆分为独立 helper 函数。

#### Scenario: build_dag 拆分后行数
- **WHEN** 重构 `TopoScheduler::build_dag()`
- **THEN** 函数体 ≤40 行 (从当前 88 行)
- **AND** 2 个 helper 存在: `parse_node_wait_for_deps()` + `seed_initial_ready_queue()`

#### Scenario: 行为保持
- **WHEN** 运行 `test_scheduler` 33 个测试用例
- **THEN** 100% PASS，DAG 构建行为完全保持
- **AND** `test_execute_parallel` 验证并行执行路径不变

### Requirement: build-dag-state-overload-removal

`TopoScheduler::build_dag(DagState&)` 重载 MUST 被删除。`DagState` 初始化 MUST 显式发生在 `execute_parallel()` 调用方。

#### Scenario: 重载删除
- **WHEN** 编译 `src/modules/scheduler/topo_scheduler.h`
- **THEN** 仅有 `void build_dag();` 单一声明
- **AND** `void build_dag(DagState& state);` 已移除

#### Scenario: execute_parallel 显式迁移
- **WHEN** `execute_parallel()` 调用 build_dag 后
- **THEN** 显式执行 7 行 `state.X = this->X_;` 迁移
- **AND** 行为与原重载等价

### Requirement: load-graphs-function-size-limit

`TopoScheduler::load_graphs()` MUST 限制在 ≤45 行。JSON → Node 创建逻辑 MUST 拆分为 `parse_single_node_spec()` helper。

#### Scenario: load_graphs 拆分后行数
- **WHEN** 重构 `TopoScheduler::load_graphs()`
- **THEN** 函数体 ≤45 行 (从当前 80 行)
- **AND** `parse_single_node_spec(const nlohmann::json&, const std::string&)` helper 存在

#### Scenario: JSON 解析行为保持
- **WHEN** 运行 `test_library_loader` 验证 library loader 集成
- **THEN** 100% PASS，节点创建行为完全保持

### Requirement: execute-parallel-function-size-limit

`TopoScheduler::execute_parallel()` MUST 限制在 ≤30 行。主 while 循环 MUST 拆分为 `execute_dag_loop()` helper。

#### Scenario: execute_parallel 拆分后行数
- **WHEN** 重构 `TopoScheduler::execute_parallel()`
- **THEN** 函数体 ≤30 行 (从当前 58 行)
- **AND** `execute_dag_loop(DagState& state, const Context&)` 主循环 helper 存在

#### Scenario: DAG 执行行为保持
- **WHEN** 运行 `test_execute_parallel` 所有用例
- **THEN** 100% PASS，并行 DAG 执行行为完全保持

### Requirement: cyclomatic-complexity-reduction

`topo_scheduler.cpp` 文件级控制流密度 MUST 降低到 < 100 (从当前 193)。每个函数平均控制流语句 MUST < 15。

#### Scenario: 文件级复杂度降低
- **WHEN** 运行 `grep -c 'if\|for\|while\|switch' src/modules/scheduler/topo_scheduler.cpp`
- **THEN** 输出 < 100 (从 193)
- **AND** 文件总行数 < 700 (从 695) 或保持 (helpers 抵消)

#### Scenario: 单函数控制流降低
- **WHEN** 统计每个 `void` / `ExecutionResult` / `Context` / `auto` / `bool` 函数的控制流密度
- **THEN** 平均每个函数 < 15 个 `if`/`for`/`while`/`switch`

### Requirement: execution-session-h-no-modules-include

`src/modules/scheduler/execution_session.h` MUST NOT 直接 `#include` 任何 `modules/` 子目录的头文件。所有 `modules/` 依赖 MUST 通过前向声明 + PIMPL-lite (`std::unique_ptr`) 模式间接持有，完整 include 移到 `execution_session.cpp`。

#### Scenario: 头文件无 modules/ include
- **WHEN** 编译 `src/modules/scheduler/execution_session.h`
- **THEN** `grep -c '#include "modules/' src/modules/scheduler/execution_session.h` 输出 0
- **AND** 7 个 classes 前向声明存在: `ContextEngine` + `BudgetController` + `TraceExporter` + `NodeExecutor` + `MarkdownParser` + `StandardLibraryLoader` + `ResourceManager`
- **AND** 7 个成员均为 `std::unique_ptr<X>`

#### Scenario: 析构外置
- **WHEN** 持有 `unique_ptr<...>` 不完整类型成员
- **THEN** `~ExecutionSession()` 必须显式声明 (头文件)
- **AND** 实现为 `= default;` (cpp 文件)
- **AND** 编译时无 "incomplete type" 错误

#### Scenario: 完整 include 移到 .cpp
- **WHEN** 编译 `src/modules/scheduler/execution_session.cpp`
- **THEN** 7 个 `modules/` include 存在 (提供完整类型)
- **AND** `make_unique<X>(...)` 构造点成功

### Requirement: execution-session-public-api-stable

`ExecutionSession` 公开方法签名 MUST 保持不变: `check_and_requeue_dynamic_deps()`, `is_budget_exceeded()`, `needs_snapshot()`。

#### Scenario: 公开方法签名保持
- **WHEN** 检查 `execution_session.h` 公开方法声明
- **THEN** 3 个方法签名与 Sprint 18 之前完全一致
- **AND** `topo_scheduler.cpp` 调用方零修改

#### Scenario: 行为保持
- **WHEN** 运行 `test_scheduler` 33 个用例
- **THEN** 100% PASS，ExecutionSession 行为完全保持
- **AND** `is_budget_exceeded` / `needs_snapshot` 决策逻辑不变

### Requirement: core-types-include-allowed

`execution_session.h` 仍 MUST 可 include `core/types/context.h` + `core/types/node.h` + `core/types/budget.h` (3 个 POD types)。`agenticdsl/contract/itool_registry.h` 仍 MUST 可 include (P1.T4 引用类型)。

#### Scenario: core/types/ include 允许
- **WHEN** 编译 `execution_session.h`
- **THEN** 3 个 `core/types/` include 存在 (POD types)
- **AND** 1 个 `agenticdsl/contract/itool_registry.h` include 存在 (引用类型)

#### Scenario: 总 include 计数
- **WHEN** 统计 `execution_session.h` 项目 include 数量
- **THEN** ≤5 个 (从 11 降至 ≤5)
- **AND** 4 个 `core/types/` + 1 个 `agenticdsl/contract/` = 4 个允许类别
- **AND** 0 个 `modules/` (强制要求)

