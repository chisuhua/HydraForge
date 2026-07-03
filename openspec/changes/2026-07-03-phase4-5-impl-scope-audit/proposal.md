# Proposal: Phase 4.5 — Implementation Scope Audit (C9)

> **STATUS: ACTIVE** 🟡
> **前置依赖**: C8 (Phase 4.5 MVP 清理) ship ✅ (2026-07-03)
> **触发条件**: Strategic Alignment Gate (`docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §9.4) — Sprint 18 收官后准备 Phase 5 启动
> **关联 ADR**: ADR-0001/0003/0004/0005/0007/0008/0019/0020/0022/0023/0033 (11 个 ADR 全部需要 impl-scope audit)
> **关联工具**: `tools/docs_drift_audit.py` (11 DRIFT items) + `docs/adr-management/STATUS-GLOSSARY.md`
> **最后更新**: 2026-07-03

## Why

Sprint 18 收官 = Phase 4.5 100% 后, Strategic Alignment Gate 触发,
需要评估 Phase 5 自举服务化的启动条件。

`tools/docs_drift_audit.py` 在 2026-07-03 报告 **11 个 DRIFT items** —
ADR 文档描述了 N 个类/接口, 但实际代码 grep 只找到 N-K 个 (K 个缺失)。
这意味着 **Phase 4.5 → Phase 5 过渡前, 必须明确"哪些 ADR 的哪些类已经实施,
哪些是远期/已废弃/被演进替代"**。

不解决:
- (a) Phase 5 启动时无法知道基础设施的"真实基线"
- (b) ADR 状态标记 (✅ Approved / 🟡 Partial) 与实际代码不符
- (c) 实施 Phase 5 任务 (如 ADR-0005 的 OpenAI Creator) 时可能重复造轮子
- (d) ADR 文档债累积, 后续 ADR 偏离更难追踪

## What Changes

### 1. 11 项 ADR drift 修正 (核心工作)

为每个 DRIFT 报告的 ADR 创建一个 **impl-scope audit** 子文档 (`*-impl-scope.md`),
把"ADR 原始描述的类"按以下三类重新归类:

| 类别 | 含义 | ADR 状态调整 |
|------|------|------------|
| **Shipped (✅)** | 已实施, 但 grep 因命名/位置/namespace 差异未找到 | 状态保持 ✅ Approved, audit 文档说明实际位置 |
| **Evolved (🔁)** | 被演进形式替代 (如 `Creator` 类被函数式注册替代) | 状态保持 ✅ Approved, audit 文档说明演进路径 |
| **Deferred (📅)** | 真正未实施, 留待未来 (Phase 5+) | 状态改 🟡 Partial (或保留 ❌), audit 文档说明推迟理由 |

### 2. ADR 状态校准

对每个 audit 后的 ADR:
- 如果所有"声称实现"的类都已 Shipped/Evolved → 状态保持 ✅ Approved
- 如果有 ≥1 个 Deferred 且为关键路径 → 状态改 🟡 Partial, 记录在 README 表格
- 如果所有都 Deferred → 状态改 ❌ Not Implemented 或 ⛔ Superseded

### 3. 文档同步

- 更新 `docs/README.md` ADR 状态表格 (与 impl-scope audit 一致)
- 更新 `docs/adr-management/relationships.md` (重跑 `tools/adr_relationships.py`)
- 在 `docs/roadmap-status.md` §一 添加 Phase 4.5 → Phase 5 过渡说明
- (可选) 添加 ADR audit 结果到 `docs/adr-management/relationships.md` §六

## Capabilities

### ADDED Requirements

- `phase4-5-adr-audit-complete`: 11 个 ADR 全部创建 `*-impl-scope.md` 子文档
- `phase4-5-drift-resolved`: `tools/docs_drift_audit.py` 报告 0 DRIFT items
- `phase4-5-adr-status-aligned`: `docs/README.md` 表格与 impl-scope audit 一致
- `phase4-5-roadmap-synced`: `docs/roadmap-status.md` Phase 4.5 → Phase 5 过渡记录追加

## Impact

**修改文件** (预期):
- 11 个新文件: `docs/adr/*-impl-scope.md` (每个 ADR 1 个 audit)
- 3 个修改: `docs/README.md`, `docs/roadmap-status.md`, `docs/adr-management/relationships.md`
- 0 个源码文件修改 (本次纯 ADR 文档修正)

**API 兼容性**: 零 breaking change (纯文档)

## Non-goals

- **不修订** ADR 主文档 (`adr-XXXX-*.md`) 的决策内容
- **不实施** 任何缺失的类 (那是 Phase 5 工作)
- **不创建** 新 ADR (本 change 是 audit 现有 ADR)
- **不删除** 任何 ADR (即使发现已废弃)

## 验证标准

- [ ] `python3 tools/docs_drift_audit.py` 输出 `0 DRIFT items` (原本 11)
- [ ] `python3 tools/adr_lint.py` exit 0
- [ ] `python3 tools/adr_relationships.py` 成功生成 `docs/adr-management/relationships.md`
- [ ] `git log --oneline | head -10` 显示所有 commit 已 push
- [ ] `openspec validate 2026-07-03-phase4-5-impl-scope-audit` exit 0

## 关联 change

- **前置**: C8 `2026-06-26-phase-4-5-mvp-cleanup` (ship 2026-07-03)
- **后续 (依赖本 change)**: 新 master plan `2026-07-XX-phase5-self-bootstrapping.md`
- **后续 (依赖 master plan)**: Phase 5 阶段 1 详细切分
