# PlanExecuteLoop 示例 — 研究量子计算进展
#
# 用途: 展示 PlanExecuteLoop 3 阶段 DSL 格式
#       Planning → Executing → Verifying (with retry)
# 循环类型: plan_execute
# 依赖: PlanExecuteLoop.run(goal, ctx, token)
#
# 使用方式:
#   PlanExecuteLoop loop(std::move(engine), bus);
#   LoopResult result = loop.run("研究量子计算的最新进展", ctx);
#
# PlanExecuteLoop 状态机:
#   Planning → Executing → Verifying → Done (成功)
#   Planning → Executing → Verifying → Retry (验证失败, 重新 Plan)
#   Planning/Executing/Verifying → Done (失败)
#
# 本文件展示 DSL 片段格式, 实际 plan 由 LLM 生成

### AgenticDSL `/__meta__`
```yaml
# --- BEGIN AgenticDSL ---
version: "3.1"
mode: dev
entry_point: "/main/start"
execution_budget:
  max_nodes: 20
  max_llm_calls: 5
# --- END AgenticDSL ---
```

### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: start
    next: ["/main/research"]
  - id: research
    type: tool_call
    tool: search_knowledge_base
    arguments:
      query: "{{ $.user_goal }}"
      top_k: 5
    output_keys: ["research_results"]
    next: "/main/analyze"
  - id: analyze
    type: tool_call
    tool: analyze_findings
    arguments:
      findings: "{{ $.research_results }}"
    output_keys: ["analysis"]
    next: "/main/summarize"
  - id: summarize
    type: tool_call
    tool: synthesize_response
    arguments:
      analysis: "{{ $.analyze }}"
      format: "detailed"
    output_keys: ["summary"]
    next: "/end"
  - id: end
    type: end
# --- END AgenticDSL ---
```

### AgenticDSL `/main/research`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: start
    next: ["/main/search"]
  - id: search
    type: tool_call
    tool: search_knowledge_base
    arguments:
      query: "{{ $.user_goal }}"
      top_k: 5
    output_keys: ["research_results"]
    next: "/end"
  - id: end
    type: end
# --- END AgenticDSL ---
```

### AgenticDSL `/end`
```yaml
# --- BEGIN AgenticDSL ---
type: end
# --- END AgenticDSL ---
```
