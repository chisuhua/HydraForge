# chat-streaming-slash-tui Specification

## Purpose

Define the event-driven streaming contract for `pdk_chat_demo`. `EventHandler` renders response chunks and loop progress as they arrive, while preserving budget alerts and event lines as independent display records. The capability also defines deterministic startup behavior for `--system-prompt` and `--append-system-prompt`. It builds on shipped adr-0068 event topics, the real `fix-loop-agent-bypass` path, and the shipped `chat-slash-commands-migration` command layer. Generic flag infrastructure remains owned by `cli-args-cxxopts`, and full provider switching remains owned by `provider-dynamic-discovery`.

## ADDED Requirements

### Requirement: eventhandler-chunk-render

`EventHandler` MUST subscribe to `llm.response` and render chunk text incrementally. It MUST NOT poll the LLM provider or wait for the complete response. Partial text MAY be coalesced for terminal throughput, but it MUST be flushed within the bounded renderer window.

#### Scenario: mock response chunks render incrementally

- **GIVEN** mock mode emits three ordered `llm.response` chunks with text `one`, ` two`, and ` three`
- **WHEN** EventHandler receives each event
- **THEN** the TUI appends visible text in the same order
- **AND** the first chunk is available before the complete response is emitted
- **AND** no provider polling call is made

#### Scenario: metadata-only response event

- **WHEN** an `llm.response` event contains no displayable chunk text
- **THEN** EventHandler preserves its metadata and does not append the string `null` or an empty display line
- **AND** the subscription remains active for the next chunk

### Requirement: loop-token-render

`EventHandler` MUST subscribe to the real loop event path and render `loop.token` and `loop.decision` through the existing event-line mechanism. It MUST preserve topic and trace context where present and MUST tolerate additional payload fields.

#### Scenario: loop token reaches the TUI

- **GIVEN** the shipped `fix-loop-agent-bypass` path emits `loop.token` with token text
- **WHEN** EventHandler receives the event
- **THEN** the token is appended through the streaming display path
- **AND** it is ordered relative to preceding `llm.response` chunks

#### Scenario: loop decision is an event line

- **GIVEN** the loop emits `loop.decision` with a decision value and trace metadata
- **WHEN** EventHandler receives the event
- **THEN** the TUI appends a distinct decision event line
- **AND** the line retains the `loop.decision` topic and trace identifier when supplied
- **AND** unknown extra payload fields do not fail rendering

### Requirement: budget-line-preservation

Streaming response text MUST NOT overwrite, reorder destructively with, or hide budget alerts and existing event lines. Partial response text, budget alerts, and event lines MUST use serialized renderer updates and an explicit scroll policy.

#### Scenario: budget alert interleaves with chunks

- **GIVEN** a response chunk is pending
- **WHEN** a budget alert arrives before the next response chunk
- **THEN** the alert is appended as its own visible line
- **AND** the next response chunk remains ordered after the alert
- **AND** the pending response buffer is not replaced by the alert

#### Scenario: cancellation preserves already rendered output

- **GIVEN** a streaming turn is cancelled through the existing `stop_token`
- **WHEN** cancellation is observed at a token boundary
- **THEN** the current turn stops without a second cancellation mechanism
- **AND** already rendered response, budget, and event lines remain visible

### Requirement: system-prompt-overwrite

`--system-prompt <text>` MUST replace the default system prompt during startup loading. The resolved prompt MUST be reused by subsequent LLM calls, and the flag help text MUST describe overwrite semantics.

#### Scenario: custom prompt replaces default

- **GIVEN** the application starts with `--system-prompt "Custom prompt"`
- **WHEN** startup configuration resolves the system prompt
- **THEN** the resolved prompt is exactly `Custom prompt`
- **AND** the default system prompt is not included
- **AND** subsequent LLM requests receive the resolved prompt

### Requirement: system-prompt-append

`--append-system-prompt <text>` MUST preserve the default prompt and append the supplied text after it, separated by one newline. If both flags are present, overwrite establishes the base and append is applied second.

#### Scenario: append text follows default

- **GIVEN** the application starts with `--append-system-prompt "Always be terse."`
- **WHEN** startup configuration resolves the system prompt
- **THEN** the default prompt appears first
- **AND** `Always be terse.` appears after one newline
- **AND** subsequent LLM requests receive the combined prompt

#### Scenario: overwrite and append precedence

- **GIVEN** the application starts with `--system-prompt "Custom prompt" --append-system-prompt "Extra rule"`
- **WHEN** startup configuration resolves the system prompt
- **THEN** the resolved prompt is `Custom prompt\nExtra rule`
- **AND** the original default prompt is absent

### Requirement: streaming-e2e-parity

Mock and real LLM modes MUST exercise the same EventHandler bus subscription path. The change MUST reuse adr-0068 topics and the real loop-agent path rather than introducing a mock-only renderer.

#### Scenario: mock and real paths share rendering behavior

- **GIVEN** equivalent ordered response chunks and loop events in mock and real configurations
- **WHEN** the TUI consumes the event streams
- **THEN** both modes render incremental response text, `loop.token`, and `loop.decision` through EventHandler
- **AND** budget alerts and event lines remain independently visible in both modes
- **AND** full ctest regression coverage remains green apart from documented pre-existing failures
