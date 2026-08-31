# ADR-0080 AppendOnly Event Log 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0080-append-only-event-log.md](adr-0080-append-only-event-log.md)
> **状态**: ✅ Approved (v1.1 amendment 2026-08-12)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (v1.1 amendment 2026-08-12 决策 Step 0 + D2 + D6 + D10 全部 ship; 当前 ADR-TRACKING-01 warning: 24h+ 无 tracking OpenSpec change, 见 B6 修正项)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 1/7 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `EngineConfig` | 📅 Phase 6a 待实施 | — | 当前 EngineConfig 实装在 `src/core/types/budget.h`, 未提取为 ADR-0080 专属 |
| `BusEvent` (envelope) | ✅ Shipped | `include/agenticdsl/contract/bus_event.h` | ADR-0019 + ADR-0068 v1.4 |
| `EventLog` | ✅ Shipped | `src/modules/event_log/event_log.cpp` | ADR-0080 Step 0 ship |
| `EventBuilder` | ✅ Shipped | `include/agenticdsl/contract/event_builder.h` | ADR-0068 V2 ship 2026-08-03 |
| `IDistillationWriter` | ✅ Shipped | `include/agenticdsl/contract/idistillation_writer.h` | ADR-0061-13 ✅ Shipped 2026-08-29 |
| `CaptureMode` (3-state) | ✅ Shipped | `include/agenticdsl/types/capture_mode.h` | ADR-0080 v1.2 amendment + ADR-0061-13 |
| `FileDistillationWriter` | ✅ Shipped | `src/modules/distillation/file_writer.cpp` | ADR-0061-13 Phase 1 ship |

## 分类详情

### ✅ Shipped

EventLog + EventBuilder + IDistillationWriter + CaptureMode + FileDistillationWriter 全部 ship, 7 环蒸馏闭环 (cap-map §一 #31) 完整闭环。

### 📅 Deferred — Phase 6a

`EngineConfig` 抽象为 ADR-0080 专属类型待 Phase 6a。当前 `src/core/types/budget.h::ExecutionBudget` 提供等价配置能力。

## ADR-TRACKING 状态

`tools/adr_lint.py` 当前 WARNING [ADR-TRACKING-01]: ADR-0080 ✅ Approved 2026-08-12 已超 24h, 但 openspec/changes/ 无含 '0080' 的 tracking change 目录。建议: (1) 创建 OpenSpec change `2026-08-31-adr-0080-eventlog-tracking`, 或 (2) 头部加 '⏳ tracking: docs-only', 或 (3) 加豁免。**B6 修正项待 W2 处理**。
