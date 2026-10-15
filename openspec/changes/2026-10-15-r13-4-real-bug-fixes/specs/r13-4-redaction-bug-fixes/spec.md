# Spec: R13.4 confidential redaction 2 真 bug 修复

**Change ID**: `2026-10-15-r13-4-redaction-bug-fixes`
**Status**: draft
**对应 proposal**: `proposal.md`
**对应 design**: `design.md`

## 1. 范围

本 spec 锁定 R13.4 confidential redaction 在 L2 reference example 中的 2 个真 bug 修复：

- **Bug 1**: `expected_eval_quality` 漏脱敏（spec 409 显式要求，impl 582-602 缺字段）
- **Bug 2**: 跨 context 泄漏（共享 ChatSession 单实例 + messages history 累积）

## ADDED Requirements

### Requirement: R13.4 `expected_eval_quality` 脱敏合规

The system MUST extend `pdk_chat_demo_evolution::detail::redact_trace_fields` (`evolution_session.cpp:582-602`) to redact the `expected_eval_quality` field per L2 spec R13.4 line 409 compliance.

#### Scenario: confidential sensitivity redacts expected_eval_quality

- **WHEN** ContextRequest `metadata.sensitivity == "confidential"`
- **AND** trace event top-level contains `expected_eval_quality` field (string type, non-null)
- **THEN** `redact_trace_fields` MUST replace `event["expected_eval_quality"]` with literal string `"[REDACTED-confidential]"`
- **AND** the replacement MUST happen BEFORE `tracer_->record_phase()` call (line 322 / line 449)
- **AND** the original value MUST NOT appear in stdout / trace JSONL / metrics.json

#### Scenario: internal sensitivity redacts expected_eval_quality

- **WHEN** ContextRequest `metadata.sensitivity == "internal"`
- **AND** trace event top-level contains `expected_eval_quality` field (string type, non-null)
- **THEN** `redact_trace_fields` MUST replace `event["expected_eval_quality"]` with literal string `"[REDACTED-internal]"`
- **NOTE**: Per spec 408, `internal` sensitivity redacts `turn_input` + `response`. Adding `expected_eval_quality` to `internal` redaction is consistent with `turn_input` + `response` handling (consistency with existing pattern, per design D2).

#### Scenario: public sensitivity preserves expected_eval_quality

- **WHEN** ContextRequest `metadata.sensitivity == "public"` (or empty)
- **AND** trace event top-level contains `expected_eval_quality` field
- **THEN** `redact_trace_fields` MUST preserve original value (no replacement)
- **NOTE**: `public` sensitivity redaction policy is `none` (per spec 407).

#### Scenario: null expected_eval_quality skip redaction

- **WHEN** trace event top-level contains `expected_eval_quality` field but value is `null` (JSON null type)
- **THEN** `redact_trace_fields` MUST skip redaction (no replacement)
- **NOTE**: This is consistent with existing `response` field handling (line 591 `!event["response"].is_null()` guard).

#### Scenario: missing expected_eval_quality skip redaction

- **WHEN** trace event top-level does NOT contain `expected_eval_quality` field
- **THEN** `redact_trace_fields` MUST skip redaction (no-op)
- **NOTE**: This is consistent with all other field handling (line 587 `event.contains("turn_input")` guard pattern).

### Requirement: 跨 context ChatSession 隔离 (per R13.4 cross-context leak prevention)

The system MUST reset `chat_session_` (EvolutionSession::Impl) per context to prevent cross-context LLM call payload pollution.

#### Scenario: per-context chat_session reset on phase2_baseline entry

- **WHEN** `EvolutionSession::run_6_phase_demo()` enters `phase2_baseline(ctx)` for any context `ctx`
- **THEN** `chat_session_` MUST be reset via `chat_session_.reset()` followed by `chat_session_ = std::make_unique<ChatSession>(...)` (re-using `phase1_init` make_unique pattern)
- **AND** the new `ChatSession` MUST have empty `impl_->messages` history (no previous context messages)
- **AND** the new `ChatSession` MUST be wired with the same `engine_` / `bus_` / `tool_registry_` / `agent_cfg_` / `cancellation_registry` as `phase1_init` initialization
- **NOTE**: Per design D1 option A (per-context reset), minimal risk, no ChatSession public API change.

#### Scenario: 2 contexts (public + confidential) cross-context isolation

- **WHEN** `run_6_phase_demo()` processes 2 contexts: `ctx1` (sensitivity=public, turn_input="CAP theorem") + `ctx2` (sensitivity=confidential, turn_input="server breach")
- **THEN** `ctx2` LLM call payload `history` field MUST NOT contain `ctx1` turn_input string `"CAP theorem"` (no cross-context pollution)
- **AND** `ctx2` trace event `response` field MUST be `"[REDACTED-confidential]"` (per R13.4 mock test line 298)
- **AND** `ctx1` trace event `response` field MUST be preserved (sensitivity=public, no redaction)
- **NOTE**: Verified via mock LLM provider lambda capture of `loop_args["history"]` in test_reverse_indicators Case 6.

#### Scenario: per-context reset preserves real LLM mode hermetic HOME

