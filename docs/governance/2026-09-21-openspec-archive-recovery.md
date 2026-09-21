# OpenSpec Archive Recovery — ADR-0086 v1.1 Change (2026-09-21)

> **事件**: commit `798b6c6` (2026-09-20) 自报"Archive integrity: 4 files ✓",实际仅完成 4 deletions (无 `R`/`A` 到 archive/),触发 AGENTS.md Day-5 lesson 陷阱复发。Oracle `bg_6a8e4397` A3 审查发现。

> **修复**: 2026-09-21 从 `git show 798b6c6^:<path>` 物理恢复 4 文件到 archive。line 数与 commit stat 完全一致 (404+173+21+150=748)。

## 时间线

| 时间 | 事件 |
|---|---|
| 2026-09-20 12:33 (commit 798b6c6) | ADR-0086 v1.0+v1.1 实施 archive。commit message 自称"Archive integrity: 4 files ✓"，但 `--stat` 显示纯 4 deletions，无 rename。Day-5 lesson **declared verified but actual partial** |
| 2026-09-21 (Oracle bg_6a8e4397 A3) | 审查发现 `2026-09-20-adr-0086-v1-1-harness-change-confounder` 在 `ls openspec/changes/archive/` 中**不存在**，但 ADR-0086 文件 line 4 + roadmap §1.5 均引用该 change。**死链 + archive 完整性陷阱复发** |
| 2026-09-21 (本次) | 从 `798b6c6^` 物理恢复 4 文件到 `archive/2026-09-20-2026-09-20-adr-0086-v1-1-harness-change-confounder/`，line 数验证 404+173+21+150=748 |

## 根本原因分析

**`.gitignore:36` 显式 ignore `openspec/changes/archive/`**（per-machine ephemeral 策略）：

```
# OpenSpec: changes/ 目录入 git (团队共享 proposals/specs/tasks);
# 但 archive/ + config.yaml + specs/ 仍是 per-machine ephemeral
openspec/changes/archive/
openspec/config.yaml
openspec/specs/
```

这意味着：
1. **archive 目录根本不入 git 历史**——`commit 798b6c6` 的 4 deletions 是从 active 树删除，archive 目录本来就不在 git tree 中。commit message 的"archive integrity verified"在文件系统层面正确（line 数对、内容对、目录在），但**在 git 视角是误导**——无法用 `git checkout 798b6c6` 恢复 archive 内容
2. Day-5 lesson "4-file integrity verified" 的真实含义是**文件系统层面**的：archive 子目录确实存在、4 文件 line 数匹配。但该 commit 实际未将 archive 内容固化到 git
3. 后续 contributor clone 仓库时**archive 目录完全为空**——除非他们从工作机器 copy 过来（per-machine ephemeral 策略的本意）

## 已恢复内容

```
openspec/changes/archive/2026-09-20-2026-09-20-adr-0086-v1-1-harness-change-confounder/
├── .openspec.yaml                            (21 lines)
├── proposal.md                              (404 lines)
├── specs/credit-assignment-v1-1/
│   └── spec.md                              (150 lines)
└── tasks.md                                 (173 lines)
                                                          Total: 748 lines
```

对比 commit `798b6c6` 删除行数：404+173+150+21=748 — **精确匹配**。

## 治理建议 (后续 follow-up)

`.gitignore` 把 archive 目录 ignore 是**项目层策略决定**，本审计不擅自动。**但当前实际效果**：
- ✅ 防止误 commit（新人 `git add -A` 不会把 archive 全量入库）
- ❌ 防止 archive 内容在协作间漂移（per-machine 各自维护，无 git diff 可见）
- ❌ Day-5 lesson "4-file integrity" 验证机制不完整（无 git baseline）

**可能的 follow-up change**（登记到 `docs/active-status.md` 待立项）：
- 选项 A: 保留 .gitignore，但删 active 后强制 `cp -r` 到 archive + 创建 `docs/audits/<date>-openspec-archive-recovery.md` 跟踪（现行模式，每次 archive 单独审计）
- 选项 B: 删 `.gitignore:36` 的 archive 行，让 archive 全部入 git（变更项目策略，需 RFC-level 决策）
- 选项 C: 混合策略——active commit 包含 `git mv` 到 archive，archive 入 git，但保持 .gitignore 防止误 add 未提交 work-in-progress

## 本次 commit 的局限

- **archive 内容不进 git**：本次恢复是文件系统层面，下一个 clone 仓库的 contributor 看不到这 4 文件（除非他们已经有 archive 目录或单独 sync）
- **未追踪根因**：`.gitignore:36` 决定的 "per-machine ephemeral archive" 策略可能与 Day-5 lesson "openspec validate" pipeline 的"全团队可读"期望冲突——这是项目级 tension，需独立决策

## 关联修复

- Oracle `bg_3c06ae5b` (2026-09-21)：原始 C1 + 闭环第 7 环断裂审查
- Oracle `bg_6a8e4397` (2026-09-21)：roadmap drift + 本审计触发
- AGENTS.md Day-5 lesson：archive 完整性验证（本次未达到 git 层面）

## 相关 commit

- `dea85f6` — fix(genome-registry): fresh-machine HMAC key + hermetic test fixture (C1)
- `5b600a6` — docs: 自进化 v1.4 + rsi-mapping C3/C4 alignment + roadmap CRD string + AGENTS.md 模式 #10
- `3076042` — feat(openspec): genome-wiring-harness-rsi-gepa v2 — closes loop ring 7
- 本次 commit — chore(governance): record ADR-0086 archive recovery + Day-5 lesson recurrence