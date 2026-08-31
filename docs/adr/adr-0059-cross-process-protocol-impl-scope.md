# ADR-0059 Cross-Process Protocol 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0059-cross-process-protocol.md](adr-0059-cross-process-protocol.md)
> **状态**: ✅ Approved (Phase 6a direction ADR)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6a 评审 2026-07-16 通过)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 3/5 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `RemoteAgentAdapter` | 📅 Phase 6a Wave 2 待实施 | — | 跨进程 agent 适配 |
| `RemoteTransportConfig` | 📅 Phase 6a Wave 2 待实施 | — | 传输配置 (gRPC/HTTP) |
| `TransportType` | 📅 Phase 6a Wave 2 待实施 | — | 传输类型枚举 |
| `IAgentComposition` | ✅ Shipped | `include/agenticdsl/contract/iagent_composition.h` | ADR-0060 实现 4 模式 (call/call_async/delegate/stream) |
| `IAgentRegistry` | ✅ Shipped | `include/agenticdsl/contract/iagent_registry.h` | ADR-0082 ✅ Approved 2026-08-21 ship V1 骨架 |

## 分类详情

### ✅ Shipped (替代)

`IAgentComposition` (ADR-0060 ✅ Approved) + `IAgentRegistry` (ADR-0082 ✅ Approved 2026-08-21) 已提供 in-process agent 通信契约。跨进程协议是 in-process 协议 + 传输层 (gRPC/HTTP) 扩展, 由 ADR-0077 (gRPC Data Plane, 🔍 Proposed Wave 4 descoped) 替代设计。

### 📅 Deferred — Phase 6a Wave 2 / Wave 4

跨进程协议实施依赖 ADR-0077 gRPC Data Plane ship (当前 Wave 4 descoped docs-only), 由 `from-roadmap-phase-6c-execution-baseline` + 后续 Phase 6c 触发条件推动。
