## Purpose

`fix-loop-agent-bypass` 补齐 `pdk_chat_demo` 中 Loop Agent 的调用路径:删除 `ChatSession::chat()` 里的 `use_direct_llm` 短路分支,使真实 LLM 模式与 mock 模式都统一通过 `call_tool("loop/run")` 进入 `pdk/loop_agent` 执行。Loop Agent 在真实 DSL 执行路径中必须发射 ADR-0068 附录 A 规定的 `loop.turn.start`、`loop.decision`、`loop.turn.end` 三个生命周期事件,让下游 streaming、compaction、steering 等能力获得可观测的 Agent 状态。

## ADDED Requirements

### Requirement: chatsession-must-route-all-messages-through-loop-run

`ChatSession::chat()` MUST NOT contain a direct LLM invocation path. All user message processing SHALL flow through `impl_->registry->call_tool("loop/run", loop_args)` regardless of whether a real LLM provider is configured.

#### Scenario: real LLM provider configured
- **WHEN** `ChatSession::chat()` is called and `impl_->engine->get_llm_provider()` returns a non-null provider
- **THEN** the method does NOT call `ILLMProvider::generate()` directly
- **AND** it invokes `call_tool("loop/run", loop_args)` with the same arguments as the mock fallback path
- **AND** it uses the returned `response`/`steps`/`tokens_used`/`cost_usd`/`success` fields to populate the `ChatResult`

#### Scenario: mock mode (no LLM provider)
- **WHEN** `ChatSession::chat()` is called and `impl_->engine->get_llm_provider()` returns null
- **THEN** it still invokes `call_tool("loop/run", loop_args)`
- **AND** the loop_agent mock fallback path returns a valid response
- **AND** `ChatResult.success` is set to true

#### Scenario: use_direct_llm symbol removed
- **WHEN** building `pdk_chat_demo` or running `grep -rn "use_direct_llm" examples/`
- **THEN** the build succeeds
- **AND** the grep command returns zero matches

### Requirement: loop-run-must-preserve-existing-tool-signature-and-mock-fallback

`pdk/loop_agent` 的 `loop/run` tool SHALL keep its existing argument schema (`loop_type`, `prompt`, `system_prompt`, `history`, `tools`, `max_steps`, optional `bus_ptr`/`session_id`) and return schema. When no parent provider has been set via `loop/set_parent_provider`, the tool SHALL return a mock response identical in shape to the current fallback.

#### Scenario: no parent provider set
- **WHEN** `call_tool("loop/run", {"loop_type":"react", "prompt":"test"})` is invoked without prior `loop/set_parent_provider`
- **THEN** the returned JSON contains `response`, `steps`, `tokens_used`, `cost_usd`, and `success` fields
- **AND** `success` is true
- **AND** the response text contains the loop_type and prompt

#### Scenario: existing test assertions unchanged
- **WHEN** `test_loop_agent_plugin.cpp` is compiled and executed without modification
- **THEN** all 6 existing TEST_CASE pass
- **AND** no new assertion failures are introduced

### Requirement: loop-agent-must-emit-turn-start-event

In the real DSL execution path, `loop/run` MUST emit `loop.turn.start` at the beginning of each ReAct turn. The payload MUST contain integer fields `turn` and `step` per ADR-0068 Appendix A.

#### Scenario: single turn ReAct
- **WHEN** `loop/run` is called with a real provider and a prompt that resolves in one turn
- **THEN** the bus receives at least one `loop.turn.start` event
- **AND** the payload contains `turn` equal to 1
- **AND** the payload contains `step` equal to 1

#### Scenario: multi-turn ReAct
- **WHEN** `loop/run` executes a ReAct loop with two turns
- **THEN** the bus receives two `loop.turn.start` events
- **AND** the first event has `turn=1`, `step=1`
- **AND** the second event has `turn=2`, `step=2`

### Requirement: loop-agent-must-emit-decision-event

In the real DSL execution path, `loop/run` MUST emit `loop.decision` whenever a decision node is reached. The payload MUST contain a string `decision` field; when `decision` is `"tool_call"`, the payload MUST additionally contain a string `tool` field.

