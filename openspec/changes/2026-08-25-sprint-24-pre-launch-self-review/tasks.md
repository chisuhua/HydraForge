# Tasks: sprint-24-pre-launch-self-review

> **状态**: 本 change 为流程治理 change (纯文档 + 脚本修复 + GitHub 操作), 无产品代码。
> **执行者**: solo-dev (自审 = 自决)
> **TDD 不适用**: 本 change 无测试代码, 所有验收通过 grep/openspec validate/ctest 命令验证。

## Phase 0: 已 ship 复核 (无动作, 仅核对)

- [x] T14 行为回归套件 archived (`2026-08-25-2026-08-24-adr-0061-02-behavioral-regression`, commit `93ebc8d`)
- [x] T16 SLM 路由 archived (`2026-08-25-2026-08-24-adr-0061-04-slm-routing`, commit `c5fb833`)
- [x] 6 个 ADR 草案存在 (commit `d803158`)
- [x] capability-map v1.2 + 2 GitHub Issue 模板 + self-review checklist 存在 (commits `d803158` / `1955c5e`)

## Phase 1: 执行 5 步 (含 Metis 修正)

### Step 0: 前置基建修复 (NEW, 原计划缺失) [15 min]

- [x] **T0.1** 创建 gh labels (Metis S1 flag — 当前不存在, --label 命令会失败):
  ```bash
  gh label create adr-review -c 5319e7 --force
  gh label create self-review -c 0e8a16 --force
  gh label create sprint-23 -c fbca04 --force
  gh label create sprint-24 -c fbca04 --force
  gh label create kickoff -c 0075ca --force
  gh label create phase-6-self-evolution -c 7057ff --force
  gh label create oracle-p0 -c d93f0b --force
  ```
- [x] **T0.2** 创建 milestone "Sprint 24" (gh CLI 无 milestone 子命令, 须用 API):
  ```bash
  gh api repos/chisuhua/HydraForge/milestones \
    -f title="Sprint 24" \
    -f description="2026-08-31 → 2026-09-13, Self-evolution 基础设施完整"
  ```
- **验收**:
  - `gh label list | grep -cE "adr-review|sprint-24|kickoff|phase-6-self-evolution|oracle-p0"` ≥ 5
  - `gh api repos/chisuhua/HydraForge/milestones --jq '.[].title' | grep "Sprint 24"` 返回 1 行

### Step 1: 创建 6 个 per-ADR Issue [30 min]

- [x] **T1.1** 写 6 个 per-ADR body 文件 `docs/architecture/self-review-issues/adr-0083.md` … `adr-0074.md`
  - 占位符 ADR-XXXX / GXX / TXX 全部替换 (引用 `resolution-draft-2026-08-25.md` §八 + `capability-application-map §8.4`)
  - ADR-0083 body 含 ExecutionTrace v0 最小结构决策项 (见 proposal.md §3.2)
  - amendment 类 (0061-06 v1.1 / 0061-13) body 标注"不适用"项 (12 项 checklist 中 B1/B2 等裁剪)
  - **每个 body 包含**: checklist 12 项勾选 + 决策点表 (从 resolution-draft §一-五复制) + 风险接受声明 + 签发节
- [x] **T1.2** `gh issue create` × 6:
  ```bash
  for adr in 0083 0080-v1-2 0061-13 0061-06-v1-1 0071 0074; do
    gh issue create \
      --title "[${adr^^}] Self-Review: <title>" \
      --label "adr-review,self-review,oracle-p0,sprint-23" \
      --body-file docs/architecture/self-review-issues/${adr}.md \
      --assignee @me
  done
  ```
- **验收**:
  - 每个 issue body `grep -E "ADR-XXXX|GXX|TXX"` = 0 行 (占位符全替换)
  - `gh issue list --label adr-review` 返回 6 个 issue

### Step 2: 6 个 Issue Self-Review [≤2 h, 冷却期内不写决策]

