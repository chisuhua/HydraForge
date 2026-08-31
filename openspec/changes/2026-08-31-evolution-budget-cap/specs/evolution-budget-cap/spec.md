# EvolutionBudgetCap Specification

## Purpose

> 修复 Oracle N1 缺口 (Blocker): 进化是正反馈循环, ExecutionBudget 只管单次执行, 无进化周期预算上限。本 change 在 ExecutionBudget 增加进化周期预算预留字段, 与现有 max_llm_calls 同构 (CAS 原子 + fail-closed), 默认 -1 无限制保证零回归。

## ADDED Requirements

### Requirement: ExecutionBudget 新增进化预算字段

`ExecutionBudget` MUST 新增 `int max_evolution_llm_calls = -1` (默认无限制) 和 `mutable std::atomic<int> evolution_llm_calls_used{0}` 字段, 与现有 `max_llm_calls` / `llm_calls_used` 同构。

#### Scenario: 字段存在

- **WHEN** 静态检查 `grep -c "max_evolution_llm_calls\|evolution_llm_calls_used" src/core/types/budget.h`
- **THEN** ≥2

#### Scenario: 默认值 -1 无限制

- **WHEN** 构造 `ExecutionBudget` 默认
- **THEN** `max_evolution_llm_calls == -1`

### Requirement: try_consume_evolution_llm_call CAS 原子消耗

`try_consume_evolution_llm_call()` MUST 使用 CAS (compare_exchange_weak) 原子消耗, 与 `try_consume_llm_call()` 同构, 超限返回 false (fail-closed)。

#### Scenario: 3 次上限第 4 次失败

- **WHEN** `max_evolution_llm_calls=3`, 调用 try_consume_evolution_llm_call 4 次
- **THEN** 前 3 次返回 true, 第 4 次返回 false

#### Scenario: 默认 -1 永不失败

- **WHEN** `max_evolution_llm_calls=-1` (默认), 调用 try_consume_evolution_llm_call 100 次
- **THEN** 全部返回 true

### Requirement: 进化预算与总预算独立

`evolution_budget_exceeded()` 与 `exceeded()` MUST 完全独立: 进化超限不触发总超限, 反之亦然。

#### Scenario: 进化超限不影响总预算 exceeded()

- **WHEN** `max_evolution_llm_calls=2` 且已消耗 3 次 (进化超限), `max_llm_calls=100` 且 llm_calls_used=5 (总预算未超)
- **THEN** `evolution_budget_exceeded()==true` 且 `exceeded()==false`

#### Scenario: 总预算超限不影响进化预算

- **WHEN** `max_llm_calls=5` 且 llm_calls_used=6 (总超限), `max_evolution_llm_calls=100` 且 evolution_llm_calls_used=2 (进化未超)
- **THEN** `exceeded()==true` 且 `evolution_budget_exceeded()==false`

### Requirement: reset_evolution_cycle 重置计数器

`reset_evolution_cycle()` MUST 将 `evolution_llm_calls_used` 重置为 0, 不影响其他计数器。

#### Scenario: 重置后重新可用

- **WHEN** `max_evolution_llm_calls=2` 消耗 2 次 (达上限), 调用 reset_evolution_cycle(), 再调用 try_consume_evolution_llm_call
- **THEN** 返回 true (计数器已重置)

### Requirement: IBudgetController 接口同步扩展 (W3 修复)

IBudgetController MUST 新增 4 个纯虚方法: `try_consume_evolution_llm_call()` / `evolution_budget_exceeded() const` / `begin_evolution_cycle()` / `end_evolution_cycle()`; MUST 新增 `set_bus()` / `get_bus()` 用于事件发射通道注入。**关键**: factory 返回 `unique_ptr<IBudgetController>`, 接入方经接口调用, 新方法必须在接口层 (不能在仅 BudgetController) 声明。

#### Scenario: 接口层 4 新方法存在

