# adr-0072-flip-to-partial Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补建 ADR-0072 翻牌 🟡 Partial 治理证据链 — 创建 OpenSpec change artifacts + GitHub issue + 12 项 Self-Review + ≥24h cooling-off + archive。

**Architecture:** 纯治理动作（zero 代码改动）。5 步结构：(1) 创建 artifacts → (2) 创建 issue + 12 项 Self-Review → (3) 等 24h cooling-off → (4) archive + iteration.json 同步 → (5) 收尾验证。

**Tech Stack:** GitHub CLI (`gh`) + openspec CLI + JSON manipulation + bash。

---

## File Structure

### Documentation Files (新建)

| File | Responsibility |
|---|---|
| `.rddf/improvements/adr-0072-flip-to-partial.md` | 改进提案（5 段格式） |
| `openspec/changes/adr-0072-flip-to-partial/proposal.md` | 翻牌决策记录 |
| `openspec/changes/adr-0072-flip-to-partial/design.md` | 设计文档（Context/Decisions/Risks） |
| `openspec/changes/adr-0072-flip-to-partial/specs/adr-0072-status-update/spec.md` | Spec delta（3 Requirements + 5 Scenarios） |
| `openspec/changes/adr-0072-flip-to-partial/tasks.md` | 任务清单（已完成） |
| `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/` | cooling-off 后 archive 目录 |
| `.rddf/plans/adr-0072-flip-to-partial.md` | 本 plan 文件 |

### State Files (修改)

| File | Responsibility |
|---|---|
| `.rddf/state/iteration.json` | +1 archived entry |
| `proposal-suggestions.md` | §3.4 标记治理补建完成（已完成） |
| `proposal-approved.md` | 收录本提案（已完成） |

### Code Files

**无代码改动**（纯治理动作）。

---

### Task 1: 创建 OpenSpec Change Artifacts

**Files:**
- Create: `openspec/changes/adr-0072-flip-to-partial/{proposal,design,tasks}.md`
- Create: `openspec/changes/adr-0072-flip-to-partial/specs/adr-0072-status-update/spec.md`

- [ ] **Step 1: 验证 artifacts 完整**

Run: `ls openspec/changes/adr-0072-flip-to-partial/`
Expected: 列出 `proposal.md design.md tasks.md specs/` 4 个条目

- [ ] **Step 2: 验证 openspec status**

Run: `openspec status --change "adr-0072-flip-to-partial" --json | python3 -c "import sys,json; d=json.load(sys.stdin); print('isComplete:', d['isComplete'])"`
Expected: `isComplete: True`

- [ ] **Step 3: 验证 openspec validate**

Run: `openspec validate "adr-0072-flip-to-partial" --strict`
Expected: exit 0（无 validation error）

---

### Task 2: 创建 GitHub Issue + Self-Review Checklist

**Files:**
- Create: GitHub issue（远程，通过 `gh` CLI）

- [ ] **Step 1: 检查 gh CLI 可用**

Run: `gh --version && gh auth status 2>&1 | head -5`
Expected: gh CLI 已安装 + 已认证（若未认证则提示用户 `gh auth login`）

- [ ] **Step 2: 创建 issue**

Run: `gh issue create --label "adr-review,governance" --title "[ADR Review] ADR-0072 🟡 Partial 翻牌治理补建 (2026-09-02)" --body-file openspec/changes/adr-0072-flip-to-partial/specs/adr-0072-status-update/spec.md`
Expected: 创建 issue + 返回 issue URL（保存到 .rddf/state/adr-0072-issue.txt）

- [ ] **Step 3: 勾选 Self-Review Checklist 12 项**

Run: 在 issue 评论中逐项勾选（per `docs/architecture/adr-self-review-checklist.md`）
- [ ] 2.3.1 状态翻转证据完整
- [ ] 2.3.2 跨文件状态一致性
- [ ] 2.3.3 计数口径一致
- [ ] 2.3.4 canonical source 声明引用
- [ ] 2.3.5 治理异常显式文档化
- [ ] 2.3.6 范围边界明确
- [ ] 2.3.7 风险识别完整
- [ ] 2.3.8 回退策略明确
- [ ] 2.3.9 与 roadmap.md 决策树一致
- [ ] 2.3.10 与 Metis 建议一致
- [ ] 2.3.11 archive 命名约定符合
- [ ] 2.3.12 零代码改动

Expected: 12 项评论全部留痕

---

### Task 3: 24h Cooling-Off

**Files:**
- Modify: `.rddf/state/adr-0072-issue.txt`（记录时间戳）

- [ ] **Step 1: 记录 issue 创建时间 T0**

