# ADR-0073 Tool JSON Schema Contract 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0073-tool-json-schema-contract.md](adr-0073-tool-json-schema-contract.md)
> **状态**: 🟡 Partial (Phase 6c C9 D2+D3+D4 已 ship, D1 待 Phase 6a)

## 状态

**📋 Audit** (impl-scope-audit 文档)

🟡 Partial — Phase 6c C9 (from-roadmap-phase-6c-schema-complete) ✅ Shipped 2026-08-18 实现 D2 + D3 + D4; D1 (Tool JSON Schema 自动生成) 待 Phase 6a 实施

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (D2 + D3 + D4 全 ship), 但 2/8 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `ToolSchemaValidator` | ✅ Shipped | `include/agenticdsl/tools/tool_schema_validator.h` | D3 实装 |
| `DECLARE_TOOL` (auto-gen schema) | ✅ Partial | `include/agenticdsl/pdk/tool_macros.h` | D2 部分自动生成 input_schema, output_schema 待 |
| `FsReadArgs` | 📅 Phase 6a 待实施 | — | D1 example schema, 待 Phase 6a |
| `SchemaValidationError` | 📅 Phase 6a 待实施 | — | 当前实现抛 std::invalid_argument, 结构化错误类待 |
| `InputSchema` / `OutputSchema` | 🟡 Partial | ToolMetadata V2 字段 | D2 部分实装, JSON Schema 2020-12 完整子集 |
| `ValidationLevel` | 📅 Phase 6a 待实施 | — | ADR-0058 与本 ADR 重叠, Phase 6a 整合 |

## 分类详情

### ✅ Shipped (Phase 6c C9)

ADR-0073 D2 (input_schema 自动生成) + D3 (ToolCoordinator 4 步校验层) + D4 (JSON Schema 2020-12 nlohmann validator 校验) 全 ship。ADR-0073 状态已从 🔍 Proposed 提升为 🟡 Partial (cap-map §一)。

### 📅 Deferred — Phase 6a

D1 (完整 DECLARE_TOOL 自动生成双向 schema) + 结构化错误类待 Phase 6a Wave 2。ADR-0073 D1 决策与 ADR-0061-03 SkillCompiler (✅ Shipped) 集成待评估。
