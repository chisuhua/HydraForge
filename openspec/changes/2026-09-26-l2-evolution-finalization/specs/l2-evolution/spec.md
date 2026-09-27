# l2-evolution Specification Delta (finalization)

## Purpose

This spec delta modifies the existing `pdk-chat-demo-evolution` capability
(archived `2026-09-24-pdk-chat-demo-evolution-reference-example` + Phase A+B
ship in `2026-09-26-l2-evolution-real-execution-chain`) to add L2-specific
Phase C requirements + housekeeping:

- **Phase 3 mutation real wiring**: replace hardcoded
  `build_meta(ctx, 1, 5, "Attributed")` with real `apply_harness_mutation`
  + complete `MutationGateContext` (6 required fields) + seed commit
  in phase 1 (Gate 0 prerequisite)
- **Phase 4 reload + rerun real wiring**: replace hardcoded stub with real
  `IGenomeRegistry::load` + hand-mapped `AgentConfig` from
  `GenomeSpec.harness` + ChatSession rebuild + skip when no mutation
  applied (Risk 3 mitigation)
- **Phase 5 compare real wiring**: replace `++mutated_passes_` stub with
  real `IEvaluator::compare` via `BehavioralEquivalenceEvaluator`
  (ADR-0083 V2) + verdict mapping narrowed to `{Attributed, Insufficient}`
  (Confounded unreachable in L2 single-turn per ADR-0086 v1.1
  `walk_ancestors`)
- **`--metrics-output <path>` flag**: replace CWD `metrics.json` pollution
  with default `/tmp/l2-metrics.json`; redefined drop_ratio formula
  `mutated_failures / max(1, mutated_passes + mutated_failures)`
- **`--capture-mode Training` → `IDistillationWriter`** wiring: replace
  inert capture mode with real `make_file_writer(output_dir, agent_id)` +
  per-context `DistillationRecord` (NOT trace event stream)
- **R13.4 sensitivity redaction** enforced in both phase 2 (baseline) +
  phase 4 (rerun) — confidential/internal → `[REDACTED-<level>]`
- **3 SoTs §十一 ship row scope qualifier removal** ("Phase B only" → full)

Supersedes: `2026-09-26-l2-evolution-real-execution-chain` (Phase B-only
ship in commit `d42b47b`). After this change ships, the predecessor
will be archived (per `finalization/.openspec.yaml` `supersession_rationale`
+ T6.3 in `finalization/tasks.md`).

> **Archive guard (per Oracle SHIP-with-fixes verdict)**: This delta
> is filed under new capability `l2-evolution`; requirement names do
> NOT map 1:1 to canonical `pdk-chat-demo-evolution/spec.md` (e.g.,
> `cli-surface` vs canonical `standalone-binary-and-dual-mode`,
> `trace-schema-strict-enum` vs `trace-jsonl-schema-stability`, etc.).
> Archive MUST be `git mv`-only per Plan of Record §Predecessor
> sequencing + AGENTS.md Pattern #11 precedent (`e643dae`/`2cd4dce`)
> — **DO NOT run `openspec archive` CLI** on this change.

Pre-cooling-off hygiene fixes already shipped (Path A from prior session,
NOT part of this delta — context only):
- `60a8982`: AGENTS.md:805 header/body alignment + Phase C pointer +
  `real-execution-chain/proposal.md:11` Why header fix
- `d0b3efa`: AGENTS.md:805 body timeless timestamp
- `8bb308f`: `.gitignore` excludes `metrics.json` pollution
- `710cadf`: archive `2026-09-25-l2-evolution-deferred-followup-superseded/`
  flatten to canonical 4-file layout

---

## ADDED Requirements

### Requirement: hermetic-home-composition

`IGenomeRegistry` factory `create_filesystem(root)` MUST be called with
an explicit hermetic root (e.g., `hermetic_guard_->path() / "genome"`),
NOT `~/.hydraforge/genome/`. HMAC key path uses `getenv("HOME")` at
runtime; `setup_hermetic_home()` MUST precede first registry use to
avoid host filesystem pollution (AGENTS.md Risk 2 mitigation).

#### Scenario: hermetic-home-precedes-registry-use

- **WHEN** test fixture calls `set_hermetic_home` then `set_contexts` then `run_6_phase_demo`
- **THEN** `IGenomeRegistry` root MUST be under hermetic HOME, not `~/.hydraforge/genome`
- **AND** MUST NOT pollute host filesystem under any path containing host `$HOME`

#### Scenario: hermetic-home-fresh-deploy-deterministic

