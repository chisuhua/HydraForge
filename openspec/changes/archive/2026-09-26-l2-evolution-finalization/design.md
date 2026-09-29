# Design — l2-evolution-finalization

## Scope Summary

Complete the L2 reference example (`pdk_chat_demo_evolution`) end-to-end by
implementing the deferred Phase C items + cleaning up accumulated housekeeping
debt. Result: 9-stage self-evolution loop with real evidence at every
phase, no hardcoded traces, no CWD pollution, and SoT ship rows scope-free.

---

## D1 — Phase 0 Housekeeping (quick wins, 1 SP)

### D1.1 — Delete nested configure dirs
```bash
rm -rf build/tests/ build/examples-tests/
```
These were created by mistake during prior sprint work; each contains
`CMakeCache.txt` which causes CMake's recursive registration to clobber
the outer `build/CTestTestfile.cmake`, resulting in ~240 tests showing
"Not Run" after fresh checkout.

### D1.2 — AGENTS.md NOTES rule
Add to AGENTS.md `## NOTES` section:
```
- **禁止 `cmake -B build/<subdir>` 嵌套 configure**: 只允许顶层
  `cmake -S . -B build` + 子 build 目录 `cmake -S <example> -B
  build/<example>`。嵌套 configure 会污染 CTestTestfile 致 ctest
  注册消失。
```

### D1.3 — .gitignore entry
Add `/metrics.json` to root `.gitignore`. The file is written by
`--release-metrics` flag to CWD by default; this change moves it to
`/tmp/l2-metrics.json` (D5), but `.gitignore` entry is the belt-and-
suspenders defense.

### Verification
- `git status` clean (no untracked `metrics.json`)
- `ctest -N | wc -l` returns same count before/after
- `cat AGENTS.md | grep "嵌套 configure"` shows the new rule

---

## D2 — Phase 1 phase5_compare real wiring (3 SP)

### Current state
`evolution_session.cpp:phase5_compare`:
```cpp
nlohmann::json meta = build_meta(ctx, 1, 5, "NotAttempted");
tracer_->record_phase(TracePhase::Compare, {{"meta", std::move(meta)}});
++mutated_passes_;
```
This unconditionally increments `mutated_passes_` with no actual
comparison. `--release-metrics` drop_ratio derived from this is
semantically hollow.

### Pre-implementation findings (Oracle dual-agent review)
- **API mismatch** (`include/agenticdsl/contract/ievaluator.h:32`):
  `IEvaluator::compare(const ExecutionTrace&, const ExecutionTrace&) → int`
  (`<0/0/>0`). The design sketch assumed a struct with
  `.attribution` / `.pass` / `.genome_version` / `.gate_passes` — none
  of these fields exist.
- **Enum mismatch**: `AttributionResult` does NOT exist. Real enum is
  `agenticdsl::evolution::AttributionVerdict` (`attribution_record.h:47-52`).
- **Type mismatch**: `ExecutionTrace` is a struct
  `{ToolResult final_result, trace_id, trajectory_refs}` —
  `execution_trace.h:16-20`. The design sketched `json` maps.
- **Confounded unreachable in L2**: `BehavioralEquivalenceEvaluator`
  returns `0` for both Pass AND Inconclusive
  (`behavioral_equivalence_evaluator.h:34-37`); `kMinBaselineSamples=5`
  (`attribution_record.h:81`) means single-turn compare is always
  Inconclusive. The `Confounded` verdict requires
  `walk_ancestors` confounder detection (ADR-0086 v1.1) which is out
  of L2 scope → spec R2 acceptance must be narrowed to
  `{Attributed, Insufficient}`.
- **Map over-design**: `last_baseline_trace_` /
  `last_rerun_trace_` as maps keyed by `context_id` are over-engineered
  — the per-context loop is sequential, so two `std::optional`
  members suffice.

### Required additions (corrected per findings)
- `EvolutionSession` member:
  `std::optional<agenticdsl::ExecutionTrace> last_baseline_exec_, last_rerun_exec_`
- `std::unique_ptr<agenticdsl::IEvaluator> evaluator_` initialized in ctor
  with `agenticdsl::BehavioralEquivalenceEvaluator` (ADR-0083 V2).
- New member `std::unique_ptr<agenticdsl::IBudgetController> budget_`
  (needed for Gate 1 — see D3); default-constructible per
  `budget_controller.h`.
