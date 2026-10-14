# parser-string-template-args.md 已 ship 决策登记 (2026-10-14)

**生成**: 2026-10-14 via 主会话（用户问："请分析 parser-string-template-args.md 提案是否合理，是否值得继续实施"）
**决策人**: 用户（Single-Dev）
**会话**: ses_ec6a3b1b4ffe2Jv7AqxsZ2WAza
**Commit**: `9b5df59` 前的 workspace 状态 (本日早 session 已 commit 3 个 P0 提案 hygiene 修正)

## 1. 核心发现

用户提问"该提案是否合理 + 是否值得继续实施"时，本会话通过事实验证发现：

**提案 4 个 Acceptance Criteria 100% 已 ship**（在用户提问前 8 天完成）：

| AC | 描述 | 实证 | 状态 |
|----|------|------|------|
| AC-1 | `make_tool_call` 接受 string arguments | `node_factory.cpp:172-188` `is_string` 分支 | ✅ |
| AC-2 | test_e2e_real_llm ChatSession case 8/8 PASS | 实测 10/2 PASS | ✅ |
| AC-3 | regression test 覆盖 string+object | `test_parser_template_args` 5/5, 28 assertions | ✅ |
| AC-4 | dsl.md §5.2 spec 更新 | `dsl.md:384` 明确说明 + 引用 ship | ✅ |

**OpenSpec 关联**:
- 主 change: `2026-09-30-fix-parser-template-args` → archived (commit `f6c903a`, 2026-10-06)
- 父 chain: `2026-09-30-fix-lib-loop-args-parsing` (lib/loop `args:` → `arguments:` rename)

## 2. 用户决策

### 决策 1: 整体策略 → **A 选项 (hygiene 修正)**
> 追加 ## Status: ✅ SHIPPED 段 + 决策登记 + AGENTS.md Recent Changes + 1 atomic commit

### 决策 2: Ship 模式 → **默认 (单 atomic + 24h cooling-off)**
> per AGENTS.md default, 不走 HARD pause override

### 决策 3: 处理范围 → **不删除提案文件**
> 保留 .rddf/improvements/parser-string-template-args.md 作为历史 traceability (per AGENTS.md Pattern #10 + 用户决策)

## 3. 与本日早 session 决策的连续性

本日 2026-10-14 早 session (ses_ec7b0fe3fffe8g9GCNiGs2a3Zw) 推进了 3 个 P0 提案的 hygiene 修正 (commit `9b5df59`)：

| 早 session 修复 | 早 session commit | 本 session 验证 |
|----------------|------------------|----------------|
| fix-tool-registry-signal-handler-shutdown | `9b5df59` 包含 | ✅ |
| fix-loop-agent-bypass | `9b5df59` 包含 | ✅ |
| adr-0069-tool-coordinator-hooks | `9b5df59` 包含 | ✅ |
| **parser-string-template-args** | 本 session 新 commit | ✅ 4/4 AC 实证 |

**4 个相同 Pattern #10 drift 实例的完整诊断** + 修正 (本日 + commit `9b5df59` + 本次新 commit)。

## 4. Pattern 沉淀 (新增)

### 4.1 "tasks.md 50 checkboxes 10 done ≠ 未 ship" 教训

用户和 AI 容易基于 OpenSpec 内部 tasks.md 完成度判断 ship 状态，但实际 ship 证据是：
- 代码已 ship (`git log` 验证)
- 测试 PASS (实跑验证)
- spec 同步 (搜关键词验证)

**规则**: 任何"提案是否 ship"判断必须三段式验证, 不依赖 OpenSpec 任务清单数字。

### 4.2 "用户提问 + 实测验证" 闭环

用户原问题"是否合理 + 是否值得继续实施" 实际触发:
- 问 "合理性" → 应 4 AC 验真 (4/4 PASS)
- 问 "是否值得" → ship 状态已 ship, 无需"继续实施", 但需 hygiene 修正

**教训**: 用户问"是否值得做" 时, 第一步是"是否已做过" 事实检查, 而非"是否容易做" 难度评估。

## 5. 决策实施路径

| 步骤 | 状态 | 备注 |
|------|------|------|
| 1. 实测验证 4 AC | ✅ 完成 | 3 个 git/grep 验证 + 1 个 ctest 实跑 |
| 2. 追加 ## Status: ✅ SHIPPED 段 | ✅ 完成 | 提案文件末尾 |
| 3. 新建决策登记 | ✅ 本文件 | `.rddf/state/decisions/2026-10-14-parser-string-template-args-shipped.md` |
| 4. AGENTS.md Recent Changes 段 | ⏳ 待执行 | 本次 commit 包含 |
| 5. 1 atomic commit | ⏳ 待执行 | commit message 含 [Reverse Indicator] 5 字段 |
| 6. ctest 零回归验证 | ⏳ 待执行 | pre-commit hook |

## 6. 跨 .rddf/improvements/ 状态盘点 (2026-10-14 baseline 增量)

- ✅ DONE: 27 (早 session) → **+1 = 28** (本 session: parser-string-template-args)
- 🟡 PARTIAL: 10
- 🔴 OPEN: 33
- ⏸ GATED: 6
- ⛔ DESCOPED: 1
