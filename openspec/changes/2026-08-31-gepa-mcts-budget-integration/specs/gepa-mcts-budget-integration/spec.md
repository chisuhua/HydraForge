# GEPA/MCTS 进化预算接入 Specification

## Purpose

> 修复 Oracle N1 缺口 (Blocker) 的闭环: T3 evolution-budget-cap 提供预算基础设施后, 本 change 将 `try_consume_evolution_llm_call()` 接入 GEPALoop/MCTSWorkflowSearch 主循环, 使预算闸有人过闸 (避免 T3 成为死代码, 重演 MCTS 零消费者教训)。
>
> **零 contract 修改** (不变量 4): 修改限于 cognitive/ 头文件 + cpp + 新测试。

## ADDED Requirements

### Requirement: GEPALoop ctor 重载注入 budget_controller

`GEPALoop` MUST 新增 ctor 重载接受 `std::shared_ptr<IBudgetController> budget_controller` 参数 (默认 nullptr 向后兼容), 原 ctor 必须委托新 ctor。

#### Scenario: 新 ctor 存在

- **WHEN** 静态检查 `grep "IBudgetController" include/agenticdsl/cognitive/gepa_loop.h`
- **THEN** ≥1 行 (ctor 参数声明)

#### Scenario: 原 ctor 委托

- **WHEN** 静态检查 `src/modules/cognitive/gepa_loop.cpp` 原 ctor 实现
- **THEN** 调用新 ctor 并传 `budget_controller=nullptr`

#### Scenario: nullptr 行为不变 (零回归)

- **WHEN** 通过原 ctor (无 budget_controller) 构造 GEPALoop, 调用 reflect_and_commit
- **THEN** 行为与 T19 ship 版本完全一致 (mock evaluator/governor/llm 迭代正常完成)

### Requirement: GEPALoop 主循环预算检查

`reflect_and_commit()` 主循环 MUST 在每次迭代顶调用 `budget_controller_->try_consume_evolution_llm_call()`, 返回 false 时 MUST `break` 终止循环 + 设 `failure_mode="evolution_budget_exceeded"` + emit `gepa.reflection.failed` (reason=evolution_budget_exceeded), **不抛异常**。

#### Scenario: 预算超限 break

- **WHEN** `max_evolution_llm_calls=1`, 构造 GEPALoop (含 budget_controller), 调用 reflect_and_commit (config.max_iterations=3)
- **THEN** 第 2 次迭代 break; result.failure_mode=="evolution_budget_exceeded"

#### Scenario: 超限事件发射

- **WHEN** 预算超限 break 时
- **THEN** emit `gepa.reflection.failed`, payload reason=="evolution_budget_exceeded"

#### Scenario: 超限不抛异常

- **WHEN** 预算超限触发 break
- **THEN** 无异常传播 (graceful, 不变量 2)

### Requirement: MCTSWorkflowSearch ctor 重载注入 budget_controller

`MCTSWorkflowSearch` MUST 新增 ctor 重载接受 `std::shared_ptr<IBudgetController> budget_controller` 参数 (默认 nullptr 向后兼容), 原 ctor 必须委托新 ctor。

#### Scenario: 新 ctor 存在

