# ADR-0086 Credit Assignment Contract 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0086-credit-assignment-contract.md](adr-0086-credit-assignment-contract.md)
> **状态**: 🔍 Proposed (self-evolution §七 #6 立项, V1 不强制 ship, Axis6 Phase 1 启动前置)

## 状态

**📋 Audit** (impl-scope-audit 文档)

🔍 Proposed (2026-08-31 立项, 取代过期 `adr-0085-credit-assignment-contract.md` 文件名建议, per self-evolution §一 边界 + §七 #6; V1 形式为 spike + ADR 不强制 ship, Axis6 chain Phase 1 启动 blocker)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 6/7 个描述的类未在 src/include 中找到。**误报**: 主状态实为 🔍 Proposed, 非 ✅ Approved。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `AttributionMethod` | 📅 V1 待实施 | — | 归因方法枚举 (VersionPairDiff V1) |
| `AttributionRecord` | 📅 V1 待实施 | — | 归因记录 |
| `AttributionVerdict` | 📅 V1 待实施 | — | 4 态判定 (Attributed/Confounded/Insufficient/NotAttempted) |
| `ConfounderRecord` | 📅 V1 待实施 | — | 混杂分层记录 |
| `IEvaluator` (already shipped) | ✅ Shipped | `include/agenticdsl/contract/ievaluator.h` | ADR-0083 ✅ Approved 2026-08-26 ship |
| `Kind` (enum) | 📅 V1 待实施 | — | ConfounderRecord 子枚举 |
| `NotAttempted` (enum value) | 📅 V1 待实施 | — | AttributionVerdict 4 态之一 |

## 分类详情

### ✅ Shipped (前置基础)

ADR-0083 IEvaluator/RewardSignal ✅ Approved 2026-08-26 ship, 提供评估信号契约, 是 ADR-0086 归因层的前置 (评估 vs 归因是不同接口, 但同一事件源)。

### 📅 V1 待实施 (spike + ADR 形式)

self-evolution §一 边界明确: "信用分配契约不是 HydraForge 已批准的通用能力"。self-evolution §六 S4 promotion criteria 要求信用分配独立评审。当前 ADR-0086 是 spike + ADR 形式, V1 不强制 ship; 触发条件: Axis6 chain Phase 1 启动 (commit `bc157fb` v1.1-draft.3 决策 4 显式声明)。

### Phase 6a 实施路径

待 Phase 6a 单主体评估 + 多主体协同 (S4 promotion criteria) 通过后, ADR-0086 正式 ship。当前 0/0 tasks, OpenSpec change 待立项。