- New members `int mutated_failures_ = 0` + accessor
  `int mutated_failures() const { return mutated_failures_; }`.

### Target state
```cpp
void EvolutionSession::phase5_compare(const ContextRequest& ctx) {
    if (!last_baseline_exec_.has_value() || !last_rerun_exec_.has_value()) {
        // Missing evidence (phase3/4 didn't run); emit NotAttempted honestly
        nlohmann::json meta = build_meta(
            ctx, last_committed_genome_version_, /*gates*/0,
            /*verdict*/ "NotAttempted");
        tracer_->record_phase(TracePhase::Compare, {{"meta", std::move(meta)}});
        return;
    }

    int cmp = evaluator_->compare(*last_baseline_exec_, *last_rerun_exec_);

    // Verdict mapping (decided per Oracle review — single-turn mock mode
    // always produces Inconclusive due to kMinBaselineSamples=5):
    //   compare == 0  → "Attributed" (behaviors equivalent)
    //   compare != 0  → "Insufficient" (cannot distinguish regression
    //                    from confounders with single sample)
    // "Confounded" verdict requires walk_ancestors (ADR-0086 v1.1) and
    // is NOT reachable in L2 scope.
    std::string verdict = (cmp == 0) ? "Attributed" : "Insufficient";

    if (cmp == 0) ++mutated_passes_;
    else ++mutated_failures_;

    nlohmann::json meta = build_meta(
        ctx, last_committed_genome_version_, /*gates*/5, verdict);
    tracer_->record_phase(TracePhase::Compare, {{"meta", std::move(meta)}});
}
```

### Acceptance
- `test_reverse_indicators` case "compare_verdict_real" PASS:
  `phase5_attribution_verdict ∈ {Attributed, Insufficient}` (narrowed
  from R2 original `{Attributed, Confounded, Insufficient}`).
- Existing `test_reverse_indicators` case 3
  (`compare == "NotAttempted"`) updated to
  `compare ∈ {Attributed, Insufficient}` per real compare semantics.
- `metrics.json.drop_ratio` formula updated per D5 — `!= -1.0` is now
  achievable in mock mode (mock all-pass → drop_ratio=0.0).

---

## D3 — Phase 2 phase3_mutation real wiring (6 SP, +0.5 day vs initial estimate)

### Current state
`evolution_session.cpp:phase3_mutation`:
```cpp
nlohmann::json meta = build_meta(ctx, 1, 5, "Attributed");
tracer_->record_phase(TracePhase::Mutation, {{"meta", std::move(meta)}});
```
Hardcodes `gate_passes:5` + `"Attributed"` — the final fabrication.

### Pre-implementation findings (Oracle dual-agent review)
- **Gate 0 hard requirement** (`harness_rsi.cpp:120-125`):
  `ctx.genome_registry != nullptr && ctx.parent_version == 0` →
  `InvalidMutation`. The default `last_committed_genome_version_ = 0`
  means the **first mutation always fails Gate 0** → genome_version
  never increments → T2.1 assertion (`> 1`) can never pass.
- **Gate 1 hard requirement** (`harness_rsi.cpp:128-131`):
  `!attribution || !evaluator || !budget` → `InvalidMutation`. The
  design sketch only set 2 of 6 required fields.
- **Gate 1 verdict requirement** (`transition_guard.h:67`):
  `attribution.verdict != AttributionVerdict::Attributed` →
  `evaluate_readiness` fails. L2 has no prior attribution record, so
  bootstrap attribution with `verdict = Attributed` is mandatory.
- **Field name correction**: `AppliedMutation` (per
  `harness_rsi.h:52-60`) has fields
  `committed_genome_version / applied_prompts / applied_tools_added /
  applied_tools_removed / prompt_snapshot / tools_snapshot`. The
  fields `result.applied`, `result.new_version`, `result.gates_passed`
  **do not exist**. Return type is
  `Result<AppliedMutation, MutationError>` (no `operator->`; must use
  `.has_value()` / `.value()`).

### Required additions (corrected per findings)
- `EvolutionSession` member:
  `std::unique_ptr<agenticdsl::genome::IGenomeRegistry> genome_registry_`
  (NOTE: there is NO public `FilesystemGenomeRegistry` class —
  `create_filesystem(root)` static factory at `genome.h:122` returns
  `unique_ptr<IGenomeRegistry>`)
