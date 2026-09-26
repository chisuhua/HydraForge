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

### Target state
Use `agenticdsl::IEvaluator::compare(baseline_trace, rerun_trace)`:
```cpp
void EvolutionSession::phase5_compare(const ContextRequest& ctx) {
    auto baseline = last_baseline_trace_.find(ctx.context_id);
    auto rerun = last_rerun_trace_.find(ctx.context_id);
    if (baseline == last_baseline_trace_.end() ||
        rerun == last_rerun_trace_.end()) {
        // Phase 3/4 not run; honest stub
        return;
    }

    auto eval = evaluator_->compare(
        baseline->second, rerun->second);

    std::string verdict;
    switch (eval.attribution) {
        case AttributionResult::Attributed: verdict = "Attributed"; break;
        case AttributionResult::Confounded: verdict = "Confounded"; break;
        case AttributionResult::Insufficient: verdict = "Insufficient"; break;
    }

    if (eval.pass) ++mutated_passes_;
    else ++mutated_failures_;

    nlohmann::json meta = build_meta(ctx, eval.genome_version, eval.gate_passes, verdict);
    tracer_->record_phase(TracePhase::Compare, {{"meta", std::move(meta)}});
}
```

### Required additions
- `EvolutionSession` member: `std::unique_ptr<agenticdsl::IEvaluator> evaluator_`
- Constructor init: `evaluator_ = std::make_unique<agenticdsl::BehavioralEquivalenceEvaluator>()` (ADR-0083 V2)
- `last_baseline_trace_` + `last_rerun_trace_` maps populated by phase2 and phase4

### Acceptance
- `test_reverse_indicators` case "compare_verdict_real" PASS:
  phase5_attribution_verdict != "NotAttempted" when phase3/4 actually ran
- `metrics.json.drop_ratio` reflects real compare output (not always -1.0)

---

## D3 — Phase 2 phase3_mutation real wiring (5 SP)

### Current state
`evolution_session.cpp:phase3_mutation`:
```cpp
nlohmann::json meta = build_meta(ctx, 1, 5, "Attributed");
tracer_->record_phase(TracePhase::Mutation, {{"meta", std::move(meta)}});
```
Hardcodes `gate_passes:5` + `"Attributed"` — the final fabrication.

### Target state
Use `agenticdsl::evolution::apply_harness_mutation` (5-param signature
confirmed at `harness_rsi.h:73-78`):
```cpp
void EvolutionSession::phase3_mutation(const ContextRequest& ctx) {
    MutationGateContext mctx;
    mctx.genome_registry = &genome_registry_;
    mctx.parent_version = last_committed_genome_version_;

    auto result = apply_harness_mutation(
        /* mutations = */ generate_test_mutations(ctx),
        /* system_prompt = */ agent_cfg_.system_prompt,
        /* tools = */ tool_names_snapshot_,
        /* registry = */ engine_->get_tool_registry(),
        /* mutation_gate_ctx = */ mctx);

    if (result.applied) {
        last_committed_genome_version_ = result.new_version;
    }

    nlohmann::json meta = build_meta(
        ctx, result.new_version, result.gates_passed, /* verdict = */ "Pending");
    tracer_->record_phase(TracePhase::Mutation, {{"meta", std::move(meta)}});
}
```

### Required additions
- `EvolutionSession` member: `agenticdsl::genome::FilesystemGenomeRegistry genome_registry_`
  initialized in constructor with hermetic HOME path
- `agenticdsl::evolution::generate_test_mutations(ctx)` helper (deterministic
  mutation set per task_class for reproducibility)
- `last_committed_genome_version_` tracking

### Acceptance
- `test_evolution_session_mutation` case "mutation_genome_version_increments" PASS:
  phase3 trace shows `genome_version > 1` after real mutation
- `FilesystemGenomeRegistry::commit("default", mutations, ctx)` succeeds
  with valid parent_version link

---

## D4 — Phase 3 phase4_reload_rerun real wiring (5 SP)

### Current state
`evolution_session.cpp:phase4_reload_rerun`: same hardcoded stub as phase3.

### Target state
```cpp
void EvolutionSession::phase4_reload_rerun(const ContextRequest& ctx) {
    // Load mutated genome from registry
    auto genome = genome_registry_.load("default", last_committed_genome_version_);
    auto new_agent_cfg = genome.to_agent_config();

    // Build new ChatSession with mutated config
    auto session_v2 = std::make_unique<hydraforge::pdk::ChatSession>(
        engine_.get(), bus_, &engine_->get_tool_registry(),
        new_agent_cfg, session_cfg_,
        nullptr, nullptr, nullptr, nullptr, nullptr, std::nullopt);

    // Rerun baseline turn_input
    auto result = session_v2->chat(ctx.turn_input);

    // Capture rerun trace
    nlohmann::json rerun_event = {
        {"meta", build_meta(ctx, last_committed_genome_version_, 5, "Pending")},
        {"turn_input", ctx.turn_input},
        {"response", result.response},
        {"tokens", result.total_tokens},
        {"cost_usd", result.cost_usd}
    };
    last_rerun_trace_[ctx.context_id] = rerun_event;
    tracer_->record_phase(TracePhase::Reload, rerun_event);
}
```

