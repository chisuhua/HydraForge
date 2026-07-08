# Tasks: ADR 文档对齐 Hotfix (fix-adr-doc-alignment-hotfix-2026-07-08)

> **STATUS: ACTIVE** 🔵
> **关联 proposal**: `proposal.md` (本目录)
> **关联 design**: `design.md` (本目录)
> **关联 spec**: `specs/adr-doc-alignment-hotfix/spec.md` (本目录)
> **估时**: ~30 min
> **最后更新**: 2026-07-08

---

## 1. P0-1 (部分): STATUS-GLOSSARY 5 处状态修正

- [ ] 1.1 修订 `docs/adr-management/STATUS-GLOSSARY.md` ADR-0021 行 (line 12): 🔍 Proposed → ✅ Approved
- [ ] 1.2 修订同文件 ADR-0022 行 (line 16): 🔍 Proposed → ✅ Approved
- [ ] 1.3 修订同文件 ADR-0023 行: 🟡 Partial → ✅ Approved
- [ ] 1.4 修订同文件 ADR-0030 行: ❌ Not Implemented → 🔍 Proposed
- [ ] 1.5 修订同文件 ADR-0034 行: ❌ Not Implemented → ✅ Approved
- [ ] 1.6 验证: `grep -E "🔍 Proposed|❌ Not Implemented" docs/adr-management/STATUS-GLOSSARY.md` 输出仅含 ADR-0024-0028 占位 + ADR-0030 Proposed (修订后) + 表格示例行
- [ ] 1.7 追加维护规则 #2 "**同步方向**: From `## 状态` 字段 → STATUS-GLOSSARY 状态表 (单向)"

## 2. P1-1: 修正 README 拼写

- [ ] 2.1 修订 `docs/adversarial-reviews/README.md` line 84: `ref-1-b2-oopenspec-arch.md` → `ref-1-b2-openspec-arch.md`
- [ ] 2.2 验证: `grep "oopenspec" docs/adversarial-reviews/README.md` 输出为空

## 3. P1-3: 修正 decisions-2026-07-07.md D5 step 编号 + 签字状态

- [ ] 3.1 跑 `git log --follow docs/adversarial-reviews/decisions-2026-07-07.md` 找 author
- [ ] 3.2 修订 D5 实施步骤 (line 99-105): step 2/step 3 重复拆分为 5 步序列
- [ ] 3.3 修订 D5 签字状态 (line 105): "待签字确认" → "🟡 待签字 (2026-07-08)" 或基于 author 的 "✅ 已签字 (2026-07-08 by [author])"
- [ ] 3.4 验证: `grep -cE "^[0-9]+\\. " docs/adversarial-reviews/decisions-2026-07-07.md` 显示 D5 段 5 行 step

## 4. P1-2 (Step 1-2): ADR-0036 git mv 归档

- [ ] 4.1 `git mv docs/adr/adr-0036-three-layer-service-protocol.md docs/archive/adr/`
- [ ] 4.2 在归档文件头部追加 `> **⛔ DEPRECATED (2026-07-08)**` 横幅 + 引用 ADR-0045
- [ ] 4.3 验证: `ls docs/adr/adr-0036-three-layer-service-protocol.md` 输出 "No such file"
- [ ] 4.4 验证: `ls docs/archive/adr/adr-0036-three-layer-service-protocol.md` 输出文件存在

## 5. P1-2 (Step 3): STATUS-GLOSSARY ADR-0036 状态标注

- [ ] 5.1 修订 `docs/adr-management/STATUS-GLOSSARY.md`: 从活跃 ADR 状态表移除 ADR-0036
- [ ] 5.2 在"已废弃"小节添加 ADR-0036: 标 `⛔ Superseded` + 引用 ADR-0045
- [ ] 5.3 验证: `grep "ADR-0036" docs/adr-management/STATUS-GLOSSARY.md` 仅命中"已废弃"段

## 6. P1-2 (Step 4-5): README + plugin/README 同步

