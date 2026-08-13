# design.md — adr-0073-d2-declare-tool-v3

## Overview

This change implements ADR-0073 D2 (ToolMetadata V3) and D4 (DECLARE_TOOL V3 auto schema generation).

## Design Decisions

### D2: ToolMetadata V3 — Additive Extension

```cpp
struct ToolMetadata {
  // V2 fields (existing, unchanged)
  std::string name;
  std::string description;
  std::string domain;
  ToolCategory category;
  LayerProfile min_layer;
  ApprovalPolicy approval;
  std::vector<LayerProfile> allowed_layers;
  double cost_estimate = 0.0;
  int timeout_ms = 30000;

  // V3 new fields (additive, std::optional)
  std::optional<nlohmann::json> input_schema;
  std::optional<nlohmann::json> output_schema;
  enum class ValidationMode { Strict, Warn, Ignore };
  ValidationMode validation_mode = ValidationMode::Strict;
};
```

### D4: DECLARE_TOOL_V3 Macro

**Macro signature** (additive, backward-compatible with V2):

```cpp
// V2 signature (unchanged, still works)
DECLARE_TOOL(name, description, category, approval_policy, body...)

// V3 signature (new, optional schema params)
DECLARE_TOOL(name, description, category, approval_policy, 
             /* InputSchema */ FsReadArgs,
             /* OutputSchema */ FsReadResult,
             body...)
```

**Schema Generation Rules** (ADR-0073 Table):

| C++ Type | JSON Schema Type | Additional Constraints |
|----------|-----------------|----------------------|
| `std::string` | `string` | `minLength`/`maxLength`/`pattern` |
| `int`, `long`, `size_t` | `integer` | `minimum`/`maximum` |
| `float`, `double` | `number` | `minimum`/`maximum` |
| `bool` | `boolean` | — |
| `std::vector<T>` | `array` | `items`/`minItems`/`maxItems` |
| `std::optional<T>` | (type of T) | `required` field if value required |
| `std::map<std::string, T>` | `object` | `additionalProperties` |
| `enum class E` | `string` | `enum: ["val1", "val2", ...]` |
| struct | `object` | nested `properties` |

### Schema Generation Helper

`include/agenticdsl/tools/schema_generation.h` provides:

```cpp
template<typename T>
struct SchemaGenerator {
  static nlohmann::json to_schema();
};

// Specializations for all supported types
// User can provide custom specializations for their types
```

## File Changes

1. `src/common/policy/execution_policy.h` — Add V3 fields to ToolMetadata
2. `include/agenticdsl/tools/schema_generation.h` — New file with type reflection helpers
3. `include/agenticdsl/pdk/tool_macros.h` — Add DECLARE_TOOL_V3 macro
4. `tests/test_declare_tool_auto_schema.cpp` — New test file
5. `docs/adr/adr-0073-tool-json-schema-contract.md` — Update status for D2 partial ship
6. `docs/adr/adr-0073-impl-scope-audit.md` — Mark D2 as Partial

## Backward Compatibility

- V2 DECLARE_TOOL calls compile unchanged (V3 fields are optional with defaults)
- Existing ToolMetadata V2 code continues to work
- No changes to tool_coordinator.cpp or validation layer
