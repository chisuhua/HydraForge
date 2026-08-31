# ADR-0079 Unified Session 4-Scope 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0079-unified-session-4scope.md](adr-0079-unified-session-4scope.md)
> **状态**: ✅ Approved (Sprint 22 ship + v1.1 amendment 2026-08-12)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (v1.1 amendment 2026-08-12, Sprint 22 ship; Session 4-scope 数据模型实施)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 2/4 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `ConvergenceScore` | 📅 Phase 6a 待实施 | — | Sprint 22 ship 未实装, v2 待 Phase 6a |
| `SessionStore` | 🟡 Partial | `src/core/session_manager.cpp` (内部实现) | Session 4-scope storage 抽象未提取为独立类, v2 提取 |
| `Conversation` / `Attempt` / `Step` / `Execution` (4-scope) | ✅ Shipped | `src/core/types/session.h` | Sprint 22 ship, 4 层 Session 数据结构 |
| `ConvergenceEntry` | ✅ Shipped | `src/core/types/session.h` | v1.1 amendment 实装 |

## 分类详情

### ✅ Shipped (Sprint 22)

4-scope Session 模型 + ConvergenceEntry ship 2026-08-12, ADR-0079 v1.1 amendment 收录 ConvergenceEntry 决策。

### 📅 Deferred — Phase 6a

`ConvergenceScore` 计算 + `SessionStore` 抽象为独立接口待 Phase 6a。当前 Session 数据结构可工作, 抽象边界尚未固化。
