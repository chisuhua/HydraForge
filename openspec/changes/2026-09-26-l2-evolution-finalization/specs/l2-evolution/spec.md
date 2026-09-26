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
- Phase 4 reload_rerun: real `FilesystemGenomeRegistry::load` + `to_agent_config` + ChatSession rebuild + rerun
- Phase 5 compare: real `IEvaluator::compare` (BehavioralEquivalence V2)
- Phase 6 emit: unchanged (trace JSONL)

### Acceptance
- 6 distinct trace events per context (no skipped phases)
- Each phase shows REAL computation result (no hardcoded `gate_passes:5` or `"Attributed"`)
- phase3 trace event has `genome_version > 1` after real mutation
- phase4 trace event has `genome_version == phase3 new_version` (chain link)
- phase5 trace event has real `attribution_verdict` ∈ {Attributed, Confounded, Insufficient}

---

## R4 — Trace schema

**Existing**:
- 8 top-level fields + 12 meta fields (per archived spec)

**Finalization**:
- `attribution_verdict` enum strict: `Attributed | Confounded | Insufficient | NotAttempted | null`
- `genome_version` int, monotonically increasing per chain
- `gate_passes` int, real from phase3 mutation result

### Acceptance
- No `"Baseline"` in any trace (per commit `1fd1450` fix — was illegal enum regression)
- All baseline traces have `attribution_verdict: "NotAttempted"` (pre-attribution)
- Mutation traces have `attribution_verdict: "Pending"` (resolved in compare)

---

## R5 — Capture-mode (training data path)

**Existing**: `--capture-mode <None|Training>` parsed but inert.

**Finalization**: capture-mode=Training activates IDistillationWriter
JSONL output to `/tmp/l2-distillation-<session_id>.jsonl`.

### Acceptance
- `--capture-mode Training` produces JSONL with ≥4 events per session
- `--capture-mode None` (default) produces no distillation file
- JSONL parseable + contains 8 top-level + 12 meta fields per event

### Strategic value
**Wave 3 Phase 2 D4 LoRA training pipeline data path enabler.** D4
implementation consumes this JSONL for training data loading
(per `openspec/changes/archive/2026-09-25-wave-3-phase-2-d4-lora-pipeline/`
improvement scope).

---

## R8 — Reverse indicator (drop_ratio + metrics)

**Existing (Phase A+B ship)**:
- `--release-metrics` real drop_ratio computation
- metrics.json write
- >5% exit non-zero

**Finalization**:
- `--metrics-output <path>` flag (default `/tmp/l2-metrics.json`)
- Never writes to CWD (prevents repo root pollution per commit `1fd1450` adjacent finding)

### Acceptance
- `metrics.json` not in `git status` after binary run
- `--metrics-output /tmp/foo.json` writes to specified path
- `drop_ratio` value reflects real phase5 compare output (not always -1.0)

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
| R1 CLI 8 flags | ✅ | n/a | +1 → 9 | `--help` |
| R2 phase1+2 real | ✅ | +phase3+4+5 | ✅ | 6 distinct trace events per context |
| R4 trace schema | partial (verdict enum regression) | ✅ | ✅ | test_reverse_indicators verdict compliance |
| R5 capture-mode | inert | Training wired | ✅ | /tmp/l2-distillation-*.jsonl |
| R8 metrics | semantic hollow (drop_ratio=-1.0) | real (depends on phase5) | ✅ + --metrics-output | /tmp/l2-metrics.json + non-CWD |
| R9 anti-cheat | ✅ | n/a | ✅ | 3 test binaries PASS |
| R13.4 redaction | dead code | wired | ✅ | confidential fixture test PASS |
| Cross-doc consistency | scope qualifier on 2/3 | n/a | all 3 full | grep "Phase B only" = 0 |

---

## Reverse Indicator

Per AGENTS.md Reverse Indicator Rule (5 fields mandatory):

```
+ new_up: L2 reference example fully shipped (Phase B + C + housekeeping),
  real 6-stage self-evolution loop evidence, real capture-mode data path,
  cross-doc consistency (no scope qualifiers)
- old_down: drop_ratio semantically mock-mode-empty (real LLM-driven
  evaluation deferred to Wave 3 D4)
failure_traces: per-phase failure → "NotAttempted" verdict + error meta
ablation: 3-segment baseline vs mutation vs rerun (mock = identical;
  real divergence requires Wave 3 D4)
context_ids: code-class-ex-001, code-class-ex-002, r13-4-confidential-test-001
```

---

## Out-of-Spec (tracked for follow-up)

| Item | Status | Recommendation |
|------|--------|----------------|
| `--real-llm` HTTP call | Environment blocked (DeepSeek balance) | After balance recovery or MINIMAX fallback confirmed |
| `--ablation-mode=full` ablation_report.json | Depends on Phase 1+2 done | Include in finalization if time permits |
| `--regression-test-suite` flag | spec R1 9th flag | Recommend spec 降级 to 8 flags |
| Wave 3 Phase 2 D4 LoRA pipeline | Separate change | After finalization archive |