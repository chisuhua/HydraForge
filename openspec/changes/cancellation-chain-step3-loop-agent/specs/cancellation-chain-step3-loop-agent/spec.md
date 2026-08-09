## ADDED Requirements

### Requirement: loop_agent parses cancellation_id from loop_args

The `pdk/loop_agent::loop/run` handler SHALL parse `cancellation_id` from `loop_args` JSON and resolve it to a `std::stop_token` via a file-static `CancellationRegistry`.

The resolved token SHALL be checked before provider invocation. If `stop_requested()` is true, the handler SHALL return a cancelled result without invoking the provider.

#### Scenario: cancellation_id resolves to valid token
- **WHEN** loop_agent receives `cancellation_id = "1700000000_0"` in loop_args
- **THEN** it registers the source in its file-static registry
- **AND THEN** resolves the token successfully
- **AND THEN** the token SHALL observe `stop_requested()` after `request_stop()` is called

#### Scenario: cancellation_id absent preserves legacy behavior
- **WHEN** loop_args lacks `cancellation_id` (legacy callers)
- **THEN** handler uses `std::stop_token{}` (never cancels)
- **AND THEN** existing behavior preserved

### Requirement: Provider bridge forwards resolved token

The loop_agent's `ProviderLLMTool::generate()` SHALL pass the resolved `std::stop_token` to `provider_.generate(req, token)`, replacing the explicit `std::stop_token{}` at `pdk_entry.cpp:229`.

#### Scenario: Token forwarded to provider
- **WHEN** loop_agent has resolved a non-empty token from cancellation_id
- **THEN** `provider_.generate(req, token)` receives the real token (NOT `std::stop_token{}`)
- **AND THEN** the provider observes `stop_requested()` after external `request_stop()` is called

### Requirement: NodeExecutor dispatch_to_tool accepts stop_token

`NodeExecutor::dispatch_to_tool()` SHALL accept an optional `std::stop_token` parameter and forward it to `ToolCoordinator::execute()`.

The default parameter `std::stop_token{}` preserves backward compatibility for existing callers.

#### Scenario: Token forwarded from dispatch_to_tool to ToolCoordinator
- **WHEN** `dispatch_to_tool(name, path, args, token)` is invoked with a real token
- **THEN** `ToolCoordinator::execute(meta, ctx, args, token)` receives the same token (instance identity check)
- **AND THEN** ToolCoordinator can observe `stop_requested()` for short-circuit decisions

### Requirement: YieldNode forwards token to generate_stream

The YieldNode path at `node_executor.cpp:475` SHALL forward the real `std::stop_token` to `llm_provider_->generate_stream()` instead of constructing `std::stop_token{}`.

#### Scenario: YieldNode cancellation triggers stream cancel
- **WHEN** YieldNode is invoked with a cancelled token
- **AND WHEN** `generate_stream(req, token)` is called
- **THEN** the stream SHALL emit `finish_reason = "cancelled"`
- **AND THEN** the YieldStreamBridge SHALL observe the cancellation

### Requirement: ToolCoordinator execute() short-circuits on cancelled token

`ToolCoordinator::execute()` SHALL check `token.stop_requested()` at entry. If true, the method SHALL immediately return a cancelled `ToolResult` and emit a `tool.audit.denied` event with `reason="cancelled"`.

#### Scenario: Short-circuit on cancelled token
- **WHEN** `execute(meta, ctx, args, token)` is called with `token.stop_requested() == true`
- **THEN** the method returns immediately with `ToolResult{cancelled: true}`
- **AND THEN** NO tool invocation occurs
- **AND THEN** a `tool.audit.denied` event is emitted with `args = {"tool": meta.name, "reason": "cancelled"}`

#### Scenario: Default token preserves legacy behavior
- **WHEN** `execute(meta, ctx, args)` is called without explicit token
- **THEN** `token.stop_requested()` returns false (default token)
- **AND THEN** normal tool invocation proceeds