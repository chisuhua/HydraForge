## ADDED Requirements

### Requirement: ChatSession owns cancellation state and accepts stop_token

The `ChatSession` SHALL own a `std::stop_source` that allows any thread to request cancellation of an in-flight chat operation.

The `ChatSession::chat(user_input)` method SHALL accept an optional `std::stop_token` parameter. When provided, the token is registered with the ChatSession's cancellation registry and forwarded through the call chain.

The cancellation registry SHALL map unique string IDs to shared `std::stop_source` instances, allowing cross-boundary serialization via `cancellation_id`.

#### Scenario: External caller requests cancellation mid-chat
- **WHEN** `ChatSession::chat(input, token)` is invoked
- **AND WHEN** another thread calls `request_stop()` on the corresponding stop_source
- **THEN** the in-flight chat operation SHALL observe the cancellation
- **AND THEN** the chat method SHALL return within 100ms with a cancellation result

#### Scenario: Default token preserves existing behavior
- **WHEN** `ChatSession::chat(input)` is invoked without explicit token
- **THEN** the default `std::stop_token{}` SHALL be used (never cancels)
- **AND THEN** behavior is identical to pre-change

### Requirement: loop/run arguments include cancellation_id

The `ChatSession::chat()` implementation SHALL serialize a `cancellation_id` string into the `loop/run` tool arguments passed to `loop_agent`.

The cancellation_id SHALL be a unique string (timestamp + atomic counter) that maps to the ChatSession's stop_source in the cancellation registry.

#### Scenario: cancellation_id present in loop_args
- **WHEN** ChatSession invokes `registry->call_tool("loop/run", loop_args)`
- **THEN** `loop_args` SHALL contain a `cancellation_id` field with a unique string value
- **AND THEN** the string SHALL map to a valid stop_source in the registry

#### Scenario: cancellation_id absent preserves legacy behavior
- **WHEN** ChatSession is constructed without cancellation support (legacy mode)
- **THEN** `cancellation_id` SHALL be empty string
- **AND THEN** loop_agent SHALL use default `std::stop_token{}` (no cancellation)

### Requirement: loop_agent resolves cancellation_id and forwards token

The `pdk/loop_agent::loop/run` handler SHALL parse the `cancellation_id` from arguments and resolve it to a `std::stop_token` via the cancellation registry.

The resolved token SHALL be forwarded to:
1. The provider bridge (`provider_.generate(req, token)`) — replacing `std::stop_token{}` at `pdk_entry.cpp:229`
2. The child DSLEngine execution path

#### Scenario: cancellation_id resolves to valid token
- **WHEN** loop_agent receives `cancellation_id = "abc123"`
- **THEN** it looks up the registry and obtains the corresponding `std::stop_token`
- **AND THEN** forwards the token to `provider_.generate(req, token)` (NOT `std::stop_token{}`)

#### Scenario: cancellation_id not found
- **WHEN** loop_agent receives an unknown `cancellation_id`
- **THEN** it logs a warning and uses `std::stop_token{}` (graceful degradation)
- **AND THEN** continues execution (no false cancellation)

### Requirement: Loop APIs accept std::stop_token parameter

`ReactLoop::run()`, `PlanExecuteLoop::run()`, and `ForkJoinLoop::run()` SHALL accept a `std::stop_token` parameter and forward it to internal LLM calls and worker pool operations.

The `ForkJoinLoop`'s condition variable wait predicate SHALL include `token.stop_requested()` to break out on cancellation.

#### Scenario: ReactLoop cancels on stop_requested
- **WHEN** `ReactLoop::run(prompt, ctx, token)` is called
- **AND WHEN** `token.stop_requested()` becomes true during execution
- **THEN** the loop SHALL exit within one iteration
- **AND THEN** return a `LoopResult` with a cancelled status

#### Scenario: PlanExecuteLoop cancels during verify phase
- **WHEN** `PlanExecuteLoop::run(...)` enters verify phase
- **AND WHEN** `token.stop_requested()` is observed
- **THEN** the verify phase SHALL exit immediately
- **AND THEN** return a `LoopResult` with cancelled status (no retry)

#### Scenario: ForkJoinLoop cancels worker pool on token
- **WHEN** `ForkJoinLoop::run(branches, ctx, token)` is called
- **AND WHEN** `token.stop_requested()` is observed in the CV wait predicate
- **THEN** the worker pool SHALL receive `pool_->stop()` call
- **AND THEN** the loop SHALL return without waiting for all branches

### Requirement: NodeExecutor forwards token to YieldNode and dispatch_to_tool

`NodeExecutor::dispatch_to_tool()` SHALL accept an optional `std::stop_token` and forward it to `ToolCoordinator::execute()`.

`NodeExecutor::execute_yield()` SHALL forward the received token to `generate_stream()` (replacing the default `std::stop_token{}` at `node_executor.cpp:475`).

#### Scenario: YieldNode cancels LLM generation
- **WHEN** `NodeExecutor::execute_yield(stream_request, token)` is called
- **AND WHEN** `token.stop_requested()` becomes true during streaming
- **THEN** `generate_stream()` SHALL observe cancellation
- **AND THEN** the stream SHALL emit `finish_reason = "cancelled"`

#### Scenario: dispatch_to_tool cancels tool execution
- **WHEN** `NodeExecutor::dispatch_to_tool(name, path, args, token)` is called
- **AND WHEN** `token.stop_requested()` becomes true before tool returns
- **THEN** the tool SHALL observe cancellation (if tool checks)
- **AND THEN** dispatch_to_tool returns with cancellation result

### Requirement: ToolCoordinator::execute() accepts and forwards token

`ToolCoordinator::execute(meta, ctx, args)` SHALL accept an optional `std::stop_token` parameter.

When the token is provided, ToolCoordinator SHALL:
1. Check `token.stop_requested()` before tool invocation
2. Forward the token to `registry->call_tool()` if supported
3. Return immediately if token is already cancelled

#### Scenario: ToolCoordinator short-circuits on cancelled token
- **WHEN** `ToolCoordinator::execute(meta, ctx, args, token)` is called
- **AND WHEN** `token.stop_requested()` is already true at entry
- **THEN** execute returns immediately with cancelled result
- **AND THEN** no tool is invoked (audit log entry: `tool.audit.denied` with reason="cancelled")

### Requirement: E2E mid-loop cancellation test

A new test SHALL verify that cancellation propagates from ChatSession through to the LLM provider.

The test SHALL use a `MockBlockingProvider` that polls `stop_requested()` in a loop, simulating a long LLM generation.

#### Scenario: request_stop interrupts blocking provider
- **WHEN** a test starts `ChatSession::chat(input)` in thread A
- **AND WHEN** thread B calls `session.request_stop()` after 50ms
- **THEN** the MockBlockingProvider SHALL observe `stop_requested()` within 100ms
- **AND THEN** `chat()` returns a cancelled result
- **AND THEN** no `loop.done` event with success=true is emitted

#### Scenario: Token forwarded through registry without modification
- **WHEN** the test inspects the MockBlockingProvider's received token
- **THEN** the token SHALL be the same instance as ChatSession's stop_source token (identity check)
- **AND THEN** `stop_requested()` on the received token returns true after `request_stop()` is called