Run: `date -u +"%Y-%m-%dT%H:%M:%S+00:00" | tee .rddf/state/adr-0072-issue.txt`
Expected: 当前 UTC 时间戳写入文件

- [ ] **Step 2: 等待 ≥24h**

说明：cooling-off 跨 Day 1-2，期间 #2/#5/#10/#3/#4 可并行启动（不阻塞）
- [ ] 2.3.1 记录等待开始时间
- [ ] 2.3.2 等待 86400s（≥24h）
- [ ] 2.3.3 记录等待结束时间

- [ ] **Step 3: 验证 cooling-off 满足**

Run: `T_archive=$(date -u +"%s"); T0=$(cat .rddf/state/adr-0072-issue.txt | xargs -I{} date -u -d "{}" +"%s"); echo "等待秒数: $((T_archive - T0))"`
Expected: 输出 ≥ 86400

---

### Task 4: OpenSpec Archive + iteration.json 同步

**Files:**
- Move: `openspec/changes/adr-0072-flip-to-partial/` → `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/`
- Modify: `.rddf/state/iteration.json`

- [ ] **Step 1: archive change 目录**

Run: `mkdir -p openspec/changes/archive && git mv openspec/changes/adr-0072-flip-to-partial openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial`
Expected: 目录成功移动

- [ ] **Step 2: 追加 archived entry 到 iteration.json**

Run:
```bash
python3 -c "
import json
from datetime import datetime, timezone
with open('.rddf/state/iteration.json', 'r') as f:
    data = json.load(f)
entry = {
    'added_at': '2026-09-08T09:00:00+00:00',
    'name': 'adr-0072-flip-to-partial',
    'status': 'archived',
    'priority': 'P0',
    'plan_path': '.rddf/plans/adr-0072-flip-to-partial.md',
    'tasks_total': 0,
    'worktree_path': None,
    'archived_at': datetime.now(timezone.utc).isoformat(),
    'filled_at': None
}
data['changes'].append(entry)
data['updated_at'] = datetime.now(timezone.utc).isoformat()
with open('.rddf/state/iteration.json', 'w') as f:
    json.dump(data, f, indent=2)
print('entry added')
"
```
Expected: `entry added`

- [ ] **Step 3: 验证 openspec validate 通过**

Run: `openspec validate archive --strict 2>&1 | head -10`
Expected: exit 0

---

### Task 5: 收尾验证

**Files:**
- Modify: `.rddf/state/adr-0072-issue.txt`（追加 issue 关闭时间）
- Verify: zero 代码改动

- [ ] **Step 1: 关闭 GitHub issue**

Run: `ISSUE_URL=$(cat .rddf/state/adr-0072-issue.txt | head -1); gh issue close "$ISSUE_URL" --comment "Self-Review 完整通过 + 24h cooling-off 完成 + archive 落地 per .rddf/plans/adr-0072-flip-to-partial.md"`
Expected: issue 状态 = Closed + 评论留痕

- [ ] **Step 2: 验证 proposal-approved.md 收录**

Run: `grep -c "adr-0072-flip-to-partial" proposal-approved.md`
Expected: ≥ 1

- [ ] **Step 3: 验证 proposal-suggestions.md §3.4 标记**

Run: `grep -A2 "3.4 ✅ GOVERNANCE COMPLETED" proposal-suggestions.md`
Expected: 标记存在 + 引用本 change

- [ ] **Step 4: 验证 zero 代码改动**

Run: `git diff --stat HEAD 2>&1 | head -20`
Expected: 仅含新建 .md 文件 + proposal-approved/proposal-suggestions 标记 + iteration.json +1 entry + plan 文件

- [ ] **Step 5: Oracle review**

说明：派发 Oracle subagent 验证 5 项检查（治理证据链 + spec delta + cross-file 一致性 + cooling-off 时间差 + zero 代码改动），期望 5/5 PASS

---

## Self-Review Checklist (Plan 自查)

- [x] Spec 覆盖：3 个 Requirements + 5 Scenarios 全部映射到 5 个 Task
- [x] 占位符扫描：无 "TBD"/"TODO"/"implement later"
- [x] 类型一致性：openspec/iteration.json/gh CLI 接口一致
- [x] 任务粒度：每步 2-5 分钟可执行
- [x] Header 完整：Goal/Architecture/Tech Stack + File Structure

---

## 风险与回退

| 风险 | 回退 |
|------|------|
| gh CLI 未认证 | `gh auth login` 后重试 Task 2 |
| cooling-off 时间不够 | 延长等待至 ≥24h（不能跳过） |
| archive 后 openspec validate 失败 | 修复 artifacts 后重新 archive |
| Oracle review 发现遗漏 | 修复后重新走 review 流程 |
