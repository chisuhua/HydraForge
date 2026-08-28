# t20-aflow-mcts Specification

## ADDED Requirements

### Requirement: MCTSWorkflowSearch 是工作流 MCTS 搜索引擎

The `MCTSWorkflowSearch` class MUST orchestrate MCTS (Monte Carlo Tree Search) over workflow graph space, using IEvaluator V2 CompositeEvaluator for reward, BehavioralRegressionGate for regression check, and MutationGovernor for variant authorization.

#### Scenario: 契约骨架

- **WHEN** 静态检查 `grep "class MCTSWorkflowSearch" include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** 类必须在 `agenticdsl` 命名空间内
- **AND** 必须接受 `shared_ptr<IEvaluator>` + `shared_ptr<MutationGovernor>` + `shared_ptr<BehavioralRegressionGate>` 构造注入

#### Scenario: 既有契约零修改

- **WHEN** `git diff HEAD~1 -- include/agenticdsl/contract/ievaluator.h include/agenticdsl/contract/imutation_governance.h include/agenticdsl/ir/trajectory_ir.h include/agenticdsl/cognitive/skill_compiler.h include/agenticdsl/testing/behavioral_regression.h`
- **THEN** 不应有 V1 implementation 相关 diff（既有契约必须 0 修改）

### Requirement: 5 轴模板状态空间

The `WorkflowNode` MUST represent instantiation of 5 axes (template variant / parameter / tool selection / control flow / error handling).

#### Scenario: 5 轴模板定义

- **WHEN** 静态检查 `grep "Axis1Template\|Axis2Param\|Axis3Tool\|Axis4Control\|Axis5Error" include/agenticdsl/cognitive/mcts_workflow_search.h`
- **THEN** 5 个轴 enum 必须全部定义

### Requirement: MCTS UCB1 选择算法

The MCTS selection MUST use UCB1 formula: `argmax(quality + c * sqrt(log(parent_visits) / child_visits))`.

#### Scenario: UCB1 选择最优子节点

- **WHEN** 运行 `test_mcts_workflow_search::mcts_ucb1_selection_best_arm`
- **THEN** 选择 visits 最多或 reward 最高的子节点

#### Scenario: UCB1 探索-利用平衡

- **WHEN** 运行 `test_mcts_workflow_search::mcts_ucb1_selection_exploration_exploitation_balance`
- **THEN** 低 visits 子节点获得更高 exploration bonus

### Requirement: 100 iterations 收敛

The MCTS search MUST converge within `max_iterations=100` iterations (AFlow default).

#### Scenario: 收敛性

- **WHEN** 运行 `test_mcts_workflow_search::mcts_search_convergence_100_iterations`
- **THEN** best_workflow 在 100 iterations 内稳定（连续 10 iterations 无变化）

### Requirement: IEvaluator V2 CompositeEvaluator 加权奖励

The MCTS reward MUST use `IEvaluator::evaluate()` with V2 CompositeEvaluator weighted aggregation.

#### Scenario: V2 CompositeEvaluator 集成

- **WHEN** 运行 `test_mcts_workflow_search::mcts_reward_evaluator_v2_composite`
- **THEN** 调用 IEvaluator (V2 CompositeEvaluator) 评估 workflow 候选
- **AND** 加权 scalar 平均 + quality 众数

### Requirement: BehavioralRegressionGate 拒绝回归

The MCTS MUST use `BehavioralRegressionGate::compute_fingerprint + hotelling_t2_test()` to reject workflow candidates that introduce regression.

#### Scenario: 回归拒绝

- **WHEN** 运行 `test_mcts_workflow_search::mcts_regression_gate_rejects_decline`
- **THEN** Verdict::Fail → reject candidate, 继续搜索
- **AND** emit `mcts.search.failed` with reason="regression_decline"

### Requirement: MutationGovernor L1 授权

The MCTS MUST use `MutationGovernor::propose()` + `commit()` for L1 workflow variant authorization. L2+ variants MUST be rejected.

#### Scenario: L1 commit 授权

- **WHEN** 运行 `test_mcts_workflow_search::mcts_mutation_governor_authorizes_commit`
- **THEN** L1 workflow variant → MutationGovernor::propose() Pass → commit() success
- **AND** mutation.committed event 由 MutationGovernor emit

#### Scenario: L2+ 拒绝

- **WHEN** MutationGovernor::propose() 收到 mutation_kind="L2_dsl" 或 "L3_skill"
- **THEN** MutationGovernor V1 emit denied + reject candidate
- **AND** emit `mcts.search.failed` with reason="mutation_denied"

### Requirement: 4 个 mcts.* 事件主题注册

The ADR-0068 附录 A v1.5 MUST register 4 new topics:
- `mcts.search.started` (owner: MCTSWorkflowSearch)
- `mcts.search.iteration` (owner: MCTSWorkflowSearch)
- `mcts.search.completed` (owner: MCTSWorkflowSearch)
- `mcts.search.failed` (owner: MCTSWorkflowSearch)

#### Scenario: 4 主题注册

- **WHEN** 静态检查 `grep "mcts.search" docs/adr/adr-0068-event-emission-contract.md`
- **THEN** 4 主题必须全部出现
- **AND** 每个主题必须有 owner + payload schema

#### Scenario: 事件发射验证

- **WHEN** 运行 `test_mcts_workflow_search::mcts_event_emission`
- **THEN** 完整搜索流程发射全部 4 事件（搜索开始 + iteration + 完成）

### Requirement: V1 边界遵守 (Mock 模板 + L1 only)

The T20 V1 MUST use Mock template instantiation (no real LLM API calls). L2+ mutation variants MUST be rejected by MutationGovernor V1.

#### Scenario: Mock 模板实例化

- **WHEN** MCTS 扩展节点
- **THEN** 使用预设 5 轴模板组合（无 LLM API 调用）

#### Scenario: L1 only

- **WHEN** MCTS 提交 workflow variant
- **THEN** 仅 mutation_kind="L1_prompt" 通过 MutationGovernor

### Requirement: ctest 全量零回归

The `ctest --output-on-failure` MUST report ALL tests PASS with zero regressions relative to T21 ship baseline (190/191, 1 pre-existing timing flake).

#### Scenario: ctest 全量 PASS

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 所有测试 PASS, 0 failures（pre-existing flake 不计入）
- **AND** 测试计数 ≥ T21 baseline + 1 (test_mcts_workflow_search target)

#### Scenario: test_mcts_workflow_search 专项

- **WHEN** 运行 `ctest --output-on-failure -R test_mcts_workflow_search`
- **THEN** ≥ 10 cases PASS, ≥ 30 assertions

### Requirement: adr_lint + docs_drift_audit 全通过

The `python3 tools/adr_lint.py` MUST exit 0, and `python3 tools/docs_drift_audit.py` MUST NOT introduce new CRITICAL drift.

#### Scenario: adr_lint 全过

- **WHEN** 运行 `python3 tools/adr_lint.py`
- **THEN** 退出码 0, 无 ADR lint 错误

#### Scenario: docs_drift_audit 零新增

- **WHEN** 运行 `python3 tools/docs_drift_audit.py`
- **THEN** MCTS 相关 drift 为 0 项新增

### Requirement: ADR-0061-08 状态翻转 🔍 → ✅

The ADR-0061-08 header MUST update from "🔍 Proposed" to "✅ Approved (V1 ship, 2026-08-28)" with ship evidence appended.

#### Scenario: ADR 状态翻转

- **WHEN** 静态检查 `grep "状态" docs/adr/skill/adr-0061-08-aflow-search.md | head -3`
- **THEN** 必须显示 ✅ Approved

### Requirement: cap-map §一 +1 + §八 T20 ✅ Completed

The capability-application-map MUST add new capability #29 (AFlow MCTS) to §一 and mark §八 T20 row as "✅ Completed".

#### Scenario: §一 +1 新能力

- **WHEN** 静态检查 `grep "AFlow MCTS" docs/architecture/capability-application-map-2026-08.md | head -3`
- **THEN** §一表格必须新增 AFlow MCTS 能力行

#### Scenario: §八 T20 Completed

- **WHEN** 静态检查 `grep "T20.*Completed" docs/architecture/capability-application-map-2026-08.md`
- **THEN** §八 T20 行必须显示 "✅ Completed"