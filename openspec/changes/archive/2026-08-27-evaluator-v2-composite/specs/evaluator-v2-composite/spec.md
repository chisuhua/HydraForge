# evaluator-v2-composite Specification

## ADDED Requirements

### Requirement: BehavioralEquivalenceEvaluator 基于 T14 行为回归

The `BehavioralEquivalenceEvaluator` MUST integrate `agenticdsl::testing::BehavioralRegressionGate` (T14 ship) for behavioral equivalence evaluation. The `compare(a, b)` method MUST use fingerprint + Hotelling T² test to determine verdict.

#### Scenario: BehavioralEquivalence evaluate V1 返回 Acceptable

- **WHEN** 运行 `test_evaluator::behavioral_equivalence_evaluate_single_returns_acceptable`
- **THEN** 输入单 ExecutionTrace，evaluate 返回 RewardSignal.quality == Acceptable
- **AND** RewardSignal.confidence 在 [0.0, 1.0] 区间

#### Scenario: BehavioralEquivalence compare V1 实现

- **WHEN** 运行 `test_evaluator::behavioral_equivalence_compare_pass_pair`
- **THEN** 相似 fingerprint + Verdict::Pass → compare 返回 0

- **WHEN** 运行 `test_evaluator::behavioral_equivalence_compare_fail_pair`
- **THEN** 差异 fingerprint + Verdict::Fail → compare 返回 +1/-1（基于 reward scalar 比较）

### Requirement: CompositeEvaluator 多评估器加权聚合

The `CompositeEvaluator` MUST aggregate multiple `shared_ptr<IEvaluator>` instances with configurable weights. Constructor MUST validate non-empty evaluators and matching weights count.

#### Scenario: Composite 2-evaluator 加权聚合

- **WHEN** 运行 `test_evaluator::composite_aggregate_two_evaluators`
- **THEN** 2 evaluators + weights [0.5, 0.5] → scalar 加权平均
- **AND** quality 取众数（Excellent > Acceptable > Poor）

#### Scenario: Composite 空 evaluators 抛异常

- **WHEN** 运行 `test_evaluator::composite_aggregate_empty_evaluators_throws`
- **THEN** 构造空 vector → 抛 `std::invalid_argument`

#### Scenario: Composite weights 数量不匹配抛异常

- **WHEN** 运行 `test_evaluator::composite_weights_mismatch_throws`
- **THEN** evaluators 数 ≠ weights 数 → 抛 `std::invalid_argument`

### Requirement: IEvaluator 接口零修改

The V2 implementation MUST NOT modify `include/agenticdsl/contract/ievaluator.h`. Only new subclasses may be added. V1 contracts MUST remain unchanged.

#### Scenario: 接口文件未变更

- **WHEN** `git diff HEAD~1 -- include/agenticdsl/contract/ievaluator.h`
- **THEN** 不应有 diff（V2 仅子类）

#### Scenario: V1 评估器实现未变更

- **WHEN** `git diff HEAD~1 -- src/modules/cognitive/task_success_evaluator.cpp`
- **THEN** 不应有 V2 相关 diff

### Requirement: V1 + V2 评估器共存

The V2 implementation MUST support simultaneous injection of V1 and V2 evaluators into existing `CognitiveWorker` and `DomainWorkerPool` via the existing `set_evaluator(shared_ptr<IEvaluator>)` API.

#### Scenario: V1 + V2 同时注入

- **WHEN** 运行 `test_evaluator::v1_v2_coexistence`
- **THEN** TaskSuccessEvaluator + BehavioralEquivalenceEvaluator 同时注入，evaluate 互不干扰

#### Scenario: Composite 包装 V1

- **WHEN** 运行 `test_evaluator::composite_with_v1_inside`
- **THEN** CompositeEvaluator 包装 TaskSuccessEvaluator → V1 在 V2 内正常工作

### Requirement: 构造函数安全校验

The `CompositeEvaluator` constructor MUST validate non-empty evaluators and matching weights count, throwing `std::invalid_argument` on invalid input.

#### Scenario: 边界 case 安全

- **WHEN** 调用 `CompositeEvaluator({}, {1.0})`
- **THEN** 抛 `std::invalid_argument` (evaluators empty)

- **WHEN** 调用 `CompositeEvaluator({evaluator1}, {0.3, 0.7})`
- **THEN** 抛 `std::invalid_argument` (size mismatch)

### Requirement: ctest 全量零回归

The `ctest --output-on-failure` MUST report ALL tests PASS with zero regressions relative to the V1 baseline (12 cases / 31 assertions, commit `21dd622`). Test count MUST grow by exactly the V2新增 cases (≥ 6 new cases for BehavioralEquivalence + Composite).

#### Scenario: ctest 全量 PASS

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 所有测试 PASS, 0 failures（pre-existing `test_event_log_query_perf` timing flake 不计入）
- **AND** 测试计数 ≥ V1 baseline + 6 (动态计数, 禁止硬编码)

#### Scenario: test_evaluator 专项

- **WHEN** 运行 `ctest --output-on-failure -R test_evaluator`
- **THEN** ≥ 18 cases PASS (V1 12 cases + V2 ≥ 6 cases)

### Requirement: adr_lint + docs_drift_audit 全通过

The `python3 tools/adr_lint.py` MUST exit 0, and `python3 tools/docs_drift_audit.py` MUST NOT introduce new CRITICAL drift.

#### Scenario: adr_lint 全过

- **WHEN** 运行 `python3 tools/adr_lint.py`
- **THEN** 退出码 0, 无 ADR lint 错误

#### Scenario: docs_drift_audit 零新增

- **WHEN** 运行 `python3 tools/docs_drift_audit.py`
- **THEN** IEvaluator V2 相关 drift 为 0 项新增

### Requirement: ADR-0083 V2 ship 注记 + cap-map §一 +1

The ADR-0083 header MUST append V2 ship evidence (commit hash, test count, V2 evaluators). The capability-application-map MUST add new capability #26 (IEvaluator V2) to §一 and bump version v1.9 → v2.0.

#### Scenario: ADR-0083 V2 状态注记

- **WHEN** 静态检查 `grep "V2 ship\|BehavioralEquivalence.*Composite" docs/adr/adr-0083-evaluator-reward-contract.md`
- **THEN** V2 ship 证据必须可见

#### Scenario: cap-map §一 +1 新能力

- **WHEN** 静态检查 `grep "IEvaluator V2\|BehavioralEquivalence" docs/architecture/capability-application-map-2026-08.md`
- **THEN** §一表格必须新增 IEvaluator V2 能力行