# ADR-0058 Tool Schema Validation 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0058-tool-schema-validation.md](adr-0058-tool-schema-validation.md)
> **状态**: ✅ Approved (Phase 6a direction ADR)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6a 评审 2026-07-16 通过)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 3/5 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `Off` (enum) | 📅 Phase 6a Wave 2 待实施 | — | 与 ADR-0080-v1-2 D10 CaptureMode 三态相关, 当前 CaptureMode 三态命名不同 |
| `SchemaCache` | ✅ Shipped | `include/agenticdsl/tools/tool_schema_validator.h` (impl) | nlohmann validator schema 缓存已实现 |
| `ValidationLevel` | 🟡 Partial | ADR-0073 D4 `ValidationLevel` enum 后续扩展 | ADR-0073 D4 (Phase 6c C9 D2+D3+D4 ship 2026-08-18) 部分采纳 |
| `ToolSchemaValidator` | ✅ Shipped | `include/agenticdsl/tools/tool_schema_validator.h` | nlohmann JSON Schema 2020-12 验证器 |
| `ValidationError` | 📅 Phase 6a Wave 2 待实施 | — | 当前实现抛 std::invalid_argument, 未结构化 |

## 分类详情

### ✅ Shipped

`ToolSchemaValidator` + `SchemaCache` 已在 `include/agenticdsl/tools/tool_schema_validator.h` 实现。ADR-0073 (Tool JSON Schema Contract, ✅ Approved 🟡 Partial) D3 实施 ToolCoordinator 4 步校验层时复用本 ADR 设计。

### 📅 Deferred — Phase 6a Wave 2

`ValidationLevel` enum 与 ADR-0080 D10 CaptureMode 三态语义重叠, 需在 Phase 6a Wave 2 整合。
