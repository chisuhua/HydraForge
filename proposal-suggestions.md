# 提案池（待架构讨论）

> arch 阶段输入。guide-arch Phase 5.5 逐个审查，批准后添加到 `proposal-approved.md`。
>
> **生命周期**: 提案从此文件创建 → 审查批准 → 移至 `proposal-approved.md` 等待实施 → 实施后归档。
> **自动清理**: 提案被批准或实施后，`sync_suggestions()` 会自动从本表移除该行（不再停留）。
> **手动审计**: 发现过期条目时，运行 `skill_use("guide")` 的审计功能自动清理。
> **格式**: 索引表（仅链接 + 元数据）。完整内容在 `improvements/<name>.md`。

## 0. 治理基线声明（2026-09-02 同步，per Q2a-1 + Q3 修订）

**真实状态**（per `roadmap.md` Q2a-1 + Oracle 5/5 审查通过 + `iteration.json`）：

| Phase | 状态 | 提案数 (iteration.json) | 提案池来源 |
|-------|------|:---:|---|
| Phase 6a | ✅ Shipped 2026-06-24 | (已 archived) | — |
| Phase 6b | ✅ Shipped 2026-07-08 | 1 archived (platform) | — |
| **Phase 6c** | **✅ Shipped 2026-09-02** | **8 archived** | — |
| Phase 7a | ⛔ 不启动（3/6 FAIL, 2026-09-02 决议） | 0 active | — |
| Phase 7b/7c/8 | ⏸ Gated by Phase 7a | 0 active | — |
| **Sprint 25+ carry-over** | 📋 6 项 (U2/U3/U4/U6/W4/W5) + baseline 重测 | 0 proposal | — |

**Dashboard stale 清理依据**: 本次清理（2026-09-02）同步 dashboard 输出与 `roadmap.md` 真实状态。

---

## 1. 待讨论提案

| 提案 | 优先级 | 来源 | 添加时间 | 状态 |
|------|--------|------|----------|------|
| _（空 — 14 个 stale 提案已在 §3 清理，0 项待讨论）_ | | | | |

**当前待讨论数**: 0 项

**Dashboard stale 同步**: rddf dashboard 仍显示 14 个"📋 待讨论"是因为 `.rddf/improvements/` 目录下文件未自动标记状态。本次清理通过：
1. `proposal-suggestions.md` §3 记录清理动作 + 依据；
2. 14 个 stale 提案文件头部加状态标记（SUPERSEDED / GATED / ALREADY-DECOMPOSED）；
3. 下次 `sync_suggestions()` 后 dashboard 应清零。

---

## 2. 已批准提案（`proposal-approved.md` 入口）

> 当前 35 total / 35 implemented（per dashboard 7b 区）。本节列出本轮清理涉及的批准 → 实施闭环：

| Phase 6c 提案 | 对应 ship change | 状态 |
|--------------|------------------|------|
| from-roadmap-phase-6c-control-plane-eval | (审计文档 + 决策树) | ✅ Implemented |
| from-roadmap-phase-6c-evidence-gate | Conditional 决议（mock 88.24%） | ✅ Implemented |
| from-roadmap-phase-6c-execution-baseline | (D1+D2+D3 ship) | ✅ Implemented |
| from-roadmap-phase-6c-execution-dsl | `from-roadmap-phase-6c-execution-dsl` archived + ADR-0072 D3+D5 ship | ✅ Implemented |
| from-roadmap-phase-6c-execution-envbackend | ADR-0075 D1+D2+D3+D5 ship | ✅ Implemented |
| from-roadmap-phase-6c-schema-complete | ADR-0073 D2+D3+D4 ship | ✅ Implemented |
| from-roadmap-phase-6c-validation-refinements | (Phase 6c 收尾验证) | ✅ Implemented |

**说明**: 8 个 Phase 6c 提案全部在 `iteration.json` 中 archived，0 个仍待批准或待实施。

---

## 3. 已清理（2026-09-02 cleanup）

> **清理日期**: 2026-09-02
> **清理依据**: `roadmap.md` Q2a-1（Phase 6c 2026-09-02 实质 ship 收官）+ Q2b（Phase 7a 不启动，3/6 FAIL）+ Q3（口径统一 + canonical source 声明）+ Oracle 5/5 审查通过。
> **清理范围**: 14 个 stale 提案 = 1 已分解 + 7 SUPERSEDED + 7 GATED
> **处理方式**: 不删除 `.rddf/improvements/<name>.md` 文件（保留作历史追溯），但加状态头标记 + 本表记录处置决策。

