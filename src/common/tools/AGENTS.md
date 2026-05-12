# tools 模块

**Generated:** 2026-05-11

## OVERVIEW
工具注册表，支持函数工具和 LLM 工具两种类型。

## WHERE TO LOOK
| Task | Location |
|------|----------|
| 工具注册 | `registry.cpp` - register_tool() |
| 工具调用 | `registry.cpp` - call_tool() |
| LLM 工具注册 | `registry.cpp` - register_llm_tool() |
| LLM 工具调用 | `registry.cpp` - call_llm_tool() |

## ANTI-PATTERNS
- 不要在运行时动态创建 `std::function` 持有 lambda（捕获风险）