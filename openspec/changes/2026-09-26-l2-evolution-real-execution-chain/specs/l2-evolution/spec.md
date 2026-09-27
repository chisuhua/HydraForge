# l2-evolution Specification (real-execution-chain, Phase A+B partial)

## Purpose

This spec adds the Phase A+B partial implementation of L2 reference
example (`pdk_chat_demo_evolution`) on top of the archived
`pdk-chat-demo-evolution` specification (which provided the CLI surface
+ 6-phase chain facade).

**Phase A+B scope (shipped in commit `d42b47b` + `2916d2f` + `1fd1450`)**:
- **Dual-mode provider**: `--mock` + `--real-llm <provider>` real wiring (was facade)
- **Phase 1 + 2 real wiring**: `DSLEngine` + `bus` + `set_llm_provider` + `ChatSession` 11-param ctor + real `chat()` capture
- **`--release-metrics` real computation**: drop_ratio formula (later corrected in finalization)
- **R13.4 sensitivity redaction**: confidential/internal → `[REDACTED-<level>]`
- **3 SoTs §十一 "L2 ✅ ship" row** scope-limited to "Phase B only"

**Phase C scope (deferred to `2026-09-26-l2-evolution-finalization`)**:
- Phase 3 mutation real wiring (still hardcoded stub)
- Phase 4 reload_rerun real wiring (still hardcoded stub)
- Phase 5 compare real wiring (still `++mutated_passes_` stub)
- `--capture-mode Training` → `IDistillationWriter` wiring (still inert)
- `--ablation-mode=full` ablation_report.json (still unwritten)

This change is superseded by `2026-09-26-l2-evolution-finalization` (per
`real-execution-chain/proposal.md` line 7 + `finalization/.openspec.yaml`
`supersedes: [2026-09-26-l2-evolution-real-execution-chain]`).

---

## ADDED Requirements

### Requirement: dual-mode-provider

`pdk_chat_demo_evolution` MUST support both `--mock` and `--real-llm <provider>` modes with REAL provider wiring (not facade). In mock mode, phase 1 init MUST construct `LLMProviderFactory::create(LLMConfig{"mock"})` and inject it into `DSLEngine` via `set_llm_provider()`. In real-LLM mode, MUST construct with the specified provider name via `LLMProviderFactory`.

#### Scenario: mock-provider-wired-real

- **WHEN** `--mock` flag is passed
- **THEN** phase1_init MUST create a `MockLLMProvider` via `LLMProviderFactory`
- **AND** `ChatSession::chat()` MUST return instantly with deterministic response
- **AND** trace JSONL MUST show `"tokens"` > 0 (mock provider returns total_steps + tokens ≥ 1)

#### Scenario: real-llm-provider-wired-real

- **WHEN** `--real-llm deepseek` flag is passed
- **THEN** phase1_init MUST construct `CloudLLMAdapter` via `LLMProviderFactory`
- **AND** `ChatSession::chat()` MUST make real HTTP call (requires `DEEPSEEK_API_KEY` env var)

---

### Requirement: phase1-init-real-wiring

`EvolutionSession::phase1_init` MUST construct `DSLEngine` (default ctor), call `set_interaction_bus(bus_)`, construct `LLMProviderFactory::create()` → `set_llm_provider()`, and instantiate `ChatSession` with 11-param constructor signature using `AgentConfig` + `SessionConfig` (NOT facade shortcut). MUST also register mock `loop`/`run` tools on `ChatSession` for `--mock` mode compat (per commit `7706a82`).

#### Scenario: phase1-init-constructs-all-components

- **WHEN** `EvolutionSession("mock", "None", true)` ctor runs
- **THEN** `engine_` MUST be a non-null `DSLEngine`
- **AND** `bus_` MUST be a non-null `IInteractionBus`
- **AND** `chat_session_` MUST be a non-null `ChatSession` constructed with 11 args
- **AND** `mock_loop_` + `mock_run_` tools MUST be registered

---

### Requirement: phase2-baseline-real-capture

`EvolutionSession::phase2_baseline` MUST call `chat_session_->chat(turn_input)` and capture REAL response fields. Trace event fields MUST come from `ChatResult`: `response` from `ChatResult.response`, `tokens` from `ChatResult.total_tokens`, `cost_usd` from `ChatResult.cost_usd`. MUST NOT use hardcoded empty strings.

#### Scenario: phase2-baseline-real-response-captured

- **WHEN** phase 2 baseline runs with mock provider
- **THEN** trace JSONL MUST show `"response": "Mock evolution response"` (was `null`)
- **AND** MUST show `"tokens": 1` (was `0`)
- **AND** MUST show `"cost_usd"` from `ChatResult.cost_usd` (was `0`)

---

### Requirement: release-metrics-real-computation

