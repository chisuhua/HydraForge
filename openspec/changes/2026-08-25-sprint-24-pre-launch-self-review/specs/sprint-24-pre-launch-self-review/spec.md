# Spec: Sprint 24 Pre-Launch Self-Review Workflow

> **Change**: `2026-08-25-sprint-24-pre-launch-self-review`
> **Status**: 🔍 Proposed (本 change 决议后进入 ✅ Approved → spec 落地)
> **关联**: AGENTS.md "Single-Developer Mode" 章节

---

## Purpose

在 Single-Developer Mode 治理范式下, 定义 Sprint 启动前的标准化 self-review 流程, 替代已废弃的议会式评审流程。

---

## ADDED Requirements

### Requirement: ADR Self-Review via GitHub Issue

The system MUST use GitHub Issue as the entry point for self-review of each ADR draft (🔍 Proposed) before transitioning to ✅ Approved status.

#### Scenario: Create Self-Review Issue

- **GIVEN** 一个 ADR 草案文件位于 `docs/adr/` 或 `docs/adr/skill/`
- **AND** ADR 状态字段为 `🔍 Proposed`
- **AND** GitHub repo 已有 `adr-review` 和 `oracle-p0` 标签
- **WHEN** solo-dev 创建 GitHub issue, title 含 `[ADR-XXXX] Self-Review:`, body 来自 per-ADR 定制文件
- **THEN** issue 进入 self-review 流程
- **AND** issue body 无 `ADR-XXXX`/`GXX`/`TXX` 占位符残留

#### Scenario: Self-Review Checklist Application

- **GIVEN** 一个 GitHub issue 已创建 (per "Create Self-Review Issue")
- **WHEN** solo-dev 跑 `docs/architecture/adr-self-review-checklist.md` (12 项通用 + 4 类专用)
- **AND** 核对 Oracle 预审决议 (ses_fcba5e477ffeG9wEBHVhU64J0o + ses_fc93a2994ffeyGbFyDJYtP1MhZ) 依据
- **THEN** solo-dev 在本地台账 `docs/architecture/adr-status-ledger-2026-08.md` 记录初步决策
- **AND** 决策 comment **不在** 冷却期结束前发布到 issue

#### Scenario: 24h Cooling-Off Period

- **GIVEN** 一个 issue 已完成 self-review (per "Self-Review Checklist Application")
- **WHEN** 冷却期结束 (默认 24h, 可缩短至 8h 并注明)
- **AND** 冷却期内无新增反对意见
- **THEN** solo-dev 在 issue 发布决策 comment (✅ Approved / ❌ / ⏸)
- **AND** 附风险接受声明 + 签发 `solo-dev`

#### Scenario: ADR Status Flip

- **GIVEN** 一个 issue 已发布决策 comment (per "24h Cooling-Off Period")
- **AND** 决策为 ✅ Approved
- **WHEN** solo-dev 跑 per-ADR 定制 sed 翻转状态行
- **THEN** `grep -m1 "状态" docs/adr/adr-XXXX-*.md` 返回 `✅ Approved`
- **AND** 每个 ADR 翻转后立即 grep 校验 (防 sed 静默失败)

#### Scenario: Capability-Map Auto-Update

- **GIVEN** 6 个 ADR 状态全部翻转为 ✅ Approved (per "ADR Status Flip")
- **WHEN** `python3 scripts/apply-meeting-resolutions.py` 实跑
- **THEN** capability-application-map §二 G10/G12/G13/G14/G15 状态变为 `✅ Closed` (含 G14, 验证脚本正则修复生效)
- **AND** §八 T14-T22 任务命运标注 `✅ APPROVED Sprint XX` 启动
- **AND** §七 变更记录新增 v1.3 行

#### Scenario: Status Mirror Three-Way Sync

- **GIVEN** capability-map v1.3 已生成 (per "Capability-Map Auto-Update")
- **WHEN** solo-dev 同步 3 个状态镜像:
  - `docs/architecture/adr-implementation-status-gap-analysis.md` (ADR 状态唯一事实源)
  - `docs/README.md` ADR 索引
  - `python3 tools/adr_relationships.py` 重跑
- **THEN** 3 个镜像文件均含 6 项目标 ADR 的当前状态行
- **AND** `tools/docs_drift_audit.py` Scenario 7 校验返回 0 DRIFT

#### Scenario: Sprint Kickoff Issue Creation

- **GIVEN** capability-map v1.3 + 状态镜像三同步完成 (per "Status Mirror Three-Way Sync")
- **AND** GitHub repo 已有 `Sprint 24` milestone
- **WHEN** solo-dev 创建 kickoff issue, body 来自已填好的 `sprint-24-kickoff.md` (非占位模板)
- **THEN** issue 挂载 `Sprint 24` milestone
- **AND** body 无 `Sprint XX` 占位符残留

