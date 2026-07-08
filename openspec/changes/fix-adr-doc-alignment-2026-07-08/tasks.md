# Tasks: ADR 文档对齐修复 (fix-adr-doc-alignment-2026-07-08)

> **STATUS: ACTIVE** 🔵
> **关联 proposal**: `proposal.md` (本目录)
> **关联 design**: `design.md` (本目录)
> **关联 spec**: `specs/adr-doc-alignment/spec.md` (本目录)
> **估时**: ~4 h (P0 + P1 全部) + ~2 h (P2)
> **最后更新**: 2026-07-08

---

## 1. P0-1: STATUS-GLOSSARY 状态表修正

- [ ] 1.1 修改 `docs/adr-management/STATUS-GLOSSARY.md` line 12 ADR-0021 行: 🔍 Proposed → ✅ Approved
- [ ] 1.2 修改 `docs/adr-management/STATUS-GLOSSARY.md` line 16 ADR-0022 候选行（如有）: 🔍 Proposed → ✅ Approved
- [ ] 1.3 修改 `docs/adr-management/STATUS-GLOSSARY.md` ADR-0023 行: 🟡 Partial → ✅ Approved
- [ ] 1.4 修改 `docs/adr-management/STATUS-GLOSSARY.md` ADR-0030 行: ❌ Not Implemented → 🔍 Proposed
- [ ] 1.5 修改 `docs/adr-management/STATUS-GLOSSARY.md` ADR-0034 行: ❌ Not Implemented → ✅ Approved
- [ ] 1.6 修改 `docs/adr-management/STATUS-GLOSSARY.md` ADR-0036 行: ❌ Not Implemented → 🔍 Proposed
- [ ] 1.7 验证: `grep -E "🔍 Proposed|❌ Not Implemented" docs/adr-management/STATUS-GLOSSARY.md` 输出仅含 ADR-0024-0028 占位 + ADR-0030/0036 Proposed
- [ ] 1.8 追加维护规则 #2 说明: "From ADR ## 状态 → STATUS-GLOSSARY 单向同步"

## 2. P0-2: 工具命名 SLASH 化 (5 个 lib/inference/*.md)

- [ ] 2.1 修订 `lib/inference/prefix_cache.md`: `prefix_cache.configure` → `prefix_cache/configure` (DOT → SLASH)
- [ ] 2.2 修订 `lib/inference/kv_cache.md`: `kv_cache.configure` → `kv_cache/configure`
- [ ] 2.3 修订 `lib/inference/decoding.md`: `decoding.configure` → `decoding/configure`
- [ ] 2.4 修订 `lib/inference/cloud_engine.md`: `cloud_engine.configure` → `cloud_engine/configure`
- [ ] 2.5 修订 `lib/inference/batching.md`: `batching.submit_and_wait` → `batching/submit_and_wait`
- [ ] 2.6 验证: `grep -E "tool: [a-z_]+\\.[a-z]+" lib/inference/*.md` 输出为空 (无 DOT 风格)
- [ ] 2.7 验证: `grep -E "tool: [a-z_]+/[a-z_]+(/[a-z_]+)?" lib/inference/*.md` 输出 5 个 SLASH 工具名

## 3. P0-3: 重写 decisions-2026-07-07.md D3 整章

- [ ] 3.1 重写 D3 描述段 (line 35-39): 明确"统一 SLASH 格式 `inference/engine/init`"
- [ ] 3.2 重写 D3 命名映射表 (line 40-49): 8 行 DOT 风格 → 8 行 SLASH 风格
- [ ] 3.3 删除 "C13 架构工具命名边界" 小节 (line 50-60): 与 ADR-0034 §命名约定矛盾
- [ ] 3.4 修订 D3 影响段 (line 61-66): 工具名引用从 DOT 改为 SLASH
- [ ] 3.5 验证: `grep -E "inference\\.[a-z_]+" docs/adversarial-reviews/decisions-2026-07-07.md` 输出为空

## 4. P0-3: 修正 D5 实施步骤 + 签字状态

- [ ] 4.1 修正 D5 step 2 与 step 3 重复: 拆分为 "新增 API" + "添加单测" 两步
- [ ] 4.2 更新 D5 签字状态 (line 105): "待签字确认" → "🟡 待签字 (2026-07-08)" 或 "✅ 已签字 (2026-07-08 by [signer])"
- [ ] 4.3 验证: `grep -c "^3\\. " decisions-2026-07-07.md` 显示 1 次 (无重复)

## 5. P0-2: 修正 C16 proposal 命名章节 (DOT → SLASH)