- **WHEN** 静态检查 `grep "IBudgetController" include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** ≥1 行 (ctor 参数声明)

#### Scenario: 原 ctor 委托

- **WHEN** 静态检查 `src/modules/cognitive/mcts_workflow_search.cpp` 原 ctor 实现
- **THEN** 调用新 ctor 并传 `budget_controller=nullptr`

#### Scenario: nullptr 行为不变 (零回归)

- **WHEN** 通过原 ctor (无 budget_controller) 构造 MCTSWorkflowSearch, 调用 search
- **THEN** 行为与 T20 ship 版本完全一致 (mock 搜索正常完成)

### Requirement: MCTS 主循环预算检查

`search()` 主循环 MUST 在每次迭代顶调用 `budget_controller_->try_consume_evolution_llm_call()`, 返回 false 时 MUST `break` 终止搜索 + 设 `failure_mode="evolution_budget_exceeded"` + emit `mcts.budget_exceeded` 事件 (payload `{task_id, iteration}`), **不抛异常**。

#### Scenario: 预算超限 break

- **WHEN** `max_evolution_llm_calls=1`, 构造 MCTSWorkflowSearch (含 budget_controller), 调用 search (config.max_iterations=100)
- **THEN** 第 2 次迭代 break; result.failure_mode=="evolution_budget_exceeded"

#### Scenario: mcts.budget_exceeded 事件

- **WHEN** 预算超限 break 时
- **THEN** emit `mcts.budget_exceeded`, payload 含 `task_id` + `iteration`

#### Scenario: 预算充足正常完成

- **WHEN** `max_evolution_llm_calls=-1` (默认) 或 ≥ max_iterations
- **THEN** search 正常完成, failure_mode 非 "evolution_budget_exceeded"

### Requirement: 5 新测试覆盖

`tests/test_gepa_mcts_budget_integration.cpp` MUST 含 ≥5 cases: GEPA nullptr 不变 / GEPA 超限 break / MCTS nullptr 不变 / MCTS 超限 break / MCTS 充足完成。

#### Scenario: 5 cases 完整

- **WHEN** 静态检查 `grep -c "TEST_CASE" tests/test_gepa_mcts_budget_integration.cpp`
- **THEN** ≥5

#### Scenario: 关键场景覆盖

- **WHEN** 静态检查测试名包含: `gepa_nullptr_budget_unchanged` / `gepa_budget_exceeded_breaks` / `mcts_nullptr_budget_unchanged` / `mcts_budget_exceeded_breaks` / `mcts_budget_adequate_completes`
- **THEN** 5 个测试名全部出现

### Requirement: mcts.budget_exceeded 事件注册 (ADR-0068 v2.0+)

`mcts.budget_exceeded` 事件 MUST 注册到 ADR-0068 Appendix A, 版本号 **v2.0+** (W4 归口: Axis6 owns v1.8, T1/T2/T3 用 v1.9+, 本 change v2.0+ 紧随)。

#### Scenario: 事件注册

- **WHEN** 静态检查 `grep "mcts.budget_exceeded" docs/adr/adr-0068-event-emission-contract.md`
- **THEN** ≥1 行 (ship 后)

#### Scenario: GEPA 复用现有主题

- **WHEN** 静态检查 GEPA 超限事件主题
- **THEN** 复用 `gepa.reflection.failed` (不新增 `gepa.budget_exceeded` — 避免主题膨胀)

## MODIFIED Requirements

### Requirement: 零 contract 修改

本 change MUST NOT 修改 `include/agenticdsl/contract/` 任何头文件。

#### Scenario: contract 零修改

- **WHEN** 运行 `git diff --stat HEAD -- include/agenticdsl/contract/`
- **THEN** 0 行变更

### Requirement: 基线测试零回归

GEPALoop 17 cases + MCTSWorkflowSearch 65 assertions 基线 MUST 0 回归 (不变量 5)。

#### Scenario: GEPA 基线通过

- **WHEN** 运行 `./build/tests/test_gepa_loop --reporter compact`
- **THEN** 17 cases all pass

#### Scenario: MCTS 基线通过

- **WHEN** 运行 `./build/tests/test_mcts_workflow_search --reporter compact`
- **THEN** 17 cases / 65 assertions all pass

## CROSS-REFERENCED Requirements

### Requirement: 与 axis6-chain-workflow N1 缺口对齐

本 change 是 `axis6-chain-workflow-architecture-2026-08.md` §六 N1 (进化预算失控) 的闭环实现。

#### Scenario: N1 缺口状态更新

- **WHEN** 本 change ship 后
- **THEN** axis6-chain-workflow 文档 N1 标注 "已修复 (接入 closed)" 或 changelog 注记

### Requirement: 与 T3 evolution-budget-cap 消费关系

本 change MUST 消费 T3 提供的 `IBudgetController::try_consume_evolution_llm_call()`, 不修改 T3 接口。

#### Scenario: T3 接口消费

- **WHEN** 静态检查 `grep "try_consume_evolution_llm_call" src/modules/cognitive/gepa_loop.cpp src/modules/cognitive/mcts_workflow_search.cpp`
- **THEN** ≥2 行 (两处主循环各 1)

#### Scenario: 不修改 T3

- **WHEN** 运行 `git diff --stat HEAD -- src/modules/budget/`
- **THEN** 0 行变更 (T3 基础设施不动)