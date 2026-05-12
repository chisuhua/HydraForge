# PROJECT KNOWLEDGE BASE

**Generated:** 2026-05-11
**Commit:** cc8c7df
**Branch:** main

## OVERVIEW
AgenticDSL 是一个 DSL 执行引擎，通过 Markdown DSL 定义工作流图（DAG），支持 LLM 调用、工具注册、资源管理和预算控制。C++20 实现，使用 llama.cpp 作为 LLM 后端。

## STRUCTURE
```
HydraForge/
├── src/
│   ├── core/          # DSLEngine 核心入口
│   │   └── types/     # Context, Node, Budget, Resource 类型定义
│   ├── common/        # 共享组件
│   │   ├── llm/       # LlamaAdapter (llama.cpp 封装)
│   │   ├── tools/     # ToolRegistry (工具注册表)
│   │   └── utils/     # YAML/JSON 解析、模板渲染
│   └── modules/       # 8 个功能模块
│       ├── parser/    # MarkdownParser → ParsedGraph
│       ├── scheduler/ # TopoScheduler (DAG 拓扑调度)
│       ├── executor/  # NodeExecutor (节点执行器)
│       ├── context/   # Context 管理
│       ├── budget/    # BudgetController (预算控制)
│       ├── trace/     # TraceRecord 追踪
│       ├── library/   # StandardLibraryLoader (标准库加载)
│       └── system/    # System 模块
├── lib/               # DSL 标准库 (.md 文件)
│   ├── auth/          # 认证相关 DSL
│   ├── human/         # 人类交互 DSL
│   ├── math/          # 数学工具 DSL
│   └── utils/         # 通用工具 DSL
├── external/          # 第三方依赖 (llama.cpp, nlohmann_json, inja, yaml-cpp)
├── tests/             # Catch2 单元测试 (15 个测试文件)
└── examples/          # 3 个示例程序
    ├── agent_basic/   # 主要示例：加载 .agent.md 工作流
    ├── agent_simple/  # 简化示例
    └── agent_loop/    # 循环执行示例
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| 添加新节点类型 | `src/modules/executor/node_executor.h` | execute_xxx 方法 + execute_node 分发 |
| 添加新 DSL 语法 | `src/modules/parser/markdown_parser.h` | create_node_from_json |
| 修改调度逻辑 | `src/modules/scheduler/topo_scheduler.cpp` | build_dag() / schedule() |
| LLM 调用修改 | `src/common/llm/llama_adapter.cpp` | generate() 底层 |
| 工具注册/调用 | `src/common/tools/registry.cpp` | call_tool() / register_tool() |
| 预算管理 | `src/modules/budget/budget_controller.cpp` | ExecutionBudget 扣费 |
| 编写测试 | `tests/test_*.cpp` | Catch2，tag 格式 `[module][stageN]` |
| DSL 标准库 | `lib/*.md` | Markdown 格式的子图定义 |

## CODE MAP (Key Symbols)

| Symbol | Type | Location | Role |
|--------|------|----------|------|
| DSLEngine | class | src/core/engine.h | 主入口，from_markdown / run |
| ParsedGraph | struct | src/core/types/node.h | 解析后的图结构 |
| TopoScheduler | class | src/modules/scheduler/topo_scheduler.h | DAG 调度器 |
| NodeExecutor | class | src/modules/executor/node_executor.h | 节点执行器 |
| ToolRegistry | class | src/common/tools/registry.h | 工具注册表 |
| LlamaAdapter | class | src/common/llm/llama_adapter.h | llama.cpp 封装 |
| ExecutionBudget | struct | src/core/types/budget.h | 预算结构 |

## CONVENTIONS
- **2 空格缩进**，中文注释（避免中英混杂）
- **命名规范**：CamelCase 类名/结构体，snake_case 变量，SCREAMING_SNAKE_CASE 宏
- **文件头注释**：功能描述、作者、日期
- **CMake**：每个模块独立 CMakeLists.txt，最终聚合成 agenticdsl_core

## ANTI-PATTERNS (THIS PROJECT)
- **禁止** `include_directories()` 全局包含 → 应用 `target_include_directories()`
- **禁止** `link_directories()` → 应在 CMake target_link_libraries 中指定完整路径
- **禁止** `as any` / `@ts-ignore` 类型压制
- **禁止** 空 catch 块 `catch(e) {}`
- **禁止** 删除失败的测试来"通过"

## BUILD SYSTEM
- CMake 3.20+，C++20
- 根 `CMakeLists.txt` 聚合 8 个模块静态库 → `agenticdsl_core`
- 构建：`./build.sh` 或 `mkdir build && cd build && cmake .. && make -j$(nproc)`
- 测试：`cmake .. -DAGENTICDSL_BUILD_TESTS=ON && make && ctest --output-on-failure`

## NOTES
- `engine.h` 直接 `#include "modules/scheduler/topo_scheduler.h"`（跨模块耦合）
- `lib/` 目录存放 `.md` DSL 文件，非 C++ 库
- `src/modules/exports/` 存放导出类型定义
- `src/modules/prompts.yaml` 包含 LLM prompt 模板
- `llm_config.json` 运行时 LLM 配置（模型路径、温度等）
- 无 `.clang-format` / `.clang-tidy` / `compile_commands.json`
- 无 GitHub Actions CI