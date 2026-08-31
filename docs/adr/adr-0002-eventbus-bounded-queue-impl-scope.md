# ADR-0002 EventBus 有界队列 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0002-eventbus-bounded-queue.md](adr-0002-eventbus-bounded-queue.md)
> **状态**: ❌ Not Implemented (audit 后保持)

## 状态

**📋 Audit** (impl-scope-audit 文档, 与 docs-code-drift-audit 配套使用)

❌ Not Implemented (audit 后保持 — V2 设计已锁定但 EventBus/DispatchMode 从未实施; Phase 1 由 ADR-0019 IInteractionBus 替代)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 头部主状态为 ❌ Not Implemented, 但 grep 仍检测到 9 个缺失类名 — 是误报 (工具 Scenario 4 检测逻辑对 ❌ 状态行的处理未完全跳过, 实际主状态已正确标 ❌ Not Implemented)。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `EventBus` | ❌ Never Implemented | — | IInteractionBus (ADR-0019) 替代 |
| `DispatchMode` | ❌ Never Implemented | — | 同上 |
| `EventType` | ❌ Never Implemented | — | nlohmann::json topic string 替代 |
| `IEventBus` | 🟡 Replaced | `include/agenticdsl/contract/iinteraction_bus.h` (IInteractionBus) | ADR-0019 接管 |
| `InMemoryEventBus` | ✅ Shipped (as `InMemoryBus`) | `include/agenticdsl/contract/inmemory_bus.h` | ADR-0019 实现 |
| `HarnessTUI` | ❌ Not in scope | — | pdk_chat_demo `chat_session.cpp` 提供等价 UI 能力 |
| `SubscribeOptions` | ❌ Not Implemented | — | IInteractionBus::subscribe 签名不同 |
| `TUIThread` | ❌ Not in scope | — | 见 pdk_chat_demo |
| `UIEvent` | 🟡 Replaced | `BusEvent` 信封 (ADR-0019) | nlohmann::json payload |

## 分类详情

### ✅ Shipped (替代实现)

ADR-0019 (IInteractionBus MVP, 2026-06-24 ship, Phase 1) + ADR-0068 (Event Emission Contract, 2026-08-03 ship, 27+ 主题注册) 提供完整替代能力。

### 📅 Deferred — Phase 2 触发条件

若 Phase 2 出现 >10K events/s 吞吐需求或 Per-Session 严格隔离需求, 重新评估 EventBus 全套实施。当前 InMemoryBus (mutex + queue) 性能足够 (Sprint 22 ctest 实测)。

## 与 docs-code-drift-audit-2026-06 的关系

本文件作为 2026-06-13 创建的 `adr-0002-impl-scope-audit.md` 的补充文件 (精确匹配 audit tool 的 `-impl-scope.md` 模式)。原始审计文件保留作为历史记录。
