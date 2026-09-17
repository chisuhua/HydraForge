# Spec: transition-guard

> **STATUS: PLACEHOLDER** — 4 placeholder Requirements with TBD Scenarios

## ADDED Requirements

### Requirement: H→D→M 强制规则
`TransitionGuard::can_transition(from, to, ...)` MUST return `{can_proceed: false, reason: "H→M forbidden, must run H→D→M"}` when `from == Harness && to == Model`. The check MUST be deterministic (no LLM call, no async) and MUST be available in constexpr context.

#### Scenario: H→M 直跳被拒绝
- **WHEN** `can_transition(Harness, Model, ...)` is called
- **THEN** it MUST return `EvolutionVerdict{can_proceed: false, reason: "H→M forbidden, must run H→D→M"}`

#### Scenario: H→D 允许
- **WHEN** `can_transition(Harness, Data, ...)` is called
- **THEN** it MUST return `EvolutionVerdict{can_proceed: true, recommended_next: Data}`

#### Scenario: 编译期验证
- **WHEN** `static_assert(can_transition(Harness, Model).can_proceed == false)` is compiled
- **THEN** the compiler MUST accept the static_assert (can_transition is constexpr)

---

### Requirement: evaluate_readiness 4 条件矩阵
`TransitionGuard::evaluate_readiness(eval_result, attribution, governance, budget)` MUST return `EvolutionVerdict` based on 4 conditions: (1) attribution == Attributed, (2) regression gate == PASS, (3) budget sufficient, (4) no uncontrolled confounder.

#### Scenario: 全条件满足 → Ready
- **WHEN** all 4 conditions are satisfied
- **THEN** `evaluate_readiness` MUST return `{verdict: Ready, recommended_next: <next operator>}`

#### Scenario: 任一条件不满足 → NotReady
- **WHEN** any of 4 conditions is not satisfied (e.g., regression gate FAILED)
- **THEN** `evaluate_readiness` MUST return `{verdict: NotReady, reason: "regression gate FAILED"}`

#### Scenario: 硬门禁触发 → Blocked
- **WHEN** `governance.verdict == Deny` (e.g., CaptureMode != Training when required)
- **THEN** `evaluate_readiness` MUST return `{verdict: Blocked, reason: "MutationGovernance denied"}`

---

### Requirement: 既有契约集成
`TransitionGuard` MUST integrate with existing ADR contracts via interface injection (NOT new operator interfaces). The 3 integration points are:
- `IEvaluator` (ADR-0083) for regression gate
- `MutationGovernance` (ADR-0084) for hard veto
- `CreditAssignment` (ADR-0086, 🔍 Proposed) for attribution (degraded to optional)

#### Scenario: 注入 IEvaluator
- **WHEN** `TransitionGuard` is constructed with an `IEvaluator*` pointer
- **THEN** `evaluate_readiness` MUST call `evaluator->evaluate()` for regression gate check

#### Scenario: CreditAssignment 缺失
- **WHEN** `CreditAssignment` (ADR-0086) is not yet shipped
- **THEN** `TransitionGuard` MUST work in degraded mode (skip attribution check)

---

### Requirement: 守卫失败时事件发射
When `can_transition` or `evaluate_readiness` returns `{can_proceed: false}`, the bus MUST emit `evolution.scheduler.denied` event with the failure reason in payload.

#### Scenario: H→M 拒绝事件
- **WHEN** `can_transition(Harness, Model, ...)` returns false
- **THEN** the bus MUST receive an `evolution.scheduler.denied` event
- **AND** payload MUST contain `{from: "Harness", to: "Model", reason: "H→M forbidden"}`
- **AND** event MUST use `EventBuilder` (per ADR-0068)

---

## REMOVED Requirements

(none)

---

## MODIFIED Requirements

(none)