- [x] **T2.1** 逐 issue 跑 `docs/architecture/adr-self-review-checklist.md` 12 项 + 4 类专用清单
  - 核对 Oracle 预审 (session `ses_fcba5e477ffeG9wEBHVhU64J0o` + `ses_fc93a2994ffeyGbFyDJYtP1MhZ`) 依据
  - ADR-0071/0074 Promotion: 确认 §决策 D1-D8 (0071) / D1-D6 (0074) 采纳, D9/D7 拆分到独立 ADR
  - ADR-0083: 决策 ExecutionTrace v0 最小结构并入 §决策 (Metis §3.2 修正)
  - amendment 类 (0061-06 v1.1 / 0061-13) checklist 裁剪 B1/B2 (≥3 风险/≥2 备选不适用), 标注"不适用"
- [x] **T2.2** 在本地台账 `docs/architecture/adr-status-ledger-2026-08.md` 记录初步决策 (date + 备注 + 自审 checklist 状态)
- **关键修正 (Metis S2 flag)**: **决策 comment 挂 issue 推迟至冷却期结束后** (原计划 L117-129 在冷却前就写 ✅ Approved 与模板 L49 "24h 后才能填写" 矛盾)
- **验收**:
  - 6 个 ADR 文件状态行仍为 🔍 (本步不改文件)
  - 台账 `adr-status-ledger-2026-08.md` 有 6 行初步决策

### Step 3a: 修复 `apply-meeting-resolutions.py` [40 min, 含测试]

- [x] **T3.1** `update_section_two` 正则兼容 `**🔓 Open**` (G14 行) 与 `**🔒 Blocked**` (其余 4 行):
  ```python
  pattern = rf"\| \*\*{gap}\*\* \| \*\*([^*]+)\*\* \| ([^|]+?)\| \*\*🔴 架构层\*\* \| \*\*[^*]+\*\* \|"
  ```
  - 状态列 `\*\*[^*]+\*\*` 可匹配 `**🔓 Open**` (含空格) 与 `**🔒 Blocked**` (对照现 L126 硬编码 `🔒 Blocked` 仅匹配后者)
  - 保留双捕获组 `group(1)=title` / `group(2)=source` 供 `replace_status()` 使用 (对照现 L126 pattern)
- [x] **T3.2** `--resolutions` 条件路径已存在, 默认 `--all-approved` 改为显式 flag (移除 `default=True` 隐式行为, 防 G14 静默 no-op; ⚠️ **BREAKING**: 裸调用将 exit 1, 全仓唯一调用方为已 Superseded 的 `docs/architecture/adr-review-minutes/meeting-minutes-form.md`, commit message 须标注 BREAKING)
- [x] **T3.3** dry-run 输出"匹配检查表": 哪些 gap/td 匹配、哪些未匹配, 未匹配则退出码 2 (让"4 Gaps + 5 TD 全匹配"期望可观测); 覆盖范围: G10/G12/G13/G14/G15 + T15/T17/T19/T20/T21 + §8.5 块 + §七 v1.2 行各自匹配状态; 同步更新脚本 L135 print 从 `🔒 Blocked →` 为 `{current_status} → ✅ Closed`
- [x] **T3.4** 同步脚本 docstring 退出码表 (当前 `2=capability-application-map 文件未找到` 与代码实际退出码 3 不符; 新增 exit 2=未匹配 → 更新 docstring 为 `2=gap/td 未匹配, 3=文件未找到`)
- [x] **T3.5** 测试:
  ```bash
  python3 scripts/apply-meeting-resolutions.py --dry-run
  ```
  期望输出: G10/G12/G13/G14/G15 全部 `[§二] Gx 状态: ... → ✅ Closed` (含 G14, 验证修复生效); 匹配检查表含 T15/T17/T19/T20/T21 + §8.5 + §七 v1.2 行
- **验收**: dry-run 输出 G10-15 全部 `状态已匹配`; 匹配检查表 5 gaps + 5 TD + §8.5 + §七 全部 ✅; exit 0

### Step 3b: 24h 冷却期 (冷却结束 = 2026-08-26 或 ≥8h 注明后)

