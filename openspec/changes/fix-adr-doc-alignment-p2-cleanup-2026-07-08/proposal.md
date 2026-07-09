# Proposal: ADR 文档对齐 P2 清理 (fix-adr-doc-alignment-p2-cleanup-2026-07-08)

> **STATUS: PLACEHOLDER** 🟡
> **创建日期**: 2026-07-08
> **创建者**: Sisyphus
> **关联 hotfix**: `openspec/changes/fix-adr-doc-alignment-hotfix-2026-07-08/` (Change A)
> **关联命名政策**: `openspec/changes/fix-adr-naming-policy-2026-07-08/` (Change B)
> **关联原始 mega-change**: `openspec/changes/fix-adr-doc-alignment-2026-07-08/` (47 tasks 拆分记录)
> **关联审查**: 2026-07-08 架构文档对齐审查 + Metis 审查 `ses_0bf414b4affe5zB7zN06vHudKN`
> **估时**: ~2 h
> **优先级**: 低 (推到 Sprint 21 follow-up)

## Why

本 change 是 `fix-adr-doc-alignment-2026-07-08` (47 tasks) 经 Metis 审查后拆分的 **Change C (P2 清理)**。

**3 个 P2 一般问题** (Metis 审查后确认):

1. **P2-1**: 重跑 `tools/adr_relationships.py` — 当前 relationships.md 缺 16 个 ADR 节点 (0035/0038-0046/0014/0029/0032 等)
2. **P2-2**: 验证 C13 4 个 schema 实际 ship 状态 (master plan §十六.5 数字修正)
3. **P2-3**: 同步 ADR-0021 状态字段与 decisions D1 决策 (追加 D1 决策注记)

**为什么不在 hotfix/Change B 中**:
- P2-1 依赖 `tools/adr_relationships.py` 工具检查 + 可能需要脚本 patch (新增 ~30 min)
- P2-2 依赖 git log 调查 + master plan 修订 (跨文件)
- P2-3 是 ADR 状态字段追加 (低优先级, 不阻塞 B2 启动)
- 三个 P2 都属于"清理改进"性质, 不影响核心命名一致性

**为什么 now + 拆分**:
- Change A 立即 ship (30 min) 修复阻塞问题
- Change B 与 B2 协调时序 (2h)
- Change C 推到 Sprint 21 follow-up, 由独立 owner 实施

## What Changes (计划, 待启动后细化)

### 1. [P2-1] 重跑 `tools/adr_relationships.py`

- 验证脚本扫描路径包含 `docs/adr/plugin/`
- 验证脚本排除 `docs/archive/adr/`
- 备份当前 `docs/adr-management/relationships.md`
- 跑脚本生成新 `relationships.md`
- 人工 review git diff, 验证 16 个新 ADR 节点 (0035/0038-0046) 包含
- 修订 "按状态统计" 表格 (13 → 16 Approved etc.)

### 2. [P2-2] 验证 C13 4 个 schema ship 状态

- `git log --oneline lib/inference/prefix_cache.md` 检查 commit hash
- 重复对 `kv_cache.md` / `decoding.md` / `cloud_engine.md` 跑同样命令
- 验证 commit 是否包含完整 schema 内容 (含 tool 字段)
- 若已 ship, 更新 master plan `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.5 "7/7 子图覆盖率" 数字
- 若未 ship, 在 tasks.md §9.2 追加"未 ship 注记"段

### 3. [P2-3] 同步 ADR-0021 状态字段

- 修订 `docs/adr/adr-0021-pdk-design.md` ## 状态 段: 追加 "2026-07-08 update: §8 SamplerStrategy 接口被 decisions-2026-07-07.md D1 决策撤销"
- 验证: `grep "SamplerStrategy.*撤销\|2026-07-08 update" docs/adr/adr-0021-pdk-design.md` 命中 1 行

### 4. [P2-4] 更新 AGENTS.md + README 数字

- 修订 `AGENTS.md`: "12 个已废弃 ADR 已归档" → "13 个已废弃 ADR 已归档" (含 ADR-0036)
- 修订 `docs/README.md`: 同步废弃 ADR 数字
- 验证: `git grep "12 个已废弃"` 输出为空

### 5. [P1-4] STATUS-GLOSSARY 📋 双语义扩展 (从原 mega-change P1-4 提升)

- 修订 `docs/adr-management/STATUS-GLOSSARY.md` 状态表 📋 行: 拆分为 Reserved + Audit 两行
- 追加 "📋 Audit | 审计补充 | impl-scope-audit 文档专用" 行
- 修订维护规则 #3: 追加例外条款
- 12 个 `adr-*-impl-scope.md` 文件 `## 状态` 字段**不修改** (经 Metis Q5 修正, README 表格用 Audit, ADR 文件保留实际状态)

## Non-goals

- **不修改** PDK 工具名 (属 Change B)
- **不修改** `lib/inference/*.md` (属 Change B)
- **不修改** STATUS-GLOSSARY 状态值 (已在 hotfix 中处理)
- **不修改** ADR-0036 归档 (已在 hotfix 中处理)
- **不修改** `decisions-2026-07-07.md` D3 决策内容 (属 Change B)
- **不修改** C16 proposal (经 Metis 审查, `inference.*` 是合法事件 topic notation)
- **不修复** `pdk/llama_engine/` 缺 `llama.h` LSP 错误 (pre-existing)

## Capabilities

### New Capabilities

- `adr-doc-alignment-p2-cleanup`: P2 清理 + 📋 双语义扩展 (在 hotfix 已 ship 的 `adr-doc-alignment-hotfix` 基础上扩展)

### Modified Capabilities

- 修改 `adr-doc-alignment-hotfix` 4 个 Requirements 扩展为 6 个 Requirements
- 新增 "📋 Audit 语义" Requirement

## Impact (待 Change C 启动时细化)

| 文件 | 变更类型 |
|------|---------|
| `docs/adr-management/relationships.md` | 重跑 + 人工 review |
| `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.5 | 数字修正 (若已 ship) |
| `docs/adr/adr-0021-pdk-design.md` ## 状态 | 追加 D1 注记 |
| `AGENTS.md` | 数字更新 |
| `docs/README.md` | 数字更新 |
| `docs/adr-management/STATUS-GLOSSARY.md` | 📋 双语义扩展 |
| `tools/adr_lint.py` (若需 patch) | 支持 📋 Audit 标签 |

## Open Questions

1. **`tools/adr_relationships.py` 是否存在**: 仓库 `tools/` 目录是否有该脚本
2. **C13 4 个 schema 实际 ship 状态**: ref-3 报告 0/32, 但 2026-07-07 后 lib/inference/ 出现 4 个新 .md, 需 git log 确认
3. **master plan 修订权限**: 是否需要 master plan owner review
4. **AGENTS.md 数字更新范围**: 是否包含其他 "X 个" 数字 (tests 数量等)

## 启动条件

- [ ] Change A 已 ship
- [ ] Change B 已 ship (或明确推到后续 Sprint)
- [ ] `tools/adr_relationships.py` 工具存在性确认
- [ ] master plan 修订权限确认
- [ ] Sprint 21 owner 分配

## 当前状态: PLACEHOLDER

**说明**: 本 change 当前仅有 proposal.md 占位, 完整 artifacts (design/specs/tasks) 待 Change A + B ship 后, 由 Sisyphus 启动正式化工作。
