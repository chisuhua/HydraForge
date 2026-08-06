## 1. EventHandler chunk streaming subscriber

- [ ] 1.1 Verify the shipped adr-0068 EventBuilder V2 payload fields and `llm.response` topic contract used by `pdk_chat_demo`
- [ ] 1.2 Add an EventHandler subscription to `llm.response` that extracts chunk text and appends it without waiting for the complete turn
- [ ] 1.3 Add bounded pending text with line-batched and approximately 16 ms flush behavior, while keeping producer-side bus emission non-blocking
- [ ] 1.4 Serialize renderer mutations so partial response text cannot interleave with other display writes
- [ ] 1.5 Add chunk ordering and empty or metadata-only payload handling tests
- [ ] 1.6 Commit: `git commit -m "feat(chat-stream): render llm.response chunks through EventHandler"`

## 2. Loop event rendering

- [ ] 2.1 Verify the real `fix-loop-agent-bypass` path emits `loop.token` and `loop.decision`
- [ ] 2.2 Subscribe EventHandler to `loop.token` and render token or progress text through the existing display path
- [ ] 2.3 Subscribe EventHandler to `loop.decision` and render a structured event line with topic and trace metadata
- [ ] 2.4 Preserve unknown loop payload fields as optional metadata and avoid failing the event consumer
- [ ] 2.5 Test loop token and decision ordering alongside `llm.response`
- [ ] 2.6 Commit: `git commit -m "feat(chat-stream): render loop token and decision events"`

## 3. Budget and event-line preservation

- [ ] 3.1 Define the incremental append and scroll policy for partial response text, completed lines, budget alerts, and event lines
- [ ] 3.2 Ensure budget alert and existing event records bypass the pending response buffer and remain independently visible
- [ ] 3.3 Add renderer tests for an alert arriving between two response chunks
- [ ] 3.4 Add cancellation coverage confirming `stop_token` stops the current turn at the next token boundary
- [ ] 3.5 Measure chunk handling latency and assert the documented P95 target where the test environment supports timing checks
- [ ] 3.6 Commit: `git commit -m "test(chat-stream): preserve budget alerts and event lines"`

## 4. System prompt CLI flags

- [ ] 4.1 Add `--system-prompt <text>` through the declarative `cli-args-cxxopts` flag registration
- [ ] 4.2 Resolve `--system-prompt` as an exact overwrite of the default system prompt during startup loading
- [ ] 4.3 Add `--append-system-prompt <text>` through the same `cli-args-cxxopts` registration path
- [ ] 4.4 Resolve append text after the overwrite base, separated by one newline, and document both flags in help output
- [ ] 4.5 Add precedence tests for neither flag, each flag alone, and both flags together
- [ ] 4.6 Confirm `/model` remains the shipped stub and does not implement provider switching from `provider-dynamic-discovery`
- [ ] 4.7 Commit: `git commit -m "feat(chat-cli): add system prompt override and append flags"`

## 5. Mock and real streaming E2E

- [ ] 5.1 Add mock streaming E2E coverage where mock chunks pass through the same EventHandler bus subscription as production
- [ ] 5.2 Assert mock output is incremental and preserves interleaved budget and event lines
- [ ] 5.3 Add real LLM streaming E2E coverage using the existing provider configuration and environment gate
- [ ] 5.4 Assert real loop-agent output reaches the TUI through `loop.token` and `loop.decision`
- [ ] 5.5 Run the slash command regression fixture from shipped `chat-slash-commands-migration` without modifying its command implementation
- [ ] 5.6 Commit: `git commit -m "test(chat-stream): cover mock and real LLM streaming paths"`

## 6. Validation and dependency checks

- [ ] 6.1 Verify dependencies `chat-slash-commands-migration` shipped, `cli-args-cxxopts` is available, and `provider-dynamic-discovery` remains out of scope
- [ ] 6.2 Run focused streaming and CLI tests with `ctest --output-on-failure`
- [ ] 6.3 Run full `ctest --output-on-failure -j$(nproc)` and record any known pre-existing failures separately
- [ ] 6.4 Run `openspec validate chat-streaming-slash-tui --json`
- [ ] 6.5 Commit: `git commit -m "test(chat-stream): validate streaming TUI change"`
