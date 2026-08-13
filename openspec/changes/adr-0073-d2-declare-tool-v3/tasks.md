# tasks.md — adr-0073-d2-declare-tool-v3

## Implementation Tasks

- [x] **T1**: Update `src/common/policy/execution_policy.h` — Add V3 fields to ToolMetadata: `std::optional<nlohmann::json> input_schema`, `std::optional<nlohmann::json> output_schema`, `ValidationMode validation_mode`
- [x] **T2**: Create `include/agenticdsl/tools/schema_generation.h` — SchemaGenerator template with specializations for: string, int/long/size_t, float/double, bool, vector, optional, map, enum class, struct
- [x] **T3**: Update `include/agenticdsl/pdk/tool_macros.h` — Add DECLARE_TOOL_V3 macro with InputSchema/OutputSchema template params and auto-schema generation
- [x] **T4**: Create `tests/test_declare_tool_auto_schema.cpp` — Test cases: string type, int type, optional<T> type, struct type, plus backward-compat V2 macro test
- [x] **T5**: Update `docs/adr/adr-0073-tool-json-schema-contract.md` — Mark D2 as partial ship, add evidence section
- [x] **T6**: Update `docs/adr/adr-0073-impl-scope-audit.md` — Mark D2 status as Partial

## Validation Tasks

- [x] **T7**: Run `cmake --build build` — Verify compilation succeeds
- [x] **T8**: Run `ctest --output-on-failure` — Verify all tests pass (123/123)
- [x] **T9**: Run `tools/adr_lint.py` — Verify exit code 0
- [x] **T10**: Run `tools/docs_drift_audit.py` — Verify 0 new DRIFT items (pre-existing mismatch)
- [x] **T11**: Run `openspec validate --strict` — Verify passes

## Ship Tasks

- [x] **T12**: Commit changes with descriptive message
- [ ] **T13**: Archive OpenSpec change
