## Why

ADR-0072 DSL 节点扩展当前状态为 🟡 Partial (2026-09-02)，但该状态翻转**未走完整 single-developer 治理流程**——`openspec/changes/` 中无对应 change 目录，GitHub issue + Self-Review Checklist 12 项 + 24h cooling-off 证据链全部缺失。这构成治理先例风险：下次"先实施后补票"将无心理门槛，single-dev 模式治理机制退化为"事后追认"。本 change 补建翻牌治理证据链，关闭 Phase 6c 收官后的"实施先于翻牌"治理异常。

## What Changes

- **新增** `openspec/changes/<name>/proposal.md` (本文件)
- **新增** `openspec/changes/<name>/design.md` (spec delta: ADR-0072 状态从 🔍 Proposed → 🟡 Partial)
- **新增** `openspec/changes/<name>/tasks.md` (5 步治理动作结构)
- **新增** `openspec/changes/<name>/specs/adr-0072/spec.md` ("ADR-0072 status update" scenario)
- **新增** `.rddf/improvements/adr-0072-flip-to-partial.md` (改进提案)
- **新增** GitHub issue 用 `.github/ISSUE_TEMPLATE/adr-review.md` 模板
- **修改** `proposal-approved.md` 收录本提案 (✅ 已完成)
- **修改** `proposal-suggestions.md` §3.4 标记治理补建
- **修改** `openspec/changes/archive/<archive-name>/` (cooling-off 完成后 archive)
- **修改** `.rddf/state/iteration.json` (新增 archived entry)

**Non-goals** (显式范围边界):

- **不**实施 ADR-0072 D1+D4 代码改动（→ Change #3+#4 阶段 A，本 change 仅补治理证据）
- **不**再次翻牌 ADR-0072（保持 🟡 Partial 不再翻，避免反复）
- **不**修改 `docs/adr/adr-0072-dsl-node-extensions.md` §状态字段（已翻牌，本次仅补证据链）
- **不**修复其他 ADR 的"实施先于翻牌"异常（仅 ADR-0072，其他留 follow-up）

## Capabilities

### New Capabilities

- `adr-0072-status-update`: ADR-0072 状态从 🔍 Proposed 翻牌至 🟡 Partial 的治理证据链补建 — 包含 issue 创建、Self-Review Checklist 12 项勾选、24h cooling-off、archive 完整流程的契约层记录。

### Modified Capabilities

_(无现有 spec 的 REQUIREMENTS 变化；本 change 仅是治理证据补建，不改变任何 spec-level 行为)_

## Impact

- **代码**: zero 代码改动（纯治理动作）
- **ADR**: ADR-0072 状态字段**不变**（已 2026-09-02 翻牌），本 change 仅补证据链
- **OpenSpec**: 新增 1 个 change → archive 后 `openspec/changes/archive/2026-09-02-adr-0072-flip-to-partial/`
- **iteration.json**: +1 archived entry
- **依赖**: 无代码依赖；治理依赖 single-dev 模式 24h cooling-off 机制
- **风险**: 
  - 治理缺口不补 = 下次类似翻牌无门槛
  - Phase 7a 复评机制可信度受连带影响
  - cooling-off 期间不阻塞 #2/#5/#10/#3/#4 并行启动