`--release-metrics` flag MUST compute and write `metrics.json` with REAL values. Fields: `baseline_total` (count of processed contexts), `baseline_failures` (count of baseline phases where `ChatResult.success == false`), `mutated_passes` (count of compare phases where `attribution_verdict == "Attributed"`). The drop_ratio formula in this Phase A+B ship is `(baseline_failures - mutated_passes) / baseline_total` (NOTE: this formula is semantically broken in mock mode — corrected in `finalization/design.md` D5).

If `drop_ratio > 0.05` MUST exit non-zero (R8.1 red line).

#### Scenario: release-metrics-writes-metrics-json

- **WHEN** `--release-metrics --mock --context-file <file>` runs
- **THEN** `metrics.json` MUST be created in CWD (NOTE: pollution risk documented in `finalization/proposal.md`; corrected to `/tmp/l2-metrics.json` in finalization D5)
- **AND** `metrics.json["drop_ratio"]` MUST be a number (was `-1.0` in pre-fix code)
- **AND** file MUST contain `baseline_total`, `baseline_failures`, `mutated_passes` fields

#### Scenario: release-metrics-drop-ratio-above-threshold-exit-nonzero

- **WHEN** `--release-metrics` is invoked AND `drop_ratio > 0.05`
- **THEN** binary MUST exit non-zero (R8.1 mechanism activated)

---

### Requirement: context-sensitivity-redaction

`ContextRequest` with `metadata.sensitivity ∈ {internal, confidential}` MUST have `turn_input`, `response`, `meta.tags`, `meta.domain` fields redacted to `[REDACTED-<level>]` in trace JSONL (per commit `1fd1450`).

| sensitivity | turn_input | response | meta.tags | meta.domain |
|-------------|------------|----------|-----------|-------------|
| none/public | passthrough | passthrough | passthrough | passthrough |
| internal | [REDACTED-internal] | [REDACTED-internal] | passthrough | passthrough |
| confidential | [REDACTED-confidential] | [REDACTED-confidential] | [REDACTED-confidential] | [REDACTED-confidential] |

#### Scenario: confidential-redacts-turn-input-and-response

- **WHEN** a `ContextRequest` has `metadata.sensitivity = "confidential"`
- **THEN** trace JSONL for that context MUST have `"turn_input": "[REDACTED-confidential]"`
- **AND** trace JSONL MUST have `"response": "[REDACTED-confidential]"`
- **AND** `meta` MUST have `"[REDACTED-confidential]"` for tags/domain fields

#### Scenario: public-passthrough-no-redaction

- **WHEN** a `ContextRequest` has `metadata.sensitivity` absent or `"public"`
- **THEN** trace JSONL MUST NOT be redacted (original values preserved)

---

### Requirement: cross-doc-consistency-ship-row

Three Source-of-Truth documents MUST carry a "L2 ✅ ship" row in §十一 referencing this change (scope-limited to "Phase B only"):

| Document | § | Row content |
|----------|---|-------------|
| `harness-architecture-2026-09.md` | §十一 | L2 reference example (real execution chain) ✅ ship 2026-09-26 |
| `rsi-architecture-2026-09.md` | §十一 | L2 reference example (real execution chain) ✅ ship 2026-09-26 |
| `self-evolution-architecture-2026-08.md` | §十一 | L2 reference example (real execution chain) ✅ ship 2026-09-26 (校订/新增) |

#### Scenario: three-sots-have-phase-b-only-scope-row

- **WHEN** `grep -n "Phase B only" docs/architecture/{harness,rsi,self-evolution}-architecture-*.md` runs
- **THEN** MUST find exactly 3 matches (one per SoT)
- **AND** each MUST reference `2026-09-26-l2-evolution-real-execution-chain` as the change slug

---

## Out-of-Spec (deferred to `2026-09-26-l2-evolution-finalization`)

| Item | Status |
|------|--------|
| Phase 3 mutation real wiring (`apply_harness_mutation`) | Hardcoded stub; finalization replaces |
| Phase 4 reload_rerun real wiring (`IGenomeRegistry::load`) | Hardcoded stub; finalization replaces |
| Phase 5 compare real wiring (`IEvaluator::compare`) | `++mutated_passes_` stub; finalization replaces |
| `--capture-mode Training` → `IDistillationWriter` | Inert string; finalization wires |
| `--ablation-mode=full` → `ablation_report.json` | Flag parsed, output never written; finalization wires |
| `--metrics-output <path>` flag | Metrics written to CWD; finalization moves to `/tmp/l2-metrics.json` |
| Confounded verdict mapping | Confounded unreachable in L2 single-turn; finalization narrows to `{Attributed, Insufficient}` per ADR-0086 v1.1 |
| 3 SoTs §十一 "Phase B only" qualifier removal | Finalization removes qualifier when Phase C ships |