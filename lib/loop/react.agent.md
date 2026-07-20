### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: [/main/think]
  - id: think
    type: llm_call
    prompt_template: |
      {{system_prompt}}

      Conversation:
      {{history}}

      User: {{user_input}}
    output_keys: [llm_response]
    next: [/main/decide]
  - id: decide
    type: tool_call
    tool_name: loop/decide_react
    args:
      response: "{{llm_response}}"
    output_keys: [decision]
    next: [/main/act]
  - id: act
    type: tool_call
    tool_name: "{{decision.action_tool}}"
    args: "{{decision.action_args}}"
    output_keys: [tool_result]
    next: [/main/observe]
  - id: observe
    type: assign
    assign:
      history: "{{history}}\nObservation: {{tool_result}}"
      step: "{{step + 1}}"
    next: [/main/end]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
