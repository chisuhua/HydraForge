# l2-evolution-finalization

## Why

Per Oracle audit session `ses_f259746caffe9yK0YoIQ6FOgWp` post-Phase A+B
SHIP-with-fixes verdict, the L2 reference example binary
(`pdk_chat_demo_evolution`) is in a half-shipped state:

- **Shipped (commits `d42b47b` + `2916d2f` + `1fd1450`)**:
  - Phase A OpenSpec change (4 file artifacts)
  - Phase B phase1_init + phase2_baseline real wiring (DSLEngine + ChatSession 11-param ctor + real `chat()` capturing response/tokens/cost)
  - `--release-metrics` real drop_ratio computation + metrics.json write + >5% exit non-zero
  - R13.4 redact_trace_fields wired (confidential/internal → `[REDACTED-<level>]`)
  - 3 SoTs §十一 L2 real-execution-chain ship row (scope-limited to "Phase B only")
  - Test fixes: test_budget_alert mock loop/run + test_reverse_indicators baseline verdict enum compliance

- **Still stubbed (this change's scope)**:
  - phase3_mutation — `build_meta(ctx, 1, 5, "Attributed")` hardcoded
  - phase4_reload_rerun — same hardcoded stub
  - phase5_compare — `mutated_passes_++` unconditional increment (semantically hollow)
  - `--ablation-mode=full` — flag parsed but ablation_report.json never written
  - capture-mode=Training — IDistillationWriter not wired
  - `--real-llm` mode — only stores string, no LLMProviderFactory path
  - `--regression-test-suite` — **already deleted in commit `f456336`** (not stubbed); CLI ships with 9 flags = 8 existing + --metrics-output (this change)

- **Housekeeping debt accumulated during Sprint 36**:
  - `build/tests/` + `build/examples-tests/` nested CMake configure residue
    (each with own CMakeCache.txt; clobbers outer CTestTestfile → 240 tests
    "Not Run" when worker runs ctest after fresh checkout)
  - `metrics.json` untracked in repo root + written to CWD
    (`--release-metrics` ship-with-known-issue: pollution risk)
  - AGENTS.md NOTES lacks "禁止嵌套 cmake -B" rule

The longer the half-shipped state persists, the more it conflicts with
project narrative ("L2 reference example ship") and the more the SoT
scope-limited "Phase B only" rows drift toward "✅ ship" implicitly
without the corresponding Phase C implementation. Closing the loop now
prevents spec-vs-impl drift from accumulating further.

## What Changes

### Phase 0 — Housekeeping (Day 1 morning, 1 SP)
- Delete `build/tests/` + `build/examples-tests/` nested configure dirs
- Add AGENTS.md NOTES rule: "禁止 `cmake -B build/<subdir>` 嵌套 configure"
- Add `.gitignore` entry: `/metrics.json`
- Verification: full ctest clean registration (no "Not Run")

### Phase 1 — phase5_compare real wiring (Day 1 afternoon, 3 SP)
- Replace `++mutated_passes_` stub with `agenticdsl::IEvaluator::compare(baseline_trace, rerun_trace)` call
- Capture real `attribution_verdict` ∈ {Attributed, Confounded, Insufficient}
- BehavioralEquivalenceEvaluator (ADR-0083 V2) is default evaluator
- Cost: 3 SP

### Phase 2 — phase3_mutation real wiring (Day 2-3, 5 SP)
- Replace `build_meta(ctx, 1, 5, "Attributed")` stub with `apply_harness_mutation(genome_mutations, system_prompt, tools, registry, mutation_gate_ctx)` call
- MutationGateContext uses hermetic HOME FilesystemGenomeRegistry
- Capture real `genome_version` (N → N+1)
- Cost: 5 SP

### Phase 3 — phase4_reload_rerun real wiring (Day 4, 5 SP)
- Replace stub with `FilesystemGenomeRegistry::load("default", parent_version=N+1)` → `to_agent_config()` → ChatSession rebuild
- Rerun baseline `turn_input` → capture distinct response/tokens
- Cost: 5 SP

### Phase 4 — --release-metrics semantic + output path fix (Day 5 morning, 2 SP)
- `drop_ratio` computed from real phase5 compare output (not stub increment)
- New `--metrics-output <path>` flag, default `/tmp/l2-metrics.json`
- Never write to CWD
- Cost: 2 SP

### Phase 5 — capture-mode=Training → IDistillationWriter (Day 5 afternoon, 3 SP)
- IDistillationWriter (ADR-0086 v1.1) hooked to ChatSession emit events
- Skip in None mode (current default); activate with `--capture-mode Training`
- Writes distillation JSONL per session_id
- This is the **Wave 3 Phase 2 D4 LoRA pipeline data path enabler**
- Cost: 3 SP

### Phase 6 — Commit + sync + archive (Day 6, 1 SP)
- Single atomic commit per sub-phase (TDD discipline)
- AGENTS.md Recent Changes entries + 5-field Reverse Indicator blocks
- 3 SoTs §十一 "Phase B only" → "✅ ship" (full L2 real execution chain)
- Archive `openspec/changes/2026-09-26-l2-evolution-real-execution-chain/`

## Impact

### Affected capabilities
- **Self-evolution architecture**: PARTIAL → PASS (real 9-stage loop evidence end-to-end)
- **Harness architecture**: PARTIAL → PASS (5-tier gate real execution + genuine reverse indicator gate)
- **L2 spec R1-R13**: PARTIAL → PASS (R2 six-phase fully real, R5 capture-mode wired, R8 metrics semantic, R13.4 already enforced)
- **Wave 3 Phase 2 D4**: unblocked (capture-mode=Training is the data path enabler)

### Affected code
- `examples/pdk_chat_demo_evolution/evolution_session.{h,cpp}` (~400 lines added/modified)
- `examples/pdk_chat_demo_evolution/main.cpp` (~30 lines added/modified)
- `examples/pdk_chat_demo_evolution/tests/test_reverse_indicators.cpp` (~50 lines added)
- `examples/pdk_chat_demo_evolution/tests/CMakeLists.txt` (~5 lines added)
- `docs/architecture/self-evolution-architecture-2026-08.md` §十一 (ship row scope update)
- `docs/architecture/harness-architecture-2026-09.md` §十一 (ship row scope update)
- `docs/architecture/rsi-architecture-2026-09.md` §十一 (ship row scope update)
- `AGENTS.md` NOTES section (1 rule added)
- `.gitignore` (1 entry added)
- `build/` (gitignored dirs deleted, non-tracked)

### Affected tests
- `test_reverse_indicators` (3 → 5-6 cases)
- `test_evolution_session_mutation` (4 → 5-6 cases)
- `test_evolution_tracer_schema` (6 → 8 cases, capture-mode wiring)
- All 9 L2 tests still PASS at end of each sub-phase

## Effort Estimate

| Phase | Story Points | Wall Clock (Single-Dev) |
|-------|--------------|-------------------------|
| Phase 0 Housekeeping | 1 SP | 0.5 day |
| Phase 1 phase5_compare | 3 SP | 1 day |
| Phase 2 phase3_mutation | **6 SP** (+0.5 day vs initial — Oracle M6 finding: seed commit + bootstrap attribution + budget controller + attribution/evaluator/budget/bus 6 required fields + Result handling) | 2.5 days |
| Phase 3 phase4_reload_rerun | 5 SP | 2 days |
| Phase 4 metrics path | 2 SP | 0.5 day |
| Phase 5 capture-mode | 3 SP | 1 day |
| Phase 6 commit + sync | 1 SP | 0.5 day |
| **Total** | **~21 SP** | **~8 working days (~1.5 weeks)** |

## Sequencing & Dependencies

- **Phase 0 → all others** (no dep on code; just gitignore + build dir)
- **Phase 1 (phase5)** → Phase 4 (metrics semantic needs phase5 real output)
- **Phase 2 (phase3)** → Phase 3 (phase4 needs mutation to have created genome)
- **Phase 5 (capture-mode)** independent; runs anytime after Phase 0
- Recommended order: 0 → 2 → 3 → 1 → 4 → 5 → 6 (mutation first to unblock reload)

## Non-Goals (deferred beyond this change)

- `--real-llm` mode → CloudLLMAdapter (DeepSeek "Insufficient Balance" environment blocker; CI skip via `HYDRAFORGE_SKIP_REAL_LLM=1` 兜底)
- `--ablation-mode=full` → ablation_report.json (Phase C #6, 3 SP, requires Phase 1+2 done for verdict distribution)
- `--regression-test-suite` flag — already deleted in commit `f456336` (no action)
- Wave 3 Phase 2 D4 LoRA training pipeline (separate change after this archive)

## Oracle Verdict Reference

See Oracle session `ses_f259746caffe9yK0YoIQ6FOgWp` strategic recommendation
output for full priority + effort analysis. This change implements:
- 5 of 8 Phase C items (#1-#5 per Oracle priority order)
- Housekeeping items (build tree + metrics.json)
- SoT scope row update (eliminates "Phase B only" qualifier)

Deferred items (real-llm / ablation-mode / regression-test-suite) explicitly
listed in Non-Goals above for tracking.

## Plan of Record (cooling-off governance + pre-cooling-off hygiene)

Per Oracle + Metis dual-agent review sessions
`ses_f216b0b26ffeEHc6xKCxUyIou0` + `ses_f216b0c2affeUoUWQP1cNBcTIg`
(post-`5a7e553` OpenSpec doc ship):

### Pre-cooling-off hygiene fixes (shipped, NOT part of this change)

- **`60a8982`** `docs(openspec): align L2 review entries with plan of record` —
  AGENTS.md:805 header/body alignment (header "ship" → timeless
  "planning docs only, implementation pending") + line 809 stale Phase C
  pointer redirect (`...real-execution-chain` → `...finalization`) +
  real-execution-chain proposal.md `## Why (动机)` → `## Why` (H2 fix
  preventing T6.3 archive parse failure).
- **`d0b3efa`** `docs(AGENTS.md): timeless L1 body timestamp` — line 805
  body hard timestamp `expire 2026-09-27 22:11:34 +08:00` →
  `24h cooling-off from change creation per finalization/proposal.md §Cooling-Off`.
- **`8bb308f`** `chore(.gitignore): exclude metrics.json pollution` —
  `/metrics.json` + `metrics.json` entries added (AGENTS.md
  `--release-metrics` ship-with-known-issue pollution mitigation).
- **`710cadf`** `chore(openspec): flatten malformed 4-file archive` —
  `archive/2026-09-25-l2-evolution-deferred-followup-superseded/` nested
  → canonical 4-file layout (Day-5 4-file integrity lesson).

### Cooling-off governance

- **Anchor**: `a903d41` (this change's creation commit).
- **Duration**: 24h from change creation per §Cooling-Off below.
- **Implementation constraint**: Implementation MUST NOT begin before
  anchor + 24h expiry (per AGENTS.md §SINGLE-DEVELOPER MODE + ADR
  cooling-off chain + 5a7e553 documented plan of record).
- **Verified by Oracle**: do NOT pre-emptively archive predecessor
  during cooling-off window (Option E+ recommendation; Option C merge
  explicitly rejected by Oracle as contradicting recorded `supersedes`
  metadata in `finalization/.openspec.yaml`).

### Predecessor sequencing

- `2026-09-26-l2-evolution-real-execution-chain` supersession is correct
  (Phase A+B ship in commit `d42b47b` + `2916d2f` + `1fd1450` is honest;
  Phase C explicitly deferred to this change per Oracle Path 1 verdict).
- Both OpenSpec changes co-exist in active list during cooling-off
  window by design (supersession chain, not drift).
- This change's T6.3 archives predecessor via **`git mv`-only** —
  **NOT** `openspec archive` CLI (H1: spec delta format now resolved,
  but project archive precedent per `e643dae`/`2cd4dce` uses manual
  `git mv`, NOT CLI).
- T6.3 implementation MUST correct `openspec archive` + `mv`
  double-command script to single `git mv` (latent bug noted in
  `finalization/tasks.md` T6.3).

### Implementation discipline (per AGENTS.md Pattern #11)

- Branch `feat/l2-evolution-finalization` + worktree (per Pattern #11).
- Async Sisyphus-Junior worker (`run_in_background=true`) with 6-segment
  delegation prompt (TASK / EXPECTED_OUTCOME / REQUIRED_TOOLS / MUST DO /
  MUST NOT DO / CONTEXT).
- Worker TDD 5-step per sub-phase + atomic commit on worktree.
- Dual-Oracle post-impl SHIP-with-fixes review per phase (single Critical
  blocks ship; Major → atomic SHIP-with-fixes commit).
- Main session MUST verify via `git show` + `git ls-files` —
  never trust worker final-report (Pattern #11 lesson #d: Sisyphus-Junior
  async worker final-report can be misleading; Oracle audit caught
  similar mis-report in G1 case study 2026-09-22).

### Provenance

- **T1 brief**: `.rddf/state/plan/l2-evolution-t1-brief.md` (persisted
  cross-session artifact; pre-implementation checklist + commit
  templates + pitfalls).

## Cooling-Off

This change supersedes an already-shipped change (`2026-09-26-l2-evolution-real-execution-chain`).
Per AGENTS.md §SINGLE-DEVELOPER MODE + ADR cooling-off chain, supersession
requires 24h cooling-off. Cooling-off starts at change creation time
(`created: 2026-09-26`).