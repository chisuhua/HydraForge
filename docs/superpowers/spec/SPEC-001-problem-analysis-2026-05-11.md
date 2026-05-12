# HydraForge 项目问题分析规格书

**文档编号:** SPEC-001
**版本:** 1.0
**日期:** 2026-05-11
**状态:** 已完成
**作者:** Sisyphus

---

## 1. 文档目的

本文档对 HydraForge 项目进行全面的问题分析，基于项目健康状态审查报告，识别所有已知问题并进行分类和优先级评估。

---

## 2. 项目概述

### 2.1 项目信息
| 项目 | 值 |
|------|-----|
| 项目名称 | HydraForge (原 AgenticDSL) |
| 项目类型 | C++ DSL 执行引擎 |
| 编程语言 | C++20 |
| 构建系统 | CMake 3.20+ |
| 代码规模 | ~32K 行 C++ 代码，130 个文件 |
| 测试框架 | Catch2 |
| LLM 后端 | llama.cpp |

### 2.2 项目结构
```
HydraForge/
├── src/
│   ├── core/          # DSLEngine 核心入口
│   ├── common/        # 共享组件 (llm, tools, utils)
│   └── modules/       # 8 个功能模块
├── lib/               # DSL 标准库 (.md 文件)
├── external/          # 第三方依赖
├── tests/             # Catch2 单元测试 (15 个文件)
└── examples/          # 3 个示例程序
```

### 2.3 关键依赖
| 依赖 | 版本 | 类型 | 状态 |
|------|------|------|------|
| llama.cpp | gguf-v0.19.0 | 源码 (编译) | ⚠️ 已改为 FetchContent |
| nlohmann_json | v3.11.2 | Header-only | ✅ |
| inja | v3.5.0 | Header-only | ✅ |
| yaml-cpp | yaml-cpp-0.9.0 | 源码 (编译) | ✅ |

---

## 3. 问题分类

### 3.1 问题总览

| ID | 类别 | 严重度 | 问题描述 | 状态 |
|----|------|--------|----------|------|
| PROB-001 | 依赖管理 | 🔴 Critical | llama.cpp submodule 无法拉取 | ✅ 已修复 |
| PROB-002 | 构建系统 | 🟡 Medium | CMakeLists.txt 重复源文件 | ✅ 已修复 |
| PROB-003 | 构建系统 | 🟡 Medium | 废弃 CMake 模式 | ⏳ 待修复 |
| PROB-004 | 代码质量 | 🟡 Medium | 缺少代码质量工具配置 | ⏳ 待修复 |
| PROB-005 | 代码规范 | 🟡 Medium | Header 宏命名不一致 | ⏳ 待修复 |
| PROB-006 | 文档一致性 | 🟢 Low | 代码注释与实现不一致 | ⏳ 待修复 |
| PROB-007 | 代码清洁 | 🟢 Low | 存在 commented-out 代码 | ⏳ 待修复 |
| PROB-008 | 构建性能 | 🟡 Medium | 编译时间过长 (llama.cpp) | ⚠️ 已知限制 |

### 3.2 问题详细分析

#### PROB-001: llama.cpp submodule 无法拉取 (🔴 Critical)

**问题描述:**
GitHub 网络问题导致 llama.cpp submodule 超时，无法完成 `git submodule update --init --recursive`，阻塞项目构建。

**根本原因:**
- llama.cpp 作为 git submodule 依赖外部网络
- 网络不稳定或防火墙导致 HTTPS 连接失败
- 超时时间不足

**影响范围:**
- 100% 项目无法构建
- 所有开发者都会遇到此问题

**已修复方案:**
- 移除 git submodule
- 改用 CMake FetchContent 自动下载
- 使用 `GIT_SHALLOW TRUE` 减少下载量

---

#### PROB-002: CMakeLists.txt 重复源文件 (🟡 Medium)

**问题描述:**
`agenticdsl_common` 库中 `llama_adapter.cpp` 和 `registry.cpp` 重复列出。

**代码位置:**
```cmake
add_library(agenticdsl_common STATIC
    src/common/llm/llama_adapter.cpp    # 第一次
    src/common/llm/llama_tool.cpp
    src/common/tools/registry.cpp        # 第一次
    src/common/llm/llama_adapter.cpp    # 重复!
    src/common/tools/registry.cpp        # 重复!
    src/common/utils/parser_utils.cpp
    src/common/utils/template_renderer.cpp
    src/common/utils/yaml_json.cpp
)
```

**影响:**
- 编译警告
- 轻微构建时间增加
- 代码维护性问题

**已修复方案:**
移除重复条目。

---

#### PROB-003: 废弃 CMake 模式 (🟡 Medium)

**问题描述:**
使用已废弃的 CMake 函数:
- `include_directories()` - 应使用 `target_include_directories()`
- `link_directories()` - 应在 `target_link_libraries()` 中指定完整路径

**代码位置:**
```cmake
# 第 35-43 行
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/modules
    ...
)

# 第 46 行
link_directories(${CMAKE_BINARY_DIR}/bin)
```

**影响:**
- CMake 官方不推荐
- 可能导致include路径污染
- 降低构建系统的可维护性

**建议修复方案:**
创建 INTERFACE library 统一管理公共includes。

---

#### PROB-004: 缺少代码质量工具配置 (🟡 Medium)

**问题描述:**
项目缺少以下配置文件:
- `.clang-format` - 代码格式化配置
- `.clang-tidy` - 静态分析配置
- `compile_commands.json` - LSP 索引文件