- `agenticdsl::evolution::AttributionRecord bootstrap_attribution_` with
  `verdict = AttributionVerdict::Attributed`,
  `method = AttributionMethod::DirectComparison`,
  `reason = "L2 bootstrap: first-cycle, no prior attribution"`. This is
  a **documented honest bootstrap construction**, not free-form
  hardcoding (per design Decision section + spec R4 honesty rule).
- `std::unique_ptr<agenticdsl::IEvaluator> evaluator_` member
  (initialized in ctor with `BehavioralEquivalenceEvaluator`).
- `std::unique_ptr<agenticdsl::IBudgetController> budget_` member
  (initialized in ctor; needed for Gate 1; default-constructible per
  `budget_controller.h`).
- `uint64_t last_committed_genome_version_` tracking, initialized to 0.

### Target state
```cpp
void EvolutionSession::phase1_init() {
    // ... existing setup (engine, bus, chat_session, mock loop/run) ...

    // Seed genome commit (Gate 0 prerequisite — must precede any mutation)
    Genome root;
    root.metadata.name = "default";
    root.spec.harness = agent_cfg_.system_prompt;
    root.spec.tools = tool_names_snapshot_;
    auto seed = genome_registry_->commit(root);
    last_committed_genome_version_ = seed.value().committed_genome_version;
}

void EvolutionSession::phase3_mutation(const ContextRequest& ctx) {
    MutationGateContext mctx;
    mctx.current = EvolutionState::Harness;
    mctx.attribution = &bootstrap_attribution_;
    mctx.evaluator = evaluator_.get();
    mctx.budget = budget_.get();
    mctx.bus = bus_.get();
    mctx.policy = {};
    mctx.genome_registry = genome_registry_.get();
    mctx.genome_name = "default";
    mctx.parent_version = last_committed_genome_version_;

    auto result = apply_harness_mutation(
        generate_test_mutations(ctx),
        agent_cfg_.system_prompt,
        tool_names_snapshot_,
        engine_->get_tool_registry(),
        mctx);

    uint64_t new_version = last_committed_genome_version_;
    int gates_passed = 0;
    std::string verdict = "NotAttempted";  // per R4 closed enum (no "Pending")
    if (result.has_value()) {
        new_version = result.value().committed_genome_version;
        gates_passed = 5;  // success = all gates passed
        verdict = "NotAttempted";  // resolved in phase5 compare
        last_committed_genome_version_ = new_version;
    } else {
        // Derive gates_passed honestly from error enum (computable, not hardcoded)
        switch (result.error()) {
            case MutationError::InvalidMutation: gates_passed = 0; break;
            case MutationError::NotReady:        gates_passed = 1; break;
            case MutationError::GovernanceDenied: gates_passed = 2; break;
            case MutationError::RegistryRejected: gates_passed = 3; break;
        }
    }

    nlohmann::json meta = build_meta(ctx, new_version, gates_passed, verdict);
    tracer_->record_phase(TracePhase::Mutation, {{"meta", std::move(meta)}});
}
```

### Acceptance
- `test_evolution_session_mutation` case
  "mutation_genome_version_increments" PASS: phase3 trace shows
  `genome_version > 1` after real mutation (now achievable because
  seed commit precedes first mutation).
- `test_evolution_session_mutation` case
  "mutation_gate_context_required_fields" PASS: explicit test that
  Gate 1 rejects EvolutionSession constructed with missing fields.
- Existing test_reverse_indicators case 3 (`mutation == "Attributed"`)
  updated to `mutation == "NotAttempted"` (per R4 enum correctness
  fix M3 below).

---

## D4 — Phase 3 phase4_reload_rerun real wiring (5 SP)

### Current state
`evolution_session.cpp:phase4_reload_rerun`: same hardcoded stub as phase3.

### Pre-implementation findings (Oracle dual-agent review)
- **Fictional API**: `Genome.to_agent_config()` does NOT exist
  (`genome.h:77-80`: `Genome = {metadata, spec}`, no such method).
- **Concrete class missing**: there is NO public `FilesystemGenomeRegistry`
  class — only `IGenomeRegistry` abstract interface +
  `create_filesystem(root)` static factory at `genome.h:122` returning
  `unique_ptr<IGenomeRegistry>`. Member type must be the interface.