### 3.1 ✅ ALREADY-DECOMPOSED（已分解，2026-08-08 早期清理）

- ✅ **chat-async-io-steering** (P2) — 2026-08-08 DECOMPOSED → 7 个子 change ship 完成 (Phase 0 + A + B×5 + C)
  - Phase 0: [fix-tool-registry-signal-handler-shutdown](openspec/changes/archive/2026-08-08-fix-tool-registry-signal-handler-shutdown/) ✅ 2026-08-08
  - Phase A: [chat-async-io-queue-infra](openspec/changes/archive/2026-08-08-chat-async-io-queue-infra/) ✅ 2026-08-08
  - Phase B: [chat-async-io-cancellation-chain](openspec/changes/archive/2026-08-09-chat-async-io-cancellation-chain/) + step3/4/5 ✅ 2026-08-09
  - Phase C: [chat-async-io-model-switching](openspec/changes/archive/2026-08-09-chat-async-io-model-switching/) ✅ 2026-08-09
  - 原始 `.rddf/improvements/chat-async-io-steering.md` 保留作为设计意图记录（已标注 DECOMPOSED）
  - **本轮清理动作**: 头部加 `> status: ALREADY-DECOMPOSED (2026-08-08)` 标记，dashboard 显示归零

### 3.2 ⛔ SUPERSEDED by Phase 6c versions（7 个 — Phase 6c all ship + archived）

> **判定依据**: 每个 Phase 6b 提案都有对应的 Phase 6c 升级版，且 Phase 6c 提案全部在 `iteration.json` 中 archived（per dashboard 输出 + iteration.json 8/8 archived）。Phase 6b 提案因此**已无独立 ship 价值**，仅作历史设计意图保留。

| Phase 6b 提案 (stale) | 优先级 | Phase 6c 升级版 (superseder) | Phase 6c 状态 |
|----------------------|:------:|------------------------------|:------------:|
| `from-roadmap-phase-6b-evidence-gate` | P0 | `from-roadmap-phase-6c-evidence-gate` | ✅ Archived |
| `from-roadmap-phase-6b-execution-baseline` | P0 | `from-roadmap-phase-6c-execution-baseline` | ✅ Archived |
| `from-roadmap-phase-6b-execution-dsl` | P0 | `from-roadmap-phase-6c-execution-dsl` | ✅ Archived (D3+D5 ship per ADR-0072 翻牌 2026-09-02) |
| `from-roadmap-phase-6b-execution-envbackend` | P0 | `from-roadmap-phase-6c-execution-envbackend` | ✅ Archived (D1+D2+D3+D5 ship per ADR-0075 ✅ Approved 2026-08-18) |
| `from-roadmap-phase-6b-schema-complete` | P0 | `from-roadmap-phase-6c-schema-complete` | ✅ Archived (D2+D3+D4 ship per ADR-0073 ✅ Approved 2026-08-18) |
| `from-roadmap-phase-6b-governance-rituals` | P1 | _（无 Phase 6c 升级版，但已被 Sprint 22 ship 覆盖）_ | ✅ 实质 complete via [2026-07-10-phase5-sprint22-drift-strategic-gate](openspec/changes/archive/2026-07-10-phase5-sprint22-drift-strategic-gate/) |

**本轮清理动作**: 7 个文件头部加 `> status: SUPERSEDED by Phase 6c (2026-09-02)` 标记 + 指向 superseder 链接。

### 3.4 ✅ GOVERNANCE COMPLETED（2026-09-02 — Sprint 25 治理收口）

> **Sprint 25 启动**（per Oracle 优先级排序 + 修订后 roadmap.md 决策树）:
> - **#1** `adr-0072-flip-to-partial` (P0, 2h+24h cooling-off) — 补建翻牌 OpenSpec change，治理证据链重建
> - **#2** `control-plane-eval-c2-alignment` (P0, 3h) — C2 脚本与决策树对齐
> - **#3** `adr-0072-d4-backend-parser` (P0, 2h) — ADR-0072 D4 契约闭环
> - **#4** `adr-0072-d1-stream-true-parser` 阶段 A (P0, 2h) — ADR-0072 D1 字段层
> - **#5** `baseline-retest-wait-condition` (P1, 2h) — interrupt-driven 契约
> - **#6** `evidence-gate-conditional-banner` (P1, 1h) — 并入 #1
> - **#10** `adr-0042-state-alignment` (P2, 2h) — 顺手做