### Acceptance
- `test_evolution_session_mutation` case "reload_rerun_distinct_response" PASS:
  baseline trace.response != rerun trace.response (when mutation actually changed behavior)
- 6 trace events for 1 context show distinct genome_version progression:
  baseline (v1) → mutation (v2) → reload (v2) → compare (v2)

---

## D5 — Phase 4 --release-metrics semantic + output path fix (2 SP)

### Current state
`main.cpp` writes `metrics.json` to CWD unconditionally; semantics
hollow (per Oracle audit finding).

### Target state
Add `--metrics-output <path>` flag, default `/tmp/l2-metrics.json`:
```cpp
std::string metrics_output_path = "/tmp/l2-metrics.json";
// CLI parser:
} else if (parse_flag_value_eq(argv[i], "--metrics-output", v)) {
    metrics_output_path = v;
} else if (parse_flag_value_next(i, argc, argv, "--metrics-output", v, opts.parse_error)) {
    metrics_output_path = v;
}

if (opts.release_metrics) {
    nlohmann::json metrics = {
        {"baseline_total", session.baseline_total()},
        {"baseline_failures", session.baseline_failures()},
        {"mutated_passes", session.mutated_passes()},
        {"mutated_failures", session.mutated_failures()},
        {"drop_ratio", compute_drop_ratio(...)}
    };

    std::ofstream f(metrics_output_path);
    f << metrics.dump(2);

    if (metrics["drop_ratio"].get<double>() > 0.05) {
        std::cerr << "ERROR: drop_ratio > 5%" << std::endl;
        return 1;
    }
}
```

### Acceptance
- `metrics.json` not in `git status` after binary run
- `drop_ratio` value changes between mock-mode runs with different mutations
- `--metrics-output /tmp/foo.json` writes to specified path

---

## D6 — Phase 5 capture-mode=Training → IDistillationWriter (3 SP)

### Current state
`capture_mode` field stored as string, no downstream effect.

### Target state
Hook IDistillationWriter (ADR-0086 v1.1) to ChatSession emit events:
```cpp
if (capture_mode_str_ == "Training") {
    auto writer = agenticdsl::stdx::IDistillationWriter::create_session_writer(
        "/tmp/l2-distillation-" + session_id + ".jsonl");

    bus_->subscribe("chat.event.emitted",
        [writer](const nlohmann::json& event) {
            writer->write_event(event);
        });
}
```

### Acceptance
- `ls /tmp/l2-distillation-*.jsonl` after `--capture-mode Training` run
- JSONL parseable + contains 4-phase events per session
- `None` mode (default) does not create distillation files

### Strategic value
This is the **Wave 3 Phase 2 D4 LoRA training pipeline data path enabler**.
D4 improvement (`openspec/changes/archive/2026-09-25-wave-3-phase-2-d4-lora-pipeline/`)
depends on IDistillationWriter SessionWriter JSONL output for training data.
Phase 5 ships the L2 production side; D4 consumes from this output.

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
`apply_harness_mutation` may fail if MutationGateContext doesn't have
all required fields. Mitigation: D3 includes explicit unit test
`mutation_gate_context_required_fields` before implementing real call.

### Risk 2 — GenomeRegistry hermetic HOME interaction
FilesystemGenomeRegistry writes to `$HOME/.hydraforge/genome/`. The
existing `setup_hermetic_home()` redirects HOME to `/tmp/l2-test-<uuid>`.
Need to verify they compose correctly. Mitigation: D4 includes
explicit test `reload_with_hermetic_home`.

### Risk 3 — ChatSession rebuild cost
Building 6 ChatSession instances per EvolutionSession run (one per
context) is expensive. Mitigation: only rebuild when mutation actually
applied; skip rebuild when `last_committed_genome_version_` unchanged.

### Risk 4 — IDistillationWriter API surface
ADR-0086 v1.1 was shipped 2026-08-27 (per AGENTS.md). Confirm the
exact factory function signature before D6 implementation.

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

# Real execution: drop_ratio != -1.0 in mock mode
./build/examples/pdk_chat_demo_evolution/pdk_chat_demo_evolution \
  --mock --release-metrics \
  --context-file=examples/pdk_chat_demo_evolution/tests/fixtures/context_request/valid_3class_combined.jsonl

cat /tmp/l2-metrics.json | jq .
# expect: baseline_total > 0, mutated_passes > 0, drop_ratio != -1.0

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
| `--regression-test-suite` | Spec R1 9th flag; implement or spec 降级 | Recommend spec 降级 to 8 flags (lower cost) |
| Wave 3 Phase 2 D4 LoRA pipeline | Separate change; depends on Phase 5 capture-mode output | Start after this change archive |