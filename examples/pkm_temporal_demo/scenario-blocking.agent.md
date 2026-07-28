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
    next: [/main/call_start]
  - id: call_start
    type: tool_call
    tool: temporal/start_workflow
    arguments:
      workflow_id: "wf-block"
      args: '{"task":"noop","latency_ms":100}'
    output_keys: "wf_result"
    next: [/main/end]
  - id: end
    type: end
# --- END AgenticDSL ---
```
