## Purpose

定义 ADR-0068 附录 A 中 5 个 Wave 1 幻影主题（`llm.request`、`llm.response`、`tool.execution.start`、`tool.execution.end`、`session.persisted`）的真实发射契约，以及 `EventBuilder` 的构造规范。本 spec 覆盖 EventBuilder 设计、28 处既有 emit 迁移、`test_e2e_mock.cpp` 真实化，确保运行时生命周期事件可被下游订阅方稳定消费。

## ADDED Requirements

### Requirement: event-builder-args-meta-separation

`EventBuilder` SHALL produce `BusEvent` payloads where `args` contains only schema-mandatory business fields and `meta` contains contextual fields such as `trace_id`, `session_id`, and debug information.

#### Scenario: mandatory business fields live in args
- **WHEN** `EventBuilder("tool.execution.start").args({{"tool", name}, {"layer", layer}}).meta({{"trace_id", tid}}).build()` is called
- **THEN** the emitted `BusEvent` payload has `args.tool == name`
- **AND** `args.layer == layer`
- **AND** `meta.trace_id == tid`

#### Scenario: context fields live in meta
- **WHEN** an event is built with `.meta({{"session_id", sid}, {"debug", info}})`
- **THEN** `session_id` and `debug` appear in `meta`
- **AND** they do not appear in `args`

### Requirement: llm-request-emitted-before-generate

The LLM Decorator chain SHALL emit `llm.request` before delegating to the underlying `ILLMProvider::generate()`.

#### Scenario: normal generate call
- **WHEN** `generate(prompt, ...)` is invoked through the Decorator chain
- **THEN** the bus receives an `llm.request` event before the provider generates
- **AND** the payload contains `model`
- **AND** the payload contains `prompt_hash`

#### Scenario: provider error path
- **WHEN** the underlying provider throws or returns an error
- **THEN** the `llm.request` event is still emitted before the error is returned

### Requirement: llm-response-emitted-after-generate

The LLM Decorator chain SHALL emit `llm.response` after `generate()` returns, including error paths.

#### Scenario: successful generate
- **WHEN** `generate()` returns a non-error response
- **THEN** the bus receives an `llm.response` event
- **AND** the payload contains `tokens`
- **AND** the payload contains `duration_ms`

#### Scenario: error generate
- **WHEN** `generate()` returns an error or throws
- **THEN** the bus still receives an `llm.response` event
- **AND** the payload contains `error_code`

### Requirement: tool-execution-start-emitted

`ToolCoordinator` SHALL emit `tool.execution.start` at the beginning of every `call_tool` linear flow.

#### Scenario: tool call begins
- **WHEN** `call_tool(name, layer, args)` is invoked
- **THEN** the bus receives a `tool.execution.start` event
- **AND** the payload contains `tool` equal to `name`
- **AND** the payload contains `layer` equal to the layer profile

### Requirement: tool-execution-end-emitted

`ToolCoordinator` SHALL emit `tool.execution.end` at the end of every `call_tool` linear flow.

#### Scenario: tool call succeeds
- **WHEN** `call_tool` returns a successful `ToolResult`
- **THEN** the bus receives a `tool.execution.end` event
- **AND** the payload contains `tool`
- **AND** the payload contains `ok` equal to `true`
- **AND** the payload contains `duration_ms`

#### Scenario: tool call fails
- **WHEN** `call_tool` returns a failed `ToolResult` or throws
- **THEN** the bus still receives a `tool.execution.end` event
- **AND** the payload contains `ok` equal to `false`

### Requirement: session-persisted-emitted

`ChatSession` or the session persistence agent SHALL emit `session.persisted` after the session state has been successfully written to disk.

#### Scenario: persistence succeeds
- **WHEN** the session save operation completes successfully
- **THEN** the bus receives a `session.persisted` event
- **AND** the payload contains `session_id`
- **AND** the payload contains `path`

### Requirement: all-legacy-emit-migrated

All existing `emit` call sites in `src/` and `examples/` (28 total) SHALL construct `BusEvent` through `EventBuilder`.

#### Scenario: acceptance grep returns zero
- **WHEN** `grep -rn "BusEvent{" src examples --include="*.cpp" | grep -v event_builder` is executed
- **THEN** the command returns zero matches

### Requirement: test-e2e-mock-fake-events-replaced

`examples/pdk_chat_demo/tests/test_e2e_mock.cpp` SHALL no longer manually emit `loop.turn.*`, `loop.decision`, `llm.request`, `llm.response`, `tool.execution.start`, or `tool.execution.end`.

#### Scenario: fake emits removed
- **WHEN** the test file is inspected
- **THEN** no line calls `emit` with those topic names manually
- **AND** the test instead triggers the real pipeline

#### Scenario: real events verified
- **WHEN** the end-to-end mock test runs
- **THEN** it asserts that the bus contains the expected real events
- **AND** the assertions check the payload fields defined in ADR-0068 Appendix A
