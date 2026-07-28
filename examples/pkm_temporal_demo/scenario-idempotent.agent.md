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
    next: [call_first_start]
  - id: call_first_start
    type: tool_call
    tool: temporal/start_async
    arguments:
      workflow_id: "dup"
      args: '{"task":"x"}'
    output_keys: "first_result"
    next: [call_second_start]
  - id: call_second_start
    type: tool_call
    tool: temporal/start_async
    arguments:
      workflow_id: "dup"
      args: '{"task":"y"}'
    output_keys: "second_result"
    next: [call_poll]
  - id: call_poll
    type: tool_call
    tool: temporal/poll
    arguments:
      workflow_id: "dup"
    output_keys: "poll_result"
    next: [end]
  - id: end
    type: end
# --- END AgenticDSL ---
```
