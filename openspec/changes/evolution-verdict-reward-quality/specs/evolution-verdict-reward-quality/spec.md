# EvolutionVerdict Reward Quality Spec

## ADDED Requirements

### Requirement: EvolutionVerdict 携带 reward_quality

`EvolutionVerdict` struct MUST 包含 `reward_quality` 字段，类型为 `agenticdsl::RewardSignal::Quality`，默认值为 `Quality::Unknown`。`evaluate_readiness()` MUST 将 evaluator 返回的 `RewardSignal.quality` 填充到 verdict.reward_quality（无论 can_proceed 真假）。

#### Scenario: 评估返回 Good
- **WHEN** evaluator 返回 RewardSignal{quality: Good, scalar: 1.0}
- **THEN** verdict.reward_quality == Quality::Good
- **AND** verdict.can_proceed == true（回归通过）

#### Scenario: 评估返回 Poor 且回归失败
- **WHEN** evaluator 返回 RewardSignal{quality: Poor, scalar: -1.0}
- **THEN** verdict.reward_quality == Quality::Poor
- **AND** verdict.can_proceed == false
- **AND** failed_conditions 包含 "regression_failed"
- **AND** reward_quality 仍为 Poor（质量信号不因失败而丢失）

#### Scenario: 默认构造
- **WHEN** `EvolutionVerdict v;` 默认构造
- **THEN** v.reward_quality == Quality::Unknown

### Requirement: readiness.denied 事件 eval_quality 真实值

`apply_harness_mutation` 发射 `evolution.readiness.denied` 事件时，`eval_quality` 字段 MUST 反映 `verdict.reward_quality` 的真实值（映射为字符串），不得硬编码 `"Unknown"`。

#### Scenario: 质量 Good 事件透传
- **WHEN** evaluator 返回 quality Good 且 readiness 失败（如 budget_exhausted）
- **THEN** 事件 payload 的 eval_quality == "Good"
- **AND** failed_conditions == ["budget_exhausted"]

#### Scenario: 质量 Poor 事件透传
- **WHEN** evaluator 返回 quality Poor 且回归失败
- **THEN** 事件 payload 的 eval_quality == "Poor"
- **AND** failed_conditions == ["regression_failed"]
