# executor 模块

**Generated:** 2026-05-11

## OVERVIEW
节点执行器，根据节点类型分发到对应 execute_xxx 方法。

## WHERE TO LOOK
| Task | Location |
|------|----------|
| 节点执行入口 | `node_executor.cpp` - `execute_node()` |
| 执行方法分发 | `node_executor.cpp` - execute_start/end/assign/dsl_call/tool_call 等 |
| 权限检查 | `node_executor.cpp` - `check_permissions()` |

## NODE TYPES (10 种)
| Type | Method | Notes |
|------|--------|-------|
| start | execute_start | 入口节点 |
| end | execute_end | 终点节点 |
| assign | execute_assign | 变量赋值 |
| dsl_call | execute_dsl_node | DSL 调用 |
| tool_call | execute_tool_call | 工具调用 |
| resource | execute_resource | 资源声明 |
| generate_subgraph | execute_generate_subgraph | 动态生成子图 |
| join | execute_join | join 节点 |
| fork | execute_fork | fork 节点 |
| assert | execute_assert | 断言节点 |

## ANTI-PATTERNS
- 不要在 execute_xxx 中直接修改 node 对象