# Spec: adr-0068-appendix-a-evolution-themes

> **Target**: Sprint 34+ follow-up to ADR-0088 v1.0 ship (commit 0ffc637 + 7a15744)
> **STATUS**: DRAFT (follow-up registered per Oracle NEEDS_FIX verdict 2026-09-20)
> **Created**: 2026-09-20

---

## ADDED Requirements

### Requirement: evolution-transition-denied-topic-registration
ADR-0068 v1.9 Appendix A MUST register the topic `evolution.transition.denied` with payload schema `{ from: string, to: string, reason: string, current_genome: { name: string, version: uint64 }, last_harness_change: { name: string, version: uint64 } }`. EventBuilder enforcement MUST accept emission of this topic without REJECT.

#### Scenario: topic registration
- **WHEN** `EventBuilder(topic="evolution.transition.denied", payload=...)` is called
- **THEN** EventBuilder MUST NOT throw `UnknownTopicError`
- **AND** emit MUST publish to bus subscribers

#### Scenario: payload schema validation
- **WHEN** a subscriber receives `evolution.transition.denied` event
- **THEN** payload MUST contain all 5 fields (from / to / reason / current_genome / last_harness_change)
- **AND** schema MUST match openspec/specs/transition-guard/spec.md §evolution-transition-denied-event

### Requirement: evolution-readiness-denied-topic-registration
ADR-0068 v1.9 Appendix A MUST register the topic `evolution.readiness.denied` with payload schema `{ failed_conditions: string[], attribution_verdict: string, eval_quality: string, budget_state: string }`. EventBuilder enforcement MUST accept emission of this topic without REJECT.

#### Scenario: topic registration
- **WHEN** `EventBuilder(topic="evolution.readiness.denied", payload=...)` is called
- **THEN** EventBuilder MUST NOT throw `UnknownTopicError`
- **AND** emit MUST publish to bus subscribers

#### Scenario: payload schema validation
- **WHEN** a subscriber receives `evolution.readiness.denied` event
- **THEN** payload MUST contain all 4 fields (failed_conditions / attribution_verdict / eval_quality / budget_state)
- **AND** schema MUST match openspec/specs/transition-guard/spec.md §evolution-readiness-denied-event

### Requirement: evolution-themes-blocked-c4-pilot
The 2 evolution event topics MUST be registered BEFORE C4 harness-rsi-pilot fill, otherwise the pilot's readiness gate rejection paths cannot emit observable events. Per Oracle NEEDS_FIX verdict 2026-09-20.

#### Scenario: blocked-by-deferred-D5-D6
- **WHEN** C4 harness-rsi-pilot attempts to emit `evolution.readiness.denied` via `EventBuilder`
- **THEN** without `2026-09-20-ig-genome-registry-walk-ancestors` (D5/D6 完整实装) + this change (D8 主题注册), the emit fails silently (UnknownTopicError)
- **AND** C4 pilot would validate an empty shell

---

## Cross-references

- ADR-0068 v1.9 amendment (Event Emission Contract)
- ADR-0088 D8 (event topic naming correction)
- openspec/specs/transition-guard/spec.md §evolution-{transition,readiness}-denied-event Requirements