- **WHEN** fresh machine (no `.hydraforge/` directory exists) runs evolution with hermetic HOME
- **THEN** registry MUST be created successfully at first use (no IOError)
- **AND** HMAC key MUST be generated atomically with 0600 perms (per AGENTS.md Pattern #10 hygiene fix)

---

## MODIFIED Requirements

### Requirement: cli-surface

`pdk_chat_demo_evolution --help` MUST show 9 CLI flags (was 8, +1 `--metrics-output <path>` with default `/tmp/l2-metrics.json` added in this delta):
- **5 main mode**: `--mock`, `--real-llm <provider>`, `--capture-mode={None|Training}`, `--trace-events`, `--context-file <path.jsonl>` (required)
- **3 reverse indicator**: `--release-metrics`, `--ablation-mode`, `--accept-contexts`
- **1 new (this delta)**: `--metrics-output <path>` (default `/tmp/l2-metrics.json`)

`--regression-test-suite` flag is REMOVED (already deleted in commit `f456336`, not in code).

#### Scenario: nine-flags-help-output

- **WHEN** `./pdk_chat_demo_evolution --help` is invoked
- **THEN** output MUST show exactly 9 flags (5 main + 3 R8 + 1 --metrics-output)
- **AND** MUST NOT show `--regression-test-suite` (already deleted per `f456336`)

#### Scenario: metrics-output-default-tmp-path

- **WHEN** `--release-metrics` is enabled without `--metrics-output`
- **THEN** binary MUST write metrics to `/tmp/l2-metrics.json` (NOT CWD)
- **AND** `git status --short` MUST NOT show `metrics.json` pollution after binary run

#### Scenario: metrics-output-custom-path

- **WHEN** `--release-metrics --metrics-output /tmp/foo.json` is invoked
- **THEN** binary MUST write metrics to `/tmp/foo.json`
- **AND** MUST open file successfully (return exit 2 if open fails)

---

### Requirement: six-phase-end-to-end-chain

Binary MUST execute the 6-phase chain end-to-end with REAL computation at every phase (no hardcoded traces). The chain (completes phase 3 + 4 + 5 real wiring; 4 trace events per context, not 6):

1. **Phase 1 init**: real `DSLEngine` + `bus` + plugin + `ChatSession` 11-param ctor + **seed genome commit** (Gate 0 prerequisite — first mutation always fails without seed)
2. **Phase 2 baseline**: real `chat()` capturing `response` / `tokens` / `cost_usd`
3. **Phase 3 mutation**: real `apply_harness_mutation(genome_mutations, system_prompt, tools, registry, mutation_gate_ctx)` + complete `MutationGateContext` (6 required fields: `current` / `attribution` / `evaluator` / `budget` / `bus` / `policy` + `genome_registry`/`genome_name`/`parent_version` when registry non-null) + bootstrap attribution (`verdict=Attributed`, `method=DirectComparison`, `reason="L2 bootstrap: first-cycle, no prior attribution"`)
4. **Phase 4 reload_rerun**: real `IGenomeRegistry::load("default", parent_version=N+1)` → hand-mapped `AgentConfig` from `GenomeSpec.harness` → ChatSession 11-param ctor rebuild → rerun baseline `turn_input` → capture distinct `response`/`tokens`/`cost_usd`; **skip rebuild when `last_committed_genome_version_ <= baseline_version_`** (Risk 3 mitigation)
5. **Phase 5 compare**: real `IEvaluator::compare(*last_baseline_exec_, *last_rerun_exec_)` via `BehavioralEquivalenceEvaluator` (ADR-0083 V2) returning `int`; verdict mapping `compare == 0 → "Attributed"` else `"Insufficient"` (Confounded unreachable in L2 single-turn per ADR-0086 v1.1 `walk_ancestors` out of scope)
6. **Phase 6 emit**: unchanged (trace JSONL) — `capture-mode=Training` adds `IDistillationWriter` hook here

#### Scenario: real-mutation-genome-version-increment

- **WHEN** phase 3 mutation runs after seed commit in phase 1
- **THEN** phase 3 trace event MUST show `genome_version > 1` (Gate 0 prerequisite satisfied)
- **AND** MUST derive `gates_passed` honestly from `MutationError` enum (InvalidMutation→0, NotReady→1, GovernanceDenied→2, RegistryRejected→3, success→5)

#### Scenario: real-compare-verdict-mapping

- **WHEN** phase 5 compare runs after phase 2 + 4 captured traces
- **THEN** `attribution_verdict` MUST be `"Attributed"` if `compare == 0`, else `"Insufficient"`
- **AND** MUST NOT emit `Confounded` (out of L2 scope per ADR-0086 v1.1 `walk_ancestors`)
- **AND** MUST increment `mutated_passes` if Attributed, `mutated_failures` if Insufficient

#### Scenario: real-reload-rerun-chain-link

- **WHEN** phase 4 reload runs after successful phase 3 mutation
- **THEN** phase 4 trace `genome_version` MUST equal phase 3 trace `committed_genome_version` (chain link corrected per Oracle M6 finding)
- **AND** MUST capture rerun `response` / `tokens` / `cost_usd` in `last_rerun_exec_`

#### Scenario: four-trace-events-per-context

- **WHEN** binary completes a full 6-phase run for any context
- **THEN** MUST emit exactly **4** trace events per context (phase 2 baseline + phase 3 mutation + phase 4 reload + phase 5 compare)
- **AND** MUST NOT emit events for phase 1 init or phase 6 emit

#### Scenario: reload-skip-when-no-mutation

- **WHEN** phase 3 mutation fails (Gate 0 `InvalidMutation` or Gate 1 missing fields)
- **THEN** phase 4 MUST emit `attribution_verdict: "NotAttempted"` with `gates_passed: 0`
- **AND** MUST skip ChatSession rebuild (zero work; Risk 3 mitigation)

---

### Requirement: trace-schema-strict-enum

Trace events MUST follow strict closed enum for `attribution_verdict` (this delta removes `"Pending"` verdict which was never in the closed enum, corrected from original draft):
`Attributed | Confounded | Insufficient | NotAttempted | null`

Per-trace verdict mapping:
- **Baseline trace**: `NotAttempted` (pre-attribution)
- **Mutation trace**: `NotAttempted` (resolved in phase 5 compare)
- **Reload trace**: `NotAttempted` (same reason)
- **Compare trace**: `Attributed | Insufficient` (post-compare)

`gates_passed` MUST be derived honestly from `MutationError` enum (InvalidMutation→0, NotReady→1, GovernanceDenied→2, RegistryRejected→3, success→5). MUST NOT be hardcoded `5`.

#### Scenario: no-illegal-verdict-values

- **WHEN** any trace event is emitted (phase 2/3/4/5)
- **THEN** `attribution_verdict` MUST be in `{Attributed, Insufficient, NotAttempted, null}`
- **AND** MUST NOT be `"Baseline"` (regression fixed in commit `1fd1450`)
- **AND** MUST NOT be `"Pending"` (enum violation per Oracle M1 finding)

#### Scenario: honest-gates-passed-derivation

- **WHEN** phase 3 mutation returns `Result.failure(error)`
- **THEN** `gates_passed` MUST equal `error_to_gates(error)` (InvalidMutation→0, NotReady→1, GovernanceDenied→2, RegistryRejected→3)
- **AND** MUST NOT be hardcoded `5` (per M3 fix)

---

### Requirement: capture-mode-training-wiring

`--capture-mode Training` MUST activate `agenticdsl::IDistillationWriter` via `make_file_writer(output_dir, agent_id)` static factory (this delta wires previously-inert capture-mode to active data path):

`DistillationRecord` schema fields (per `distillation_record.h:49-71`): `agent_id`, `convergence` (`{agent_id, teacher_version, task_id}`), `reward`, `input`, `output`, `steps`, `capture_mode`, `timestamp_iso8601`. `agent_id` MUST be non-empty (three-fold enforcement per `file_writer.cpp:43-46`).

#### Scenario: capture-mode-training-writes-jsonl-per-context

- **WHEN** `--capture-mode Training --context-file <path.jsonl>` is invoked with 3 contexts
- **THEN** binary MUST write 3 JSONL files (one per context, NOT 3 events in 1 file)
- **AND** files MUST be named `/tmp/l2-distillation-<session_id>_<seq:06d>.distill.v1.jsonl`
- **AND** each file MUST contain valid `DistillationRecord` JSON with all required fields

#### Scenario: capture-mode-none-inert

- **WHEN** `--capture-mode None` is invoked (default)
- **THEN** binary MUST NOT write any distillation files
- **AND** MUST NOT instantiate `IDistillationWriter`

---

### Requirement: reverse-indicator-metrics-path

`--release-metrics` MUST compute `drop_ratio = mutated_failures / max(1, mutated_passes + mutated_failures)` (regression ratio; semantically meaningful) and MUST write metrics to `--metrics-output <path>` (default `/tmp/l2-metrics.json`), NEVER to CWD (this delta fixes the drop_ratio formula and the CWD pollution path):

`drop_ratio > 0.05` MUST trigger exit 1 (R8.1 red line enforced).

#### Scenario: mock-mode-zero-drop-ratio

- **WHEN** `--release-metrics` is invoked in mock mode (all-pass baseline + all-pass mutation)
- **THEN** `drop_ratio == 0.0` (mutated_failures=0 per redefined formula)
- **AND** exit code MUST be 0 (within 5% threshold)

#### Scenario: metrics-never-in-cwd

- **WHEN** `--release-metrics` is invoked without `--metrics-output`
- **THEN** `/tmp/l2-metrics.json` MUST exist after run
- **AND** `metrics.json` MUST NOT exist in CWD (per AGENTS.md pollution mitigation + `.gitignore` ship `8bb308f`)

#### Scenario: drop-ratio-above-threshold-triggers-exit-1

- **WHEN** `--release-metrics` is invoked AND `mutated_failures / max(1, mutated_passes + mutated_failures) > 0.05`
- **THEN** binary MUST exit 1 (R8.1 mechanism activated)
- **AND** MUST print stderr "ERROR: drop_ratio X > 5% threshold"

---

### Requirement: context-sensitivity-redaction

`turn_input`, `response`, `tags`, `domain` fields MUST be redacted to `[REDACTED-<level>]` in phase 2 baseline + phase 4 rerun trace events + capture-mode=Training distillation records, when `ContextRequest.metadata.sensitivity ∈ {internal, confidential}` (this delta extends R13.4 redaction wiring from baseline-only to baseline + rerun, per `finalization/design.md` D4 target state):

#### Scenario: confidential-redacts-baseline-turn-input

- **WHEN** a context with `sensitivity: confidential` is processed in phase 2 (baseline)
- **THEN** phase 2 trace event `turn_input` MUST be `"[REDACTED-confidential]"` (per commit `1fd1450` R13.4 fix)
- **AND** `response` MUST be `"[REDACTED-confidential]"`

#### Scenario: phase4-rerun-also-redacts

- **WHEN** a context with `sensitivity: internal` or `confidential` is processed in phase 4 (rerun)
- **THEN** phase 4 trace event `turn_input` and `response` MUST be redacted to `"[REDACTED-<level>]"` (per `finalization/design.md` D4 target state)
- **AND** MUST match the same redaction level as phase 2 baseline

---

### Requirement: cross-doc-consistency-three-SoT-ship-row

Three Source-of-Truth documents MUST carry a "L2 ✅ ship" row in §十一 referencing this change (with NO scope qualifier — full L2 real execution chain shipped, replacing prior "Phase B only" qualifier). After this delta ships, `grep "Phase B only"` across the 3 SoTs MUST return 0 matches:

| Document | § | Post-ship row content |
|----------|---|------------------------|
| `docs/architecture/harness-architecture-2026-09.md` | §十一 | L2 reference example ✅ ship 2026-09-26 (full chain) |
| `docs/architecture/rsi-architecture-2026-09.md` | §十一 | L2 reference example ✅ ship 2026-09-26 (full chain) |
| `docs/architecture/self-evolution-architecture-2026-08.md` | §十一 | L2 reference example ✅ ship 2026-09-26 (full chain) |

Per `finalization/design.md` D7 SoT scope updates.

#### Scenario: three-sots-have-full-ship-row-no-phase-b-only

- **WHEN** this delta's Phase 6 (T6.3) commits the SoT updates
- **THEN** `grep -n "Phase B only" docs/architecture/{harness,rsi,self-evolution}-architecture-*.md` MUST return 0 matches
- **AND** each of the 3 SoTs §十一 MUST reference `2026-09-26-l2-evolution-finalization` (NOT the prior `real-execution-chain` slug)
- **AND** the `verification_matrix` (per `openspec/specs/pdk-chat-demo-evolution/spec.md:278`) "Cross-doc consistency" row MUST read "all 3 full" (not "scope qualifier on 2/3")

---

## REMOVED Requirements

### Requirement: regression-test-suite-flag

**REMOVED** — flag was already deleted in commit `f456336` (Phase B quick-win
fixes, 2026-09-26 per AGENTS.md Recent Changes). Not a real removal from this
delta — documenting the prior deletion for clarity.

The original R1 acceptance requirement to expose `--regression-test-suite`
in `pdk_chat_demo_evolution --help` is no longer applicable. CLI ships with
8 existing flags + `--metrics-output` (this delta) = 9 total.

---

## Out-of-Spec (tracked for follow-up, NOT in this delta)

| Item | Status | Recommendation |
|------|--------|----------------|
| `--real-llm` HTTP call | Environment blocked (DeepSeek "Insufficient Balance") | After balance recovery or MINIMAX fallback confirmed |
| `--ablation-mode=full` `ablation_report.json` | Depends on Phase 3+4 done for verdict distribution | Include in this change if time permits; else next sprint |
| Wave 3 Phase 2 D4 LoRA pipeline | Separate change (`archive/2026-09-25-wave-3-phase-2-d4-lora-pipeline/`) | Start after this change archive |
| T0.1 housekeeping (build/tests cleanup + AGENTS.md NOTES rule + check-no-nested-configure.sh) | Day 1 morning, 1 SP, pre-implementation | Pre-Phase 1 in `finalization/tasks.md` T0.1 |