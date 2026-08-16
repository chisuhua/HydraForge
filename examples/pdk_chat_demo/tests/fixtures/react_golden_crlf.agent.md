# Test fixture — React loop golden example (LF)
# Schema-conforming .agent.md for DslValidator YAML parsing tests
# Frontmatter: name/version/agent_loop (all required)
# Nodes use validator whitelist types (start/end/llm_generate/call_tool/condition)

```yaml
# --- BEGIN AgenticDSL ---
name: react-demo
version: 0.1.0
agent_loop: react
nodes:
  - id: n_start
    type: start
  - id: n_think
    type: llm_generate
  - id: n_decide
    type: condition
  - id: n_act
    type: call_tool
    tool_name: shell_exec
  - id: n_end
    type: end
```