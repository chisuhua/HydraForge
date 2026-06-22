<!-- STATUS NOTE (2026-06-22 Oracle Code Review 决议)
本 spec 部分验收项 Sprint 6 ship 时未达, 详细偏离项见 `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` §6.1 与 §6.3。

**Sprint 6 实际偏离 (Oracle ses_112a9f9c5ffesqpYeefOBgMkjH 实测)**:
- `TopoScheduler::execute()` 222 行 (L149-370), spec 要求 ≤ 60 行 — 🟠 Major 偏离
- 未引入 `struct DagState`, 3 子函数非纯函数式 (直接改成员) — 🟠 Major 偏离
- 3 子函数命名 2/3 不符 spec: 实际 `dispatch_next_node` (spec: `dispatch_ready_nodes`) + `finalize_execution` (spec: `handle_node_completion`) — 🟠 Major
- `handle_node_completion` 失败传播 + 触发下游逻辑 **未提取**, 仍内联 `execute()` — 🟠 Major
- scheduler 测试 ≥ 7 个要求 → **0 个交付** (`tests/test_scheduler.cpp` 零改动) — 🔴 零交付
- Hub out_degree < 30 未验证 — 🟡 未验证

**Sprint 6 行为保持**: DAG 序/错误传播/状态转换语义未变, 33/33 ctest pass, `execute_single_branch` 118 行未动。

**Sprint 7 follow-up**: 全部推迟到 OpenSpec change `2026-07-22-sprint-7-tech-debt-followup`。本 change 不 archive。
-->

## ADDED Requirements

### Requirement: scheduler-three-stage-pipeline

`TopoScheduler::execute()` MUST 拆分为 3 个独立子函数,每个子函数 MUST 为纯函数式（输入 DAG + Context,输出新状态或副作用）:
- `prepare_dag_state(const ParsedGraph&, Context&) -> DagState`: 解析 + 拓扑排序 + 入度计算
- `dispatch_ready_nodes(DagState&, ExecutionSession&) -> size_t`: 从 ready 队列取出节点并启动 worker
- `handle_node_completion(DagState&, const NodeResult&) -> bool`: 收集结果 + 失败传播 + 触发下游

`execute()` MUST 仅作编排层,代码行数 MUST ≤ 60。

#### Scenario: execute 函数行数上限

- **WHEN** `wc -l src/modules/scheduler/topo_scheduler.cpp` 在 `execute()` 函数体内统计
- **THEN** MUST ≤ 60 行
- **AND** 包含子函数调用 + 错误处理 + 返回值构造

#### Scenario: 3 子函数存在且为 private

- **WHEN** `grep -n "DagState prepare_dag_state\|dispatch_ready_nodes\|handle_node_completion" src/modules/scheduler/topo_scheduler.h`
- **THEN** MUST 命中 3 个函数声明
- **AND** 3 个函数 MUST 声明为 `private` (PIMPL 保护, 不暴露给外部)

#### Scenario: 子函数为纯函数式

- **WHEN** 检查 3 子函数的实现
- **THEN** `prepare_dag_state` MUST 仅修改 `DagState` (返回值), 不修改外部状态
- **AND** `dispatch_ready_nodes` MUST 仅通过 `ExecutionSession` API 派发, 不直接调用 worker
- **AND** `handle_node_completion` MUST 仅根据 `NodeResult` 更新 `DagState`, 不抛异常

#### Scenario: execute_single_branch 不受影响

- **WHEN** 实施 scheduler 三段式重构
- **THEN** `execute_single_branch()` MUST 保持原 118 行不动（向后兼容）

### Requirement: scheduler-test-coverage-extension

`tests/test_scheduler.cpp` MUST 新增 ≥ 7 个 Catch2 test case 验证三段式流水线:

- `prepare_dag_state_simple_linear`: 3 节点线性 DAG 解析正确
- `prepare_dag_state_diamond`: 4 节点菱形 DAG 拓扑排序正确
- `prepare_dag_state_cycle_detection`: 检测循环依赖并返回错误
- `dispatch_ready_nodes_initial`: 初始 ready 队列正确
- `dispatch_ready_nodes_parallel`: 并行派发 3 个独立节点
- `handle_node_completion_success`: 成功完成后下游节点入 ready 队列
- `handle_node_completion_failure`: 失败传播到下游节点（标记为 skipped）

#### Scenario: 7 个新 test case 通过

- **WHEN** `cd build && ctest -R test_scheduler --output-on-failure`
- **THEN** MUST 至少 7 个新 TEST_CASE 通过
- **AND** 既有 test_scheduler test MUST 零回归

#### Scenario: Hub 出度降低

- **WHEN** `code-review-graph get_hub_nodes --top_n 5`
- **THEN** `topo_scheduler::execute` 的 `out_degree` MUST 从 89 降至 < 30
- **AND** 3 子函数各自 `out_degree` MUST < 25