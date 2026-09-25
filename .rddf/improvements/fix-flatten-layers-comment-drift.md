# fix-flatten-layers-comment-drift (PLACEHOLDER archived — D3 唯一遗留)

**优先级**: P3 (cosmetic docs drift) | **阶段**: 自由 (phase-n/a) | **分类**: Pattern 沉淀
**类型**: debt-tracking | **主题**: AGENTS.md Pattern #1 step 4 追加 "初判与确认根因分离" 子条目
**状态**: placeholder-archived (2026-09-25)
**生成时间**: 2026-09-25
**关联 ADR**: 无 (纯粹工程 pattern 沉淀, 非架构决策)
**原 OpenSpec change**: `openspec/changes/archive/2026-09-25-fix-flatten-layers-comment-drift-placeholder-cancelled/2026-09-18-fix-flatten-layers-comment-drift/` (PLACEHOLDER 状态, 2026-09-18 创建, 2026-09-25 归档)

## 背景

原 OpenSpec change 是 PLACEHOLDER 状态 (仅 proposal.md 117 行 + tasks.md 28 行 + spec.md 51 行), 2026-09-18 创建, 2026-09-25 归档。7 天未进展。

**触发根源**: F1 `fix-react-decide-empty-response` SHIPPED 后 (2026-09-18) 残留 drift。Oracle `ses_f4d05cdb0ffe0BhMdEADyfsdTz` 在 F1 SHIP 期间纠正初判根因 (flatten_layers 嵌套 → think 节点 LLM 空 text silent 穿透)。

**5 个 Acceptance 项当前状态** (Sprint 36 / 2026-09-25 grep 实证):
- ✅ D1 code comments audit (react/loop 路径无 flatten_layers 误引用)
- ✅ D2 plan docs alignment (master plan §四 L619-620 + §十一 L1139 + §三.5 L1163 均标 "**初判（错）**" + Oracle `ses_f4d05cdb0` cite)
- ✅ D4 test file comments (test_dsl_engine_ctx_bridge.cpp:3 已含 Oracle 真实根因注释)
- ✅ D5 docs drift gate (`openspec validate` 通过, "Change is valid")
- ⏸ **D3 AGENTS.md Pattern #1 step 4 "初判与确认根因分离" 子条目** —— 唯一未完成

**4/5 项已由 F1 SHIP 期间 Oracle/Metis 累计审计闭环** (per AGENTS.md Pattern #4 SHIP-with-fixes 流程 + Oracle session `ses_f4c6e14f` "中期审计直接发现并修正了 3 处 spec drift").

## Why (为什么现在归档)

1. **F1 SHIP 已闭环 80% 工作**: D1/D2/D4/D5 5 个 Acceptance 项中 4 个已被 F1 SHIP + Oracle 审计 闭环, change 主体目标已达成
2. **D3 边际价值递减**: 单 "AGENTS.md sub-bullet" 增量价值低于当前 P1/P2 工作 (DECISION B(a)/B(c)/C); 沉淀到 pattern 比创建 OpenSpec change 更经济
3. **避免 noise**: 空壳 proposal 污染 `openspec/changes/` active 列表, `active-status.md L21` 4 active 已包含此 change, 7 天未推进明显违反 WIP 上限 2 纪律
4. **Pattern #10 hygiene 沉淀 (per AGENTS.md 1022d49 DECISION D 同款)**: 用 `git mv` 而非删除, 确保 archived history 可追溯 (Day 5 4-file integrity lesson)

## Why (为什么现在不实施 D3)

- 当前 P1 工作流已被 Oracle 推荐路径占满：
  - DECISION B(a): ProviderLLMTool 空文本守卫 (1-2h, P2, F1 Latent Site #3) ✅ SHIPPED 2026-09-25
  - DECISION B(c): chat-real-llm-coverage Phase H (3-5h, P2, 6 E2E cases) ✅ SHIPPED 2026-09-25
  - DECISION C: Wave 3 Phase 2 D4 规划（独立 24h cooling-off） 立项 ✅ 2026-09-25
- Single-Dev 串行纪律：WIP 上限 2，本周无额外 slot
- D3 子条目是 docs-only 改动, 但需要独立思考时间 (Oracle 实战教训沉淀 ≠ 模板复制), 不适合夹在 P1 ship cycle 中"顺手做"

## Acceptance (重新启动条件)

- [ ] 任何独立"初判错 vs 确认根因"future case study 出现时 (例如 新 Sprint 中再次出现"先猜根因 → Oracle 纠正"模式)
- [ ] P1 工作流 (DECISION B/C 系列) 全部 ship 后, WIP 释放
- [ ] 用户手动 trigger (若认为"初判与确认根因分离"对未来重要)

## 实施内容 (D3 单 task, 估时 10-15 min)

如触发, 实施步骤:

1. 在 `AGENTS.md` Pattern #1 step 4 (line 181) 后追加子条目:
   ```
   5. **初判与确认根因分离** — 当调试流程中出现"先猜根因 + 后续 Oracle/Metis 纠正"模式时,
      **必须**显式标记初判描述 + cite 确认根因的 session/文档. 防"知识传递误传初判为最终结论"
      (正例: Oracle `ses_f4d05cdb0` 纠正 "flatten_layers 嵌套 → think 节点 LLM 空 text silent 穿透",
      master plan §四 L619-620 用 "**初判（错）**" 标签 + §十一 L1139 完整 ship 记录).
   ```
2. 验证 grep `flatten_layers` src/ tests/ docs/ 不返回误关联 (D1/D4 持续 gate)
3. git atomic commit + Reverse Indicator

## Reverse Indicator (R8 per AGENTS.md)

```
[Reverse Indicator]
+ new_up: 7-day PLACEHOLDER 信息保留至 improvement 跟踪 (D3 follow-up tracker)
- old_down: drop_ratio=0% (纯文档归档, 无行为变更)
- failure_traces: N/A (no code change)
- ablation: N/A (no Harness 变化)
- context_ids: N/A (docs-only change)
```

## 关联文档

- 原 placeholder (archived): `openspec/changes/archive/2026-09-25-fix-flatten-layers-comment-drift-placeholder-cancelled/2026-09-18-fix-flatten-layers-comment-drift/`
- F1 主 fix (D1-D4 闭环 source): `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/`
- Oracle 真实根因 session: `ses_f4d05cdb0ffe0BhMdEADyfsdTz`
- Oracle 中期审计 session (3 处 spec drift 修正): `ses_f4c6e14f`
- AGENTS.md Pattern #1 step 4 (D3 目标修改点): line 181
- AGENTS.md Pattern #4 (SHIP-with-fixes 流程): F1 ship 时已闭环 D1/D2/D4/D5 的源动力
- AGENTS.md Pattern #10 (hygiene): 本归档操作的源动力 (per 1022d49 DECISION D 同款)
- master plan F1 §四 L619-620 + §十一 L1139 (D2 已闭环实证)
- companion archived 1022d49 DECISION D 同款 PLACEHOLDERs:
  - `.rddf/improvements/fix-generate-subgraph-static-next.md` (GenerateSubgraphNode static-next 漂移)
  - `.rddf/improvements/intent-classification-router.md` (intent classification router 漂移)