- [ ] 6.1 修订 `docs/README.md`: 删除 ADR-0036 行 (若存在)
- [ ] 6.2 修订 `docs/adr/plugin/README.md`: 追加 ADR-0036 renumber 注记
- [ ] 6.3 验证: `git grep "adr-0036-three-layer-service-protocol" -- 'docs/README.md' 'docs/adr/plugin/README.md'` 输出为空

## 7. P1-2 (Step 6): 旧链接同步

- [ ] 7.1 跑 `git grep "adr-0036" -- 'docs/' 'openspec/'` 全面扫描
- [ ] 7.2 修订 `docs/adr/adr-0030-async-runtime-v2.md:318` 旧引用 (删除或更新到 ADR-0045)
- [ ] 7.3 修订 `docs/handoff/2026-07-06-architecture-completion.md:51` 旧引用
- [ ] 7.4 验证: `git grep "adr-0036-three-layer-service-protocol" -- 'docs/' 'openspec/'` 输出仅 `docs/archive/adr/` 路径

## 8. 验证 + Ship Gate

- [ ] 8.1 跑 `git status`: 验证变更文件清单 (6-8 个文档)
- [ ] 8.2 跑 `git diff --stat`: 验证 +X/-Y 行数合理 (~50-100 行)
- [ ] 8.3 跑 `openspec validate fix-adr-doc-alignment-hotfix-2026-07-08`: exit 0
- [ ] 8.4 跑 `git grep -E "🔍 Proposed|❌ Not Implemented" -- 'docs/adr-management/STATUS-GLOSSARY.md'`: 输出符合预期 (仅占位 + ADR-0030)
- [ ] 8.5 跑 `git grep "oopenspec" -- 'docs/'`: 输出仅 `docs/archive/` 或 git 历史
- [ ] 8.6 跑 `git grep "adr-0036-three-layer-service-protocol" -- 'docs/' 'openspec/'`: 输出仅 `docs/archive/adr/` 路径

## 9. Commit

- [ ] 9.1 `git add docs/adr-management/STATUS-GLOSSARY.md docs/adversarial-reviews/README.md docs/adversarial-reviews/decisions-2026-07-07.md docs/README.md docs/adr/plugin/README.md docs/adr/adr-0030-async-runtime-v2.md docs/handoff/2026-07-06-architecture-completion.md docs/archive/adr/adr-0036-three-layer-service-protocol.md`
- [ ] 9.2 `git status`: 验证 staged 文件清单
- [ ] 9.3 `git commit -m "docs: hotfix ADR document alignment (5 status fixes + ADR-0036 archive)"`
- [ ] 9.4 `git log --oneline -1`: 验证 commit hash

---

**总任务数**: 30 个 (9 个章节)
**总估时**: ~30 min
**Ship gate**: 8.3 OpenSpec validate exit 0 + 8.4-8.6 git grep 验证通过

---

## 任务依赖图

```
§1 STATUS-GLOSSARY 5 处状态
  └→ §5 ADR-0036 归档后状态更新
§2 README 拼写 (独立)
§3 D5 step + 签字 (独立)
§4 git mv ADR-0036 归档
  └→ §5 STATUS-GLOSSARY ADR-0036 状态
  └→ §6 README 同步
  └→ §7 旧链接同步
§8 验证 (依赖 §1-§7 全部)
§9 commit (依赖 §8 验证通过)
```

## Rollback 策略

```bash
# 单文件回滚
git restore docs/adr-management/STATUS-GLOSSARY.md
git restore docs/adversarial-reviews/README.md
git restore docs/adversarial-reviews/decisions-2026-07-07.md
git restore docs/README.md
git restore docs/adr/plugin/README.md
git restore docs/adr/adr-0030-async-runtime-v2.md
git restore docs/handoff/2026-07-06-architecture-completion.md

# ADR-0036 归档回滚
git mv docs/archive/adr/adr-0036-three-layer-service-protocol.md docs/adr/adr-0036-three-layer-service-protocol.md
```

**回滚影响**: 仅恢复文档到本 hotfix 前的状态, 无代码影响, 无 ctest 影响。
