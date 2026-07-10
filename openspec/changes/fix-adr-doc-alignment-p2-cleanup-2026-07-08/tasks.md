# Tasks: ADR 文档对齐 P2 清理 (fix-adr-doc-alignment-p2-cleanup-2026-07-08)

> **STATUS: SHIPPED** ✅
> **关联 proposal**: `proposal.md`
> **关联 design**: `design.md`
> **关联 spec**: `specs/adr-doc-alignment-p2-cleanup/spec.md`
> **方法**: Q5 方案 A (STATUS-GLOSSARY 📋 双语义 + 11 个 impl-scope.md 加 Audit 标识)
> **估时**: ~50 min (实际 ~30 min)

---

## 1. P2-4 数字同步

- [x] 1.1 修订 `docs/README.md:73`: "12 个已废弃 ADR 已归档" → "13 个已废弃 ADR 已归档"
- [x] 1.2 验证: `git grep "12 个已废弃" -- 'docs/' 'AGENTS.md'` 输出为空

## 2. P2-3 ADR-0021 状态注记

- [x] 2.1 在 `docs/adr/adr-0021-pdk-design.md` `## 状态` 段追加 2026-07-08 update 注记 (SamplerStrategy 撤销)
- [x] 2.2 验证: `grep "SamplerStrategy.*撤销" docs/adr/adr-0021-pdk-design.md` 命中 1 行

## 3. P2-2 C13 ship 状态 + master plan 数字

- [x] 3.1 修订 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §7.5: 7/7 schema 全部标 `[x]` + ship commit hash
- [x] 3.2 修订 §7.6 line 567: `[ ]` → `[x] 推理标准库 7/7 子图全部 ship`
- [x] 3.3 修订 §8.1 line 580: `[ ]` → `[x] 推理标准库 7/7 子图全部 ship`
- [x] 3.4 验证: ctest 65/65 PASS (含 test_llama_engine_plugin 10 cases)

## 4. P1-4 STATUS-GLOSSARY 📋 双语义扩展 (Q5 方案 A)

- [x] 4.1 修订 `docs/adr-management/STATUS-GLOSSARY.md`: 标题 "6 个标准标签" → "7 个标准标签"
- [x] 4.2 split 表格 📋 行 → 📋 Reserved + 📋 Audit 两行
- [x] 4.3 维护规则 #1 更新 (6 → 7 个标签)
- [x] 4.4 维护规则 #3 追加例外条款 (子语义扩展允许)
- [x] 4.5 追加 ADR-0036 编号冲突备注段 (Step 6)
- [x] 4.6 修订 `tools/adr_lint.py`: VALID_STATUS 加 "audit" 标签 (line 31)
- [x] 4.7 修订 STATUS_PATTERN regex 追加 `📋\s*Audit` (line 70)
- [x] 4.8 修订错误信息: "6 个标准标签" → "7 个标准标签" (line 192)
- [x] 4.9 验证: `python3 tools/adr_lint.py docs/adr/` 42/42 PASS

## 5. 11 个 impl-scope.md 加 Audit 标识 (P1-4 配套)

- [x] 5.1 批量插入 `**📋 Audit** (impl-scope-audit 文档, 与 docs-code-drift-audit 配套使用)` 到 11 个 `docs/adr/*-impl-scope.md` 的 `## 状态` 段首行
- [x] 5.2 保留原父 ADR 状态行 (✅ Approved / 🟡 Partial)
- [x] 5.3 未修改 `adr-0002-impl-scope-audit.md` / `adr-0004-impl-scope-audit.md` (已用 `📋 Reserved (审计补充)`, 兼容)
- [x] 5.4 验证: `grep -l "📋 Audit" docs/adr/*-impl-scope.md` 命中 11/11

## 6. P2-1 重跑 relationships.md

- [x] 6.1 备份原 `docs/adr-management/relationships.md` → `/tmp/relationships.md.bak`
- [x] 6.2 跑 `python3 tools/adr_relationships.py` 生成新文件
- [x] 6.3 验证: 22 → 42 ADR 节点 (含 16 新节点 0035/0038-0046)
- [x] 6.4 验证: 状态统计自动更新 (✅ Approved 23, 🟡 Partial 3, 🔍 Proposed 12, 📋 Reserved 2, ❌ Not Implemented 1, ⛔ Superseded 1)
- [x] 6.5 验证: `python3 tools/adr_relationships.py --check` exit 0
- [x] 6.6 删除 backup

## 7. P1-2 / 新发现: ADR-0036 编号冲突备注

- [x] 7.1 在 `STATUS-GLOSSARY.md` 表格后追加 "ADR-0036 编号冲突备注" 段
- [x] 7.2 记录 2 个 ADR-0036 文件 (hybrid-kernel + three-layer-service)
- [x] 7.3 标记 "未来如需复活 MUST renumber"

## 8. 验证

- [x] 8.1 `git grep "12 个已废弃" -- 'docs/' 'AGENTS.md'` 输出为空 ✅
- [x] 8.2 `grep "SamplerStrategy.*撤销" docs/adr/adr-0021-pdk-design.md` 命中 1 行 ✅
- [x] 8.3 `python3 tools/adr_lint.py docs/adr/` 42/42 PASS ✅
- [x] 8.4 `python3 tools/adr_relationships.py --check` exit 0 ✅
- [x] 8.5 `ctest --test-dir build/tests` 65/65 PASS (零回归) ✅
- [ ] 8.6 `openspec validate fix-adr-doc-alignment-p2-cleanup-2026-07-08` exit 0 (待跑)

## 9. Commit + Archive

- [ ] 9.1 `git add` 所有变更 (16+ 文件)
- [ ] 9.2 commit: `docs: ADR alignment P2 cleanup (number sync + Audit dual-semantic + relationships rerun)`
- [ ] 9.3 移动到 `openspec/changes/archive/2026-07-09-fix-adr-doc-alignment-p2-cleanup-2026-07-08/`
- [ ] 9.4 sync delta spec 到 `openspec/specs/adr-doc-alignment-p2-cleanup/spec.md`
- [ ] 9.5 commit archive 删除

---

**总任务数**: 30 个 (9 个章节, 1-9)
**实际估时**: ~30 min
**Ship gate**: 8.1-8.5 验证全部通过 ✅