---

### Requirement: GitHub Infrastructure Prerequisite

The system MUST require GitHub labels (adr-review, self-review, sprint-23, sprint-24, kickoff, phase-6-self-evolution, oracle-p0) and the Sprint 24 milestone to exist before self-review issues can be created.

#### Scenario: Create Required Labels

- **GIVEN** GitHub repo 标签列表不含 `adr-review`/`self-review`/`sprint-23`/`sprint-24`/`kickoff`/`phase-6-self-evolution`/`oracle-p0`
- **WHEN** solo-dev 跑 `gh label create <name> --force` × 7
- **THEN** `gh label list` 含全部 7 个目标标签

#### Scenario: Create Sprint 24 Milestone

- **GIVEN** GitHub repo milestone 列表不含 `Sprint 24`
- **WHEN** solo-dev 跑 `gh api .../milestones -f title="Sprint 24"`
- **THEN** `gh api .../milestones --jq '.[].title'` 含 `Sprint 24`
- **AND** kickoff issue 可成功挂载该 milestone (不报 unknown milestone 错误)

---

### Requirement: Apply-Meeting-Resolutions Script Hardening

`scripts/apply-meeting-resolutions.py` MUST support G14 (🔓 Open) regex matching + `--resolutions` conditional path + dry-run match-check table with non-zero exit code on any non-match.

#### Scenario: G14 Regex Compatibility

- **GIVEN** capability-map §二 G14 行格式为 `| **G14** | **...** | ... | **🔴 架构层** | **🔓 Open** | **...** |`
- **WHEN** `python3 scripts/apply-meeting-resolutions.py --dry-run` 跑 ALL_APPROVED_RESOLUTIONS
- **THEN** 输出包含 `[§二] G14 状态: 🔴 架构层 🔓 Open → ✅ Closed` (验证正则修复生效)
- **AND** 不静默 no-op (退出码非 0 或输出明示未匹配)

#### Scenario: Dry-Run Match Check Table

- **GIVEN** `apply-meeting-resolutions.py --dry-run` 跑 ALL_APPROVED_RESOLUTIONS
- **WHEN** 命令完成
- **THEN** 输出含"匹配检查表"列: G10/G12/G13/G14/G15 各自匹配状态 + T15/T17/T19/T20/T21 各自匹配状态
- **AND** 任一未匹配时退出码 = 2

---

### Requirement: Offline / Blocked Recovery

The system MUST support offline decision recording via local ledger (`docs/architecture/adr-status-ledger-2026-08.md`) when GitHub is unavailable, AND MUST allow cooling-off period to be shortened to 8h (with explicit annotation) when solo-dev opts out of the default 24h window.

#### Scenario: Local Decision Ledger

- **GIVEN** solo-dev 因断网/出差无法访问 GitHub
- **WHEN** solo-dev 在本地 `docs/architecture/adr-status-ledger-2026-08.md` 记录决策 (date + ADR + 决策 + 备注)
- **THEN** 决策信息不丢失
- **AND** 恢复后手动同步到 issue 评论

#### Scenario: Shorter Cooling-Off

- **GIVEN** solo-dev 选择缩短冷却期 (24h → 8h)
- **WHEN** solo-dev 在 issue body 或台账注明 "冷却期缩短至 8h (睡一觉即可)"
- **THEN** 冷却期按 8h 计时 (非默认 24h)
- **AND** 台账需注明缩短原因

---

## Out of Scope (明确不做)

- 不实施 T17 SkillCompiler 实施 (移入 Sprint 24 启动周各自 change)
- 不实施 ADR-0071 v1.1 amendment / ADR-0080 v1.2 ship (移入 Sprint 25)
- 不修改 `resolution-draft-2026-08-25.md` (保留作 self-review 参考)
- 不引入委员会/法定人数/表决 (Single-Dev 约束)

---

## Dependencies

### 前置
- AGENTS.md "Single-Developer Mode" 章节 (commit 1955c5e)
- capability-application-map v1.2 (commit d803158)
- 2 GitHub Issue 模板 + self-review checklist (commits d803158 / 1955c5e)
- 6 个 ADR 草案 (commit d803158)

### 被依赖
- T17 SkillCompiler 启动 (Sprint 24, 独立 change)
- T15 Trajectory IR 启动 (Sprint 25, 独立 change)
- T21 Prompt Evidence Gate 启动 (Sprint 25, 独立 change)
