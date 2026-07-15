# Post-Ship Validation Baseline + Oracle Round 4 Brief

> **创建日期**: 2026-07-15 (C20-Spike ship day)
> **关联任务**: `openspec/changes/archive/2026-07-15-phase6-service-ification-v1/tasks.md` §12.1 + §12.5
> **状态**: §12.1 baseline ✅ · §12.5 input prepared ⏳ (Oracle session ~2026-08-04)

---

## §12.1: 7-Day Validation Baseline (re-run scheduled 2026-07-22)

### Day 0 Baseline Results (2026-07-15)

| Check | Result | Notes |
|-------|:------:|-------|
| **Debug ctest** | 77/77 (100%) | 72 baseline + 5 C20-Spike executables |
| **ASan ctest** | 76/77 (99%) | 1 pre-existing ASan failure (documented per ship gate §10.4) |
| **G3 handler line count** | 24 lines | ≤30 per §2.13+§4.10 |
| **new executables** | 5 | test_g3_knowledge_base, test_g1_coding_assistant, test_service_v1, test_g3_audit_fields, test_escalation_triggers |
| **core modifications** | 1 file | tool_coordinator.{h,cpp} (white-listed per ADR-0051 §Decision 5) |

### Re-run Instructions (2026-07-22)

```bash
cd build && cmake .. && cmake --build . -j$(nproc)
ctest --output-on-failure                    # expect 77/77 or 77+/77+
cd build/asan && ctest --output-on-failure   # expect 76/77 or 76+/77+
wc -l pdk/g3_knowledge_base/src/g3_query.cpp | awk '{if($1<=30) print "≤30 PASS"; else print ">30 FAIL"}'
```

### Regression Check Targets (7-day)

- C10 (Lazy ModuleState): ~19d post-ship (should have zero hotfix)
- C11 (Session Registry): ~18d post-ship
- C12 (YIELD/STREAM): ~18d post-ship
- C16 (ILLMProvider v2): post-ship regression
- C20-Spike (G1+G3 + ToolCoordinator): 7d post-ship

### Expected Outcome

| Scenario | Action |
|----------|--------|
| 77+/77+ PASS, zero new regressions | ✅ Validation passed. Close §12.1. |
| New regression for C20-Spike code | 🔴 Hotfix. Open follow-up change. |
| New regression for pre-existing code (C10-C16) | 🔴 Fix. Re-calculate stability window. |

---

## §12.5: Oracle Round 4 Input Brief (scheduled ~2026-08-04)

### Session Purpose

> Validate that internal C20-Spike evidence supports ADR-0050 §启动条件 #5 reinterpretation ("internal Spike evidence supports external agent/tool demand"). Required prerequisite for ADR-0052 proposal (§13.5) and Spike → Candidate B v1 promotion decision.

### Key Questions for Oracle

| # | Question | Evidence Package |
|---|----------|------------------|
| Q1 | Does G1→G3 in-process composition demonstrate a viable service pattern? | test_service_v1.cpp (4 E2E cases, all PASS) |
| Q2 | Do the 5 escalation triggers provide adequate runtime safety for multi-Agent composition? | test_escalation_triggers.cpp (6 cases, all PASS) |
| Q3 | Does the Layer 3 dual memo divergence justify DECLARE_SERVICE formalization? | layer3-comparison.md (6 consensus + 4 divergent findings) |
| Q4 | Are the awkward patterns surfaced by L1+L2+L3 sufficient to justify v2 IPC transport? | layer1-review-primary.md (15 findings) + layer1-review-reviewer.md (16 findings) |
| Q5 | What evidence threshold justifies Candidate B v1 promotion vs continuing Spike? | §13 promotion criteria (≥3 awkward patterns, ≥2 L1 categories, dual memo convergence) |
| Q6 | Is ADR-0050 §启动条件 #5 satisfied by internal evidence (re-interpretation)? | This session's primary deliverable |

### Evidence Package (docs to provide Oracle)

| Document | Lines | Content |
|----------|:-----:|---------|
| `docs/audits/2026-07-15-spike-ship-gate-verification.md` | ~200 | 11/11 ship gate items PASS |
| `docs/service-composition/layer3-comparison.md` | ~90 | Dual memo divergence analysis |
| `docs/service-composition/layer1-review-primary.md` | ~180 | 15 findings (6🔴/2🟠/5🟡/2🟢) |
| `docs/service-composition/layer1-review-reviewer.md` | ~190 | 16 findings (1🔴/4🟠/8🟡/3🟢) |
| `docs/adr/adr-0051-phase6-pdk-composition-spike.md` | ~300 | Full Spike scope + contract + invariants |

### Decision Tree

```
Oracle Round 4 输出:
├── Evidence SUFFICIENT for Candidate B v1
│   └── Create ADR-0052 (Candidate B v1 proposal)
│       ├── Section: Service Contract (MCP + OpenAI API)
│       ├── Section: G1+G3 findings → v1 design
│       └── Schedule: Sprint 25+ implementation
│
├── Evidence INSUFFICIENT for Candidate B v1
│   └── Continue Spike (phase6-service-ification-v2)
│       ├── Implement G2 Agentic Memory plugin
│       ├── Implement G4 Agentic Browser plugin
│       └── Re-evaluate after 2+ more composition pairs
│
└── Evidence supports ALTERNATIVE direction
    └── Re-evaluate ADR-0050 Candidate B vs other candidates
```

### Pre-Session Checklist (2026-08-04)

- [ ] 77+/77+ ctest regression (re-run on session day)
- [ ] ASan 76+/77+ regression
- [ ] ADR-0051 status remains ✅ Approved
- [ ] Awkward pattern count updated (any new findings since 7-15?)
- [ ] Stage Gate 2026-07-18 PASS confirmed
- [ ] Sprint 23 W2-W3 95/95 tasks confirmed

---

## 关联

| 文档 | 用途 |
|------|------|
| `docs/audits/2026-07-15-spike-ship-gate-verification.md` | Ship gate 11/11 PASS evidence |
| `docs/service-composition/layer3-comparison.md` | Oracle Q3-Q4 主要输入 |
| `docs/adr/adr-0051-phase6-pdk-composition-spike.md` | Spike 范围 + 合约权威源 |
| `docs/adr/adr-0050-phase6-strategic-evaluation.md` | Candidate B 战略目标 |

---

**最后更新**: 2026-07-15 (Sisyphus, C20-Spike ship day)
**下次动作**: 2026-07-22 (§12.1 7-day re-run) + 2026-08-04 (§12.5 Oracle round 4)
