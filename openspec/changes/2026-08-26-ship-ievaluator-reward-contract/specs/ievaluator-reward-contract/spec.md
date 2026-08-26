# ievaluator-reward-contract Specification

## ADDED Requirements

### Requirement: IEvaluator 是评估执行的抽象接口

The IEvaluator MUST provide a pure virtual interface for evaluating a single execution trace, returning a scalar reward signal in `[-1.0, 1.0]` or a three-valued Verdict.

#### Scenario: 评估 happy path

- **GIVEN** 一个 ExecutionTrace（包含 final_result.ok + error_code + trajectory 引用）
- **AND** 一个 IEvaluator 子类（TaskSuccessEvaluator V1 实现）
- **WHEN** 调用 `evaluator->evaluate(trace)`
- **THEN** 返回 RewardSignal.quality ∈ {Excellent, Acceptable, Poor}
- **AND** RewardSignal.scalar ∈ [-1.0, 1.0]

#### Scenario: 比较两个轨迹

- **GIVEN** 两个 ExecutionTrace（a 与 b）
- **AND** 一个 IEvaluator 子类
- **WHEN** 调用 `evaluator->compare(a, b)`
- **THEN** 返回 +1（a 更好）, -1（b 更好）, 或 0（平局）

#### Scenario: 评估器不修改输入 trace

- **GIVEN** 一个 ExecutionTrace
- **WHEN** 调用 `evaluator->evaluate(trace)` 多次
- **THEN** trace 内容保持不变（评估 side-effect-free，per ADR-0083 §不变量 4）

#### Scenario: 评估器线程安全

- **GIVEN** 一个 IEvaluator 子类实例
- **WHEN** 多个线程并发调用 `evaluate()`
- **THEN** 无数据竞争（评估器无状态或仅 readonly 状态，per ADR-0083 §不变量 3）

### Requirement: V1 TaskSuccessEvaluator 基于 ToolResult.ok 简化实现

The TaskSuccessEvaluator MUST be the V1 default evaluator that maps `ToolResult.ok` to RewardSignal.quality.

#### Scenario: ok=true → Excellent

- **GIVEN** ExecutionTrace.final_result.ok = true
- **WHEN** 调用 `TaskSuccessEvaluator::evaluate(trace)`
- **THEN** 返回 `RewardSignal::excellent(1.0)`

#### Scenario: ok=false → Poor

- **GIVEN** ExecutionTrace.final_result.ok = false
- **WHEN** 调用 `TaskSuccessEvaluator::evaluate(trace)`
- **THEN** 返回 `RewardSignal::poor(1.0)`

#### Scenario: BehavioralEquivalenceEvaluator 与 CompositeEvaluator 推迟到 V2

- **GIVEN** ADR-0083 §决策 5 V1 简化决定
- **THEN** `BehavioralEquivalenceEvaluator` 与 `CompositeEvaluator` **不**在 V1 实现
- **AND** 留作 OpenSpec follow-up change（提议名: `2026-08-XX-ship-evaluator-v2-composite`）