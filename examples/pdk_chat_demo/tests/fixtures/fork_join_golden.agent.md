# Test fixture — ForkJoin loop golden example (LF)
# Schema-conforming .agent.md for DslValidator YAML parsing tests

```yaml
# --- BEGIN AgenticDSL ---
name: fork-join-demo
version: 0.3.0
agent_loop: fork_join
nodes:
  - id: fj_start
    type: start
  - id: fj_branch_a
    type: call_tool
    tool_name: search_web
  - id: fj_branch_b
    type: call_tool
    tool_name: search_docs
  - id: fj_branch_c
    type: call_tool
    tool_name: search_code
  - id: fj_join
    type: join
  - id: fj_end
    type: end
```