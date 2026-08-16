# Test fixture — PlanExecute loop golden example (LF)
# Schema-conforming .agent.md for DslValidator YAML parsing tests

```yaml
# --- BEGIN AgenticDSL ---
name: plan-execute-demo
version: 0.2.0
agent_loop: plan_execute
nodes:
  - id: pe_start
    type: start
  - id: pe_plan
    type: assign
  - id: pe_execute
    type: call_tool
    tool_name: file_read
  - id: pe_verify
    type: llm_generate
  - id: pe_end
    type: end
```