**影响:**
- 无法进行自动代码格式检查
- LSP 无法准确工作
- 代码风格不统一
- 静态分析工具无法运行

**建议修复方案:**
添加 `.clang-format` 和 `.clang-tidy` 配置文件。

---

#### PROB-005: Header 宏命名不一致 (🟡 Medium)

**问题描述:**
头文件保护宏命名不统一，存在多种格式:

| 格式 | 示例 | 文件数 |
|------|------|--------|
| `AGENTICDSL_*` | `AGENTICDSL_CORE_ENGINE_H` | 5 |
| `AGENTICDSL_MODULES_*` | `AGENTICDSL_MODULES_PARSER_MARKDOWN_PARSER_H` | 6 |
| `AGENTICDSL_TYPES_*` | `AGENTICDSL_TYPES_CONTEXT_H` | 4 |
| `AGENTICDSL_LLM_*` | `AGENTICDSL_LLM_LLAMA_ADAPTER_H` | 3 |
| `AGENTICDSL_COMMON_*` | `AGENTICDSL_COMMON_UTILS_YAML_JSON_H` | 2 |
| `COMMON_*` | `COMMON_TOOLS_REGISTRY_H` | 1 |
| `AGENTICDSL_SCHEDULER_*` | `AGENTICDSL_SCHEDULER_RESOURCE_MANAGER_H` | 1 |

**主要不一致:**
1. `COMMON_TOOLS_REGISTRY_H` 缺少 `AGENTICDSL_` 前缀
2. `AGENTICDSL_TYPES_*` vs `AGENTICDSL_CORE_TYPES_*` - 混用
3. `AGENTICDSL_LLM_*` vs `AGENTICDSL_COMMON_*` - llm 目录无 COMMON 前缀
4. `AGENTICDSL_SCHEDULER_*` vs `AGENTICDSL_MODULES_SCHEDULER_*` - 缺少 MODULES

**建议规范:**
统一为 `AGENTICDSL_<MODULE>_<FILE>_H` 格式

---

#### PROB-006: 代码注释与实现不一致 (🟢 Low)

**问题描述:**
文档与代码存在不一致:
1. `src/modules/executor/AGENTS.md` 列出 12 种节点类型，但 `NodeType` enum 只有 10 种
2. AGENTS.md 说有 `llm_call` 类型，但 enum 中是 `DSL_CALL`
3. AGENTS.md 说有 `loop` 节点，但 enum 中不存在

**实际 NodeType enum (10 种):**
```cpp
enum class NodeType : uint8_t {
    START, END, ASSIGN, DSL_CALL, TOOL_CALL,
    RESOURCE, FORK, JOIN, GENERATE_SUBGRAPH, ASSERT
};
```

**建议修复:**
更新 AGENTS.md 移除不存在的类型 (`llm_call`, `loop`)，统一为 10 种

---

#### PROB-007: 存在 Commented-Out 代码 (🟢 Low)

**问题描述:**
`node_executor.cpp:21-24` 存在被注释掉的资源上下文注入代码:
```cpp
//auto resources_ctx = ResourceManager::instance().get_resources_context();
//if (!resources_ctx.empty()) {
//    context_with_resources["resources"] = resources_ctx;
//}
```

**建议:**
确认是否需要，如不需要则删除。

---

#### PROB-008: 编译时间过长 (🟡 Medium)

**问题描述:**
llama.cpp 源码巨大 (870MB+)，完整编译需要 10+ 分钟。

**影响:**
- 开发迭代速度降低
- CI/CD 时间延长

**缓解措施:**
- 使用浅克隆 (`GIT_SHALLOW TRUE`)
- 考虑预编译库分发 (未来)

---

## 4. 风险评估

### 4.1 风险矩阵

| 风险 | 可能性 | 影响 | 风险等级 |
|------|--------|------|----------|
| 构建环境网络问题 | 高 | 高 | 🔴 高 |
| 代码风格不统一 | 中 | 中 | 🟡 中 |
| Header 宏冲突 | 低 | 高 | 🟡 中 |
| 文档过时导致误导 | 中 | 中 | 🟡 中 |

### 4.2 关键观察

1. **构建阻塞**: PROB-001 是唯一的阻塞性问题，已修复
2. **代码质量**: PROB-004, PROB-005 影响长期维护
3. **文档准确性**: PROB-006 可能导致开发者困惑

---

## 5. 修复优先级

### 5.1 优先级定义

| 优先级 | 定义 | 问题 |
|--------|------|------|
| P0 (Critical) | 阻塞构建 | PROB-001 |
| P1 (High) | 影响核心功能 | PROB-002 |
| P2 (Medium) | 影响代码质量/维护 | PROB-003, PROB-004, PROB-005 |
| P3 (Low) | 改进项 | PROB-006, PROB-007, PROB-008 |

### 5.2 当前状态

```
P0: ✅ PROB-001 已修复
P1: ✅ PROB-002 已修复
P2: ⏳ PROB-003, 004, 005 待修复
P3: ⏳ PROB-006, 007, 008 待修复
```

---

## 6. 附录

### A. 审查方法

- CMake 配置分析
- 源代码静态分析
- 构建测试
- Git submodule 状态检查

### B. 相关文件

- `plans/fix-plan-001.md` - 修复计划文档
- `AGENTS.md` - 项目知识库
- `.gitmodules` - Git 子模块配置
- `CMakeLists.txt` - CMake 构建配置

### C. 参考资料

- [CMake 官方推荐做法](https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [LLVM 代码格式化文档](https://clang.llvm.org/docs/ClangFormat.html)
