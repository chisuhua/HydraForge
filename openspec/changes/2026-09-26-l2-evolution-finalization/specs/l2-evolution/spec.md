# l2-evolution spec (finalization)

This spec evolves the existing `pdk-chat-demo-evolution/spec.md`
(archived `2026-09-24-pdk-chat-demo-evolution-reference-example`) + the
Phase A+B partial updates in `2026-09-26-l2-evolution-real-execution-chain`.

The finalization change ships Phase C + housekeeping, making all R1-R13
requirements fully satisfied.

---

## R1 — CLI surface (dual-mode)

**Existing (Phase A+B ship)**:
8 flags: `--context-file` (req), `--real-llm`, `--capture-mode`,
`--trace-events`, `--mock`, `--release-metrics`, `--ablation-mode`,
`--accept-contexts`.

**Finalization adds**:
- `--metrics-output <path>` (default `/tmp/l2-metrics.json`)

**Deferred**:
- `--regression-test-suite` (spec 降级 recommendation: remove from R1)

### Acceptance
- `pdk_chat_demo_evolution --help` shows 9 flags (was 8)
- `--metrics-output /tmp/foo.json` writes metrics to /tmp/foo.json
- Missing `--metrics-output` defaults to `/tmp/l2-metrics.json`

---

## R2 — Six-phase chain

**Existing (Phase B ship, partial)**:
- Phase 1 init: real DSLEngine + bus + plugin + ChatSession 11-param ctor
- Phase 2 baseline: real `chat()` capturing response/tokens/cost

**Finalization completes**:
- Phase 3 mutation: real `apply_harness_mutation` + MutationGateContext + FilesystemGenomeRegistry
- Phase 4 reload_rerun: real `IGenomeRegistry::load` + hand-mapped AgentConfig + ChatSession rebuild + rerun (skip when no mutation applied)
- Phase 5 compare: real `IEvaluator::compare` (BehavioralEquivalence V2)
- Phase 6 emit: unchanged (trace JSONL) — capture-mode=Training adds distillation writer hook here

### Acceptance
- **4 distinct trace events per context** (corrected from original "6"
  — phase1_init and phase6_emit_jsonl do not emit trace events; only
  phase2-5 do)
- Each phase shows REAL computation result (no hardcoded `gate_passes:5` or `"Attributed"`)
- phase3 trace event has `genome_version > 1` after real mutation
  (achievable only with seed commit in phase1_init per Gate 0 requirement)
- phase4 trace event has `genome_version == phase3 trace genome_version`
  (chain link)
- phase5 trace event has real `attribution_verdict ∈ {Attributed, Insufficient}`
  (corrected from original `{Attributed, Confounded, Insufficient}` —
  `Confounded` requires `walk_ancestors` confounder detection
  ADR-0086 v1.1 which is out of L2 scope)

---

## R4 — Trace schema

**Existing**:
- 8 top-level fields + 12 meta fields (per archived spec)

**Finalization**:
- `attribution_verdict` enum strict: `Attributed | Confounded | Insufficient | NotAttempted | null`
  (**"Pending" removed** — was never in closed enum; corrected from
  original "Mutation traces have attribution_verdict: 'Pending'"
  acceptance that violated closed enum)
- `genome_version` int, monotonically increasing per chain
- `gate_passes` int, derived honestly from `MutationError` enum
  (InvalidMutation→0, NotReady→1, GovernanceDenied→2,
  RegistryRejected→3, success→5)