- **WHEN** real LLM mode is enabled (per `l2-evolution-real-llm-provider` design D1)
- **AND** `phase2_baseline(ctx)` per-context reset
- **THEN** the new `ChatSession` MUST use the same `real_provider_` borrow (no provider re-injection)
- **AND** the `loop/run` tool registration from `phase1_init` MUST still be effective
- **AND** `setup_hermetic_home()` from `phase0_load_contexts` MUST still isolate HOME env vars
- **NOTE**: Per design D1, per-context reset only touches `chat_session_` member, not `engine_` / `real_provider_` / `bus_` (these are already per-`EvolutionSession`-instance).

### Requirement: 既有 9/9 mock tests 零回归

The system MUST preserve all 9 existing `l2-evolution` mock test cases after this change (R13.4 redaction bug fixes).

#### Scenario: test_l2_real_llm_e2e 5 cases still PASS

- **WHEN** running `ctest -L l2-evolution`
- **THEN** test_l2_real_llm_e2e 5 cases (real trace / real drop_ratio / unknown provider / missing key / hermetic dual-track) MUST still PASS

#### Scenario: test_reverse_indicators 4 existing + 2 new cases PASS

- **WHEN** running `ctest -R test_reverse_indicators`
- **THEN** existing 4 cases (R8 pre-cond / eval_quality null / attribution_verdict distribution / phase5_compare) MUST still PASS
- **AND** new 2 cases (R13.4 expected_eval_quality redact / cross-context isolation) MUST PASS

#### Scenario: test_l2_audit 3 cases still PASS

- **WHEN** running `ctest -R test_l2_audit`
- **THEN** 3 reverse_indicator R8 cases MUST still PASS (drop_ratio invariant / 3-segment ablation / failure-sample-retention-rate)

#### Scenario: test_l2_ship_validation PASS

- **WHEN** running `ctest -R test_l2_ship_validation`
- **THEN** all ship gate assertions MUST still PASS (Phase A+B+C finalization acceptance)

## MODIFIED Requirements

### Requirement: R13.4 Sensitivity Redaction Policy (per L2 spec line 401-417)

The `L2 spec §R13.4 Sensitivity Redaction Policy` MUST be interpreted as the following invariant (per this change):

> `redact_trace_fields(json, sensitivity)` MUST be **complete** (cover `turn_input` + `response` + `expected_eval_quality` + `meta.tags` + `meta.domain` for `confidential`, plus `internal` partial coverage of `turn_input` + `response` + `expected_eval_quality`).

> Cross-context isolation MUST be enforced via per-context `ChatSession` reset, ensuring LLM call payload `history` field does NOT contain previous contexts' content.

The original spec text (line 409) lists `expected_eval_quality` in the `confidential` redaction column. This change makes the implementation match the spec, by extending the redaction function. No spec text change is required (spec is already correct; implementation gap is being fixed).

#### Scenario: redact_trace_fields MUST cover all 5 spec-line-409 fields for confidential sensitivity

- **WHEN** any `phase2_baseline` / `phase4_reload_rerun` / `phase5_compare` emits a trace event
- **AND** the ContextRequest `metadata.sensitivity == "confidential"`
- **THEN** `redact_trace_fields(json, "confidential")` MUST replace, before `tracer_->record_phase()` is called:
  - `event["turn_input"]` → `"[REDACTED-confidential]"` (per spec 409)
  - `event["response"]` → `"[REDACTED-confidential]"` (per spec 409)
  - `event["expected_eval_quality"]` → `"[REDACTED-confidential]"` (per spec 409, **previously missing — Bug 1 of this change**)
  - `event["meta"]["tags"]` → `"[REDACTED-confidential]"` (per spec 409)
  - `event["meta"]["domain"]` → `"[REDACTED-confidential]"` (per spec 409)
- **AND** MUST preserve `event["meta"]["context_id"]` + `event["meta"]["task_class"]` + `event["meta"]["sensitivity"]` (audit trail, per spec 409 NOTE)

#### Scenario: per-context ChatSession reset MUST prevent cross-context leak

- **WHEN** `run_6_phase_demo` processes context `ctx_n` after processing `ctx_{n-1}`
- **AND** `ctx_{n-1}.turn_input` was sent through `ChatSession::chat` previously
- **THEN** `ctx_n` LLM call payload `history` field MUST NOT contain `ctx_{n-1}.turn_input` plaintext
- **AND** `chat_session_` MUST be reset on `phase2_baseline` entry (per design D1 option A, **Bug 2 of this change**)

## REMOVED Requirements

None. This change is purely additive (extending existing redaction function with one more field check, and adding per-context reset to phase2_baseline).

## Cross-references

- L2 spec source: `openspec/changes/archive/2026-09-24-pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md:401-417`
- R13.4 mock test: `examples/pdk_chat_demo_evolution/tests/test_reverse_indicators.cpp:265-301`
- Implementation: `examples/pdk_chat_demo_evolution/evolution_session.cpp:582-602` (redact_trace_fields) + `:290-301` (phase2_baseline) + `:252` (phase1_init make_unique)
- ChatSession history management: `pdk/chat_session/src/chat_session.cpp:ChatSession::chat` (`impl_->messages.push_back`)
- AGENTS.md §Reverse Indicator Rule 5 字段
- AGENTS.md §ENGINEERING PATTERNS Pattern #4 (SHIP-with-fixes atomic commits)
- Dual-agent review: `.rddf/audits/2026-10-15-r13-4-confidential-redaction-real-llm-dual-agent-review.md`