- [ ] 5.1 修订 `openspec/changes/phase5-illmprovider-call-chain-v2/proposal.md` "命名统一" 段: `inference.*` → `inference/*`
- [ ] 5.2 修订同文件 "文档修订" 段: `统一 inference.*` → `统一 inference/*`
- [ ] 5.3 验证: `grep -E "inference\\.\\*|inference\\.[a-z_]+" openspec/changes/phase5-illmprovider-call-chain-v2/proposal.md` 输出为空

## 6. P0 验证 + 跨文档一致性检查

- [ ] 6.1 跑 `tools/adr_lint.py` (若存在): exit 0, 无 error
- [ ] 6.2 跑 `openspec validate fix-adr-doc-alignment-2026-07-08`: exit 0
- [ ] 6.3 跑 `git grep -E "inference\\.engine|inference\\.model|prefix_cache\\.configure|kv_cache\\.configure|decoding\\.configure|cloud_engine\\.configure|batching\\.[a-z_]+"` 输出为空 (跨目录)
- [ ] 6.4 验证 4 个文档源 SLASH 风格一致: lib/inference/*.md + decisions + C16 proposal + ADR-0034 §命名约定

## 7. P1-1: 修正 README 拼写

- [ ] 7.1 修订 `docs/adversarial-reviews/README.md` line 84: `ref-1-b2-oopenspec-arch.md` → `ref-1-b2-openspec-arch.md`
- [ ] 7.2 验证: `grep "oopenspec" docs/adversarial-reviews/README.md` 输出为空

## 8. P1-4: STATUS-GLOSSARY 📋 双语义扩展

- [ ] 8.1 修订 `docs/adr-management/STATUS-GLOSSARY.md` 状态表 📋 行: 拆分为 Reserved + Audit 两行
- [ ] 8.2 追加 "📋 Audit | 审计补充 | impl-scope-audit 文档专用" 行 + 使用场景 (12 个 adr-*-impl-scope.md)
- [ ] 8.3 修订维护规则 #3: 追加例外条款 "现有标签的子语义扩展允许 (如 📋 双语义)"
- [ ] 8.4 验证: 12 个 `adr-*-impl-scope.md` 文件 `## 状态` 字段使用 📋 Audit (若原文为 📋 Reserved 则需修订)

## 9. P1-2: ADR-0036 软归档

- [ ] 9.1 `git mv docs/adr/adr-0036-three-layer-service-protocol.md docs/archive/adr/`
- [ ] 9.2 在归档文件头部追加 DEPRECATED 横幅 + 引用 ADR-0045
- [ ] 9.3 修订 `docs/README.md`: 删除 `adr-0036-three-layer-service-protocol.md` 行 (若存在)
- [ ] 9.4 修订 `docs/adr/plugin/README.md`: 追加 ADR-0036 renumber 注记
- [ ] 9.5 验证: `git grep "adr-0036-three-layer-service-protocol" -- 'docs/'` 仅命中 `docs/archive/adr/` 路径
- [ ] 9.6 验证: `git grep "adr-0036" -- 'openspec/'` 输出 ADR-0036 引用全部更新到 ADR-0045 或保持原始引用 (设计决策记录)

## 10. P1 验证

- [ ] 10.1 跑 `tools/adr_lint.py`: exit 0
- [ ] 10.2 验证 AGENTS.md "12 个已废弃 ADR" 数字 (后续 P2-4 修订): 占位记录

## 11. P2-1: 重跑 `tools/adr_relationships.py`

- [ ] 11.1 检查 `tools/adr_relationships.py` 是否存在
- [ ] 11.2 若存在, 验证脚本扫描路径包含 `docs/adr/plugin/`
- [ ] 11.3 验证脚本排除 `docs/archive/adr/`
- [ ] 11.4 备份当前 `docs/adr-management/relationships.md`
- [ ] 11.5 跑脚本, 生成新 `relationships.md`
- [ ] 11.6 人工 review git diff, 验证 16 个新 ADR 节点 (0035/0038-0046) 包含
- [ ] 11.7 修订 `relationships.md` "按状态统计" 表格: 13 → 16 Approved etc.

## 12. P2-2: 验证 C13 4 个 schema ship 状态

- [ ] 12.1 `git log --oneline lib/inference/prefix_cache.md` 检查 commit hash
- [ ] 12.2 重复对 `kv_cache.md` / `decoding.md` / `cloud_engine.md` 跑同样命令
- [ ] 12.3 验证 commit 是否包含完整 schema 内容 (含 tool 字段)
- [ ] 12.4 若已 ship, 更新 master plan `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.5 "7/7 子图覆盖率" 数字
- [ ] 12.5 若未 ship, 在本 tasks.md §12 追加"未 ship 注记"段, 列出缺失的 4 个 .md

## 13. P2-3: 同步 ADR-0021 状态字段

- [ ] 13.1 修订 `docs/adr/adr-0021-pdk-design.md` ## 状态 段: 追加 "2026-07-08 update: §8 SamplerStrategy 接口被 decisions-2026-07-07.md D1 决策撤销"
- [ ] 13.2 验证: `grep "SamplerStrategy.*撤销\|2026-07-08 update" docs/adr/adr-0021-pdk-design.md` 命中 1 行

## 14. P2-4: 更新 AGENTS.md

- [ ] 14.1 修订 `AGENTS.md`: "12 个已废弃 ADR 已归档" → "13 个已废弃 ADR 已归档" (含 ADR-0036)
- [ ] 14.2 验证: `grep "13 个已废弃" AGENTS.md` 命中 1 行
- [ ] 14.3 验证: `git grep "12 个已废弃"` 输出为空

## 15. P2 验证 + 跨工具 lint

- [ ] 15.1 跑 `tools/adr_lint.py`: exit 0, 无 error/warning
- [ ] 15.2 跑 `tools/adr_relationships.py --validate`: exit 0
- [ ] 15.3 跑 `tools/docs_drift_audit.py` (若存在): exit 0, 0 critical drift
- [ ] 15.4 跑 `openspec validate fix-adr-doc-alignment-2026-07-08`: exit 0

## 16. 文档更新 + Commit

- [ ] 16.1 修订 `docs/README.md`: 标注本 change ship 状态
- [ ] 16.2 修订 `AGENTS.md`: 追加 "2026-07-08 (Sprint 21 / fix-adr-doc-alignment-2026-07-08, ship)" 记录
- [ ] 16.3 跑 `git status` 验证变更文件清单: docs/adr-management/STATUS-GLOSSARY.md + lib/inference/*.md (5) + docs/adversarial-reviews/* (2) + openspec/changes/phase5-illmprovider-call-chain-v2/proposal.md + docs/adr/adr-0021-pdk-design.md + AGENTS.md + docs/README.md = 11-13 个文件
- [ ] 16.4 跑 `git diff --stat`: 验证 +X/-Y 行数合理 (~100-300 行)
- [ ] 16.5 commit: `docs: fix ADR document alignment (3 P0 + 4 P1 + 3 P2 issues)`
- [ ] 16.6 跑 `git log --oneline -1`: 验证 commit hash

## 17. Ship Gate

- [ ] 17.1 跑 `tools/adr_lint.py` 最终验证: exit 0
- [ ] 17.2 跑 `tools/adr_relationships.py` 最终验证: exit 0
- [ ] 17.3 跑 `openspec validate fix-adr-doc-alignment-2026-07-08`: exit 0
- [ ] 17.4 跑 `git status` 确认工作区干净
- [ ] 17.5 准备 OpenSpec archive: 通知 Sisyphus 启动 `/opsx-archive` 工作流

---

**总任务数**: 47 个 (17 个章节, 1-17)
**总估时**: ~4 h (P0 §1-6) + ~1 h (P1 §7-10) + ~2 h (P2 §11-15) + ~30 min (commit + ship gate §16-17)
**Ship gate**: 17.1-17.3 三个工具 lint 全部 exit 0

---

## 任务依赖图

```
§1 STATUS-GLOSSARY (P0-1)
  └→ §6 验证
§2 lib/inference SLASH (P0-2)
  └→ §6 验证
§3 decisions D3 重写 (P0-3)
  └→ §6 验证
§4 decisions D5 修正 (P0-3)
  └→ §6 验证
§5 C16 proposal 修正 (P0-2)
  └→ §6 验证
§6 P0 验证 ─→ §7+ 后续任务
§7 README 拼写 (P1-3)
§8 STATUS-GLOSSARY 📋 双语义 (P1-4)
  └→ §10 验证
§9 ADR-0036 归档 (P1-2)
  └→ §10 验证
§10 P1 验证
§11 relationships 重跑 (P2-1)
§12 C13 ship 验证 (P2-2)
§13 ADR-0021 状态同步 (P2-3)
§14 AGENTS.md 更新
  └→ §15 验证
§15 P2 验证 + 跨工具 lint
§16 文档更新 + commit
§17 Ship gate ─→ /opsx-archive
```
