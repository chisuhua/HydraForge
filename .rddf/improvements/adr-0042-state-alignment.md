# adr-0042-state-alignment

**优先级**: P2 | **来源**: governance（C16 ship 实证反映到 ADR 状态字段）
**阶段**: post-6c | **分类**: governance
**类型**: governance
**主题**: ADR-0042 状态翻转 🔍 Proposed → 🟡 Partial；C16 部分实施已 ship

## 架构依据

C16 `phase5-illmprovider-call-chain-v2` (2026-07-09) 已 ship ADR-0042 提出的 5 项决策：
1. Decorator 链（CostTrackingDecorator + ComplianceDecorator + RateLimitDecorator）
2. Dual Consumer Model（OrchestrationILLMProvider 直连）
3. `available_models()` pure virtual
4. PluginLoader V2（ABI v2）
5. LlamaAdapter deprecated 标注

但 ADR-0042 状态字段仍为 🔍 Proposed（与实际不符）。本 change 同步状态字段到 🟡 Partial。

## 范围

- **In Scope**:
  - ADR-0042 状态字段更新
  - docs/README.md 同步
  - docs/adr-management/relationships.md 重新生成
  - proposal-approved.md 收录
  - iteration.json +1 entry

- **Out of Scope**:
  - ADR-0042 内容改动（仅状态字段）
  - C17+ 演进路径（待独立 change）

## Why

ADR 状态与实际代码不一致 → 治理元数据失真 → capability-application-map / gap-analysis 等下游工具误判。

## What Changes

- **修改** `docs/adr/adr-0042-illmprovider-evolution-path.md` 状态字段（+历史）
- **修改** `docs/README.md` 表格行
- **重新生成** `docs/adr-management/relationships.md`
- **修改** `proposal-approved.md` 收录
- **修改** `.rddf/state/iteration.json` +1 entry

## Acceptance

- [ ] ADR-0042 状态 = 🟡 Partial + 历史段含 3 行（2026-07-06 Proposed / 2026-07-09 C16 / 2026-09-03 Partial）
- [ ] docs/README.md 表格行 = 🟡 Partial (C16 部分 ship)
- [ ] docs/adr-management/relationships.md 重新生成后 git diff 有变化
- [ ] zero 代码改动（纯 governance 元数据）
- [ ] Oracle review 5/5 PASS
