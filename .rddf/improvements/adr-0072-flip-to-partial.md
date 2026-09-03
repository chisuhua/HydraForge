# adr-0072-flip-to-partial

**优先级**: P0 | **来源**: governance（ADR-0072 翻牌治理补建）
**阶段**: post-6c | **分类**: governance
**类型**: governance
**主题**: ADR-0072 翻牌治理证据链补建;实施先于翻牌异常关闭

## 架构依据

ADR-0072 DSL 节点扩展当前状态为 🟡 Partial (2026-09-02)，但该状态翻转未走完整 single-developer 治理流程：

- ADR-0072 §状态已翻牌（🔍 Proposed → 🟡 Partial）
- 但 `openspec/changes/2026-09-02-adr-0072-flip-to-partial/` 目录**不存在**
- GitHub issue + Self-Review Checklist + 24h cooling-off 治理证据链**断掉**

per `roadmap.md` Q1 翻牌记录 + Oracle 5/5 审查通过 + Metis 评级 B 标识的治理缺口：
- 治理证据缺口 → 下次"先实施后补票"无心理门槛
- Phase 7a 复评依赖治理机制可信度
- 翻牌 OpenSpec change 必须存在 = single-dev 模式 24h cooling-off 唯一证据载体

## 范围

- **In Scope**:
  - 创建 `openspec/changes/2026-09-02-adr-0072-flip-to-partial/` 完整 artifacts（proposal.md + design.md + tasks.md + specs/）
  - 创建 `.rddf/improvements/adr-0072-flip-to-partial.md`（本文件）
  - 创建 GitHub issue（用 `.github/ISSUE_TEMPLATE/adr-review.md` 模板）
  - 勾选 Self-Review Checklist 12 项（per `docs/architecture/adr-self-review-checklist.md`）
  - 等 24h cooling-off
  - archive change 至 `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/`
  - 同步 `iteration.json` 新增 archived entry
  - 同步 `proposal-suggestions.md` §3.4 标记本次治理补建完成

- **Out of Scope**:
  - ADR-0072 状态再次翻转（保持 🟡 Partial 不再翻）
  - ADR-0072 D1+D4 实施（→ Change #3+#4-阶段A）
  - 任何代码改动（纯治理动作）

## 关键场景

1. **治理证据链重建**:
   - Given: ADR-0072 当前 🟡 Partial，但 OpenSpec change 不存在
   - When: 创建 change + 走 5 步流程 + cooling-off + archive
   - Then: `openspec/changes/archive/` 有完整目录，治理证据链可追溯

2. **Self-Review 12 项勾选**:
   - Given: 12 项 ADR 自审清单
   - When: 按清单逐项验证 ADR-0072 翻牌决策
   - Then: 12 项全 ✅ + issue 评论留痕

3. **24h Cooling-off 不阻塞并行工作**:
   - Given: 7 个 change 中 #1 需 24h 等待
   - When: Day 1 创建 issue + Day 1-2 cooling-off + Day 2 archive
   - Then: 期间 #2/#5/#10/#3/#4 可并行启动

## 关键决策

1. **change name**: `2026-09-02-adr-0072-flip-to-partial`（per roadmap 命名约定 YYYY-MM-DD-<topic>）
2. **archive 路径**: `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/`（与其他 archive 一致）
3. **Self-Review 模板**: `.github/ISSUE_TEMPLATE/adr-review.md`（per 2026-08-25 governance）
4. **cooling-off 时机**: Day 1 周一创建 → Day 2 周二 archive（sprint 内自然跨日）

## Why

ADR-0072 翻牌是 Phase 6c 收官后的治理异常——"实施先于翻牌"。如果不补建 OpenSpec change + 24h cooling-off + Self-Review 证据链：
- single-dev 模式治理的"24h cooling-off"机制失去强制力
- 下次类似翻牌可跳过完整流程，治理退化为"事后再补"
- Phase 7a 复评机制（依赖 control-plane-eval.py）的可信度受连带影响

本 change 是 Sprint 25 的治理底线，必须最先启动（cooling-off 跨 Day 1-2 不阻塞其他 change 并行）。

## What Changes

- **新增** `openspec/changes/2026-09-02-adr-0072-flip-to-partial/proposal.md`（翻牌决策记录）
- **新增** `openspec/changes/2026-09-02-adr-0072-flip-to-partial/design.md`（spec delta：ADR-0072 状态变更）
- **新增** `openspec/changes/2026-09-02-adr-0072-flip-to-partial/tasks.md`（5 步 TDD 结构）
- **新增** `openspec/changes/2026-09-02-adr-0072-flip-to-partial/specs/adr-0072/spec.md`（"ADR-0072 status update" scenario）
- **新增** `.rddf/improvements/adr-0072-flip-to-partial.md`（本文件）
- **新增** GitHub issue（adr-review 模板）
- **修改** `.rddf/state/iteration.json`（自动同步 +1 archived entry）
- **修改** `proposal-suggestions.md` §3.4 标记治理补建完成
- **修改** `openspec/changes/archive/` 目录（archive 完成后）

## Acceptance

- [ ] `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/` 目录存在
- [ ] 4 个 artifacts 完整（proposal.md + design.md + tasks.md + specs/adr-0072/spec.md）
- [ ] GitHub issue 状态 = Closed + 12 项 Self-Review Checklist 全勾选
- [ ] `openspec validate --strict` exit 0
- [ ] `python3 tools/adr_lint.py` 0 errors
- [ ] `iteration.json` 新增 archived entry（status=archived, archived_at=2026-09-02）
- [ ] cooling-off ≥24h（issue 创建时间 vs archive 时间差 ≥ 86400s）
- [ ] `proposal-suggestions.md` §3.4 标记本次治理补建完成
- [ ] Oracle review 5/5 PASS（治理证据链 + spec delta + cross-file 一致性）
- [ ] zero 代码改动（git diff 仅含新增 .md/.json 文件 + proposal-suggestions 标记）
