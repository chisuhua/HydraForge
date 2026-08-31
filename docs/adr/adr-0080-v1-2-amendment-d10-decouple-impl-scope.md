# ADR-0080 v1.2 Amendment D10 Decouple 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0080-v1-2-amendment-d10-decouple.md](adr-0080-v1-2-amendment-d10-decouple.md)
> **状态**: ✅ Approved (评审通过 2026-08-25, Oracle G12 解锁 ADR-0081/0082 死锁)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (评审通过 2026-08-25, Oracle G12 解锁)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 1/4 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `CaptureMode` (Online/Training/None) | ✅ Shipped | `include/agenticdsl/types/capture_mode.h` | ADR-0061-13 同步 ship 2026-08-29 |
| `Off` (capture disabled) | 🟡 Partial | CaptureMode::None 等价 | ADR-0080 v1.2 设计为独立 `Off` 状态, 实装为 `CaptureMode::None` 枚举值, 命名不同语义一致 |
| `ScrubHook` (decoupled) | 📅 Phase 6a 待实施 | — | ADR-0081 (IAgentHookRegistry) L2 已 ship 提供 hook 机制, 但未与 CaptureMode::Training 强制联动 |
| `DistillationCapture` | ✅ Shipped | `IDistillationWriter` 实现 | CaptureMode Training 时启用 capture |

## 分类详情

### ✅ Shipped

CaptureMode 三态 + IDistillationWriter 实现 ship, ADR-0061-13 (Distillation Output Format) 完整 ship 2026-08-29。

### 🟡 Partial — 命名差异

`Off` 状态实装为 `CaptureMode::None` 枚举值, 语义一致但命名不同。Phase 6a 整合时统一命名。

### 📅 Deferred — Phase 6a

ScrubHook 与 CaptureMode::Training 强制联动 (Training fail-open 三重保护) 当前通过 ADR-0081 hook 机制实现, 未在 CaptureMode 层强制。
