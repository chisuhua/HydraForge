# Proposal: Doc Alignment & ADR State Reconciliation (Sprint 11 P0)

> **变更类型**: 文档修复 + ADR 状态对齐
> **作者**: Sisyphus
> **创建日期**: 2026-06-26
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C0
> **关联 ADR**: ADR-0030 (V1 archive, V2 编写) / ADR-0032 (状态修正) / ADR-0019 §1.4 (状态同步)
> **前置**: 无 (Sprint 11 Day 0, 立即启动)
> **后续**: C1 (sprint-7-tech-debt-execution) 依赖本 change 同步 ADR-0019 §1.4

## Why

2026-06-26 Sprint 10 收官后审计发现 4 处 doc/ADR 状态不一致, 阻碍后续 Sprint 11-18 计划:

1. **ADR-0030 V1 archive 理由过时**: V1 (docs/archive/adr/adr-0030-async-runtime-dual-layer.md) 标注 ❌ Not Implemented 归档原因 = "Taskflow + async_simple 依赖未引入", 但 Slice 00 (docs/implementation-roadmap.md §Slice 00) 已 100% 完成, 依赖实际已引入. Phase 2 缺失一个有效 ADR.

2. **ADR-0032 状态错误**: docs/archive/adr/adr-0032-cost-collector.md 标注 ❌ Not Implemented 归档原因 = "由 BudgetController::CostTracker 替代", 但 docs/roadmap-status.md §五 显示 `test_cost_collector` ✅ 全通过 (2026-06-14). 实际有部分实现, 不应处于 archive 状态.

3. **implementation-roadmap.md §Phase 2 描述与实际 Slice 00 状态脱节**: 文档仍按 V1 假设描述 (Taskflow 未引入), 与实际已 ship 矛盾.

4. **AGENTS.md § Recent Changes 未提及 Sprint 10 收官**: 与 commit `1e3e110` (Sprint 10 close) 状态不一致.

不解决此问题: (a) Sprint 12 (Phase 2 实施) 无法启动, 因为 ADR-0030 V2 缺位; (b) ADR 状态词汇表 (`docs/adr-management/STATUS-GLOSSARY.md`) 与实际状态失同步; (c) docs-code-drift 审计 (openspec/specs/docs-code-drift-audit) 会重复报告.

## What Changes

### 1. 写 ADR-0030 V2 (替代 V1)

新建 `docs/adr/adr-0030-async-runtime-v2.md`:
- 状态: 🔍 **Proposed** (待 Sprint 12 实施后转 ✅ Approved)
- 基于 Slice 00 已 ship 的 Taskflow v4.0 + async_simple v1.4 实际状态
- 明确"双层架构"在 2026-06 当前架构下的取舍:
  - **保留双层**: Taskflow 处理 DAG 节点并行 (短时计算), async_simple 处理 LLM Token 流 + 用户审批 suspend (长生命周期协程)
  - **替代方案**: std::jthread + std::stop_token 替代 async_simple (Sprint 2/3 CognitiveWorker / DomainWorkerPool 已采用)
- Phase 2 范围: 并行 DAG executor + Fleet 模式 16 路并行 (Slice 04)
- 关联 ADR: ADR-0002 / ADR-0019 / ADR-0020 / ADR-0025

**V1 处置**: 保留在 `docs/archive/adr/`, 不删除 (可追溯), frontmatter 追加 "V1 SUPERSEDED by V2, 2026-06-26"

### 2. 修正 ADR-0032 状态

编辑 `docs/archive/adr/adr-0032-cost-collector.md`:
- 状态: ❌ Not Implemented → 🟡 **Partial** (test_cost_collector 实施, 但未集成到 BudgetController)
- 明确"Phase 3 收尾"工作 (在 C8 phase-4-5-mvp-cleanup 中处理)
- 或替代方案: `git mv` 移回 `docs/adr/adr-0032-cost-collector.md` 恢复 active 状态

**决策点** (Sprint 11 Day 1 启动时用 Oracle 咨询): 移回 vs 留 archive+修正 frontmatter

### 3. 同步 docs/implementation-roadmap.md

- §Phase 2 描述: 修正"Taskflow 依赖未引入" → "Taskflow v4.0 + async_simple v1.4 已 ship (Slice 00, 2026-06-07)"
- §Phase 2 范围: 调整 ADR-0030 V1 引用 → ADR-0030 V2 引用
- 附录 ADR 状态: 追加 ADR-0030 V2 状态行

### 4. 同步 docs/roadmap-status.md

- §一 Phase 2 状态: 0% 阻塞 → 0% 待启动 (Sprint 12)
- §四 实施日志: 追加 2026-06-26 行 (本 change 收官)
- §七 已知遗留: 删除"ADR-0030 依赖未引入"遗留 (已解决)

### 5. 同步 AGENTS.md

- § Recent Changes 顶部追加:
  ```
  - 2026-06-26 (Sprint 11 启动): OpenSpec change `2026-06-26-doc-alignment-adr-states` 创建, 修 ADR-0030 V2 + ADR-0032 状态 + 4 处 docs 同步
  ```

### 6. 验证

- `python3 tools/adr_lint.py docs/adr/ docs/archive/adr/ docs/adr/plugin/` exit 0
- `python3 tools/docs_drift_audit.py` 0 critical drift
- `openspec validate 2026-06-26-doc-alignment-adr-states` exit 0
- `git status` clean

## Capabilities

### Modified Capabilities

- `docs-code-drift-audit`: 新增 ADR 状态一致性检查 (ADR-0030 V2 引用 + ADR-0032 Partial 标记)

## Impact

**修改文件** (6 文件, ~30 lines 总变更):
- `docs/adr/adr-0030-async-runtime-v2.md` (新建, ~300 lines)
- `docs/archive/adr/adr-0030-async-runtime-dual-layer.md` (追加 V1 SUPERSEDED 标记, ~5 lines)
- `docs/archive/adr/adr-0032-cost-collector.md` (状态修正, ~10 lines)
- `docs/implementation-roadmap.md` (§Phase 2 + §Slice 00 描述, ~15 lines)
- `docs/roadmap-status.md` (§一 + §四 + §七, ~10 lines)
- `AGENTS.md` (Recent Changes 顶部, ~5 lines)

**API 稳定性**: 零影响 (纯文档变更)

**依赖变更**: 无

**测试影响**: 无 (不涉及代码)

## Non-goals

- **不改** ADR-0030 V1 主体内容 (保留可追溯)
- **不改** ADR-0032 实施位置 (test_cost_collector 已在, 不动)
- **不实施** Phase 2 (Sprint 12 范围)
- **不重写** ADR-0030 V2 详细 spec (本 change 只写 ADR 草案, 详细 design 在 C2 实施时完善)
- **不修复** 其他 ADR 状态 (如 ADR-0007 仍 🟡 Partial, 不在本 change 范围)

## Estimated Effort

- ADR-0030 V2 草案撰写: 0.3 天
- ADR-0032 状态修正: 0.1 天
- 4 个 docs 同步: 0.1 天
- 验证 (adr_lint + docs_drift_audit + openspec validate): 0.05 天
- 1 PR commit: 0.05 天

**总计**: 0.6 天 (1 个工作日)

**总提交数**: 1 PR with 6 commits (每个文件 1 commit + 1 final sync commit)
