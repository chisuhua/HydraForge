# ForkJoinLoop 示例 — 并行多源搜索
#
# 用途: 展示 ForkJoinLoop 4 阶段 DSL 格式
#       Forking → Executing → Joining → Done
# 循环类型: fork_join
# 依赖: ForkJoinLoop.run(branches, ctx, token)
#
# 使用方式:
#   ForkJoinLoop loop(std::move(engine), bus, /*num_threads=*/4);
#   std::vector<std::string> branches = {"search_web", "search_docs", "search_code"};
#   LoopResult result = loop.run(branches, ctx);
#
# ForkJoinLoop 状态机:
#   Forking → Executing → Joining → Done (全部 branch 成功)
#   Forking/Executing → Done (任一 branch 失败, fail-fast)
#
# 并发执行:
#   - 3 个 branch 并行执行 (search_web, search_docs, search_code)
#   - 各 branch 返回 {branch_id, data: args}
#   - Joining 阶段合并到 final_context.working["data"]

### AgenticDSL `/__meta__`
```yaml
# --- BEGIN AgenticDSL ---
version: "3.1"
mode: dev
entry_point: "/main/start"
execution_budget:
  max_nodes: 25
  max_llm_calls: 3
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
    next: ["/main/parallel_search"]
  - id: parallel_search
    type: fork_join
    branches:
      - "/main/search_web"
      - "/main/search_docs"
      - "/main/search_code"
    join_strategy: merge_all
    next: "/main/merge_results"
  - id: merge_results
    type: tool_call
    tool: merge_search_results
    arguments:
      sources:
        web: "{{ $.search_web }}"
        docs: "{{ $.search_docs }}"
        code: "{{ $.search_code }}"
    output_keys: ["merged_results"]
    next: "/end"
  - id: end
    type: end
# --- END AgenticDSL ---
```

### AgenticDSL `/main/search_web`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: start
    next: ["/main/web_search"]
  - id: web_search
    type: tool_call
    tool: web_search
    arguments:
      query: "{{ $.user_goal }}"
      max_results: 10
    output_keys: ["web_results"]
    next: "/end"
  - id: end
    type: end
# --- END AgenticDSL ---
```

### AgenticDSL `/main/search_docs`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: start
    next: ["/main/docs_search"]
  - id: docs_search
    type: tool_call
    tool: docs_search
    arguments:
      query: "{{ $.user_goal }}"
      max_results: 5
    output_keys: ["docs_results"]
    next: "/end"
  - id: end
    type: end
# --- END AgenticDSL ---
```

### AgenticDSL `/main/search_code`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: start
    next: ["/main/code_search"]
  - id: code_search
    type: tool_call
    tool: code_search
    arguments:
      query: "{{ $.user_goal }}"
      max_results: 5
    output_keys: ["code_results"]
    next: "/end"
  - id: end
    type: end
# --- END AgenticDSL ---
```
