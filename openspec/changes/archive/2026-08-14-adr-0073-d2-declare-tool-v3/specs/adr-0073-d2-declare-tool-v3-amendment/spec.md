## ADDED Requirements

### Requirement: DECLARE_TOOL_V3 MUST generate JSON Schema for C++ types

The DECLARE_TOOL_V3 macro MUST generate valid JSON Schema 2020-12 for input_schema and output_schema when C++ struct types are provided.

#### Scenario: DECLARE_TOOL_V3 generates schema for string type

- GIVEN a DECLARE_TOOL_V3 call with a struct containing `std::string path`
- WHEN the macro is instantiated
- THEN `metadata.input_schema` contains `{"type": "object", "properties": {"path": {"type": "string"}}}`

#### Scenario: DECLARE_TOOL_V3 generates schema for int type

- GIVEN a DECLARE_TOOL_V3 call with a struct containing `int max_lines`
- WHEN the macro is instantiated
- THEN `metadata.input_schema` contains `{"type": "object", "properties": {"max_lines": {"type": "integer"}}}`

#### Scenario: DECLARE_TOOL_V3 generates schema for optional<T> type

- GIVEN a DECLARE_TOOL_V3 call with a struct containing `std::optional<int> max_results`
- WHEN the macro is instantiated
- THEN `metadata.input_schema` contains `{"type": "object", "properties": {"max_results": {"type": "integer"}}}`

#### Scenario: DECLARE_TOOL_V3 generates schema for struct type

- GIVEN a DECLARE_TOOL_V3 call with a struct `FsReadArgs { std::string path; int max_lines; }`
- WHEN the macro is instantiated
- THEN `metadata.input_schema` contains `{"type": "object", "properties": {"path": {"type": "string"}, "max_lines": {"type": "integer"}}}`

### Requirement: DECLARE_TOOL V2 MUST remain backward compatible

Existing V2 DECLARE_TOOL macro calls MUST compile unchanged with no modifications.

#### Scenario: V2 DECLARE_TOOL compiles without changes

- GIVEN an existing DECLARE_TOOL call with 4 parameters
- WHEN the code is compiled with updated tool_macros.h
- THEN it compiles without error
- AND `metadata.input_schema` equals `std::nullopt`
- AND `metadata.output_schema` equals `std::nullopt`

### Requirement: ToolMetadata V3 fields MUST be additive

The V3 fields to ToolMetadata MUST NOT break existing code using V2 fields.

#### Scenario: ToolMetadata V3 fields have default values

- GIVEN a DECLARE_TOOL (V2) call
- WHEN ToolMetadata is constructed
- THEN `validation_mode` defaults to `ValidationMode::Strict`
- AND `input_schema` defaults to `std::nullopt`
- AND `output_schema` defaults to `std::nullopt`