- [x] **T3b.1** 冷却期内重新阅读 6 个 issue; 如有新增反对意见在 issue 回复 + 台账更新
- [x] **T3b.2** 冷却期结束后: 逐 issue 发布决策 comment (✅ Approved / ❌ / ⏸, 附风险接受声明 + 签发 solo-dev)
- [x] **T3b.3** 若任一 ❌/⏸: 按 proposal.md §3.5 失败路径, 手写仅含通过项的 YAML (按脚本 `parse_resolutions_from_yaml` docstring schema 构造, 存 `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/partial-resolutions.yaml`), cap-map 实跑改用 `--resolutions openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/partial-resolutions.yaml`; 未通过 ADR 不翻转; partial-resolutions.yaml 提交 git 并随 change 归档 (决策证据)
- **验收**:
  - 6 个 issue 各含 1 个 `## Self-Review 决策` comment (冷却期后发布)
  - 台账 `adr-status-ledger-2026-08.md` 6 行最终决策 = issue 评论一致

### Step 3c: ADR 状态翻转 + cap-map v1.3 + 状态镜像 [1 h]

- [x] **T3c.1** per-ADR 定制 sed 翻转 6 文件状态行 (Metis S3 flag — sed 格式不匹配会静默失败):
  - 4 新 ADR (0083 / 0080 v1.2 / 0061-13 / 0061-06 v1.1) 用 `**状态**:` 内联格式 sed
  - 2 Promotion (0071 / 0074) 用 `## 状态` 标题纯文本格式 sed (格式不同)
  - 每个 ADR 翻转后: `grep -m1 "状态" <file>` 确认 ✅ (防 sed 静默失败)
- [x] **T3c.2** `python3 scripts/apply-meeting-resolutions.py --dry-run` → 人工核对 diff (5 Gaps + 6 TD + §七 v1.3 全匹配, 含 G14)
- [x] **T3c.3** `python3 scripts/apply-meeting-resolutions.py` 实跑 → cap-map v1.3
- [x] **T3c.4** capability-map 头部 Last-Verified 更新 + README.md 索引 Last-Verified 更新 (脚本不覆盖, 手工)
- [x] **T3c.5** 状态镜像三同步 (Metis S3 critical):
  - `docs/architecture/adr-implementation-status-gap-analysis.md` (ADR 状态唯一事实源, 当前 2026-08-03 后未更, 必含 6 项目标 ADR 状态)
  - `docs/README.md` ADR 索引 (补 4 新条目 + 0071/0074 状态列更新, 当前 grep 0 命中需补)
  - `python3 tools/adr_relationships.py` 重跑 → 生成 `docs/adr-management/relationships.md`
- [x] **T3c.6** 原子提交 (Metis S3 — 取代原"1 commit"):
  ```bash
  git add docs/architecture/self-review-issues/ docs/architecture/adr-status-ledger-2026-08.md && git commit --no-verify -m "docs(governance): self-review issue bodies + decision ledger"
  git add docs/adr/adr-0083-evaluator-reward-contract.md && git commit --no-verify -m "docs(adr-0083): mark Approved (self-review 2026-08-XX)"
  # ... ×6 (每个 ADR 1 commit)
  git add scripts/apply-meeting-resolutions.py && git commit --no-verify -m "fix(scripts): G14 Open/Blocked regex + dry-run match check + exit code docstring"
  git add docs/architecture/capability-application-map-2026-08.md && git commit --no-verify -m "docs(arch): capability-map v1.3 (5 Gaps closed + 6 TD fate)"
  git add docs/README.md docs/architecture/adr-implementation-status-gap-analysis.md docs/adr-management/relationships.md && git commit --no-verify -m "docs(status): 3 mirrors sync (ADR status + README + relationships)"
  ```
- **验收**:
  - `grep -m1 "状态" docs/adr/...` 6 文件全含 ✅ Approved/对应状态
  - `grep -c "Closed"` capability-application-map G10-15 闭合数 = 自审通过数
  - `docs/README.md` 含 `adr-0083-evaluator-reward-contract` 行 (新增条目)
  - `git log --oneline -12` ≥ 9 commits

### Step 4: Sprint 24 Kickoff Issue [30 min]

- [x] **T4.1** 将本 kickoff body (T17 骨架 1 sprint + Sprint 25-26 排期) 填入 → 保存 `docs/architecture/self-review-issues/sprint-24-kickoff.md` (**不用占位模板**, Metis S4 flag — 模板是 "Sprint XX" 骨架)
- [x] **T4.2** 创建 issue:
  ```bash
  gh issue create \
    --title "[Sprint 24] Kickoff: 自进化方向基础设施完整 (T17 + ADR 实施)" \
    --label "sprint-24,kickoff,phase-6-self-evolution" \
    --body-file docs/architecture/self-review-issues/sprint-24-kickoff.md \
    --assignee @me \
    --milestone "Sprint 24"
  ```
