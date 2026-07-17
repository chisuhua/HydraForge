# ReactLoop Agent
# 关联: docs/adr/adr-0021-pdk-design.md (DEFINE_AGENT)
#      docs/adr/adr-0061-03-skill-compiler.md (DSL template)

## metadata
- version: 0.1.0
- loop_type: react
- max_steps: 50
- budget_inheritance: strict
- entry_tool: loop/run

## nodes

### start
- type: start
- next: [/loop/think]

### think
- type: generate
- prompt: |
    {{system_prompt}}
    
    Conversation:
    {{history}}
    
    User: {{user_input}}
- tools: {{active_tools}}
- output: llm_response
- next: [/loop/decide]

### decide
- type: condition
- condition: "{{llm_response.tool_calls.length}} > 0"
- true: /loop/tool_call
- false: /loop/respond

### tool_call
- type: tool
- name: "{{llm_response.tool_calls[0].name}}"
- args: "{{llm_response.tool_calls[0].args}}"
- output: tool_result
- next: [/loop/observe]

### observe
- type: assign
- history: "{{history}}\nTool: {{tool_result}}"
- next: /loop/think

### respond
- type: assign
- response: "{{llm_response.content}}"
- next: /loop/end

### end
- type: end
- output: "{{response}}"