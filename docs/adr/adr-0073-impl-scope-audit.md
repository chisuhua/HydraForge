# ADR-0073 实现范围审计 (Implementation Scope Audit)

## 状态

**📋 Partial (证据基础，Phase 6a W1)** (2026-08-13) — 本文件是 ADR-0073 实施范围审计，记录 Phase 6a PDK manifest schema 边界的部分采纳证据。状态变更历史见头部备注段落。

> **🟡 Partial (证据基础，Phase 6a W1)** (2026-08-13) — 本文件由 OpenSpec change `adr-0073-partial-flip` 创建。
> 状态：证据基础 Partial——D1 在 PDK manifest 边界部分采纳（manifest.h string 字段 + manifest_validator.cpp 字符串校验），D2/D3/D4 属于 Phase 6c C8/C9 待实施，D5/D6 随 D2/D3 实施。
> **本审计不修改任何 C++ 或测试源代码。**

## ADR 描述（引用 ADR-0073 原文要点）

ADR-0073 (Tool JSON Schema 契约) 描述：
- **D1**: 采用 JSON Schema 2020-12 作为 Tool 契约标准
- **D2**: ToolMetadata V3 — additive 扩展（input_schema / output_schema / ValidationMode）
- **D3**: 运行时校验 — 在 ToolCoordinator 4 步 sanitization pipeline 中
- **D4**: PDK DECLARE_TOOL 宏自动生成 schema（C++ 类型反射）
- **D5**: 向后兼容 — V2 工具无需 schema
- **D6**: Schema 版本与兼容性

## 代码实际状态（grep 验证 2026-08-13）

### 验证命令

```bash
# D1: manifest schema evidence
rg -n "input_schema|output_schema" include/agenticdsl/pdk/manifest.h
# → manifest.h:17-18: std::string input_schema; std::string output_schema; (raw JSON string)

# D2: ToolMetadata V3 — NOT implemented
rg -n "input_schema|output_schema" src/common/policy/execution_policy.h
# → 0 matches — ToolMetadata V2 only, no input_schema/output_schema fields

# D3: ToolCoordinator schema validation — NOT implemented
rg -n "JSON Schema|ToolSchemaValidator|ERR_SCHEMA_VALIDATION" src/common/tools/tool_coordinator.cpp
# → 0 matches

# D4: DECLARE_TOOL V3 — NOT implemented
rg -n "input_schema|output_schema|auto.*schema" include/agenticdsl/pdk/tool_macros.h
# → 0 matches for V3 features
```

### 实际状态

| ADR-0073 决策 | 状态 | 证据 |
|---|---|---|
| **D1**: JSON Schema 2020-12 adoption | 🟡 **Partial（manifest 边界）** | `manifest.h:17-18` — `input_schema`/`output_schema` 作为 `std::string`（raw JSON）存在于 PDK manifest 结构；`manifest_validator.cpp` 校验字段存在且为非空字符串；但**不做 JSON Schema 2020-12 内容校验**；这不是 ToolMetadata V3（见下） |
| **D2**: ToolMetadata V3 结构 | ❌ **Not implemented** | `execution_policy.h:66-81` — ToolMetadata 仅含 V2 字段（name/description/domain/category/min_layer/approval/allowed_layers/cost_estimate/timeout_ms）；无 `input_schema`/`output_schema`/`validation_mode` 字段；`include/agenticdsl/tools/` 中无 schema_validator.h |
| **D3**: 运行时校验（ToolCoordinator） | ❌ **Not implemented** | `src/common/tools/tool_coordinator.cpp` — 无 JSON Schema 校验逻辑；无 `ERR_SCHEMA_VALIDATION` 错误码；4 步 pipeline 不含 schema 校验步骤 |
| **D4**: DECLARE_TOOL V3 自动生成 | ❌ **Not implemented** | `include/agenticdsl/pdk/tool_macros.h` — DECLARE_TOOL 宏仍是 V2 签名（name/description/category/approval/body）；无 schema 自动生成参数或 C++ 类型反射 |
| **D5**: V2 向后兼容 | ⏸ **Pending** | 依赖 D2 实施后方可验证 |
| **D6**: Schema 版本与兼容性 | ⏸ **Pending** | 依赖 D2/D3 实施后方可验证 |

### 关键观察

1. **D1 的 manifest 边界 ≠ ToolMetadata V3**: `manifest.h` 的 `input_schema`/`output_schema` 字段是**插件清单级别**的字符串字段（供 manifest_validator.cpp 做非空字符串校验），**不是** `execution_policy.h` 中 `ToolMetadata` 结构体的 JSON Schema 字段。两者在不同的抽象层。

2. **D2/D3/D4 属于 Phase 6c C8/C9**: 这些决策需要：
   - `ToolMetadata` 结构体增加 `std::optional<nlohmann::json>` 字段
   - `ToolSchemaValidator` 包装类（基于 nlohmann/json_schema_validator）
   - `tool_coordinator.cpp` 插入 schema 校验步骤
   - `tool_macros.h` 增加 C++ 类型反射自动生成逻辑

3. **manifest_validator.cpp 当前只做字符串非空校验**: 虽然 ADR-0073 D1 声称 "JSON Schema 2020-12"，但实现只检查字段是否为非空字符串，**不解析/校验 JSON Schema 内容**。这符合 Phase 6a scope。

4. **Phase 6c 完整实施路径**: C8（DECLARE_TOOL V3）+ C9（ToolCoordinator 校验层）完成后，D1 将在运行时校验层再次出现（但 ToolMetadata 级别而非 manifest 级别）。

## 后续

| 决策 | 后续工作 | 所属阶段 |
|---|---|---|
| D1 | manifest_validator.cpp 增加 JSON Schema 内容校验（可选，Phase 6a 范围外） | Phase 6a |
| D2 | ToolMetadata V3 结构体扩展 | Phase 6c C8 |
| D3 | ToolCoordinator 4 步 sanitization pipeline 插入 schema 校验 | Phase 6c C9 |
| D4 | DECLARE_TOOL V3 C++ 类型反射自动生成 | Phase 6c C8 |
| D5 | V2 向后兼容验证 | Phase 6c C8-C9 后 |
| D6 | Schema 兼容性测试套件 | Phase 6c C8-C9 后 |

## 验证证据

```bash
# D1 partial evidence (manifest boundary)
grep -n "input_schema\|output_schema" include/agenticdsl/pdk/manifest.h
# 17:   std::string input_schema;                  // JSON Schema 2020-12
# 18:   std::string output_schema;                 // JSON Schema 2020-12

# D2 NOT implemented (ToolMetadata stays V2)
grep -n "input_schema\|output_schema" src/common/policy/execution_policy.h
# (no output — ToolMetadata has no schema fields)

# D3 NOT implemented (no schema validation in ToolCoordinator)
grep -n "JSON Schema\|ToolSchemaValidator\|ERR_SCHEMA_VALIDATION" src/common/tools/tool_coordinator.cpp
# (no output — no schema validation logic)

# D4 NOT implemented (DECLARE_TOOL stays V2)
grep -n "input_schema\|output_schema" include/agenticdsl/pdk/tool_macros.h
# (no output — no V3 features)
```

---

*文档版本: v1.0*
*创建日期: 2026-08-13*
*作者: HydraForge 架构组*
*状态: 🟡 Partial (证据基础，Phase 6a W1)*