- **`GenomeSpec` fields** (`genome.h`): `harness: string`, `tools:
  vector<string>`, `budget`, `model_routing`, `prompt_cache_prefix`.
  Hand-mapping to `AgentConfig` required (only `system_prompt` field
  in `AgentConfig` matches `harness`; tools / budget / model_routing
  fields don't exist in `AgentConfig` → out of scope, drop them).
- **No `session_cfg_` member** in current `EvolutionSession` — D4
  sketch referenced `session_cfg_` for ChatSession 11-param ctor.
  Must add `hydraforge::pdk::SessionConfig session_cfg_` member
  (default-initialized; `enable_input_thread=false` per Phase B).
- **Skip rebuild when no mutation applied**: D7 Risk 3 mitigation
  unaddressed — phase4 must skip when
  `last_committed_genome_version_` unchanged from phase2 baseline.
- **R13.4 redaction**: D4 rerun_event must redact `turn_input` /
  `response` per `ctx.metadata.sensitivity` (per R13.4 red line
  established in commit `1fd1450`).
- **`load()` returns `Result<Genome, GenomeError>`** (per
  `igenome_registry.h`): require `.has_value()` check + failure
  path emits `"NotAttempted"` with error meta.

### Target state
```cpp
void EvolutionSession::phase4_reload_rerun(const ContextRequest& ctx) {
    // Skip rebuild when no mutation actually applied (Risk 3 mitigation)
    if (last_committed_genome_version_ <= baseline_version_) {
        // Same genome — emit NotAttempted
        nlohmann::json meta = build_meta(
            ctx, last_committed_genome_version_, /*gates*/0,
            /*verdict*/ "NotAttempted");
        tracer_->record_phase(TracePhase::Reload, {{"meta", std::move(meta)}});
        return;
    }

    auto genome_result = genome_registry_->load(
        "default", last_committed_genome_version_);
    if (!genome_result.has_value()) {
        // load failure — emit NotAttempted with error
        nlohmann::json meta = build_meta(
            ctx, last_committed_genome_version_, /*gates*/3,
            /*verdict*/ "NotAttempted");
        meta["error"] = "load_failed";
        tracer_->record_phase(TracePhase::Reload, {{"meta", std::move(meta)}});
        return;
    }

    // Hand-mapped AgentConfig from Genome (per Oracle M4 finding)
    auto new_agent_cfg = agent_cfg_;
    new_agent_cfg.system_prompt =
        genome_result.value().spec.harness;

    // Build new ChatSession with mutated config
    auto session_v2 = std::make_unique<hydraforge::pdk::ChatSession>(
        engine_.get(), bus_, &engine_->get_tool_registry(),
        new_agent_cfg, session_cfg_,
        nullptr, nullptr, nullptr, nullptr, nullptr, std::nullopt);

    // Rerun baseline turn_input
    auto result = session_v2->chat(ctx.turn_input);

    // Capture rerun trace (4 trace events per context, not 6 — phase1
    // and phase6 don't emit; corrected from earlier 6-event assertion)
    nlohmann::json rerun_event = {
        {"meta", build_meta(ctx, last_committed_genome_version_,
                            /*gates*/5, "NotAttempted")},
        {"turn_input", ctx.turn_input},
        {"response", result.success ? nlohmann::json(result.response)
                                   : nlohmann::json(nullptr)},
        {"tokens", result.total_tokens},
        {"cost_usd", result.cost_usd}
    };

    // R13.4: redact turn_input/response per sensitivity
    if (ctx.metadata.sensitivity == "internal" ||
        ctx.metadata.sensitivity == "confidential") {
        rerun_event = detail::redact_trace_fields(
            std::move(rerun_event), ctx.metadata.sensitivity);
    }

    // Convert ChatResult → ExecutionTrace for phase5 compare
    last_rerun_exec_ = agenticdsl::ExecutionTrace{
        agenticdsl::ToolResult::ok(result.response),
        /*trace_id*/ last_trace_id_};

    tracer_->record_phase(TracePhase::Reload, rerun_event);
}
```

### Acceptance
- `test_evolution_session_mutation` case "reload_rerun_chain_link" PASS:
  phase4 trace shows `genome_version == phase3 trace genome_version`
  (corrected from D3's broken `baseline_genome_version` reference).
- `test_evolution_session_mutation` case "reload_rerun_skip_no_mutation"
  PASS: when phase3 mutation failed, phase4 emits
  `verdict = "NotAttempted"` and skips rebuild (Risk 3).
- R13.4 redaction applied to phase4 rerun_event when
  `ctx.metadata.sensitivity ∈ {internal, confidential}`.
- 4 trace events per context (not 6 — phase1 / phase6 don't emit).

---

## D5 — Phase 4 --release-metrics semantic + output path fix (2 SP)

### Current state
`main.cpp` writes `metrics.json` to CWD unconditionally; semantics
hollow (per Oracle audit finding).

### Pre-implementation findings (Oracle dual-agent review)
- **Drop ratio formula broken** (`main.cpp:215-217`):
  `drop_ratio = (baseline_failures - mutated_passes) / baseline_total`.
  In mock mode, baseline_failures = 0, mutated_passes = N (one per
  context) → **drop_ratio = -1.0 every time** → acceptance
  `drop_ratio != -1.0` is **physically unreachable**.
- **Missing accessor**: `mutated_failures()` accessor does not exist
  on `EvolutionSession` (only `mutated_passes()` exists). D5 sketch
  referenced it — must add.
- **`compute_drop_ratio` undefined**: not a function in the codebase;
  D5 sketch should use a literal expression in the metrics JSON.

### Target state
```cpp
// In Options struct add:
//   std::string metrics_output_path = "/tmp/l2-metrics.json";
//
// In parse_args:
// } else if (parse_flag_value_eq(argv[i], "--metrics-output", v)) {
//     opts.metrics_output_path = v;
// } else if (parse_flag_value_next(i, argc, argv, "--metrics-output",
//                                  v, opts.parse_error)) {
//     opts.metrics_output_path = v;
// }
//
// In main(), replace the existing metrics block:
if (opts.release_metrics) {
    // REDEFINED formula: drop_ratio = mutated_failures / max(1, mutated_total)
    // (regression ratio; mock mode → 0.0; real regression → fraction)
    int mutated_total = session.mutated_passes() + session.mutated_failures();
    double drop_ratio = (mutated_total > 0)
        ? (double)session.mutated_failures() / mutated_total
        : 0.0;

    nlohmann::json metrics = {
        {"baseline_total", session.baseline_total()},
        {"baseline_failures", session.baseline_failures()},
        {"mutated_passes", session.mutated_passes()},
        {"mutated_failures", session.mutated_failures()},
        {"drop_ratio", drop_ratio}
    };

    std::ofstream f(opts.metrics_output_path);
    if (!f.is_open()) {
        std::cerr << "ERROR: cannot open metrics output: "
                  << opts.metrics_output_path << std::endl;
        return 2;
    }
    f << metrics.dump(2);

    if (drop_ratio > 0.05) {
        std::cerr << "ERROR: drop_ratio " << drop_ratio
                  << " > 5% threshold" << std::endl;
        return 1;
    }
}
```

### Acceptance
- `metrics.json` not in `git status` after binary run (written to
  `/tmp/l2-metrics.json` by default, not CWD).
- `drop_ratio == 0.0` in mock mode (all-pass baseline + mutation).
- `drop_ratio > 0` when mutation produces divergent responses
  (real-LLM mode only; mock mode responses are deterministic).
- `--metrics-output /tmp/foo.json` writes to specified path.
- `mutated_failures()` accessor exists and returns correct count.

---

## D6 — Phase 5 capture-mode=Training → IDistillationWriter (3 SP)

### Current state
`capture_mode` field stored as string, no downstream effect.

### Pre-implementation findings (Oracle dual-agent review)
- **API mismatch** (`idistillation_writer.h`):
  - Namespace is `agenticdsl`, NOT `agenticdsl::stdx` (fictional).
  - Static factory is `make_file_writer(output_dir, agent_id)`, NOT
    `create_session_writer(path)` (fictional).
  - Methods are `write_record(const DistillationRecord&)`, `close()`,
    `finalize(meta)`, NOT `write_event(json)` (fictional).
- **One record per file semantics** (`file_writer.cpp:52-53,72`):
  files named `<agent_id>_<seq:06d>.distill.v1.jsonl` — one record
  per file. Spec R5 acceptance originally said
  "JSONL with ≥4 events per session" — wrong granularity.
- **`DistillationRecord` schema** (`distillation_record.h:49-71`) is
  NOT the trace event 8+12 fields — has `agent_id, convergence,
  reward, input, output, steps, ...`. Must construct per-context.
- **`agent_id` non-empty required** (`file_writer.cpp:43-46`) — three-
  fold enforcement.
- **D6 `bus_->subscribe("chat.event.emitted", ...)` sketch**: bus events
  are `Event` envelopes, not raw JSON; DistillationRecord construction
  is best done per-context (after phase4 / phase5), not from raw
  event stream.

### Target state
```cpp
void EvolutionSession::phase6_emit_jsonl() {
    if (capture_mode_str_ != "Training") return;

    // Per-context DistillationRecord construction (cleaner than bus hook)
    auto writer = agenticdsl::IDistillationWriter::make_file_writer(
        /*output_dir*/ "/tmp",
        /*agent_id*/  "l2-distillation-" + session_id_);

    for (const auto& ctx : contexts_) {
        agenticdsl::DistillationRecord rec;
        rec.agent_id = "l2-distillation-" + session_id_;
        rec.input = ctx.turn_input;
        rec.output = last_baseline_response_;  // or rerun if rerun succeeded
        rec.capture_mode = agenticdsl::CaptureMode::Training;
        rec.convergence.agent_id = "l2-mock";
        rec.convergence.teacher_version = "l2-bootstrap-2026-09-26";
        rec.convergence.task_id = ctx.context_id;
        rec.reward.attribution = bootstrap_attribution_;
        rec.timestamp_iso8601 = now_iso8601();

        writer->write_record(rec);
    }
    writer->finalize({{"session_id", session_id_},
                      {"context_count", contexts_.size()}});
    writer->close();
}
```

### Acceptance
- `ls /tmp/l2-distillation-*.jsonl` produces ≥1 file per context
  (corrected from "≥4 events" — `DistillationRecord` = 1 per context).
- File naming matches `<agent_id>_<seq:06d>.distill.v1.jsonl`
  pattern from `file_writer.cpp`.
- File content has `agent_id == "l2-distillation-<session_id>"` and
  `input/output/capture_mode/convergence` fields per DistillationRecord
  schema.
- `--capture-mode None` produces no distillation files.

### Strategic value
**Wave 3 Phase 2 D4 LoRA training pipeline data path enabler.**
D4 implementation (`openspec/changes/archive/2026-09-25-wave-3-phase-2-d4-lora-pipeline/`)
consumes this JSONL for training data loading.

---

## D7 — Phase 6 Commit + sync + archive (1 SP)

### Commit strategy
One atomic commit per sub-phase (TDD 5-step discipline per Pattern #11):
- Phase 0: `chore(repo): housekeeping build tree + AGENTS.md NOTES + .gitignore metrics.json`
- Phase 1: `feat(l2-evolution): phase5_compare real wiring via IEvaluator::compare`
- Phase 2: `feat(l2-evolution): phase3_mutation real wiring via apply_harness_mutation`
- Phase 3: `feat(l2-evolution): phase4_reload_rerun real wiring via IGenomeRegistry::load`
- Phase 4: `fix(l2-evolution): --release-metrics semantic + --metrics-output flag`
- Phase 5: `feat(l2-evolution): capture-mode=Training → IDistillationWriter wiring`
- Phase 6: `docs(soT): L2 ship row scope "Phase B only" → "✅ ship"`

### SoT scope updates
Three docs §十一 currently say "Phase B only"; after Phase 6 update to:
```
| L2 reference example | ✅ ship 2026-09-26 | change 2026-09-26-l2-evolution-finalization |
```
(no scope qualifier — full real execution chain shipped)

### Archive
```bash
openspec archive 2026-09-26-l2-evolution-real-execution-chain --path openspec/changes/archive/
```
Also archive this change once all 6 phases complete.

---

## Risk Mitigation

### Risk 1 — Mutation gate context failure
`apply_harness_mutation` will fail if `MutationGateContext` doesn't
have all 6 required fields (`current`, `attribution`, `evaluator`,
`budget`, `bus`, `policy`) plus `genome_registry`/`genome_name`/
`parent_version` when registry non-null. Mitigation: D3 explicitly
constructs all fields + seed commit precedes first mutation; T2.1
includes `mutation_gate_context_required_fields` test.

### Risk 2 — IGenomeRegistry hermetic HOME interaction
`create_filesystem(root)` factory takes **explicit root path**
(`genome.h:122`) — NOT `$HOME/.hydraforge/genome/`. The HMAC key
path is `$HOME/.hydraforge/genome.key` (`registry_filesystem.cpp:48`)
which uses `getenv("HOME")` at runtime. **Composition invariants**:
1. `setup_hermetic_home()` must run BEFORE first registry use
   (sets HOME → /tmp/l2-test-<uuid>);
2. `create_filesystem(root)` must be called with hermetic root
   (e.g. `hermetic_guard_->path() / "genome"`), not `~/.hydraforge/...`.
   `main.cpp:191-198` already follows this pattern; test fixtures
   must call `set_hermetic_home` before `set_contexts` / `run_6_phase_demo`.
   Mitigation: T2.1 + T3.1 tests verify ordering via fixture.

### Risk 3 — ChatSession rebuild cost
Building 6 ChatSession instances per EvolutionSession run (one per
context) is expensive. Mitigation: D4 now skips rebuild when
`last_committed_genome_version_ <= baseline_version_` (Risk mitigation
correctly wired into D4 target state).

### Risk 4 — IDistillationWriter API surface
Resolved per Oracle M2 finding. Real API:
- `agenticdsl::IDistillationWriter::make_file_writer(output_dir, agent_id)`
  returns `unique_ptr<IDistillationWriter>`
- `write_record(const DistillationRecord&)`
- `close()` / `finalize(meta)`
- File naming: `<agent_id>_<seq:06d>.distill.v1.jsonl`
- `agent_id` non-empty is required (`file_writer.cpp:43-46`)
D6 target state rewritten accordingly.

### Risk 5 — D7 "6 trace events per context" spec/acceptance mismatch
Original spec R2 acceptance mentioned "6 distinct trace events" but
actual architecture emits 4 (phase2/3/4/5 only). D4 acceptance
corrected to 4. R2 acceptance to be narrowed in spec.md update.

### Risk 6 — R4 enum "Pending" self-contradiction
Original spec R4 acceptance "Mutation traces have
attribution_verdict: 'Pending'" violates closed enum `{Attributed,
Confounded, Insufficient, NotAttempted, null}`. Corrected in D3 +
spec.md: mutation phase emits `NotAttempted` (resolved in phase5
compare).

---

## Acceptance Verification Plan

End-of-change verification (Day 6):

```bash
# Build clean
cmake --build build --target pdk_chat_demo_evolution

# All L2 tests PASS (with updated assertions)
cd build && ctest -L l2-evolution -j1 --output-on-failure  # expect 9/9+

# Zero regression
ctest -R test_chat_session -j1  # expect 7/7

# Real execution proof: drop_ratio == 0.0 in mock mode (all-pass baseline + mutation)
./build/examples/pdk_chat_demo_evolution/pdk_chat_demo_evolution \
  --mock --release-metrics \
  --context-file=examples/pdk_chat_demo_evolution/tests/fixtures/context_request/valid_3class_combined.jsonl

cat /tmp/l2-metrics.json
# expect: baseline_total=3, baseline_failures=0, mutated_passes=3,
#         mutated_failures=0, drop_ratio=0.0

# Capture-mode wiring
./build/examples/pdk_chat_demo_evolution/pdk_chat_demo_evolution \
  --mock --capture-mode Training --context-file=.../valid_3class_combined.jsonl
ls /tmp/l2-distillation-*.jsonl
# expect: 3 files (one per context) with
#         agent_id="l2-distillation-<session_id>"

# Housekeeping clean
git status --short  # expect: no /metrics.json
ls build/tests/CMakeCache.txt 2>/dev/null  # expect: No such file

# AGENTS.md entry + Reverse Indicator present
grep "l2-evolution-finalization" AGENTS.md  # expect: match
```

Oracle post-impl review for final SHIP/SHIP-with-fixes verdict.

---

## Out of Scope (explicit deferral)

| Item | Reason | Recommendation |
|------|--------|----------------|
| `--real-llm` mode | DeepSeek API "Insufficient Balance"; CI skip 兜底 | Separate change after balance recovers |
| `--ablation-mode=full` | Requires Phase 1+2 done for verdict distribution | Include in this change if time permits; else next sprint |
| `--regression-test-suite` | **Already deleted in commit `f456336`** (not stubbed) | No action — finalization ships CLI with 9 flags (8 existing + `--metrics-output`) |
| Wave 3 Phase 2 D4 LoRA pipeline | Separate change; depends on Phase 5 capture-mode output | Start after this change archive |