#### Scenario: tool_call decision
- **WHEN** the ReAct loop decides to invoke a tool
- **THEN** the bus receives a `loop.decision` event
- **AND** the payload `decision` equals `"tool_call"`
- **AND** the payload contains `tool` equal to the tool name being invoked

#### Scenario: respond decision
- **WHEN** the ReAct loop decides to produce the final response
- **THEN** the bus receives a `loop.decision` event
- **AND** the payload `decision` equals `"respond"`
- **AND** the payload does NOT contain a `tool` field

### Requirement: loop-agent-must-emit-turn-end-event

In the real DSL execution path, `loop/run` MUST emit `loop.turn.end` at the end of each ReAct turn. The payload MUST contain integer `turn` and string `decision` fields per ADR-0068 Appendix A.

#### Scenario: turn ends after observation
- **WHEN** a ReAct turn completes its observe phase
- **THEN** the bus receives a `loop.turn.end` event
- **AND** the payload contains `turn` equal to the current turn number
- **AND** the payload contains `decision` equal to the decision made during the turn (e.g. `"observe"` or `"respond"`)

#### Scenario: turn ends after tool execution
- **WHEN** a turn that invoked a tool reaches its end
- **THEN** the bus receives a `loop.turn.end` event
- **AND** the payload `decision` reflects the post-tool observation (e.g. `"observe"`)
- **AND** the event is emitted after the corresponding `loop.decision` event

### Requirement: event-payload-must-match-adr-0068-appendix-a

All `loop.turn.*` and `loop.decision` events emitted by `loop/run` MUST use the exact payload field names and types specified in `docs/adr/adr-0068-event-emission-contract.md` Appendix A.

#### Scenario: field names and types
- **WHEN** `loop.turn.start`, `loop.decision`, or `loop.turn.end` events are captured
- **THEN** `loop.turn.start` payload contains exactly `turn` (integer) and `step` (integer)
- **AND** `loop.decision` payload contains `decision` (string) and optionally `tool` (string)
- **AND** `loop.turn.end` payload contains exactly `turn` (integer) and `decision` (string)

#### Scenario: topic registry status updated
- **WHEN** ADR-0068 Appendix A is reviewed after this change ships
- **THEN** the status of `loop.turn.start`, `loop.turn.end`, and `loop.decision` is marked ✅ instead of 👻
- **AND** the owner remains `loop_agent (L4)`

### Requirement: e2e-mock-test-must-verify-real-emission

`examples/pdk_chat_demo/tests/test_e2e_mock.cpp` MUST replace the hand-crafted `emit("loop.turn.start", ...)` / `emit("loop.decision", ...)` / `emit("loop.turn.end", ...)` fake sequence with assertions that verify the events are produced by `loop/run` itself.

#### Scenario: fake events removed
- **WHEN** `test_e2e_mock.cpp` is inspected
- **THEN** no line manually emits `loop.turn.start`, `loop.decision`, or `loop.turn.end` into the bus
- **AND** the test instead triggers `ChatSession::chat()` or `call_tool("loop/run", ...)`

#### Scenario: real events verified
- **WHEN** the end-to-end mock test runs
- **THEN** it asserts that the bus contains a `loop.turn.start` event with `turn` and `step`
- **AND** it asserts that the bus contains a `loop.decision` event with `decision` and `tool` when applicable
- **AND** it asserts that the bus contains a `loop.turn.end` event with `turn` and `decision`

### Requirement: design-document-must-reflect-unified-call-path

`examples/pdk_chat_demo/DESIGN.md` MUST describe a single call path from `ChatSession` to `loop/run`. Any mention of a direct LLM invocation bypass or dual-path fallback SHALL be updated or removed.

#### Scenario: DESIGN.md section eight reviewed
- **WHEN** reading `DESIGN.md` section "八" or the Loop Agent sequence diagram
- **THEN** the diagram shows `ChatSession -> call_tool("loop/run")` without a direct LLM edge
- **AND** the text does not state that real LLM mode bypasses the Loop Agent

#### Scenario: docs drift audit
- **WHEN** `python3 tools/docs_drift_audit.py` is executed
- **THEN** it exits with zero DRIFT items related to `pdk_chat_demo` call paths or Loop Agent ownership
