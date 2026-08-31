# ADR-0054 Capability Discovery 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0054-capability-discovery.md](adr-0054-capability-discovery.md)
> **状态**: ✅ Approved (Phase 6a direction ADR)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6a 评审 2026-07-16 通过, 14 个 Phase 6 ADR 系列之一)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 3/5 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `AgentCapability` | 📅 Phase 6a Wave 2 待实施 | — | 描述 agent 能力描述符 |
| `CapabilityRegistry` | 📅 Phase 6a Wave 2 待实施 | — | 描述能力查询/注册 API |
| `QueryOptions` | 📅 Phase 6a Wave 2 待实施 | — | 描述能力查询过滤参数 |
| `ICapabilityProvider` | ✅ Partial | `include/agenticdsl/contract/iagent_registry.h` (复用 IAgentRegistry) | IAgentRegistry 提供等价查询能力 |
| `IDiscoveryService` | 📅 Phase 6a Wave 2 待实施 | — | 描述服务发现 |

## 分类详情

### ✅ Partial Shipped

`IAgentRegistry` (ADR-0082 ✅ Approved 2026-08-21 ship) 提供 agent-level 查询/注册能力, 与 `CapabilityRegistry` 设计目标重叠。Phase 6a Wave 2 实施时需决策: 是否抽象 `CapabilityRegistry` 为独立 service 或扩展 IAgentRegistry。

### 📅 Deferred — Phase 6a Wave 2

ADR-0054 与 ADR-0053 同期设计, 同属 Phase 6a Agent Manifest 系列, 子 Change 待 Wave 2 启动。