- **WHEN** 静态检查 `grep -E "try_consume_evolution_llm_call|evolution_budget_exceeded|begin_evolution_cycle|end_evolution_cycle" include/agenticdsl/budget/budget_controller.h`
- **THEN** ≥4 行 (IBudgetController 4 纯虚方法声明, 在 line 23-41 范围)

#### Scenario: 接口层 set_bus/get_bus 存在

- **WHEN** 静态检查 `grep "set_bus\|get_bus" include/agenticdsl/budget/budget_controller.h`
- **THEN** ≥2 行 (IBudgetController 声明)

#### Scenario: BudgetController 委托路径工作

- **WHEN** BudgetController 调用 try_consume_evolution_llm_call
- **THEN** 与直接调用 ExecutionBudget 行为一致

#### Scenario: 周期边界重置

- **WHEN** begin_evolution_cycle() 调用
- **THEN** evolution_llm_calls_used 重置为 0

#### Scenario: BudgetController 持有 bus 成员 (W3 修复)

- **WHEN** 静态检查 `grep "bus_\|std::shared_ptr<IInteractionBus> bus" src/modules/budget/budget_controller.h`
- **THEN** ≥1 行 (bus 成员存在, 事件发射通道)

### Requirement: budget.evolution_cycle.* 事件发射与注册

3 个事件主题 (`budget.evolution_cycle.start` / `.end` / `.exceeded`) MUST 经 IInteractionBus 发射, 并注册到 ADR-0068 Appendix A。

#### Scenario: 超限事件发射

- **WHEN** try_consume_evolution_llm_call 返回 false
- **THEN** emit `budget.evolution_cycle.exceeded` 事件 (payload 含 used/max)

#### Scenario: ADR-0068 附录 A 注册

- **WHEN** 静态检查 `grep "budget.evolution_cycle" docs/adr/adr-0068-event-emission-contract.md`
- **THEN** ≥3 行 (3 主题, ship 后)

### Requirement: 5 测试覆盖 + 零回归

`tests/test_evolution_budget_cap.cpp` MUST 含 ≥5 cases, 全量 ctest 零回归。

#### Scenario: 5 cases 完整

- **WHEN** 静态检查 `grep -c "TEST_CASE" tests/test_evolution_budget_cap.cpp`
- **THEN** ≥5

#### Scenario: 零回归

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 0 failures

## MODIFIED Requirements

### Requirement: 零 contract 修改

本 change MUST NOT 修改 `include/agenticdsl/contract/` 任何头文件。

#### Scenario: contract 零修改

- **WHEN** 运行 `git diff --stat HEAD -- include/agenticdsl/contract/`
- **THEN** 0 行变更

### Requirement: 移动构造/赋值同步更新

ExecutionBudget 移动构造函数和移动赋值运算符 MUST 将 `evolution_llm_calls_used` 重置为 0 (与现有 nodes_used 等同模式)。

#### Scenario: 移动构造重置

- **WHEN** 移动构造 ExecutionBudget
- **THEN** `evolution_llm_calls_used == 0` (与其他计数器一致)

## CROSS-REFERENCED Requirements

### Requirement: 与 axis6-chain-workflow N1 缺口对齐

本 change 是 `axis6-chain-workflow-architecture-2026-08.md` §六 N1 (进化预算失控) 的修复。文档更新时本 spec 同步。

#### Scenario: N1 缺口状态更新

- **WHEN** 本 change ship 后
- **THEN** axis6-chain-workflow 文档 N1 标注 "已修复" 或 changelog 注记

### Requirement: 接入方分离

本 change 仅提供预算基础设施, MUST NOT 修改 GEPALoop / MCTSWorkflowSearch 接入 try_consume (接入属对应组件的后续 change)。

#### Scenario: 无 GEPA/MCTS 修改

- **WHEN** 运行 `git diff --stat HEAD -- include/agenticdsl/cognitive/gepa_loop.h include/agenticdsl/cognitive/mcts_workflow_search.h src/modules/cognitive/`
- **THEN** 0 行变更
