# from-roadmap-phase-6b-governance-rituals

> **status: SUPERSEDED by 2026-07-10-phase5-sprint22-drift-strategic-gate ship (2026-09-02)** — 2026-09-02 cleanup.
> 无 Phase 6c 升级版，但治理节奏实质 complete via [2026-07-10-phase5-sprint22-drift-strategic-gate](openspec/changes/archive/2026-07-10-phase5-sprint22-drift-strategic-gate/) ship + drift-gate.md template 已落地.
> 本文件保留作历史设计意图记录，详见 `proposal-suggestions.md` §3.2.

**优先级**: P1 | **来源**: from-roadmap (phase-6b/governance, ADR-0003 + ADR-0017 + 治理节奏)
**阶段**: phase-6b | **分类**: governance
**类型**: governance
**主题**: Sprint Review；Drift Gate；ADR与spec对齐

## 架构依据

ADR-0003（三阶段架构）+ ADR-0017（rddf-session）+ 主计划 `2026-06-26-sprint-11-to-18-roadmap.md` §9-§13 Review Gates：

- 治理节奏由 3 部分组成：(1) Sprint Review（每双周 Sprint 结束）+ (2) Drift Gate（每 Sprint 中段）+ (3) ADR↔spec↔code 三方对齐审计。
- 当前治理节奏零散（各 sprint 临时推动），缺统一脚本与文档模板。
- Phase 6b Sprint 22 已 ship `2026-07-10-phase5-sprint22-drift-strategic-gate` 作为模板。

## 范围

- **In Scope**:
  - `scripts/sprint-review.sh` 一键 Sprint Review 脚本（基线 ctest + ADR 状态变更 + OpenSpec 归档统计 + active-status 联动）。
  - `scripts/drift-gate.sh` 一键 Drift Gate 脚本（ADR↔code tools + ADR↔docs 跨文档 + active-status↔master plan + code↔docs）。
  - `docs/templates/sprint-review.md` 模板（含基线 + 变更列表 + ADR 状态 + 后续建议）。
  - `docs/templates/drift-gate.md` 模板（含 4 路检测 + 风险评级 + 修复建议）。
  - `scripts/adr-spec-alignment-audit.sh` 主动检查 ADR 决策与 specs/*.md 是否对齐。
  - 3 类测试：Sprint Review 脚本可执行 / Drift Gate 4 路检测 / ADR-spec 对齐报告生成。
- **Out of Scope**:
  - 自动修复脚本（决策权在 human）。
  - 跨仓库治理（Hub-Spoke 留独立 follow-up）。
  - 大型 ADR 决策树（仅审计，不决策）。

## 关键场景

- GIVEN Sprint 24 结束
  WHEN sprint-review.sh 执行
  THEN 输出：(1) ctest 152/152 PASS 基线 + (2) 21 个 OpenSpec changes archived + (3) ADR 状态变更列表 + (4) active-status.md 联动建议。

- GIVEN Sprint 24 中段（Day 5）
  WHEN drift-gate.sh 执行
  THEN 4 路检测结果：ADR↔code 0 CRITICAL / ADR↔docs 0 WARNING / active-status↔master plan 一致 / code↔docs 0 DRIFT。

- GIVEN 新增 ADR-0080
  WHEN adr-spec-alignment-audit.sh 执行
  THEN 报告 ADR-0080 §决策 是否在 specs/*.md 中有对应 spec delta（OPEN / MERGED / MISSING）。

## 技术约束

- MUST 3 个脚本独立可执行（无外部依赖，bash + python3）。
- MUST Sprint Review 模板与 active-status.md §一 基线引用联动。
- MUST Drift Gate 退出码语义化（0=pass, 1=drift warning, 2=critical drift）。
- MUST ADR-spec 对齐报告含 file:line 引用（可追溯）。
- MUST NOT 在治理脚本中引入 LLM 决策（仅审计）。
- SHOULD Sprint Review 报告 git-tracked（`docs/sprints/<date>-sprint-NN.md`）。

## 验收标准

- 3 个脚本可执行（sprint-review / drift-gate / adr-spec-alignment）。
- 2 个文档模板 git-tracked。
- Drift Gate 退出码语义化测试通过。
- ctest 全量零回归。
- 阻塞 Phase 7 启动评估 `governance` 项 ✅（6 项启动条件之一）。