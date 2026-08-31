# ADR-0057 Agent Lifecycle 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0057-agent-lifecycle.md](adr-0057-agent-lifecycle.md)
> **状态**: ✅ Approved (Phase 6a direction ADR)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6a 评审 2026-07-16 通过)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 3/3 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `AgentLifecyclePayload` | 📅 Phase 6a Wave 2 待实施 | — | Lifecycle 事件 payload |
| `AgentDescriptor` | 📅 Phase 6a Wave 2 待实施 | — | 与 ADR-0053 共享 |
| `LOADED` (enum) | 📅 Phase 6a Wave 2 待实施 | — | Lifecycle state machine enum |

## 分类详情

### 📅 Deferred — Phase 6a Wave 2

ADR-0057 是 Phase 6a Agent Lifecycle 方向 ADR。当前 `examples/pdk_chat_demo/` 提供等价 lifecycle 能力 (`chat_session.cpp`),但未抽象为 Phase 6a 全系统契约。子 Change 待 Wave 2 启动。
