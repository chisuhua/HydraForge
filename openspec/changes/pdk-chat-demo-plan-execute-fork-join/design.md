# Design: PDK Chat Demo — PlanExecuteLoop + ForkJoinLoop DSL 示例与集成

## Context

`include/agenticdsl/pdk/agent_loops/plan_execute_loop.h` 和 `fork_join_loop.h` 已 ship (Sprint 20, `openspec/changes/2026-07-01-pdk-plan-execute-fork-join`)。`examples/pdk_chat_demo/` 缺少对应的 DSL 示例和工作流集成。本 change 为 Phase 6a U1 实施。

## Goals / Non-Goals

**Goals:**
- 创建 2 个 `.agent.md` DSL 示例文件 (PlanExecuteLoop + ForkJoinLoop)
- 创建 2 个集成测试 (1 per loop) 验证 end-to-end 执行
- 更新 `examples/pdk_chat_demo/README.md`

**Non-goals:**
- 不修改 loop APIs (plan_execute_loop.h / fork_join_loop.h / react_loop.h)
- 不实施新 DSL 语法
- 不修改 ToolCoordinator / ToolMetadata / ADR-0068 Appendix A / 任何 ADR 文档

## Decisions

### Decision 1: DSL 示例文件位置
**选择**: `examples/pdk_chat_demo/dsl/` 子目录

```
examples/pdk_chat_demo/dsl/
├── plan_execute_example.agent.md   # PlanExecuteLoop 示例
└── fork_join_example.agent.md      # ForkJoinLoop 示例
```

**理由**: 与 `examples/agent_basic/workflow.agent.md` 模式一致, 集中管理 DSL 示例。

### Decision 2: PlanExecuteLoop DSL 示例结构
**内容**:
- `/main` 子图: Start → ToolCall (规划工具) → End
- PlanExecuteLoop.plan_phase 生成子图 DSL
- MockLLMProvider 预设: plan DSL 片段 + "yes" verify 响应

```yaml
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: start
    next: ["/main/explore"]
  - id: explore
    type: tool_call
    tool: search_knowledge_base
    arguments:
      query: "{{ $.user_goal }}"
    next: "/main/synthesize"
  - id: synthesize
    type: tool_call
    tool: synthesize_response
    arguments:
      context: "{{ $.explore.result }}"
    next: "/end"
  - id: end
    type: end
# --- END AgenticDSL ---
```
```

### Decision 3: ForkJoinLoop DSL 示例结构
**内容**:
- `/main` 子图: Start → ForkJoin (并发分支) → End
- ForkJoinLoop.run() 接受 branches 列表, 每个 branch 并行执行

```yaml
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
    next: "/end"
  - id: search_web
    type: tool_call
    tool: web_search
    arguments: {query: "{{ $.user_goal }}"}
    next: "/end"
  - id: search_docs
    type: tool_call
    tool: docs_search
    arguments: {query: "{{ $.user_goal }}"}
    next: "/end"
  - id: search_code
    type: tool_call
    tool: code_search
    arguments: {query: "{{ $.user_goal }}"}
    next: "/end"
  - id: end
    type: end
# --- END AgenticDSL ---
```
```

### Decision 4: 集成测试策略
**选择**:
- `test_plan_execute_loop_integration.cpp`: 直接调用 `PlanExecuteLoop::run()`, MockLLMProvider 预设响应
- `test_fork_join_loop_integration.cpp`: 直接调用 `ForkJoinLoop::run()`, 验证 3 branch 并发成功

**理由**: 与 `tests/test_pdk_plan_execute.cpp` 和 `tests/test_pdk_fork_join.cpp` 模式一致, 但位于 `examples/pdk_chat_demo/tests/` 目录。

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| DSL 示例与真实 API 不匹配 | 基于既有 loop API 契约设计 (plan_phase/execute_phase/verify_phase) |
| 集成测试依赖复杂设置 | 使用与 root tests 相同的 MockLLMProvider 模式 |
| Mock provider 响应预设错误 | 参考 `tests/test_pdk_plan_execute.cpp` 的响应队列模式 |

## Migration Plan

### Step 1: 创建 DSL 示例文件
1. 创建 `examples/pdk_chat_demo/dsl/` 目录
2. 创建 `plan_execute_example.agent.md`
3. 创建 `fork_join_example.agent.md`

### Step 2: 创建集成测试
1. 创建 `examples/pdk_chat_demo/tests/test_plan_execute_loop_integration.cpp`
2. 创建 `examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp`
3. 更新 `examples/pdk_chat_demo/tests/CMakeLists.txt` 注册新测试

### Step 3: 更新 README
1. 在 `examples/pdk_chat_demo/README.md` 添加 DSL 示例章节

### Step 4: 验证
1. cmake build
2. ctest -R pdk_chat
3. adr_lint
4. docs_drift_audit
5. openspec validate

## Open Questions

无 — 所有 API 已在 Sprint 20 ship, 本 change 仅创建示例和测试。