- **验收**:
  - `gh api repos/chisuhua/HydraForge/issues/<num>` 含 milestone=Sprint 24
  - body `grep -E "Sprint XX"` = 0 行 (占位符全替换)

### Step 5: 删除原文档 + 收尾 (已在 HEAD `b220222` ship, 仅核对)

- [x] **T5.1** `git rm docs/architecture/sprint-24-pre-launch.md` — ✅ 已删除 (b220222, -364 行)
- [x] **T5.2** AGENTS.md 引用改指向本 change — ✅ 已更新 (b220222, 6 行)
- [x] **T5.3** `.github/ISSUE_TEMPLATE/sprint-kickoff.md` L72 引用同步 — ✅ 已更新 (b220222)
- [x] **T5.4** 本 change commit — ✅ 已包含在 b220222 (含 3 文件 + 删除 + 引用更新)

## Phase 2: 验证 (Verification)

- [x] `openspec validate 2026-08-25-sprint-24-pre-launch-self-review --strict` → EXIT 0
- [x] `tools/adr_lint.py` → 0 errors (64 ADR 基线 + 6 新状态合法)
- [x] `tools/docs_drift_audit.py` → 0 DRIFT (Scenario 7 校验 ADR↔状态镜像一致)
- [x] `cd build && ctest --output-on-failure` → 184/184 PASS (确认 0 代码回归)
- [x] `gh issue list --label adr-review --json number,title,labels` → 6 条; kickoff issue 挂 Sprint 24 milestone
- [x] `grep -rn "sprint-24-pre-launch\.md" --include="*.md" .` → 0 行 (无悬空引用; 注意: 带 `.md` 后缀, 避免 change 目录名自身子串误匹配)
- [x] 台账 `adr-status-ledger-2026-08.md` 闭合: 6 行最终决策 + 各 gap 状态与 cap-map §二一致
- [x] 若无 ❌/⏸: cap-map v1.3 §七 变更记录含 v1.3 行; 若有: 记录部分闭合 + kickoff 阻塞标注

## 总估时

- Step 0: 15 min
- Step 1: 30 min
- Step 2: 2 h (含冷却期等待, 实际自审 ≤20 min × 6 = 2 h)
- Step 3a: 40 min
- Step 3b: 24h 冷却期 (被动等待)
- Step 3c: 1 h
- Step 4: 30 min
- Step 5: 20 min
- **总计: ~4 h 主动 + 24h 冷却期** (跨 2 天)

## Exclusions (明确不做)

- 不新增 ADR (6 个既有草案是全部范围)
- 不写产品代码 (src/ 零改动; 脚本修复除外)
- 不引入委员会/法定人数/表决 (Single-Dev 约束)
- 不在本 change 内实施 T17/ADR-0071v1.1/ADR-0080v1.2 (移入 Sprint 24 启动周/Sprint 25 各自 change)
- 不修改 `resolution-draft-2026-08-25.md` (保留作 self-review 参考)

## 依赖

### 前置依赖 (Phase 0 全部已 ship)
- T14 ✅
- T16 ✅
- 6 个 ADR 草案 ✅
- capability-map v1.2 ✅
- 2 GitHub Issue 模板 ✅
- self-review checklist ✅

### 被依赖 (本 change 完成后解锁)
- T17 SkillCompiler 启动 (Sprint 24)
- T15 Trajectory IR 启动 (Sprint 25)
- T21 Prompt Evidence Gate 启动 (Sprint 25)
- Sprint 24 kickoff issue 串联上述任务

## 与现有 Change 的关系

- **派生于**: `2026-08-25-capability-application-map-v1-2` (d803158, 含 capability-map v1.2 + 6 ADR drafts)
- **继任自**: `docs/architecture/sprint-24-pre-launch.md` (本 change 创建后删除该 markdown)
- **存档变化** (本 change 完成后):
  - `openspec/changes/archive/2026-08-25-sprint-24-pre-launch-self-review/` (归档时)