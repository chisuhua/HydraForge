# pdk_chat_demo_evolution

> **L2 reference example demo** for `pdk-chat-demo-evolution-reference-example` OpenSpec change.
> Standalone sub-project: consumes existing public APIs only, no `include/` diff (N2 hard-block).
> Zero diff on `examples/pdk_chat_demo/main.cpp` (N1 hard-block).

## Build

```bash
cmake -S . -B build -DAGENTICDSL_BUILD_EXAMPLES=ON -DAGENTICDSL_BUILD_TESTS=ON
cmake --build build --target pdk_chat_demo_evolution -j$(nproc)
```

## Run

### Mock mode (zero API key required, default)

```bash
# via dispatcher:
./run_evolution_demo.sh mock

# via binary directly:
./build/examples/pdk_chat_demo_evolution/pdk_chat_demo_evolution \
  --mock \
  --context-file=examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl \
  --trace-events
```

### Real-LLM mode (DeepSeek)

```bash
export DEEPSEEK_API_KEY=sk-...
./run_evolution_demo.sh real-llm
```

## CLI flags (9 total per P1-6 fix: 5 R1 + 3 R8 + 1 R13 helper)

| Flag | R-class | Default | Description |
|------|---------|---------|-------------|
| `--context-file <path.jsonl>` | R1 (req) | — | ContextRequest JSONL file (R13.1 schema) |
| `--provider <mock\|deepseek\|custom>` | R1 | `mock` | LLM provider |
| `--capture-mode <None\|Training>` | R1 | `None` | Distillation capture mode |
| `--trace-events` | R1 | off | Emit 4-phase trace JSONL to stdout |
| `--mock` | R1 | off | Shortcut for `--provider mock` |
| `--release-metrics` | R8 | off | Verify `drop_ratio <= 5%` (S40); exit non-zero if exceeded |
| `--ablation-mode=<full\|none>` | R8 | `none` | Emit `ablation_report.json` (R8.3) |
| `--failure-event-format=<v1\|v2>` | R8 | `v1` | Failure event schema |
| `--accept-contexts=<list>` | R13 | all | Comma-separated context_ids to run |

## 14 fixture catalog (3 SHIPPED + 11 test)

| Fixture | Type | Drives |
|---------|------|--------|
| `fixtures/contexts/code-class-context.jsonl` | SHIPPED | Task 11 e2e mock mode |
| `fixtures/contexts/research-class-context.jsonl` | SHIPPED | R2 research class |
| `fixtures/contexts/debug-class-context.jsonl` | SHIPPED | R3 debug class |
| `tests/fixtures/context_request/valid_3class_combined.jsonl` | test | R13.1.1 schema + multi-class |
| `tests/fixtures/context_request/valid_single_code.jsonl` | test | R13.1.1 valid single |
| `tests/fixtures/context_request/is_hidden_true.jsonl` | test | R13.4 hidden bucket |
| `tests/fixtures/context_request/anti_cheat_hint_input.jsonl` | test | R9.1 hint regex |
| `tests/fixtures/context_request/anti_cheat_metric_tampering.jsonl` | test | R9.2 prefix-rejection |
| `tests/fixtures/context_request/anti_cheat_network_keyword.jsonl` | test | R9.3 network keyword |
| `tests/fixtures/context_request/invalid_missing_context_id.jsonl` | test | S29 negative |
| `tests/fixtures/context_request/invalid_empty_turn_input.jsonl` | test | S30 negative |
| `tests/fixtures/context_request/invalid_task_class_enum.jsonl` | test | S31 negative |
| `tests/fixtures/context_request/invalid_invocation_mode.jsonl` | test | S32 negative |
| `tests/fixtures/context_request/r8_failure_fixtures.jsonl` | test | R8.3 ablation mechanism |

## 9 test binaries with `LABELS "l2-evolution"`

| Binary | Task | Cases | Coverage |
|--------|------|-------|----------|
| `test_hermetic_home` | Task 2 | 3 | P0'-4 hermetic HOME |
| `test_context_request_validation` | Task 3 | 12 | R13.1 schema + 9 LoadResult |
| `test_evolution_tracer_schema` | Task 4 | 6 | 4-phase trace + 12 meta |
| `test_evolution_session_mutation` | Task 5 | 4 | Case 1 mock mutation |
| `test_l2_event_emission` | T6.8a | 5 | 4 emit sites via bus |
| `test_anti_cheat_search_solution` | Task 6 | 3 | R9.1 hint regex |
| `test_anti_cheat_metric_tampering` | Task 6 | 3 | R9.2 prefix + IEvaluator |
| `test_anti_cheat_sandbox_escape` | Task 6 | 3 | R9.3 network keyword |
| `test_reverse_indicators` | Task 7 | 3 | R8 framework invariants |

Run: `cd build && ctest -L l2-evolution` (expect 9/9 PASS).
Exclude L2 from baseline run: `cd build && ctest -LE l2-evolution`.

## Architecture (Phase 0-6)

```
ContextRequest (JSONL) → load_context_file (with bus*)
  → EvolutionSession::set_contexts
  → run_6_phase_demo:
    Phase 0: load_contexts (already loaded)
    Phase 1: init (engine + bus wiring)
    Phase 2: baseline (ChatSession 11-param ctor)
    Phase 3: mutation (apply_harness_mutation 5-param)
    Phase 4: reload_rerun (ChatSession rebuild + load(genome@N))
    Phase 5: compare (IEvaluator V2 BehavioralEquivalence)
    Phase 6: emit_jsonl (4-phase trace to stdout)
```

Per R3 + P2-1:
- Trace event top-level (8 fields): `phase`, `timestamp_iso8601`, `session_id`,
  `turn_input`, `response`, `tokens`, `cost_usd`, `meta`
- Trace event meta (12 fields): `context_id`, `task_class`, `is_hidden`, `hidden_bucket`,
  `sensitivity`, `expected_eval_quality`, `trace_id`, `capture_mode`, `genome_version`,
  `gate_passes`, `eval_quality` (always null per P0-4 C5), `attribution_verdict`

## Hard-blocks (mandatory)

- **N1**: Zero diff on `examples/pdk_chat_demo/main.cpp` (verified via `git diff`).
- **N2**: Zero diff on `include/` (verified via `git diff`).
- **N5**: Contract layer / EventBuilder / Genome CRD 三层 freeze (zero modification).

## Deferred to follow-up (not L2 scope, see Wave 4+ plans)

- Wave 4: real sandbox network isolation (R9.3 true `network_mode=none`)
- Wave 3 Phase 2 D4-D7: LoRA + full eval + AgenticMind + serving
- S4: Agent-Agent co-evolution (research path)

## Related

- OpenSpec change: `openspec/changes/pdk-chat-demo-evolution-reference-example/`
- Plan: `.rddf/plans/pdk-chat-demo-evolution-reference-example.md` (923 lines, 12 tasks)
- Improvement: `.rddf/improvements/pdk-chat-demo-evolution-reference-example.md`
- SoT docs: `docs/architecture/{harness-architecture-2026-09, self-evolution-architecture-2026-08, rsi-architecture-2026-09}.md`
- ADR-0068 v2.4: `docs/adr/adr-0068-event-emission-contract.md` (Appendix A)
- AGENTS.md: `AGENTS.md` (Reverse Indicator Rule + pattern #11)