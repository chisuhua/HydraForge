### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: start
    next: [/main/assign_input]
  - id: assign_input
    type: assign
    assign:
      user_input: "hello"
    next: [/main/call_start_async]
  - id: call_start_async
    type: tool_call
    tool: temporal/start_async
    arguments:
      workflow_id: "wf-sig"
      args: '{"task":"wait_signal"}'
    output_keys: "async_result"
    next: [/main/call_signal]
  - id: call_signal
    type: tool_call
    tool: temporal/signal
    arguments:
      workflow_id: "wf-sig"
      signal_name: "go"
      payload: '{}'
    output_keys: "signal_result"
    next: [/main/call_poll]
  - id: call_poll
    type: tool_call
    tool: temporal/poll
    arguments:
      workflow_id: "wf-sig"
    output_keys: "poll_result"
    next: [/main/end]
  - id: end
    type: end
# --- END AgenticDSL ---
```