### Acceptance
- No `"Baseline"` in any trace (per commit `1fd1450` fix — was illegal enum regression)
- No `"Pending"` in any trace (corrected from this change's original draft)
- All baseline traces have `attribution_verdict: "NotAttempted"` (pre-attribution)
- Mutation traces have `attribution_verdict: "NotAttempted"` (resolved in phase5 compare)
- Reload traces have `attribution_verdict: "NotAttempted"` (same reason)
- Compare traces have `attribution_verdict ∈ {Attributed, Insufficient}`

---

## R5 — Capture-mode (training data path)

**Existing**: `--capture-mode <None|Training>` parsed but inert.

**Finalization**: capture-mode=Training activates
`agenticdsl::IDistillationWriter` via `make_file_writer(output_dir,
agent_id)`. Per-context `DistillationRecord` written (NOT a stream of
trace events). File naming follows `file_writer.cpp:72`:
`<agent_id>_<seq:06d>.distill.v1.jsonl`.

### Acceptance
- `--capture-mode Training` produces **≥1 file per context**
  (corrected from "≥4 events" — `DistillationRecord` schema is one
  record per file, not one trace event per JSONL line)
- File naming: `/tmp/l2-distillation-<session_id>_<seq:06d>.distill.v1.jsonl`
  (matches `make_file_writer("/tmp", "l2-distillation-<session_id>")`)
- File content has `agent_id / input / output / capture_mode / convergence`
  fields per `distillation_record.h:49-71` schema (NOT the 8+12
  trace event schema)
- `--capture-mode None` (default) produces no distillation file

### Strategic value
**Wave 3 Phase 2 D4 LoRA training pipeline data path enabler.** D4
implementation consumes this JSONL for training data loading
(per `openspec/changes/archive/2026-09-25-wave-3-phase-2-d4-lora-pipeline/`
improvement scope).

---

## R8 — Reverse indicator (drop_ratio + metrics)

**Existing (Phase A+B ship)**:
- `--release-metrics` real drop_ratio computation (formula was
  semantically broken: `(baseline_failures - mutated_passes) /
  baseline_total` → always -1.0 in mock mode)
- metrics.json write to CWD (polluted repo root)

**Finalization**:
- **Redefined formula**: `drop_ratio = mutated_failures /
  max(1, mutated_passes + mutated_failures)` — regression ratio
  semantically meaningful (0.0 in mock mode; >5% in regression cases)
- `--metrics-output <path>` flag (default `/tmp/l2-metrics.json`)
- Never writes to CWD (prevents repo root pollution per commit `1fd1450` adjacent finding)
- New `mutated_failures` field + `mutated_failures()` accessor

### Acceptance
- `metrics.json` not in `git status` after binary run (written to
  `/tmp/l2-metrics.json` by default, never CWD)
- `--metrics-output /tmp/foo.json` writes to specified path
- `drop_ratio == 0.0` in mock mode (all-pass baseline + mutation
  per the redefined formula `mutated_failures / max(1, mutated_total)`)
- `drop_ratio > 5%` triggers exit 1 (R8.1 red line)

---

## R9 — Anti-cheat (parser-side)

**Existing**:
- Hint regex detection (`text.contains("baseline answer")` etc.)
- Prefix-before-enum rejection (`mutation_metric_evaluation` task_class guard)
- Network keyword detection (`fetch http://...` guard)
- Events emitted per ADR-0068 v2.4

**Finalization**: unchanged. R9 parser-side detection already shipped.

---

## R13 — Context-driven

**Existing**:
- ContextRequest schema: 6 top-level + 4 metadata sub-fields
- Closed enums (sensitivity, invocation_mode, task_class)
- Hidden bucket (is_hidden=true) schema valid

**Finalization**:
- R13.4 sensitivity redaction enforced (per commit `1fd1450`)
  - `internal` → `[REDACTED-internal]`
  - `confidential` → `[REDACTED-confidential]`
  - Applies to turn_input, response, tags, domain

### Acceptance
- Confidential fixture test `R13.4 reverse_indicators: confidential
  sensitivity redacts turn_input` PASS (already shipped in commit `1fd1450`)
- `turn_input` field shows `[REDACTED-confidential]` for confidential context

---

## Cross-doc Consistency (finalization guarantee)

### Self-evolution §十一 ship row
Before: `| L2 reference example | ✅ ship 2026-09-26 | change 2026-09-26-l2-evolution-real-execution-chain (Phase B only) |`
After:  `| L2 reference example | ✅ ship 2026-09-26 | change 2026-09-26-l2-evolution-finalization (Phase B + Phase C) |`

### Harness §十一 ship row
Before: `| L2 reference example | ✅ ship 2026-09-26 | change 2026-09-26-l2-evolution-real-execution-chain (Phase B only) |`
After:  `| L2 reference example | ✅ ship 2026-09-26 | change 2026-09-26-l2-evolution-finalization (Phase B + Phase C) |`

### RSI §十一 ship row
Before: `| L2 reference example | ✅ ship 2026-09-26 | change 2026-09-26-l2-evolution-real-execution-chain (Phase B only) |`
After:  `| L2 reference example | ✅ ship 2026-09-26 | change 2026-09-26-l2-evolution-finalization (Phase B + Phase C) |`

(No scope qualifier — full L2 real execution chain shipped)

---

## Verification Matrix (finalization end-state)

| Requirement | Phase B ship | Phase C ship | Finalization | Evidence |
|-------------|--------------|--------------|--------------|----------|
| R1 CLI 8 flags | ✅ | n/a | +1 → 9 | `--help` (8 + --metrics-output) |
| R2 phase1+2 real | ✅ | +phase3+4+5 | ✅ | **4** distinct trace events per context (phase2-5 only; phase1/phase6 don't emit) |
| R4 trace schema | partial (verdict enum regression) | ✅ | ✅ (Pending removed, NotAttempted used) | test_reverse_indicators verdict compliance |
| R5 capture-mode | inert | Training wired | ✅ | `/tmp/l2-distillation-*.jsonl` (≥1 file per context) |
| R8 metrics | semantic hollow (drop_ratio=-1.0) | real (depends on phase5) | ✅ + --metrics-output | `/tmp/l2-metrics.json` + non-CWD + redefined formula |
| R9 anti-cheat | ✅ | n/a | ✅ | 3 test binaries PASS |
| R13.4 redaction | dead code | wired (phase2) | ✅ + phase4 also redacts | confidential fixture test PASS |
| Cross-doc consistency | scope qualifier on 2/3 | n/a | all 3 full | grep "Phase B only" in 3 SoTs = 0 |

---

## Reverse Indicator

Per AGENTS.md Reverse Indicator Rule (5 fields mandatory):

```
+ new_up: L2 reference example fully shipped (Phase B + C + housekeeping),
  real 4-phase self-evolution loop evidence per context (phase2-5),
  real capture-mode data path, R13.4 redaction enforced in both
  baseline and rerun phases, cross-doc consistency (no scope qualifiers)
- old_down: drop_ratio semantically mock-mode-empty (real LLM-driven
  evaluation deferred to Wave 3 D4; redefined formula gives 0.0 in mock)
failure_traces: per-phase failure → "NotAttempted" verdict + error meta;
  bootstrap attribution documented honestly (not "free-form hardcoding")
ablation: 3-segment baseline vs mutation vs rerun (mock = identical;
  real divergence requires Wave 3 D4 LoRA-tuned evaluator)
context_ids: code-class-ex-001, code-class-ex-002, r13-4-confidential-test-001
```

---

## Out-of-Spec (tracked for follow-up)

| Item | Status | Recommendation |
|------|--------|----------------|
| `--real-llm` HTTP call | Environment blocked (DeepSeek balance) | After balance recovery or MINIMAX fallback confirmed |
| `--ablation-mode=full` ablation_report.json | Depends on Phase 1+2 done | Include in finalization if time permits |
| `--regression-test-suite` flag | **Already deleted in commit `f456336`** (not stubbed, not in code) | No action — CLI ships with 9 flags (8 existing + --metrics-output) |
| Wave 3 Phase 2 D4 LoRA pipeline | Separate change | After finalization archive |