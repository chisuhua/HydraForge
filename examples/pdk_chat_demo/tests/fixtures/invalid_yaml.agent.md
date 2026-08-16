# Test fixture — invalid YAML (truncated mapping)
# Validates INVALID_YAML error type

```yaml
# --- BEGIN AgenticDSL ---
name: invalid-demo
version: 0.1.0
agent_loop: react
nodes:
  - id: n1
    type: start
  - id: n2
    type: call_tool
    config:
      timeout_ms: 5000
      retries: 3
        extra_nested_indent_broken: true
```