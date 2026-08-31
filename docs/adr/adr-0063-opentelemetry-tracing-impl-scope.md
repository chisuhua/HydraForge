# ADR-0063 OpenTelemetry Tracing 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0063-opentelemetry-tracing.md](adr-0063-opentelemetry-tracing.md)
> **状态**: ✅ Approved (Phase 6a direction ADR)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6a 评审 2026-07-16 通过)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 2/3 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `OpenTelemetryExporter` | 📅 Phase 6a Wave 2 待实施 | — | OTel OTLP 导出器 |
| `ConformanceLevel3` | 📅 Phase 6a Wave 2 待实施 | — | OTel conformance Level 3 |
| `TracingDecorator` | ✅ Shipped | `include/agenticdsl/llm/tracing_decorator.h` (impl) | ADR-0001 / Phase 5 C16 ship, emit `llm.request`/`llm.response` |

## 分类详情

### ✅ Shipped (本地 tracing)

`TracingDecorator` 已 ship, emit `llm.request` + `llm.response` 事件到 `IInteractionBus`, 27+ 主题覆盖完整 LLM 调用链。

### 📅 Deferred — Phase 6a Wave 2

OTel OTLP 导出器 + Conformance Level 3 实施待 Phase 6a Wave 2 (依赖 execution-baseline handoff + ADR-0073 schema complete)。