**本轮清理动作**: 创建 `2026-09-02-adr-0072-flip-to-partial` 完整 OpenSpec artifacts（proposal + design + tasks + specs）+ 创建 `.rddf/improvements/adr-0072-flip-to-partial.md` + 创建 GitHub issue + 24h cooling-off + archive。

### 3.5 ⏸ GATED by Phase 7a 不启动（7 个 — Phase 7a 3/6 FAIL 决议）

> **判定依据**: `roadmap.md` Q2b + `docs/active-status.md` §四 决议 — Phase 7a 控制平面评估 6 项条件 3 FAIL（C1/C2/C5），**Phase 7a 不启动**，结构性 FAIL 项无文档解药。Phase 7a 启动条件:
> - C1 (AgentForge ≥2 agents) FAIL — 当前 1 agent
> - C2 (Solo Dev ≥2 人 OR ≥80h/双周) FAIL — 当前 1 人 27h/周
> - C5 (Evidence Gate 真实 PASS) FAIL — 当前 mock 88.24% Conditional，非真实 PASS
>
> 所有 Phase 7 提案依赖 Phase 7a 启动 → **全部 GATED**，等待 Phase 7a 复评（Sprint 25+ carry-over）。

| Phase 7 提案 (gated) | 优先级 | 阻塞条件 | 复评时机 |
|---------------------|:------:|----------|----------|
| `from-roadmap-phase-7-control-stdio` | P0 | Phase 7a 启动 + ADR-0079 实施 | Sprint 25+ U4 完成后复评 |
| `from-roadmap-phase-7-control-token` | P0 | Phase 7a 启动 + 鉴权层落地 | 同上 |
| `from-roadmap-phase-7-control-prompts` | P0 | Phase 7a 启动 + MCP prompt schema | 同上 |
| `from-roadmap-phase-7-control-tools` | P0 | Phase 7a 启动 + MCP tools schema | 同上 |
| `from-roadmap-phase-7-control-client` | P1 | Phase 7b 启动（gated by 7a）+ 外部 MCP client | 7a 完成后才评估 7b |
| `from-roadmap-phase-7-control-resources` | P1 | Phase 7a 启动 + MCP resources schema | 同 7a |
| `from-roadmap-phase-7-control-http-sse` | P2 | Phase 7c descoped（per Phase 6c 评估） | 不评估，保留作历史 |

**本轮清理动作**: 7 个文件头部加 `> status: GATED by Phase 7a 不启动 (2026-09-02, 3/6 FAIL)` 标记 + 指向复评条件。

---

## 4. Dashboard stale 同步说明

### 4.1 当前 dashboard 输出（清理前）

```
7. Pending
   📋 14 pending proposal suggestion(s)
   NAME                           PRI    STATUS             PHASE
   chat-async-io-steering         P2     📋 待讨论              wave-3
   from-roadmap-phase-6b-evidence P0     📋 待讨论              phase-6b
   from-roadmap-phase-6b-executio P0     📋 待讨论              phase-6b
   from-roadmap-phase-6b-executio P0     📋 待讨论              phase-6b
   from-roadmap-phase-6b-executio P0     📋 待讨论              phase-6b
   from-roadmap-phase-6b-governan P1     📋 待讨论              phase-6b
   from-roadmap-phase-6b-schema-c P0     📋 待讨论              phase-6b
   from-roadmap-phase-7-control-c P1     📋 待讨论              phase-7
   from-roadmap-phase-7-control-h P2     📋 待讨论              phase-7
   from-roadmap-phase-7-control-p P0     📋 待讨论              phase-7
   from-roadmap-phase-7-control-r P1     📋 待讨论              phase-7
   from-roadmap-phase-7-control-s P0     📋 待讨论              phase-7
   from-roadmap-phase-7-control-t P0     📋 待讨论              phase-7
   from-roadmap-phase-7-control-t P0     📋 待讨论              phase-7
```

