## ADDED Requirements

### Requirement: MockBlockingProvider observes stop_token

A new `MockBlockingProvider` SHALL be implemented as an `ILLMProvider` test helper. Its `generate()` method SHALL loop checking `token.stop_requested()` every 10ms and return a cancelled `ToolResult` when cancellation is requested.

#### Scenario: MockBlockingProvider cancels when token is stopped
- **WHEN** `generate(req, token)` is called and `token.stop_requested()` becomes true within 100ms
- **THEN** the method returns a `ToolResult` with cancelled status
- **AND THEN** the call completes within 100ms of stop_requested being set

#### Scenario: MockBlockingProvider returns success if not cancelled
- **WHEN** `generate(req, token)` is called and `token.stop_requested()` is never set
- **THEN** the method SHALL return success after a default timeout (e.g., 1 second)
- **AND THEN** the test SHALL rely on cancellation timing for verification

### Requirement: E2E mid-loop cancellation completes within 100ms

The E2E test SHALL verify that calling `ChatSession::request_stop()` mid-chat causes the in-flight chat operation to observe the cancellation within 100ms.

#### Scenario: request_stop interrupts blocking provider
- **WHEN** a test starts `ChatSession::chat(input)` in a background thread
- **AND WHEN** another thread calls `session.request_stop()` after 50ms
- **THEN** the MockBlockingProvider SHALL observe `stop_requested()` within 100ms
- **AND THEN** `chat()` returns a cancelled result
- **AND THEN** no `loop.done` event with success=true is emitted

### Requirement: Token identity preserved through registry

The E2E test SHALL verify that the `std::stop_token` resolved from the CancellationRegistry is the same instance as the original token passed to `ChatSession::chat()`.

#### Scenario: Resolved token identity matches
- **WHEN** the test calls `ChatSession::chat(input, token)` with a token
- **AND WHEN** the test resolves the corresponding source via `cancellation_registry_->resolve_source(id)`
- **THEN** `resolved->get_token()` SHALL refer to the same cancellation state
- **AND THEN** `request_stop()` on the original source SHALL be observable from the resolved token

### Requirement: Default token never cancels

The E2E test SHALL verify that `ChatSession::chat(input)` without an explicit token SHALL never observe cancellation (since default `std::stop_token{}` is never cancellable).

#### Scenario: Default token never triggers cancellation
- **WHEN** `ChatSession::chat(input)` is called without explicit token
- **THEN** the chat operation SHALL NOT observe cancellation
- **AND THEN** cancellation registry SHALL be empty after chat() returns (no entries to clean)

### Requirement: cancellation_id resolves to valid token in loop_agent

The E2E test SHALL verify that the `cancellation_id` field in `loop_args` JSON resolves to a valid `std::stop_token` when passed to `pdk/loop_agent`.

#### Scenario: cancellation_id field present in loop_args
- **WHEN** `ChatSession::chat(input, token)` is invoked
- **AND WHEN** the test inspects the `loop/run` arguments passed to loop_agent
- **THEN** `loop_args.cancellation_id` SHALL be a non-empty string
- **AND THEN** resolving this id via the registry SHALL yield a token that observes `stop_requested()` after `request_stop()`

### Requirement: Full regression verification

After all E2E tests pass, the full ctest suite SHALL be run to verify no regressions in other tests.

#### Scenario: All tests pass after Step 5 ship
- **WHEN** `ctest -j$(nproc)` is run
- **THEN** the 5 new test_chat_session_cancellation tests SHALL PASS
- **AND THEN** pre-existing 3 failures SHALL remain unchanged
- **AND THEN** ASan preset SHALL show 0 new errors