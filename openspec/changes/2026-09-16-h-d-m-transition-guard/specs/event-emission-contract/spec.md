# Spec Delta: event-emission-contract — 2 new evolution.* topics

> **Change**: C3 h-d-m-transition-guard (registers 2 new event topics)
> **STATUS**: DRAFT (post Oracle dual-agent review `bg_fed9d7c0`, 🟠-5 幻影主题修正)
> **Target delta**: `docs/adr/adr-0068-event-emission-contract.md` Appendix A (Canonical Topic Registry)
> **Created**: 2026-09-20

---

## MODIFIED Requirements

### Requirement: evolution-transition-denied-topic-registered
ADR-0068 v1.2.1 Appendix A MUST register the topic `evolution.transition.denied` emitted by `TransitionGuard::can_transition()` when returning `can_proceed: false`. This replaces the previous (phantom) topic `evolution.scheduler.denied` referenced in placeholder C3 spec.

#### Scenario: 主题注册到 Appendix A
- **WHEN** ADR-0068 v1.2.1 Appendix A is read
- **THEN** it MUST contain an entry for `evolution.transition.denied` with:
  - **Payload schema**: `{from: EvolutionState, to: EvolutionState, reason: string, current_genome: GenomeVersion, last_harness_change: GenomeVersion}`
  - **Producer**: `agenticdsl::evolution::TransitionGuard::can_transition()`
  - **Trigger condition**: `EvolutionVerdict.can_proceed == false`

#### Scenario: 幻影主题 evolution.scheduler.denied 禁用
- **WHEN** C3 code emits events
- **THEN** it MUST NOT use `evolution.scheduler.denied` topic (phantom, not registered in Appendix A)
- **AND** it MUST use `evolution.transition.denied` instead

#### Scenario: 主题 payload 完整性
- **WHEN** `evolution.transition.denied` is emitted
- **THEN** payload MUST contain all 5 required fields:
  - `from` (EvolutionState enum value)
  - `to` (EvolutionState enum value)
  - `reason` (non-empty string, e.g., "H→M forbidden, must run H→D→M")
  - `current_genome` (GenomeVersion with non-empty name + version >= 0)
  - `last_harness_change` (GenomeVersion with non-empty name + version >= 0)

---

### Requirement: evolution-readiness-denied-topic-registered
ADR-0068 v1.2.1 Appendix A MUST register the topic `evolution.readiness.denied` emitted by `TransitionGuard::evaluate_readiness()` when returning `can_proceed: false`.

#### Scenario: 主题注册到 Appendix A
- **WHEN** ADR-0068 v1.2.1 Appendix A is read
- **THEN** it MUST contain an entry for `evolution.readiness.denied` with:
  - **Payload schema**: `{failed_conditions: string[], attribution_verdict: AttributionVerdict, eval_quality: RewardSignal::Quality, budget_state: {max: int64_t, used: int64_t, remaining: int64_t}}`
  - **Producer**: `agenticdsl::evolution::TransitionGuard::evaluate_readiness()`
  - **Trigger condition**: `EvolutionVerdict.can_proceed == false`

#### Scenario: 主题 payload 完整性
- **WHEN** `evolution.readiness.denied` is emitted
- **THEN** payload MUST contain all 4 required fields:
  - `failed_conditions` (non-empty string array, e.g., ["attribution_not_attributed", "budget_insufficient"])
  - `attribution_verdict` (AttributionVerdict enum value)
  - `eval_quality` (RewardSignal::Quality enum value)
  - `budget_state` (object with max/used/remaining LLM call counts)

#### Scenario: fail-closed 默认产生拒绝事件
- **WHEN** ADR-0086 v1.1 is not yet shipped (attribution unavailable)
- **THEN** `evaluate_readiness` returns can_proceed=false with reason="attribution_unavailable"
- **AND** MUST emit `evolution.readiness.denied` event with `failed_conditions = ["attribution_not_attributed"]`

---

## Cross-references

- **Consumer**: `openspec/changes/2026-09-16-h-d-m-transition-guard/` (C3 h-d-m-transition-guard)
- **Producer**: `agenticdsl::evolution::TransitionGuard` (will be implemented in C3)
- **ADR-0068 base**: `docs/adr/adr-0068-event-emission-contract.md` ✅ v1.2.1
- **Oracle review**: `bg_fed9d7c0` (2026-09-20, 🟠-5 幻影主题风险)

## Rationale (Oracle 🟠-5 决策依据)

原 placeholder C3 spec 引用 `evolution.scheduler.denied` 主题。Oracle dual-agent review 识别该主题**在 ADR-0068 Appendix A 中不存在**（与 ADR-0068 决策 5 "Appendix A 强制注册"原则冲突）。修正策略：
1. 拆分单一主题为两个语义清晰的主题（transition vs readiness）
2. 命名与 ADR-0068 命名约定一致（`evolution.*` 域）
3. C3 change 在 Appendix A 增补 MODIFIED delta（保留 ADR-0068 governance 严格性）