### 4.2 清理后预期 dashboard 输出（待 `sync_suggestions()` 重跑）

```
7. Pending
   （空 — 14 个 stale 提案已在 2026-09-02 cleanup 中分类处置，0 项待讨论）
```

### 4.3 同步动作清单（已执行）

| # | 文件 | 头部加标记 |
|---|------|-----------|
| 1 | `.rddf/improvements/chat-async-io-steering.md` | `> status: ALREADY-DECOMPOSED (2026-08-08, 7 子 change ship)` |
| 2 | `.rddf/improvements/from-roadmap-phase-6b-evidence-gate.md` | `> status: SUPERSEDED by from-roadmap-phase-6c-evidence-gate (2026-09-02)` |
| 3 | `.rddf/improvements/from-roadmap-phase-6b-execution-baseline.md` | `> status: SUPERSEDED by from-roadmap-phase-6c-execution-baseline (2026-09-02)` |
| 4 | `.rddf/improvements/from-roadmap-phase-6b-execution-dsl.md` | `> status: SUPERSEDED by from-roadmap-phase-6c-execution-dsl (2026-09-02)` |
| 5 | `.rddf/improvements/from-roadmap-phase-6b-execution-envbackend.md` | `> status: SUPERSEDED by from-roadmap-phase-6c-execution-envbackend (2026-09-02)` |
| 6 | `.rddf/improvements/from-roadmap-phase-6b-schema-complete.md` | `> status: SUPERSEDED by from-roadmap-phase-6c-schema-complete (2026-09-02)` |
| 7 | `.rddf/improvements/from-roadmap-phase-6b-governance-rituals.md` | `> status: SUPERSEDED by 2026-07-10-phase5-sprint22-drift-strategic-gate (2026-09-02)` |
| 8 | `.rddf/improvements/from-roadmap-phase-7-control-stdio.md` | `> status: GATED by Phase 7a 不启动 (2026-09-02, 3/6 FAIL)` |
| 9 | `.rddf/improvements/from-roadmap-phase-7-control-token.md` | `> status: GATED by Phase 7a 不启动 (2026-09-02, 3/6 FAIL)` |
| 10 | `.rddf/improvements/from-roadmap-phase-7-control-prompts.md` | `> status: GATED by Phase 7a 不启动 (2026-09-02, 3/6 FAIL)` |
| 11 | `.rddf/improvements/from-roadmap-phase-7-control-tools.md` | `> status: GATED by Phase 7a 不启动 (2026-09-02, 3/6 FAIL)` |
| 12 | `.rddf/improvements/from-roadmap-phase-7-control-client.md` | `> status: GATED by Phase 7a + 7b (2026-09-02)` |
| 13 | `.rddf/improvements/from-roadmap-phase-7-control-resources.md` | `> status: GATED by Phase 7a 不启动 (2026-09-02, 3/6 FAIL)` |
| 14 | `.rddf/improvements/from-roadmap-phase-7-control-http-sse.md` | `> status: GATED by Phase 7c descoped (2026-09-02)` |

**总计**: 14 文件头部加状态标记，0 文件删除（保留作历史追溯）。

---

## 5. 历史归档（pre-2026-09-02）

### 5.1 已归档（2026-08-12 cleanup）

- ✅ **chat-async-io-steering** (P2) — 2026-08-08 DECOMPOSED → 7 个子 change ship 完成 (Phase 0 + A + B×5 + C)
  - Phase 0: [fix-tool-registry-signal-handler-shutdown](openspec/changes/archive/2026-08-08-fix-tool-registry-signal-handler-shutdown/) ✅ 2026-08-08
  - Phase A: [chat-async-io-queue-infra](openspec/changes/archive/2026-08-08-chat-async-io-queue-infra/) ✅ 2026-08-08
  - Phase B: [chat-async-io-cancellation-chain](openspec/changes/archive/2026-08-09-chat-async-io-cancellation-chain/) + step3/4/5 ✅ 2026-08-09
  - Phase C: [chat-async-io-model-switching](openspec/changes/archive/2026-08-09-chat-async-io-model-switching/) ✅ 2026-08-09
  - 原始 `.rddf/improvements/chat-async-io-steering.md` 保留作为设计意图记录（已标注 DECOMPOSED）
