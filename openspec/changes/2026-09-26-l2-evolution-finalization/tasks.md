# Tasks — l2-evolution-finalization

**TDD 5-step discipline**: Write failing test → Verify fail → Implement → Verify pass → Commit
**Pattern**: AGENTS.md Pattern #11 (Async Worker + Dual Oracle review SHIP-with-fixes cycle)
**Branch**: `feat/l2-evolution-finalization` (worktree per Pattern #11)
**Sub-phase commits**: atomic per sub-phase, no amend

---

## Phase 0 — Housekeeping (Day 1 morning, 1 SP)

### T0.1 — Verify ctest pollution state
**TDD Step 1 (Write failing test)**: Write `tests/test_housekeeping.cpp`:
```cpp
TEST_CASE("housekeeping: no nested cmake configure in build tree") {
    CHECK_FALSE(fs::exists("build/tests/CMakeCache.txt"));
    CHECK_FALSE(fs::exists("build/examples-tests/CMakeCache.txt"));
}
TEST_CASE("housekeeping: metrics.json gitignored") {
    // Simulate metrics.json write to repo root, verify .gitignore matches
    std::ofstream f("metrics.json"); f << "{}"; f.close();
    // run git check-ignore
    CHECK(exec("git check-ignore -q metrics.json") == 0);
    fs::remove("metrics.json");
}
```
**TDD Step 2 (Verify fail)**: Run — expect FAIL (nested dirs exist, .gitignore missing)
**Step 3-4**: Implement fix + verify PASS
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

## Phase 2 — phase3_mutation real wiring (Day 2-3, 5 SP)

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
```
**Step 2**: Verify FAIL (currently `build_meta(ctx, 1, 5, "Attributed")` always returns version=1)
**Step 3-4**: Implement apply_harness_mutation + FilesystemGenomeRegistry + MutationGateContext + verify PASS
**Step 5 (Commit)**:
```
feat(l2-evolution): phase3_mutation real wiring via apply_harness_mutation

[Reverse Indicator]
+ new_up: phase3 mutation produces real genome_version increment (was
  hardcoded 1); 5-tier gate runs against real context (was fake 5/5);
  trace event reflects actual mutation outcome
- old_down: 0% (real wiring, no capability removed)
failure_traces: MutationGateContext missing fields → runtime_error
  → catch in phase3 + emit "NotAttempted" with error meta
ablation: 3-segment mock comparison baseline vs mutated vs rerun
  identical in mock mode (expected; mutation deterministic)
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
    auto reload = find(events, "phase", "reload");
    REQUIRE(baseline != events.end());
    REQUIRE(reload != events.end());

    // genome_version in reload must equal mutation output (chain link)
    CHECK((*reload)["meta"]["genome_version"].get<int>() ==
          baseline_genome_version);
}
```
**Step 2**: Verify FAIL (reload hardcodes same as mutation stub)
**Step 3-4**: Implement FilesystemGenomeRegistry::load + to_agent_config + ChatSession rebuild + verify PASS
**Step 5 (Commit)**:
```
feat(l2-evolution): phase4_reload_rerun real wiring via IGenomeRegistry::load

[Reverse Indicator]
+ new_up: phase4 reload captures distinct ChatResult from new ChatSession
  with mutated AgentConfig; genome chain links baseline(N) →
  mutation(N+1) → reload(N+1) → compare(N+1)
- old_down: 0%
failure_traces: FilesystemGenomeRegistry::load returns empty
  → catch + emit "NotAttempted" with error meta + abort chain
ablation: same as Phase 2 (mock identicality)
context_ids: code-class-ex-001
```

---

## Phase 4 — --release-metrics semantic + output path fix (Day 5 morning, 2 SP)

### T4.1 — Write failing test
```cpp
TEST_CASE("--release-metrics: --metrics-output flag writes to specified path") {
    // Use /tmp/l2-test-metrics.json
    exec("pdk_chat_demo_evolution --mock --release-metrics "
         "--metrics-output /tmp/l2-test-metrics.json "
         "--context-file=.../valid_3class_combined.jsonl");
    CHECK(fs::exists("/tmp/l2-test-metrics.json"));
    CHECK_FALSE(fs::exists("metrics.json"));  // CWD pollution prevention
    fs::remove("/tmp/l2-test-metrics.json");
}
```
**Step 2**: Verify FAIL (current writes to CWD unconditionally)
**Step 3-4**: Add `--metrics-output` flag, change default to `/tmp/l2-metrics.json` + verify PASS
**Step 5 (Commit)**:
```
fix(l2-evolution): --release-metrics semantic + --metrics-output flag

[Reverse Indicator]
+ new_up: --metrics-output <path> flag (default /tmp/l2-metrics.json);
  CWD pollution fix (repo root metrics.json never written); drop_ratio
  now reflects real phase5 compare output (depends on Phase 1)
- old_down: 0% (default path change is opt-in via flag)
failure_traces: invalid --metrics-output path → write fails → exit 2
  + stderr error
ablation: N/A
context_ids: code-class-ex-001
```

---

## Phase 5 — capture-mode=Training → IDistillationWriter (Day 5 afternoon, 3 SP)

### T5.1 — Write failing test
```cpp
TEST_CASE("capture-mode=Training: emits IDistillationWriter JSONL") {
    exec("pdk_chat_demo_evolution --mock --capture-mode Training "
         "--context-file=.../valid_3class_combined.jsonl");
    CHECK(fs::exists("/tmp/l2-distillation-*.jsonl"));
    auto jsonl = read_lines("/tmp/l2-distillation-*.jsonl");
    CHECK(jsonl.size() >= 4);  // 1 context × 4 phases
}
TEST_CASE("capture-mode=None: does NOT emit distillation file") {
    exec("pdk_chat_demo_evolution --mock --capture-mode None ...");
    CHECK_FALSE(any_file_matches("/tmp/l2-distillation-*.jsonl"));
}
```
**Step 2**: Verify FAIL (capture-mode is currently inert string)
**Step 3-4**: Hook IDistillationWriter to bus subscribe + verify PASS
**Step 5 (Commit)**:
```
feat(l2-evolution): capture-mode=Training → IDistillationWriter wiring

[Reverse Indicator]
+ new_up: capture-mode=Training activates IDistillationWriter JSONL output
  (Wave 3 Phase 2 D4 LoRA data path enabler); capture-mode=None (default)
  unchanged (no distillation file)
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