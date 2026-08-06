## Context

`adr-0068-event-emission-contract` Wave 1 shipped on 2026-08-03 and established the EventBuilder V2 contract plus the `llm.response`, `tool.execution.start`, `tool.execution.end`, and `session.persisted` event path. The `llm.response` payload can therefore carry response metadata and chunk content without adding a second provider polling path. The existing bus remains the source of truth for event delivery.

`fix-loop-agent-bypass` shipped the real `loop_agent` execution path and connected loop events to the runtime. This change consumes that path through `loop.token` and `loop.decision`; it does not recreate loop execution or invent a parallel callback channel. The related `chat-slash-commands-migration` change is shipped and provides the unified command entry point, including the `/model` stub. Its command-layer work is a dependency only, not part of this change.

The Wave 2 `cli-args-cxxopts` change supplies the declarative CLI flag foundation used by `--system-prompt` and `--append-system-prompt`. `provider-dynamic-discovery` remains the dependency for full `/model` provider switching and is referenced here only to keep that capability out of scope. The pi-agent borrowing path, §五 Streaming, identifies incremental rendering, uninterrupted budget alerts, and event-line preservation as the core capability for this change.

## Goals / Non-Goals

**Goals:**

- Subscribe `EventHandler` to chunk-level `llm.response` events and append visible response text as chunks arrive, without waiting for the complete response.
- Subscribe `EventHandler` to the real `loop.token` and `loop.decision` topics, rendering loop progress and decisions through the same TUI event path.
- Keep rendering decoupled from `ChatSession`: the renderer subscribes to the bus and does not call session methods to obtain or mutate display content.
- Preserve budget alerts and event lines while response chunks arrive, using incremental append and a stable scroll policy.
- Align streaming with the shipped `loop_agent` path and preserve existing `stop_token` cancellation semantics at token boundaries.
- Add `--system-prompt` overwrite behavior and `--append-system-prompt` concatenation behavior to startup loading through `cli-args-cxxopts`.
- Cover mock and real LLM streaming through end-to-end tests, with mock mode exercising the same EventHandler path as real mode.

**Non-Goals:**

- Slash command migration, which belongs to the shipped `chat-slash-commands-migration` change.
- Full `/model` provider switching, which belongs to `provider-dynamic-discovery`.
- Async input steering and follow-up coordination, which belongs to Wave 3 `chat-async-io-steering`.
- Generic CLI parser infrastructure, which belongs to `cli-args-cxxopts`.
- RPC mode or a new transport protocol, which remains governed by ADR-0059.
- Replacing the EventBuilder or changing the existing event topic contract.

## Decisions

### Decision 1: EventHandler subscribes to the bus

**Rationale:**

The TUI must consume the same events produced by mock and real execution. A bus subscription keeps `ChatSession` responsible for conversation state and `EventHandler` responsible for presentation. It also allows budget and telemetry lines to remain independent records, so a response chunk cannot overwrite an alert. The subscription must be non-blocking for the producer and must use the existing event metadata and topic names.

**Alternatives Considered:**

- **Callback into ChatSession:** rejected because it couples presentation to session lifecycle and makes mock and real paths diverge.
- **Polling the LLM provider:** rejected because it duplicates delivery, adds latency, and violates the event-driven contract.
- **A new renderer-specific queue:** rejected because it would create a second event path and split ordering guarantees from the existing bus.

### Decision 2: Coalesce chunks in short line-batched windows

**Rationale:**

The renderer will append chunks immediately when they complete a display line and coalesce partial text into a small pending buffer. A flush also occurs at a short bounded interval, targeted at 16 ms, so rapid token streams do not force one terminal write per token. This keeps visible latency low while protecting terminal throughput. Event lines and budget alerts bypass the text buffer and are appended as independent lines.

**Alternatives Considered:**

- **Every-token rendering:** rejected as a default because terminal writes can outrun the bus consumer and corrupt scroll behavior under fast output.
- **Large fixed batches:** rejected because they make streaming feel delayed and weaken the value of chunk-level events.
- **Only line-batched rendering:** rejected because a long line could remain invisible until the model emits a newline.

### Decision 3: `--system-prompt` overwrites, `--append-system-prompt` concatenates

**Rationale:**

`--system-prompt TEXT` replaces the default prompt exactly, including an intentionally empty string if the parser permits it. `--append-system-prompt TEXT` preserves the default prompt and appends the supplied text after it, separated by one newline. When both flags are present, overwrite establishes the base and append is applied second. The resolved prompt is created once during startup loading and is passed to subsequent LLM calls, avoiding per-turn mutation.

**Alternatives Considered:**

- **Both flags append:** rejected because `--system-prompt` would not provide a reliable way to replace defaults.
- **Append before the default:** rejected because user supplied constraints should be later in the prompt and therefore easier to apply without changing the default text.
- **Mutually exclusive flags:** rejected because combining overwrite with a short additional instruction is useful and has deterministic semantics.

### Decision 4: Loop events use the existing EventHandler event-line path

**Rationale:**

`loop.token` renders token or progress text, while `loop.decision` renders a structured decision line. Both retain topic and trace metadata in the display record. The implementation consumes events emitted by the shipped `fix-loop-agent-bypass` path and does not add loop-specific callbacks. This preserves one ordering model for LLM, loop, tool, and budget output.

**Alternatives Considered:**

- **Render only `llm.response`:** rejected because loop-agent decisions would be invisible to users.
- **Expose loop state through ChatSession:** rejected because it breaks the required render/session boundary.

## Risks / Trade-offs

- **Slow bus subscription throughput:** a fast model can produce chunks faster than the terminal can draw. Mitigation: bounded pending text, 16 ms coalescing, non-blocking producer behavior, and a measurable P95 chunk handling target below 50 ms.
- **Terminal corruption during rapid chunks:** concurrent writes or mixing partial text with alerts can produce broken lines. Mitigation: serialize renderer mutations, keep partial response text in one buffer, and append budget or event records as separate atomic display lines.
- **System prompt injection ordering:** an append flag could accidentally precede or replace the default. Mitigation: resolve flags once, document overwrite then append precedence, and assert the exact final prompt in CLI tests.
- **Loop topic drift:** the real loop path may emit payload variants. Mitigation: test `loop.token` and `loop.decision` against the shipped EventBuilder schema and treat unknown payload fields as non-fatal display metadata.
- **Real LLM test availability:** network or model configuration can make real-path tests environment-sensitive. Mitigation: keep deterministic mock E2E coverage mandatory, gate real tests on the existing provider configuration, and preserve clear skip diagnostics without weakening mock assertions.

## Migration Plan

No persisted data migration is required. Existing sessions remain valid because display subscription is attached at runtime. Startup flag resolution is additive, and absent flags preserve the current default system prompt. Rollback is a source revert that removes the subscriber and two flag registrations without changing bus producers or session data.

## Open Questions

- The exact terminal widget used by the demo may determine whether the 16 ms flush uses a timer or the existing event loop tick. The implementation should choose the existing loop primitive and keep the externally visible coalescing contract unchanged.
- The real LLM E2E test may require the repository's existing environment gate. No new provider or RPC fixture should be introduced by this change.
