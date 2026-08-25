## Context

2026-08-25 对 `docs/architecture/capability-application-map-2026-08.md`（v1.3）的结构化审查在 **1 个 capability-map 文件 + 2 个 ADR 文件（ADR-0071 / ADR-0074）** 中识别出 **12 处文档 drift**，后续 ship 时扩展为 16 项（含 4 项同类 Oracle 评审 stale 同步）。审查方式：文件内一致性 + 实证 ctest + 关联文件抽查 + OpenSpec 验证。

最大风险是 **结构错乱**：`§七 变更记录` 排到 `§八 蒸馏专题` 之后，`§六.6 验证命令` 也错位到 §八 之后。

## Goals / Non-Goals

**Goals**：修复 16 处 drift，恢复 capability-map 标准章节流（一→二→三→四→五→六→七→八），不引入任何代码/ADR 编号/OpenSpec spec 变更。

**Non-Goals**：❌ 不修改源代码、不重编号、不改变计数真实性、不创建新 ADR、不修改现有 ADR 状态字段（drift-7 仅 footer 注记同步）、不修改已 approved specs、不修复 Oracle 方法论本身、不解决其他文档 drift。

## Decisions

### Decision 1: 章节位置调整 vs 编辑性标注
**选项 A** 物理移动章节（采用）。理由：sprint-closeout.sh Step 8 + docs-drift-detect.sh B.2 自动化脚本依赖章节顺序，banner 修复保留问题。

### Decision 2: §六.6 路径修复范围
**选项 A** 仅修 §六.5 line 421 关键路径（采用）。理由：控制 scope 避免引入其他未验证路径变化。

### Decision 3: G15 状态字段同步深度
G10/G12/G15 三类 stale 都源于 Oracle 评审通过后的 ADR Approved 状态，统一处理。

### Decision 4: ADR-0071/0074 footer 同步范围
header line 5 + footer 行都改（采用）。理由：AGENTS.md / docs/README.md 表格 grep 目标在 header，archive 查询在 footer，两者均需同步。

### Decision 5: §八.5 重复块删除范围
**选项 A** 仅删除下部分"下个 Sprint"重复块（采用）。理由：上半部分已是规范 Sprint 24/25/26 评审通过后表，下半部分 v1.2 阶段旧排期残留。

### Decision 6: §三 "零工程" 段计数同步 vs capability-matrix 整体重算
**选项 A** 仅改计数文字 22→23（采用）。理由：控制 scope。

### Decision 7: §一 L4 表头加注
**选项 A** 表头加注 "(4 项，#23 T14 v1.2 后置增补)"（采用）。理由：与 §六.1.1 视觉一致。

## Refactor vs Bugfix 边界声明
严格 Bugfix Rule：12+4 处均为最小文本修订 + 1 处章节位置调整，无整章节重写、无 ADR 状态字段变更、无代码修改。

## Verification Strategy

| 验证 | 命令 | 预期 |
|------|------|------|
| OpenSpec validate | `openspec validate --strict` | exit 0 + valid |
| ADR lint | `python3 tools/adr_lint.py` | 0 errors |
| Docs drift audit | `python3 tools/docs_drift_audit.py` | 0 DRIFT 增加 |
| Ctest 完整性 | `cd build && ctest -j4` | 185/185 PASS |
| 代码未触碰 | `git diff --stat HEAD -- 'src/*' 'include/*' 'pdk/*' 'tests/*' 'examples/*'` | 0 lines |
| OpenSpec archive 未触碰 | `git diff --stat HEAD -- 'openspec/changes/archive/'` | 0 lines |
| capability-map 结构流 | `awk '/^## /'` | 一→二→...→八 |
