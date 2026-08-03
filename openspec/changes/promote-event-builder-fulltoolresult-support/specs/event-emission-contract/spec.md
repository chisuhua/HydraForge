## Purpose

扩展 ADR-0068 中定义的 `EventBuilder` L1 契约层,使其支持 operation-result 事件 (携带完整 `ToolResult` 含 `ok=false` / `error_code` / `latency_ms` / `trace_id` / `metadata` 等 optional 字段),迁移 Wave 1 ship 阶段保留的 8 处 raw `BusEvent` 构造,达到 §5.11 grep 验收 (返回 0 行),并解锁 ADR-0068 状态从 🟡 Partial → ✅ Approved。

## ADDED Requirements

### Requirement: event-builder-full-payload-constructor

`EventBuilder` SHALL provide a constructor `EventBuilder(std::string topic, ToolResult payload)` that takes a complete `ToolResult` (including all 7 fields: `ok` / `data` / `meta` / `error_code` / `latency_ms` / `trace_id` / `metadata`) and preserves all fields verbatim in the produced `BusEvent.payload`.

#### Scenario: full ToolResult with all optional fields preserved
- **WHEN** `EventBuilder("tool.completed", tool_result).build()` is called where `tool_result` has `ok=true`, `latency_ms=150`, `trace_id="tid-x"`, `error_code=ErrorCode::Unknown`
- **THEN** the emitted `BusEvent.payload.ok == true`
- **AND** `BusEvent.payload.latency_ms == 150`
- **AND** `BusEvent.payload.trace_id == "tid-x"`
- **AND** `BusEvent.payload.error_code == ErrorCode::Unknown`
- **AND** `BusEvent.payload.data` and `BusEvent.payload.meta` equal `tool_result.data` and `tool_result.meta`

#### Scenario: failure ToolResult with ok=false preserved
- **WHEN** `EventBuilder("tool.audit.denied", ToolResult::error(ErrorCode::PermissionDenied, "msg", meta)).build()` is called
- **THEN** the emitted `BusEvent.payload.ok == false`
- **AND** `BusEvent.payload.error_code == ErrorCode::PermissionDenied`
- **AND** `BusEvent.payload.meta == meta`

### Requirement: event-builder-ok-setter

`EventBuilder` SHALL provide a `.ok(bool)` setter that overrides the default `payload.ok = true` value produced by the empty-args constructor.

#### Scenario: explicit ok=false override
- **WHEN** `EventBuilder("x").ok(false).build()` is called
- **THEN** the emitted `BusEvent.payload.ok == false`

#### Scenario: explicit ok=true returns to default
- **WHEN** `EventBuilder("x").ok(true).build()` is called
- **THEN** the emitted `BusEvent.payload.ok == true`

### Requirement: event-builder-error-code-setter

`EventBuilder` SHALL provide a `.error_code(ErrorCode)` setter that sets `payload.error_code`.

#### Scenario: error_code setter on telemetry event
- **WHEN** `EventBuilder("tool.failed").error_code(ErrorCode::ResourceExhausted).build()` is called
- **THEN** the emitted `BusEvent.payload.error_code == ErrorCode::ResourceExhausted`

### Requirement: event-builder-latency-ms-setter

`EventBuilder` SHALL provide a `.latency_ms(std::uint64_t)` setter that sets `payload.latency_ms`.

#### Scenario: latency_ms setter
- **WHEN** `EventBuilder("llm.response").latency_ms(250).build()` is called
- **THEN** the emitted `BusEvent.payload.latency_ms == 250`

### Requirement: event-builder-trace-id-setter

`EventBuilder` SHALL provide a `.trace_id(std::string)` setter that sets `payload.trace_id`.

#### Scenario: trace_id setter
- **WHEN** `EventBuilder("tool.execution.start").trace_id("abc-123").build()` is called
- **THEN** the emitted `BusEvent.payload.trace_id == "abc-123"`

### Requirement: event-builder-metadata-setter

`EventBuilder` SHALL provide a `.metadata(nlohmann::json)` setter that sets `payload.metadata` (P4 REQ-TR-004 semantic, distinct from `meta`).

#### Scenario: metadata setter
- **WHEN** `EventBuilder("x").metadata({{"custom", "value"}}).build()` is called
- **THEN** the emitted `BusEvent.payload.metadata` is an object containing `{"custom": "value"}`
- **AND** `BusEvent.payload.meta` is the default empty object

## MODIFIED Requirements

### Requirement: all-legacy-emit-migrated

All existing `emit` call sites in `src/` and `examples/` (28 originally per ADR-0068 spec, plus 8 operation-result events from Wave 1 follow-up) SHALL construct `BusEvent` through `EventBuilder`.

#### Scenario: acceptance grep returns zero (extended scope)
- **WHEN** `grep -rn "BusEvent{" src examples --include="*.cpp" | grep -v event_builder` is executed
- **THEN** the command returns zero matches
- **AND** `grep -rn "bus_->emit(BusEvent{" src --include="*.cpp" | grep -v event_builder` returns zero matches

#### Scenario: operation-result events migrated
- **WHEN** the 8 identified operation-result sites (`tool.completed` / `execution.failed` / `cognitive.task.completed` / `domain.task.{completed,failed}` x3 / `tool.audit.denied` x2) are inspected
- **THEN** each site uses `EventBuilder` constructor with full `ToolResult` argument
- **AND** the `payload.ok` field reflects the operation success/failure semantic
- **AND** the `payload.error_code` / `payload.latency_ms` / `payload.trace_id` optional fields are preserved
