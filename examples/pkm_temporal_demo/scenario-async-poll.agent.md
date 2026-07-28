### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: start
    next: [assign_input]
  - id: assign_input
    type: assign
    assign:
      user_input: "hello"
    next: [call_start_async]
  - id: call_start_async
    type: tool_call
    tool: temporal/start_async
    arguments:
      workflow_id: "wf-async"
      args: '{"task":"long","latency_ms":5000}'
    output_keys: "async_result"
    next: [call_poll]
  - id: call_poll
    type: tool_call
    tool: temporal/poll
    arguments:
      workflow_id: "wf-async"
    output_keys: "poll_result"
    next: [end]
  - id: end
    type: end
# --- END AgenticDSL ---
```
