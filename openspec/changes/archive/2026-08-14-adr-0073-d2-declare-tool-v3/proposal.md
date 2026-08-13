# adr-0073-d2-declare-tool-v3

## Why

ADR-0073 D2 (ToolMetadata V3) and D4 (DECLARE_TOOL V3) are the next concrete ships in Phase 6c C8. The previous P0 item `adr-0073-partial-flip` established that D1 (JSON Schema 2020-12) is partially adopted in PDK manifest, but D2 (ToolMetadata V3 structure), D3 (ToolCoordinator validation), and D4 (DECLARE_TOOL auto schema generation) are not implemented.

This change implements D2 (ToolMetadata V3 fields) and D4 (DECLARE_TOOL V3 with C++ type reflection to JSON Schema 2020-12), enabling plugin authors to supply typed C++ structs and have the macro emit JSON Schema automatically.

## What Changes

**In Scope**:

- Update `src/common/policy/execution_policy.h`: Add `input_schema`, `output_schema`, `validation_mode` fields to ToolMetadata (V3 additive extension)
- Create `include/agenticdsl/tools/schema_generation.h`: C++ type reflection helpers (string, int, float, bool, vector, optional, enum class, struct → JSON Schema 2020-12)
- Update `include/agenticdsl/pdk/tool_macros.h`: Add DECLARE_TOOL_V3 macro with optional input/output schema parameters; V2 macro calls remain unchanged
- Create `tests/test_declare_tool_auto_schema.cpp`: Test cases for 4 representative types (string, int, optional<T>, struct), plus backward-compat test for V2 macro calls

### Key Scenarios

- GIVEN a plugin author defines a C++ struct `FsReadArgs { std::string path; int max_lines = 0; bool include_hidden = false; }`
  WHEN they use DECLARE_TOOL_V3 with that struct type
  THEN the macro auto-generates a JSON Schema 2020-12 input_schema with correct types and constraints
- GIVEN an existing V2 DECLARE_TOOL call site
  WHEN the code is compiled with the V3 macro definitions
  THEN it compiles unchanged and emits no schema (backward compatible)

## Out of Scope

- `src/common/tools/tool_coordinator.cpp` (D3 ToolCoordinator validation layer — Phase 6c C9)
- `docs/specs/dsl.md` §6.2 (schema signature — Phase 6c C8/C9)
- ToolCoordinator schema validation logic
- Schema validator implementation (nlohmann/json_schema_validator wrapper)

## Capabilities

- MUST keep existing V2 DECLARE_TOOL macro calls working unchanged
- MUST generate valid JSON Schema 2020-12 output
- MUST support at least 4 representative types: string, int, optional<T>, struct
- MUST NOT modify tool_coordinator.cpp behavior
- MUST NOT break any existing tests

## Acceptance

- [ ] `src/common/policy/execution_policy.h` has ToolMetadata V3 fields (input_schema, output_schema, validation_mode)
- [ ] `include/agenticdsl/tools/schema_generation.h` exists with type-to-schema helpers
- [ ] `include/agenticdsl/pdk/tool_macros.h` has DECLARE_TOOL_V3 with schema generation
- [ ] `tests/test_declare_tool_auto_schema.cpp` covers string, int, optional<T>, struct cases
- [ ] V2 macro calls compile unchanged (backward-compat test)
- [ ] `ctest --output-on-failure` passes all tests
- [ ] `tools/adr_lint.py` exits 0
- [ ] `tools/docs_drift_audit.py` shows 0 new DRIFT items
- [ ] `openspec validate --strict` passes
