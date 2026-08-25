# Proposal: Sprint 24 Pre-Launch Self-Review (Single-Developer Mode)

## 1. Why (为什么做)

### 1.1 背景

- 2026-08-25 正式确立 Single-Developer Mode (AGENTS.md, commit `1955c5e`), 取代 "5 议程 + 4 文件 + 邮件/Slack" 议会式流程。
- 6 个 ADR 草案已就绪 (commit `d803158`): ADR-0083 / ADR-0080 v1.2 / ADR-0061-13 / ADR-0061-06 v1.1 / ADR-0071 / ADR-0074, 全部 🔍 Proposed。
- Sprint 24 启动前置:
  - 6 个 ADR 决议落地 (4 新 + 2 Promotion)
  - capability-application-map v1.3 更新
  - Sprint 24 kickoff issue
  - T17 SkillCompiler 启动准备
- `docs/architecture/sprint-24-pre-launch.md` (364 行) 为启动安排文档, 经 Metis 评审 (session `ses_fc8cdde58ffeNXOrZSr5BrsZQv`) 发现多处硬阻塞 (gh label/milestone 缺失、脚本 G14 正则 bug、sed 格式不匹配、状态镜像缺失、排期过载), 需修订后以 OpenSpec change 形式落地。

### 1.2 目标

- 以可执行、可验证、单一事实源的方式完成 6 个 ADR 自审 + capability-map v1.3 + Sprint 24 kickoff。
- 原 markdown 文档在本 change 创建后删除 (本 change 是其正式继任者, 防止双份引用漂移)。

## 2. What Changes (改变什么)

### 2.1 前置基建复核 (Phase 0, 已 ship)

- T14 行为回归套件 (ADR-0061-02): OpenSpec archive `2026-08-25-2026-08-24-adr-0061-02-behavioral-regression` (commit `93ebc8d`)
- T16 SLM 路由 (ADR-0061-04): OpenSpec archive `2026-08-25-2026-08-24-adr-0061-04-slm-routing` (commit `c5fb833`)
- 6 个 ADR 草案 + capability-map v1.2 + 3 个已存在资产 (2 GitHub Issue 模板 + self-review checklist) (commits `d803158` / `1955c5e`)

### 2.2 本 change 执行 (Phase 1)

- **Step 0 (NEW, 原计划缺失)**: 创建 gh labels + milestone "Sprint 24" 前置基建
- **Step 1**: 创建 6 个 per-ADR 定制 body 的 GitHub Issue (占位符 ADR-XXXX/GXX/TXX 全部替换; 决策点引用 `capability-application-map §8.4` + `resolution-draft-2026-08-25.md`)
- **Step 2**: 6 个 Issue Self-Review (≤20 min/issue, **决策 comment 推迟到 24h 冷却期结束后发布**; ADR-0083 的 ExecutionTrace v0 最小结构定义纳入 ADR 本文)
- **Step 3a (冷却期前)**: 修复 `scripts/apply-meeting-resolutions.py` (G14 正则兼容 🔓 Open + 支持 `--resolutions` 条件路径 + dry-run 匹配检查表 + 退出码 docstring 同步)
- **Step 3b**: 冷却期 (24h, 可缩短至 8h 并注明)
- **Step 3c (冷却期结束后)**: 6 个 issue 发布决策 comment + 本地台账 `adr-status-ledger-2026-08.md` 记录
- **Step 3d (冷却期结束后)**: per-ADR 定制 sed 翻转状态 + `apply-meeting-resolutions.py` 实跑 + capability-map v1.3 + 3 个状态镜像同步 (原子 commit, ≥9 commits)
- **Step 4**: 创建 Sprint 24 kickoff issue (body 用 Sprint 24 已填好的内容, 非占位模板; 挂 milestone "Sprint 24")
- **Step 5**: 已在 HEAD commit `b220222` ship — 本 change 仅核对 (见 tasks.md Phase 0)

### 2.3 Sprint 24 排期修正 (排期过载收敛)

- **原计划**: T17(2s) + ADR-0080 v1.2(0.5s) + ADR-0083(1s) + ADR-0071 v1.1(0.5s) ≈ 4 sprint-week 工作量塞进 2 周 Sprint 24
- **修正**: Step 5 kickoff 仅承诺 **T17 骨架 (1 sprint = Sprint 24 全预算)**; ADR-0071 v1.1 amendment + ADR-0080 v1.2 ship 移入 Sprint 25 启动周; T15/T19/T20/T21 按 §8.4 排期

### 2.4 删除

- `docs/architecture/sprint-24-pre-launch.md` (git rm, 由本 change 承担其职能)
- AGENTS.md "Sprint 24 Pre-Launch" 章节引用改指向本 change

## 3. Key Design (关键设计决策)

### 3.1 单通道 + 本地台账双保险

- GitHub Issue = 审查入口 (不变, AGENTS.md Single-Developer Mode 规定)
- 新增 `docs/architecture/adr-status-ledger-2026-08.md` 本地决策台账 (ADR / 决策 / 日期 / 备注)
- 断网/中断时: 本地台账记录 → 恢复后同步到 issue (避免全链停滞)
- 冷却期可缩短至 8h (睡一觉即可), 但须在 issue 注明

### 3.2 ADR-0083 ExecutionTrace 循环依赖显式化

