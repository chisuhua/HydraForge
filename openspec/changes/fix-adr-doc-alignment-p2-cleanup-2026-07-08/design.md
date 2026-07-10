# Design: ADR 文档对齐 P2 清理 (fix-adr-doc-alignment-p2-cleanup-2026-07-08)

> **STATUS: SHIPPED** ✅
> **决策日期**: 2026-07-09
> **方法**: Q5 方案 A (STATUS-GLOSSARY 📋 双语义 + 11 个 impl-scope.md 加 Audit 标识)

---

## 1. 任务清单与执行结果

### 1.1 P2-4: 数字同步 ✅

| 文件 | 变更 |
|------|------|
| `docs/README.md:73` | "12 个已废弃 ADR 已归档" → "13 个已废弃 ADR 已归档" |

**验证**: `git grep "12 个已废弃" -- 'docs/' 'AGENTS.md'` 输出为空 ✅

### 1.2 P2-3: ADR-0021 状态注记 ✅

| 文件 | 变更 |
|------|------|
| `docs/adr/adr-0021-pdk-design.md` ## 状态 段 | 追加 "2026-07-08 update: §8 SamplerStrategy 接口被 decisions-2026-07-07.md D1 决策撤销" |

**验证**: `grep "SamplerStrategy.*撤销" docs/adr/adr-0021-pdk-design.md` 命中 1 行 ✅

### 1.3 P2-2: C13 ship 验证 + master plan 数字 ✅

| 文件 | 变更 |
|------|------|
| `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §7.5 | 7/7 子图 ship 状态 + checkbox `[x]` |
| 同文件 §7.6 line 567 | `[ ]` → `[x] 推理标准库 7/7 子图全部 ship` |
| 同文件 §8.1 line 580 | `[ ]` → `[x]` 同上 |

**验证**: 65/65 ctest pass (含 test_llama_engine_plugin 10 cases) ✅

### 1.4 P1-4: STATUS-GLOSSARY 📋 双语义 + lint 工具 (Q5 方案 A) ✅

| 文件 | 变更 |
|------|------|
| `docs/adr-management/STATUS-GLOSSARY.md` | 标题 "6 个标准标签" → "7 个标准标签"; 表格 split 📋 行 → Reserved + Audit 两行; 维护规则 #1/#3 更新 |
| `tools/adr_lint.py` | VALID_STATUS 加 "audit" 标签; STATUS_PATTERN regex 追加 `📋\s*Audit`; 错误信息更新为 7 个标签 |

**验证**: `python3 tools/adr_lint.py docs/adr/` 42/42 PASS ✅

### 1.5 P2-1: 重跑 relationships.md ✅

| 文件 | 变更 |
|------|------|
| `docs/adr-management/relationships.md` | 重新生成: 22 → 42 ADR, 包含 0035/0038-0046/11 个 impl-scope 等新节点; 状态统计自动更新 |

**验证**: `python3 tools/adr_relationships.py --check` exit 0 ✅

### 1.6 P1-2 / 新发现: ADR-0036 编号冲突备注 ✅

| 文件 | 变更 |
|------|------|
| `docs/adr-management/STATUS-GLOSSARY.md` | 追加 "ADR-0036 编号冲突备注" 段, 记录 2 个同号 ADR-0036 议题 |

**说明**: 冲突为 pre-existing, 未来如需复活 MUST renumber。

---

## 2. 11 个 impl-scope.md 文件变更 (P1-4 配套)

对 11 个 `docs/adr/*-impl-scope.md` 文件:
- 在 `## 状态` 段第一行 (原父 ADR 状态行之前) 插入 `**📋 Audit** (impl-scope-audit 文档, 与 docs-code-drift-audit 配套使用)`
- 保留原父 ADR 状态行 (✅ Approved / 🟡 Partial / 📋 Reserved)

**示例 (adr-0001)**:
```diff
 ## 状态

+**📋 Audit** (impl-scope-audit 文档, 与 docs-code-drift-audit 配套使用)
+
 ✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)
```

**未修改文件** (已使用 📋 格式):
- `docs/adr/adr-0002-impl-scope-audit.md` — 已用 `📋 Reserved (审计补充)` (2026-06-13 创建)
- `docs/adr/adr-0004-impl-scope-audit.md` — 已用 `📋 Reserved (审计补充)` (2026-06-13 创建)

---

## 3. 验证矩阵

| 验证项 | 命令 | 期望 | 实际 |
|--------|------|------|------|
| openspec validate | `openspec validate fix-adr-doc-alignment-p2-cleanup-2026-07-08` | exit 0 | (待跑) |
| ctest 零回归 | `ctest --test-dir build/tests` | 65/65 PASS | ✅ (Step 3 验证) |
| adr_lint pass | `python3 tools/adr_lint.py docs/adr/` | exit 0 | ✅ (Step 4 验证) |
| adr_relationships 一致 | `python3 tools/adr_relationships.py --check` | exit 0 | ✅ (Step 5 验证) |
| ADR-0021 注记 | `grep "SamplerStrategy.*撤销" docs/adr/adr-0021-pdk-design.md` | 1 行 | ✅ (Step 2 验证) |
| 12→13 数字 | `git grep "12 个已废弃"` | 空 | ✅ (Step 1 验证) |
| 7/7 ship | `grep "7/7" master plan` | ✓ | ✅ (Step 3 验证) |

---

## 4. Non-goals

- 不解决 ADR-0036 编号冲突本身 (pre-existing, 留 follow-up)
- 不修改 `pdk/llama_engine/` 缺 `llama.h` LSP 错误 (pre-existing)
- 不修改 C16 proposal 命名 (经 Metis 审查, `inference.*` 合法事件 topic)
- 不删除 `docs/adr/adr-0002-impl-scope-audit.md` / `adr-0004-impl-scope-audit.md` (历史审计记录)

## 5. 依赖

- **前置**: Change A ✅ (62aafa1) + Change B ✅ (641b036) + C16 ✅ (已 ship, commit 514c441)
- **后续**: 无 (本 change 是 cleanup, 不阻塞其他工作)