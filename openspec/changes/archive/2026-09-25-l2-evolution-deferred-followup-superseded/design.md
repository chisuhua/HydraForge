# l2-evolution-deferred-follow-up — Design

> **Change Slug**: `l2-evolution-deferred-follow-up`
> **Status**: 🔍 Proposed Design

---

## D1: 7 Deferred Items Catalog

Per L2 `design.md` + `spec.md` + `README.md`, 7 items deferred:

| # | Item | L2 Reference | Current State | Required For | Priority |
|---|------|--------------|---------------|--------------|----------|
| **D1.1** | R9.3 true sandbox isolation | spec R9.3, design §695 | L2: turn_input keyword-rejection only | R9全量防御, 真sandbox evals | P0 (Wave 4) |
| **D1.2** | R8.1 full drop_ratio | spec S36, design §682 | L2: `--release-metrics` stub (drop_ratio=0%) | 反向指标门 R8.1 red-line | P0 |
| **D1.3** | R8.3 ablation 3-segment | spec S37-38, design §683 | L2: `--ablation-mode=full` stub | 消融实验 R8.3 | P1 |
| **D1.4** | R8.2 failure_event v2 | spec R8.2, design §D8 | L2: `--failure-event-format=v2` not wired | 失败可追溯 R8.2 | P1 |
| **D1.5** | workflow_patch V1 (Case 1.5) | design §D6, design §358 | `harness_rsi` returns UnsupportedVariant | Full harness-rsi 覆盖 | P1 (Wave 3 Ph2) |
| **D1.6** | ≥5 baseline samples statistics | design §372 | BehavioralEquivalence returns Insufficient/NotAttempted | 统计显著性断言 | P2 (Wave 3 Ph2 D5) |
| **D1.7** | S4 Agent-Agent co-evolution | design §D11, N4 | Not started (research path) | 全新方向 | P3 (research) |

## D2: Phase Strategy

Per user's Wave 3 cooling-off requirement (AGENTS.md Wave 3 SHIP 2026-09-23 → cooling off 24h → 2026-09-24T05:35Z expiry):

| Phase | Items | OpenSpec Change | Target Sprint |
|-------|-------|-----------------|---------------|
| **Phase 1 (Post-Wave-3)** | D1.2 + D1.3 + D1.4 (R8 full metrics) | `l2-evolution-r8-full-metrics` | Sprint 37 |
| **Phase 2 (Wave 4)** | D1.1 (sandbox isolation) | `l2-evolution-wave4-sandbox` | Wave 4 |
| **Phase 3 (Wave 3 Ph2 D4-D7)** | D1.5 + D1.6 (workflow_patch + stats) | `wave3-phase2-d4-to-d7` | Wave 3 Phase 2 |
| **Phase 4 (Research)** | D1.7 (S4) | N/A — research path | indefinite |

## D3: Hard-Block Constraints

- **H1**: 保持 N1 (zero diff on examples/pdk_chat_demo/main.cpp) — follow-up changes also maintain this
- **H2**: 保持 N2 (zero diff on include/ for pure L2 demo changes) — sandbox infra is the single exception
- **H3**: 保持 N5 (Contract/EventBuilder/Genome CRD freeze) — unless explicit ADR amendment
- **H4**: Wave 4 sandbox infra change requires ADR (new contract layer for Docker backend config)
- **H5**: All follow-up tests must use `LABELS "l2-evolution"` for exclusion from baseline ctest

## D4: R8 .1/.2/.3 Full Metrics Architecture

```
main.cpp
  ├── --release-metrics (P0):
  │     for each ContextRequest[i]:
  │       run Phase 2-5
  │       collect attribution_verdict from IEvaluator V2
  │       compute drop_ratio = (fail_count / total_count)
  │     if drop_ratio > 0.05 → exit non-zero
  │     write metrics.json { new_up, new_down, old_up, old_down, drop_ratio }
  │
  ├── --ablation-mode=full (P1):
  │     Segment 1: same-task-different-Harness
  │       → compare attribution_verdict distributions
  │     Segment 2: same-Harness-different-task
  │       → multi-class consistency matrix per R13.3
  │     Segment 3: failure-sample-retention-rate
  │       → using r8_failure_fixtures.jsonl, compute retention vs expected_eval_quality
  │     write ablation_report.json
  │
  └── --failure-event-format=v2 (P1):
        → full failure_event + rule_id + rule_shipped_commit +
          reproduce_in_new_task_demo + context_id chain
```

## D5: R9.3 True Sandbox Isolation

Current: `docker_backend.cpp:186` hardcodes `"NetworkMode", "bridge"`.
Fix: Add configuration point for `network_mode` — either env var or config struct field.

```
class DockerBackendConfig {
    // ... existing fields ...
    std::string network_mode = "bridge";  // NEW: default bridge for backward compat
};

// If network_mode == "none":
//   → container has no external network access
//   → sandbox_escape test goes from keyword-rejection to actual network isolation
//   → R9.3 anti-cheat coverage: CRITICAL (was degraded in L2)
```

**NOTE**: This change is BREAKING to the implicit contract of "containers always have network".
Requires ADR amendment + contract layer extension. Independent Wave 4 change.

## D6: Dependency Graph

```
l2-evolution-deferred-follow-up (this doc)
  ├── phase 1: l2-evolution-r8-full-metrics
  │     depends on: l2-evolution-reference-example (shipped ✅)
  │     depends on: IEvaluator V2 BehavioralEquivalence wiring
  │
  ├── phase 2: l2-evolution-wave4-sandbox
  │     depends on: ADR review for DockerBackendConfig
  │     depends on: l2-evolution-reference-example (fixtures + tests)
  │
  └── phase 3: wave3-phase2-d4-to-d7
        depends on: Wave 3 Phase 1 (shipped ✅)
        depends on: l2-evolution-reference-example (evolution_session integration)
```

## D7: Timeline / Dependencies (per Pre-Wave3 Plan)

| Change | Depends On | Sprint | Status |
|--------|-----------|--------|--------|
| L2 reference example (shipped) | Wave 3 G1/G3/G4 | S35-36 | ✅ shipped 2026-09-24 |
| R8 full metrics | IEvaluator V2 + IDistillationWriter wiring | S37+ | 📋 this doc |
| Wave 4 sandbox | ADR contract | Wave 4 | 📋 this doc |
| Wave 3 Phase 2 D4-D7 | Wave 3 cooling-off + Phase 1 | Wave 3 Ph2 | 📋 this doc |

## Cross-Doc References

| SoT 文档 | Deferred 事项引用 |
|---------|--------|
| [`design.md`](design.md) D1-D7 | 本设计 |
| [`docs/architecture/sandbox-isolation-wave4-blueprint.md`](../../docs/architecture/sandbox-isolation-wave4-blueprint.md) (to create) | Wave 4 sandbox ADR |
| [`docs/architecture/rsi-architecture-2026-09.md`](../../docs/architecture/rsi-architecture-2026-09.md) §11.8 | 反作弊 R9.3 红线 |
| [`AGENTS.md` Reverse Indicator Rule](../../AGENTS.md) | R8.1 drop_ratio > 5% block |