- 在 ADR-0083 §决策 新增 "ExecutionTrace v0 最小结构" (trace_id, final_result: ToolResult, steps: vector<StepSummary>)
- 作为本 ADR 实施前置交付物, 消除 "实施时定义" 的悬空
- T15 集成时以 amendment 演进

### 3.3 状态镜像三同步 (防止事实源漂移)

- `docs/architecture/adr-implementation-status-gap-analysis.md` (ADR 状态唯一事实源, 同步 6 项状态)
- `docs/README.md` ADR 索引 (补 4 新条目 + 0071/0074 状态列更新)
- `python3 tools/adr_relationships.py` 重跑 → 生成 `docs/adr-management/relationships.md`

### 3.4 原子提交 (取代原计划"1 commit")

- 每个 ADR 状态翻转 1 commit (6 commits)
- capability-map v1.3 1 commit
- 脚本修复 1 commit
- 状态镜像 + 台账 1 commit
- 共 ≥9 commits (single-dev atomic commit doctrine)

### 3.5 失败路径

- 任一 ADR 被拒/延期: 该 ADR 不翻转 + 对应 Gap 保持 🔴 + cap-map 用 `--resolutions partial.yaml` 只闭合通过项
- 影响项在 kickoff issue 标注阻塞
- 本 change 记录结果后归档变更

## 4. Impact (影响)

| 影响面 | 说明 |
|---|---|
| 治理流程 | 议会式 → issue self-review + 冷却期; ADR 状态事实源保持单一 |
| ADR 状态 | 6 项 🔍 → ✅ (4 新 + 2 Promotion, 取决于自审结果) |
| capability-map | v1.2 → v1.3 (G10/G12/G13/G14/G15 状态 + §八 TD 命运) |
| 代码 | 0 (纯文档/流程); 脚本修复为 `apply-meeting-resolutions.py` 1 处正则 |
| 删除 | `sprint-24-pre-launch.md` (继任至本 change) |
| 反面影响 | 无 (不触碰 src/ 与已 ship 能力) |

## 5. Risks (风险)

| 风险 | 影响 | 缓解 |
|---|---|---|
| gh 不可用/断网 | 全流程停滞 | 本地台账离线记录 + 恢复后同步; Step 1/4 可后补 |
| sed/脚本静默失败 | 状态未翻转但无报错 | 每个 ADR 翻转后 grep 校验状态行; cap-map dry-run 输出 diff 人工确认 |
| 自审 rubber-stamp | 架构风险漏审 | 每个 issue 引用 Oracle 预审 session + resolution-draft §八 决议记录为决策证据 |
| 排期过载 | Sprint 24 滑期 | Step 5 砍至仅 T17 骨架 (1 sprint 预算); ADR-0071 v1.1 + ADR-0080 v1.2 ship 移 Sprint 25 |
| 任一 ADR 被拒 | Gap 未闭合 | §3.5 失败路径 + kickoff 阻塞标注 |
| ExecutionTrace 未定义 | ADR-0083 不可编译 | §3.2 最小版本纳入 ADR 决策本体 |

## 6. Success Criteria (验收标准)

1. `gh issue list --label adr-review` 返回 6 个 issue, body 无 `ADR-XXXX`/`GXX`/`TXX` 占位符残留 (`grep` 检出 = 0 行)
2. 6 个 issue 均含冷却期结束后的 ✅/❌/⏸ 决策 comment
3. `grep -m1 "状态"` 6 个 ADR 文件含目标状态 (或 ❌/⏸ 在台账注明)
4. `grep -c "✅ Closed"` capability-application-map 中 G10/G12/G13/G14/G15 闭合数 ≥ 自审通过数 (≥5, 含 G14, 验证脚本修复生效)
5. `docs/architecture/adr-implementation-status-gap-analysis.md` 含 6 个目标 ADR 的当前状态行
6. `gh api repos/chisuhua/HydraForge/milestones --jq '.[].title'` 含 `Sprint 24`; kickoff issue 挂载该 milestone
7. `git log --oneline -12` 显示 ≥8 个原子 commit (6 ADR + cap-map + 脚本/台账)
8. `ls docs/architecture/sprint-24-pre-launch.md` → No such file (已删除); `grep -rn "sprint-24-pre-launch\.md" --include="*.md" .` → 0 行 (无悬空引用)
9. `openspec validate 2026-08-25-sprint-24-pre-launch-self-review --strict` → EXIT 0
10. `tools/adr_lint.py` → 0 errors (64 ADR 基线 + 6 新状态合法)
11. `tools/docs_drift_audit.py` → 0 DRIFT (Scenario 7 校验 ADR↔状态镜像一致)
12. `cd build && ctest --output-on-failure` → 184/184 PASS (确认 0 代码回归)

## 7. Out of Scope (明确不做)

- 不新增 ADR (6 个既有草案是全部范围)
- 不写产品代码 (src/ 零改动; 脚本修复除外)
- 不引入委员会/法定人数/表决 (Single-Dev 约束)
- 不在本 change 内实施 T17/ADR-0071v1.1/ADR-0080v1.2 (移入 Sprint 24 启动周/Sprint 25 各自 change)
- 不修改 `resolution-draft-2026-08-25.md` (保留作 self-review 参考; Oracle 预审决策不需 change 化)
