# Tasks — l2-evolution-finalization

**TDD 5-step discipline**: Write failing test → Verify fail → Implement → Verify pass → Commit
**Pattern**: AGENTS.md Pattern #11 (Async Worker + Dual Oracle review SHIP-with-fixes cycle)
**Branch**: `feat/l2-evolution-finalization` (worktree per Pattern #11)
**Sub-phase commits**: atomic per sub-phase, no amend

---

## Phase 0 — Housekeeping (Day 1 morning, 1 SP)

### T0.1 — Verify ctest pollution state (refactored per Oracle Mi1)
**TDD Step 1 (Write failing test)**: Add to `scripts/check-no-nested-configure.sh`
(NOT `tests/test_housekeeping.cpp` — concerns separation, per Oracle Mi1):
```bash
#!/bin/bash
# scripts/check-no-nested-configure.sh — repo hygiene check
set -e
errors=0
for path in build/tests build/examples-tests; do
    if [ -f "$path/CMakeCache.txt" ]; then
        echo "ERROR: nested cmake configure detected: $path/CMakeCache.txt"
        errors=$((errors+1))
    fi
done
exit $errors
```
Register as ctest test with `WORKING_DIRECTORY=${CMAKE_SOURCE_DIR}`:
```cmake
add_test(NAME housekeeping_no_nested_configure
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
         COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-no-nested-configure.sh)
```
**TDD Step 2 (Verify fail)**: Run — expect FAIL (nested dirs exist)
**Step 3-4**: rm nested dirs + verify PASS
**Step 5 (Commit)**:
```
chore(repo): housekeeping build tree + AGENTS.md NOTES + .gitignore metrics.json

[Reverse Indicator]
+ new_up: build tree pollution fix (240-test "Not Run" recovery),
  AGENTS.md NOTES anti-nested-configure rule, .gitignore metrics.json
- old_down: 0% (pure housekeeping, no behavior change)
failure_traces: N/A
ablation: N/A
context_ids: N/A
```

---

## Phase 1 — phase5_compare real wiring (Day 1 afternoon, 3 SP)

### T1.1 — Write failing test
`tests/test_reverse_indicators.cpp` add new case:
```cpp
TEST_CASE("phase5_compare: real attribution verdict from IEvaluator::compare",
          "[l2-evolution]") {
    // Setup: 1 context with baseline + rerun captured
    auto session = pdk_chat_demo_evolution::EvolutionSession("mock", "None", true);
    session.set_contexts({ctx});
    session.set_hermetic_home(guard);
    session.run_6_phase_demo();

    StdoutCapture cap;
    auto events = parse_lines(cap.str());
    auto compare = find(events, "phase", "compare");
    REQUIRE(compare != events.end());

    // Real verdict must NOT be hardcoded "NotAttempted" when compare ran
    const auto& verdict = (*compare)["meta"]["attribution_verdict"].get<std::string>();
    CHECK(verdict != "NotAttempted");  // failure mode of this change
}
```
**Step 2**: Verify FAIL (current implementation hardcodes "NotAttempted")
**Step 3-4**: Implement IEvaluator::compare + verify PASS
**Step 5 (Commit)**:
```
feat(l2-evolution): phase5_compare real wiring via IEvaluator::compare

[Reverse Indicator]
+ new_up: phase5 attribution_verdict from real BehavioralEquivalence
  compare (was hardcoded "NotAttempted"); drop_ratio gains semantic
  meaning (was hollow -1.0)
- old_down: drop_ratio still 0% baseline (real evaluation depends on
  Phase 3 mutation creating non-trivial diff — expected in mock mode)
failure_traces: N/A
ablation: mock baseline vs mock rerun identical → all "Attributed" (mock
  expected); semantic empty until real mutation
context_ids: code-class-ex-001, code-class-ex-002
```

---

## Phase 2 — phase3_mutation real wiring (Day 2-3, **6 SP**, +0.5 day vs initial estimate per Oracle M6 finding)

### T2.1 — Write failing test for MutationGateContext required fields
`tests/test_evolution_session_mutation.cpp` add:
```cpp
TEST_CASE("phase3_mutation: genome_version increments after apply_harness_mutation") {
    auto session = EvolutionSession("mock", "None", true);
    session.set_contexts({ctx});
    session.set_hermetic_home(guard);
    session.run_6_phase_demo();

    StdoutCapture cap;
    auto events = parse_lines(cap.str());
    auto mutation = find(events, "phase", "mutation");
    REQUIRE(mutation != events.end());
    CHECK((*mutation)["meta"]["genome_version"].get<int>() > 1);
}

TEST_CASE("phase3_mutation: gate0_rejects_parent_version_zero",
          "[l2-evolution]") {
    // Verifies Gate 0 hard requirement: parent_version == 0 → InvalidMutation
    // (Oracle C1 finding — first mutation always fails without seed commit)
    auto session = EvolutionSessionNoSeed("mock", "None", true);
    session.set_contexts({ctx});
    session.set_hermetic_home(guard);
    session.run_6_phase_demo();

    auto mutation = find(events, "phase", "mutation");
    CHECK((*mutation)["meta"]["attribution_verdict"].get<std::string>() == "NotAttempted");
    CHECK((*mutation)["meta"]["gate_passes"].get<int>() == 0);  // Gate 0 fail
}

TEST_CASE("phase3_mutation: gate1_rejects_missing_evaluator_or_budget",
          "[l2-evolution]") {
    // Verifies Gate 1 hard requirement: !evaluator || !budget → InvalidMutation
    auto session = EvolutionSessionMissingEvaluator("mock", "None", true);
    // ... setup ... expect Gate 1 fail
}
```
**Step 2**: Verify FAIL (currently `build_meta(ctx, 1, 5, "Attributed")` always returns version=1; without seed commit, Gate 0 always rejects)
**Step 3-4**: Implement apply_harness_mutation + IGenomeRegistry (NOT FilesystemGenomeRegistry) + complete MutationGateContext (6 required fields + bootstrap attribution) + seed commit in phase1_init + verify PASS
**Step 5 (Commit)**:
```
feat(l2-evolution): phase3_mutation real wiring via apply_harness_mutation

[Reverse Indicator]
+ new_up: phase3 mutation produces real genome_version increment (was
  hardcoded 1); 5-tier gate runs against real context (was fake 5/5);
  bootstrap attribution + seed commit + budget controller members added
- old_down: 0% (real wiring, no capability removed)
failure_traces: MutationGateContext missing required fields →
  InvalidMutation → emit "NotAttempted" with gates_passed from error
  enum mapping (InvalidMutation→0, NotReady→1, GovernanceDenied→2,
  RegistryRejected→3, success→5)
ablation: 3-segment mock comparison baseline vs mutated vs rerun
  identical in mock mode (expected; mutation deterministic)
context_ids: code-class-ex-001, code-class-ex-002
```

---

## Phase 3 — phase4_reload_rerun real wiring (Day 4, 5 SP)

### T3.1 — Write failing test
```cpp
TEST_CASE("phase4_reload: rerun produces distinct trace from baseline") {
    // Setup: same ctx, baseline trace captured in phase2
    auto session = EvolutionSession("mock", "None", true);
    session.set_contexts({ctx});
    session.set_hermetic_home(guard);
    session.run_6_phase_demo();

    auto events = parse_lines(cap.str());
    auto baseline = find(events, "phase", "baseline");
    auto mutation = find(events, "phase", "mutation");
    auto reload = find(events, "phase", "reload");
    REQUIRE(baseline != events.end());
    REQUIRE(mutation != events.end());
    REQUIRE(reload != events.end());

    // CORRECTED per Oracle M6 finding — chain link is reload == mutation's
    // new_version, NOT reload == baseline version (which was wrong semantics
    // in original draft using undefined `baseline_genome_version` variable)
    int mutation_version = (*mutation)["meta"]["genome_version"].get<int>();
    int reload_version = (*reload)["meta"]["genome_version"].get<int>();
    CHECK(reload_version == mutation_version);  // chain link correct
}

TEST_CASE("phase4_reload: skip_when_mutation_not_applied") {
    // When phase3 mutation fails (Gate 0/1 reject), phase4 must
    // emit NotAttempted + skip ChatSession rebuild (Risk 3 mitigation)
    auto session = EvolutionSessionNoSeed("mock", "None", true);
    // ... setup ... mutation fails ...
    auto reload = find(events, "phase", "reload");
    CHECK((*reload)["meta"]["attribution_verdict"] == "NotAttempted");
    // No new ChatSession constructed (verified via session counter)
}
```
**Step 2**: Verify FAIL (reload hardcodes same as mutation stub; baseline_genome_version undefined variable; no skip-on-no-mutation logic)
**Step 3-4**: Implement IGenomeRegistry::load (NOT FilesystemGenomeRegistry) + hand-mapped AgentConfig from GenomeSpec.harness + ChatSession rebuild with skip-when-no-mutation + new `session_cfg_` member + verify PASS
**Step 5 (Commit)**:
```
feat(l2-evolution): phase4_reload_rerun real wiring via IGenomeRegistry::load

[Reverse Indicator]
+ new_up: phase4 reload captures distinct ChatResult from new ChatSession
  with mutated AgentConfig (hand-mapped from GenomeSpec.harness); genome
  chain links baseline → mutation(N+1) → reload(N+1); skip-on-no-mutation
  applied (Risk 3); R13.4 redaction applied to rerun_event
- old_down: 0%
failure_traces: IGenomeRegistry::load returns Result error →
  emit "NotAttempted" with gates_passed=3 + error meta + abort chain
ablation: same as Phase 2 (mock identicality)
context_ids: code-class-ex-001
```

### T3.2 — Add new EvolutionSession members (per Oracle M7 finding)
```cpp
// evolution_session.h additions:
class EvolutionSession {
    // ... existing members ...
    hydraforge::pdk::SessionConfig session_cfg_;     // T3 — for ChatSession rebuild ctor
    int mutated_failures_ = 0;                        // D5 — mutated_failures() accessor
    std::optional<agenticdsl::ExecutionTrace>
        last_baseline_exec_, last_rerun_exec_;        // D2 — replace map members
public:
    int mutated_failures() const { return mutated_failures_; }
};
```
session_cfg_ default-initialized with `enable_input_thread=false`
(matches Phase B pattern). T3.1 references this member.

---

## Phase 4 — --release-metrics semantic + output path fix (Day 5 morning, 2 SP)

### T4.1 — Write failing test (in-process, per Oracle Mi4 finding)
```cpp
TEST_CASE("--release-metrics: --metrics-output flag writes to specified path") {
    // In-process refactor (NOT exec() of binary — fragile per Mi4)
    auto session = EvolutionSession("mock", "None", true);
    session.set_contexts(load_valid_3class_combined());
    session.set_hermetic_home(setup_hermetic_home());
    session.run_6_phase_demo();

    auto opts = make_opts();
    opts.metrics_output_path = "/tmp/l2-test-metrics.json";
    opts.release_metrics = true;

    auto rc = write_metrics(session, opts);  // extracted free function
    CHECK(rc == 0);
    CHECK(fs::exists("/tmp/l2-test-metrics.json"));
    CHECK_FALSE(fs::exists("metrics.json"));  // CWD pollution prevention
    fs::remove("/tmp/l2-test-metrics.json");
}

TEST_CASE("--release-metrics: drop_ratio_zero_in_mock_mode") {
    // Verifies the redefined formula gives 0.0 in mock (all-pass)
    // (Oracle C4 finding — original formula gave -1.0 always)
    auto session = EvolutionSession("mock", "None", true);
    // ... setup ...
    auto metrics = collect_metrics(session);

    CHECK(metrics["drop_ratio"].get<double>() == Approx(0.0));
    CHECK(metrics["mutated_passes"].get<int>() == 3);  // valid_3class fixture
    CHECK(metrics["mutated_failures"].get<int>() == 0);
}
```
**Refactor prerequisite**: Extract `write_metrics(session, opts)` free
function from `main.cpp` so tests can call in-process (per Oracle Mi4
finding — `exec()` helper doesn't exist; binary path resolution +
env inheritance fragile).

**Step 2**: Verify FAIL (current `main.cpp` writes `metrics.json` to CWD;
redefined formula not yet in scope; in-process refactor not yet done)
**Step 3-4**: Extract `write_metrics()` free function + add
`--metrics-output` flag + change default to `/tmp/l2-metrics.json` +
implement redefined `drop_ratio` formula + verify PASS
**Step 5 (Commit)**:
```
fix(l2-evolution): --release-metrics semantic + --metrics-output flag

[Reverse Indicator]
+ new_up: --metrics-output <path> flag (default /tmp/l2-metrics.json);
  CWD pollution fix (repo root metrics.json never written); drop_ratio
  formula redefined to mutated_failures/max(1,mutated_total) — 0.0
  in mock mode (was always -1.0); write_metrics() free function
  extracted for in-process testability
- old_down: 0% (default path change is opt-in via flag)
failure_traces: invalid --metrics-output path → write fails → exit 2
  + stderr error
ablation: N/A
context_ids: code-class-ex-001
```

---

## Phase 5 — capture-mode=Training → IDistillationWriter (Day 5 afternoon, 3 SP)

### T5.1 — Write failing test (refactored per Oracle Mi4 finding)
```cpp
TEST_CASE("capture-mode=Training: emits IDistillationWriter JSONL") {
    // In-process (NOT exec binary) — refactor write_metrics + writer_hook
    auto session = EvolutionSession("mock", "Training", true);  // capture=Training
    session.set_contexts(load_valid_3class_combined());
    session.set_hermetic_home(setup_hermetic_home());

    auto writer_root = fs::temp_directory_path() / "l2-distillation-test";
    fs::remove_all(writer_root);
    session.set_distillation_output_dir(writer_root);  // NEW member

    session.run_6_phase_demo();

    // Glob via directory_iterator (fs::exists doesn't support globs)
    std::vector<fs::path> files;
    for (auto& e : fs::directory_iterator(writer_root)) {
        if (e.path().string().ends_with(".distill.v1.jsonl")) files.push_back(e);
    }
    CHECK(files.size() >= 3);  // 1 per context (corrected from "4 events")

    // Verify file content schema matches DistillationRecord (not trace events)
    auto rec = nlohmann::json::parse(read_file(files[0]));
    CHECK(rec.contains("agent_id"));
    CHECK(rec.contains("input"));
    CHECK(rec.contains("output"));
    CHECK(rec.contains("capture_mode"));
    CHECK(rec.contains("convergence"));
    // agent_id matches expected pattern
    CHECK(rec["agent_id"].get<std::string>().starts_with("l2-distillation-"));

    fs::remove_all(writer_root);
}

TEST_CASE("capture-mode=None: does NOT emit distillation file") {
    auto session = EvolutionSession("mock", "None", true);
    // ... setup with output_dir ...
    session.run_6_phase_demo();
    auto files = list_distillation_files(output_dir);
    CHECK(files.empty());
}
```
**Refactor prerequisite**: Extract `setup_distillation_writer(session,
output_dir)` free function + add `session.set_distillation_output_dir()`
method (per Oracle Mi4 finding — `exec()` fictional; glob via
`directory_iterator`).

**Step 2**: Verify FAIL (capture-mode is currently inert string;
IDistillationWriter not called; refactored interface doesn't exist)
**Step 3-4**: Implement per-context `DistillationRecord` construction
in phase6_emit_jsonl (per Oracle M2 finding — `IDistillationWriter::
make_file_writer(output_dir, agent_id)` real API) + verify PASS
**Step 5 (Commit)**:
```
feat(l2-evolution): capture-mode=Training → IDistillationWriter wiring

[Reverse Indicator]
+ new_up: capture-mode=Training activates IDistillationWriter per-context
  DistillationRecord output (Wave 3 Phase 2 D4 LoRA data path enabler);
  capture-mode=None (default) unchanged (no distillation file);
  tests refactored to in-process (no exec() helper)
- old_down: 0% (opt-in feature)
failure_traces: IDistillationWriter factory returns null → exit 3 + stderr
ablation: N/A
context_ids: code-class-ex-001
```

---

## Phase 6 — Commit + sync + archive (Day 6, 1 SP)

### T6.1 — AGENTS.md Recent Changes entry + 5-field Reverse Indicator block
Add entry referencing this change with:
```
+ new_up: L2 reference example fully shipped (Phase B+C, no scope qualifier),
  housekeeping build tree fix, 3 SoTs §十一 "✅ ship" (no scope qualifier)
- old_down: drop_ratio semantic still mock-mode-empty (real LLM-driven
  evaluation is Wave 3 D4 scope, not L2)
failure_traces: N/A (all phases ship clean)
ablation: 3-segment baseline vs mutation vs rerun (mock = identical;
  real divergence requires Wave 3 D4 LoRA-tuned evaluator)
context_ids: code-class-ex-001, code-class-ex-002, r13-4-confidential-test-001
```

### T6.2 — 3 SoTs §十一 ship row scope update
For each of:
- `docs/architecture/self-evolution-architecture-2026-08.md` §十一
- `docs/architecture/harness-architecture-2026-09.md` §十一
- `docs/architecture/rsi-architecture-2026-09.md` §十一

Change current row:
```
| L2 reference example | ✅ ship 2026-09-26 | change 2026-09-26-l2-evolution-real-execution-chain (Phase B only) |
```
to:
```
| L2 reference example | ✅ ship 2026-09-26 | change 2026-09-26-l2-evolution-finalization (Phase B + Phase C) |
```

### T6.3 — Archive predecessor change
```bash
openspec archive 2026-09-26-l2-evolution-real-execution-chain
mv openspec/changes/2026-09-26-l2-evolution-real-execution-chain/ openspec/changes/archive/
```

### T6.4 — Final atomic commit + AGENTS.md sync
```
docs(architecture): L2 reference example full ship (Phase B + Phase C)

3 SoTs §十一 ship row scope updated from "Phase B only" → "Phase B + Phase C".
Predecessor change 2026-09-26-l2-evolution-real-execution-chain archived.

[Reverse Indicator]
+ new_up: SoT cross-doc consistency (no scope qualifier in any of 3 docs)
- old_down: 0%
failure_traces: N/A
ablation: N/A
context_ids: code-class-ex-001
```

---

## Final verification (Day 6 end)

```bash
# Build clean
cmake --build build --target pdk_chat_demo_evolution
# All L2 tests PASS
cd build && ctest -L l2-evolution -j1 --output-on-failure  # expect 9+/9
# Zero regression
ctest -R test_chat_session -j1  # expect 7/7
# Real execution proof: drop_ratio != -1.0 in mock mode
./build/examples/pdk_chat_demo_evolution/pdk_chat_demo_evolution \
  --mock --release-metrics \
  --context-file=examples/pdk_chat_demo_evolution/tests/fixtures/context_request/valid_3class_combined.jsonl
cat /tmp/l2-metrics.json | jq .
# Housekeeping clean
git status --short  # expect: nothing
# Self-evolution loop end-to-end: 6 distinct trace events per context
./build/examples/pdk_chat_demo_evolution/pdk_chat_demo_evolution \
  --mock --trace-events --capture-mode Training \
  --context-file=.../valid_3class_combined.jsonl
ls /tmp/l2-distillation-*.jsonl
```

Oracle post-impl review for final SHIP/SHIP-with-fixes verdict.

---

## Anti-pattern reminders (per Sprint 36 lessons)

- **No hardcoded trace data** in any new code (per Oracle audit finding 1)
- **No helper without caller** (per redact_trace_fields dead code lesson)
- **No .gitignore-less CWD writes** (per metrics.json pollution lesson)
- **No "改测试匹配实现"** (per baseline verdict enum regression lesson)
  - If test fails because new code violates spec, FIX the code, not the test
- **Atomic commits per sub-phase** (no amend, per Pattern #4)
- **5-field Reverse Indicator per commit** (no exception, per Reverse Indicator Rule)
- **No spec-vs-impl drift** — if new code can't satisfy spec MUST, update spec FIRST

---

## Out-of-Scope Tracking

Items explicitly deferred beyond this change (tracked in proposal.md
Non-Goals section):
- `--real-llm` mode → CloudLLMAdapter (DeepSeek balance)
- `--ablation-mode=full` → ablation_report.json (depends on Phase 1+2)
- `--regression-test-suite` flag (recommend spec 降级)
- Wave 3 Phase 2 D4 LoRA pipeline (separate change after archive)