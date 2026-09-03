## 1. 创建 OpenSpec Change Artifacts

- [ ] 1.1 创建 `openspec/changes/adr-0072-flip-to-partial/` 目录（已完成 — `openspec new change adr-0072-flip-to-partial`）
- [ ] 1.2 创建 `proposal.md`（已完成 — 翻牌决策记录 + 范围 + Non-goals）
- [ ] 1.3 创建 `design.md`（已完成 — Context + Goals/Non-Goals + Decisions + Risks + Migration）
- [ ] 1.4 创建 `specs/adr-0072-status-update/spec.md`（已完成 — 3 个 ADDED Requirements + 5 Scenarios）
- [ ] 1.5 创建 `tasks.md`（本文件）
- [ ] 1.6 创建 `.rddf/improvements/adr-0072-flip-to-partial.md`（已完成）

## 2. 治理证据链：GitHub Issue + Self-Review

- [ ] 2.1 创建 GitHub issue 用 `.github/ISSUE_TEMPLATE/adr-review.md` 模板，标题 `[ADR Review] ADR-0072 🟡 Partial 翻牌治理补建`
- [ ] 2.2 勾选 Self-Review Checklist 12 项（per `docs/architecture/adr-self-review-checklist.md`）：
  - [ ] 2.2.1 状态翻转证据完整（D3+D5 ship commit 引用）
  - [ ] 2.2.2 跨文件状态一致性（README + active-status + gap-analysis + relationships.md + agent-collab L1081）
  - [ ] 2.2.3 计数口径一致（ADR 总数 +1 🟡 Partial）
  - [ ] 2.2.4 canonical source 声明引用（per Q3）
  - [ ] 2.2.5 治理异常显式文档化（ADR-0072 footer §治理异常段）
  - [ ] 2.2.6 范围边界明确（Non-goals 5 项）
  - [ ] 2.2.7 风险识别完整（4 项 Risks + 缓解）
  - [ ] 2.2.8 回退策略明确（git revert 单 commit）
  - [ ] 2.2.9 与 roadmap.md 决策树一致（Q1 翻牌决议 + Oracle 5/5）
  - [ ] 2.2.10 与 Metis 建议一致（治理补建为 P0）
  - [ ] 2.2.11 archive 命名约定符合（2026-09-02-adr-0072-flip-to-partial）
  - [ ] 2.2.12 零代码改动（git diff 仅含新增 .md/.json 文件）
- [ ] 2.3 在 issue 评论留 Self-Review Checklist 勾选记录

## 3. 24h Cooling-Off

- [ ] 3.1 记录 issue 创建时间 `T0 = 2026-09-08 (周一) 09:00`
- [ ] 3.2 **等待 ≥24h**（不阻塞其他 change 并行：#2/#5/#10/#3/#4 可启动）
- [ ] 3.3 验证 `T_archive - T0 ≥ 86400s`

## 4. OpenSpec Archive

- [ ] 4.1 移动 `openspec/changes/adr-0072-flip-to-partial/` → `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/`
- [ ] 4.2 更新 `iteration.json` 新增 archived entry：
  ```json
  {
    "added_at": "2026-09-08T09:00:00+00:00",
    "name": "adr-0072-flip-to-partial",
    "status": "archived",
    "priority": "P0",
    "plan_path": ".rddf/plans/adr-0072-flip-to-partial.md",
    "tasks_total": 0,
    "worktree_path": null,
    "archived_at": "2026-09-09T09:30:00+00:00",
    "filled_at": null
  }
  ```
- [ ] 4.3 验证 `openspec validate --strict` exit 0

## 5. 同步收尾

- [ ] 5.1 关闭 GitHub issue（评论"Self-Review 完整通过 + 24h cooling-off 完成 + archive 落地"）
- [ ] 5.2 验证 `proposal-approved.md` 含本提案
- [ ] 5.3 验证 `proposal-suggestions.md` §3.4 标记本治理补建完成
- [ ] 5.4 Oracle review（5 项检查：治理证据链 + spec delta + cross-file 一致性 + cooling-off 时间差 + zero 代码改动）
- [ ] 5.5 验证 `git status` 显示 zero 代码改动（仅新增 .md/.json 文件 + proposal-suggestions/proposal-approved 标记）
