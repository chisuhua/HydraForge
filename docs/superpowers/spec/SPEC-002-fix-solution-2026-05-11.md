# HydraForge 修复方案规格书

**文档编号:** SPEC-002
**版本:** 1.0
**日期:** 2026-05-11
**状态:** 已完成
**基于:** SPEC-001-problem-analysis.md

---

## 1. 文档目的

本文档为 SPEC-001 中识别的问题提供具体的修复方案和技术规格。

---

## 2. 已完成修复

### 2.1 PROB-001: llama.cpp submodule 移除

**修复方案:** FetchContent 替代 submodule

**变更文件:**
- `.gitmodules` - 移除 llama.cpp 条目
- `CMakeLists.txt` - 新增 FetchContent 配置

**技术规格:**
```cmake
include(FetchContent)
FetchContent_Declare(
    llama_cpp
    GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
    GIT_TAG gguf-v0.19.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(llama_cpp)

set(LLAMA_LIB llama)
set(LLAMA_INCLUDE_DIR "${llama_cpp_SOURCE_DIR}/include")
set(GGML_INCLUDE_DIR "${llama_cpp_SOURCE_DIR}/ggml/include")
```

**验证方法:**
```bash
rm -rf build && mkdir build && cd build && cmake ..
```

---

### 2.2 PROB-002: 重复源文件修复

**修复方案:** 移除 CMakeLists.txt 中的重复条目

**变更文件:**
- `CMakeLists.txt:48-57`

**修复前:**
```cmake
add_library(agenticdsl_common STATIC
    src/common/llm/llama_adapter.cpp    # 重复
    src/common/llm/llama_tool.cpp
    src/common/tools/registry.cpp        # 重复
    ...
)
```

**修复后:**
```cmake
add_library(agenticdsl_common STATIC
    src/common/llm/llama_adapter.cpp
    src/common/llm/llama_tool.cpp
    src/common/tools/registry.cpp
    ...
)
```

---

## 3. 待修复方案

### 3.1 PROB-003: 现代化 CMake 配置

**问题:** 使用废弃的 `include_directories()` 和 `link_directories()`

**修复方案:** 创建 INTERFACE library 管理公共includes

**目标文件:** `CMakeLists.txt`

**技术规格:**
```cmake
# 创建 INTERFACE library 存放公共 includes 和链接
add_library(agenticdsl_includes INTERFACE)

# 设置 include 路径
target_include_directories(agenticdsl_includes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/modules>
    $<INSTALL_INTERFACE:include>
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${INJA_INCLUDE_DIR}
    ${LLAMA_INCLUDE_DIR}
    ${GGML_INCLUDE_DIR}
    ${YAML_CPP_INCLUDE_DIR}
)

# 设置链接库
target_link_libraries(agenticdsl_includes INTERFACE
    yaml-cpp::yaml-cpp
    ${LLAMA_LIB}
)

# 各 target 通过 link_directories 传递
target_link_libraries(agenticdsl_common PUBLIC agenticdsl_includes)
target_link_libraries(agenticdsl_core PUBLIC agenticdsl_includes)
```

**验证方法:**
```bash
cmake .. && make -j$(nproc) && ctest --output-on-failure
```

---

### 3.2 PROB-004: 添加代码质量工具配置

**问题:** 缺少 `.clang-format` 和 `.clang-tidy`

**修复方案:** 创建配置文件

#### 3.2.1 .clang-format

**目标文件:** `/workspace/project/HydraForge/.clang-format`

**技术规格:**
```yaml
BasedOnStyle: LLVM
IndentWidth: 2
UseTab: Never
ColumnLimit: 120
PointerAlignment: Left
ReferenceAlignment: Left
NamespaceIndentation: None
BreakBeforeBraces: Attach
AllowShortFunctionsOnASingleLine: Inline
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
```

#### 3.2.2 .clang-tidy

**目标文件:** `/workspace/project/HydraForge/.clang-tidy`

**技术规格:**
```yaml
Checks: >
  -*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  performance-*,
  modernize-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers,
  -cppcoreguidelines-avoid-magic-numbers
WarningsAsErrors: ''
HeaderFilterRegex: '.*/src/.*'
FormatStyle: file
```

#### 3.2.3 compile_commands.json

**生成方式:**
```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
# 或在 CMakeLists.txt 中添加
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

**验证方法:**
```bash
clang-tidy -p build src/modules/executor/node_executor.cpp
clang-format --dry-run --ferror-threshold=1 src/modules/executor/node_executor.cpp
```

---

### 3.3 PROB-005: 统一 Header 宏命名

**问题:** 宏命名不一致

**修复方案:** 统一为 `AGENTICDSL_<MODULE>_<FILE>_H` 格式

**需修改文件清单:**

| 当前宏 | 文件 | 新宏 |
|--------|------|------|
| `COMMON_TOOLS_REGISTRY_H` | registry.h | `AGENTICDSL_COMMON_TOOLS_REGISTRY_H` |
| `AGENTICDSL_TYPES_*` | node.h, context.h | `AGENTICDSL_CORE_TYPES_*` |
| `AGENTICDSL_COMMON_*` | common.h | `AGENTICDSL_CORE_TYPES_COMMON_H` |
| `AGENTICDSL_SCHEDULER_*` | resource_manager.h | `AGENTICDSL_MODULES_SCHEDULER_*` |

**技术规格:**
```cpp
// 旧格式
#ifndef COMMON_TOOLS_REGISTRY_H
#define COMMON_TOOLS_REGISTRY_H

// 新格式
#ifndef AGENTICDSL_COMMON_TOOLS_REGISTRY_H
#define AGENTICDSL_COMMON_TOOLS_REGISTRY_H
```

**验证方法:**
```bash
grep -r "#ifndef AGENTICDSL" src/ | wc -l
# 应覆盖所有 .h 文件
```

---

### 3.4 PROB-006: 修复代码注释不一致

**问题:** AGENTS.md 文档与代码不一致

**修复方案:** 更新文档

**需修改文件:**
- `src/modules/executor/AGENTS.md`

**修改内容:**
1. 移除 `llm_call` 类型引用 (应为 `dsl_call`)
2. 移除 `loop` 节点类型 (不存在于 enum)
3. 更新节点类型表格

**修改后表格:**
```
## NODE TYPES (10 种)
| Type | Method | Notes |
|------|--------|-------|
| start | execute_start | 入口节点 |
| end | execute_end | 终点节点 |
| assign | execute_assign | 变量赋值 |
| dsl_call | execute_dsl_node | DSL 调用 (原 llm_call) |
| tool_call | execute_tool_call | 工具调用 |
| resource | execute_resource | 资源声明 |
| generate_subgraph | execute_generate_subgraph | 动态生成子图 |
| join | execute_join | join 节点 |
| fork | execute_fork | fork 节点 |
| assert | execute_assert | 断言节点 |
```

---

### 3.5 PROB-007: 清理 Commented-Out 代码

**问题:** `node_executor.cpp:21-24` 存在无用注释代码

**修复方案:** 删除注释代码或恢复功能

**代码位置:**
```cpp
//auto resources_ctx = ResourceManager::instance().get_resources_context();
//if (!resources_ctx.empty()) {
//    context_with_resources["resources"] = resources_ctx;
//}
```

**选项 A:** 如果不需要，删除
**选项 B:** 如果需要，恢复并修复

**建议:** 选项 A (如果确实不需要)

---

## 4. 变更影响分析

### 4.1 兼容性影响

| 修复 | 向后兼容 | 说明 |
|------|----------|------|
| PROB-001 | ✅ | 行为不变，仅构建方式改变 |
| PROB-002 | ✅ | 无影响 |
| PROB-003 | ✅ | 行为不变，CMake 内部改进 |
| PROB-004 | ✅ | 无影响，仅新增工具 |
| PROB-005 | ⚠️ | Header 宏变更，但一般用户不直接使用 |
| PROB-006 | ✅ | 文档修复 |
| PROB-007 | ✅ | 删除无用代码 |

### 4.2 构建影响

| 修复 | 构建时间 | 依赖 |
|------|----------|------|
| PROB-001 | 首次增加 (FetchContent) | 网络 |
| PROB-002 | 无变化 | 无 |
| PROB-003 | 无变化 | CMake |
| PROB-004 | 无变化 | clang-format/tidy |
| PROB-005 | 无变化 | 无 |
| PROB-006 | 无变化 | 无 |
| PROB-007 | 无变化 | 无 |

---

## 5. 验证矩阵

| 修复 ID | 验证命令 | 预期结果 |
|---------|----------|----------|
| PROB-001 | `cmake .. && make` | 构建成功，无 submodule 警告 |
| PROB-002 | `make` | 无 "file listed twice" 警告 |
| PROB-003 | `make` | 构建成功，include 正确传递 |
| PROB-004 | `clang-format --check src/` | 代码格式符合规范 |
| PROB-004 | `clang-tidy src/` | 无重大警告 |
| PROB-005 | `grep "#ifndef AGENTICDSL" src/` | 所有 .h 匹配 |
| PROB-006 | `grep -r "llm_call\|loop" AGENTS.md` | 无匹配 |
| PROB-007 | `grep -n "//auto resources" src/` | 无匹配 |

---

## 6. 附录

### A. 相关文档

- SPEC-001-problem-analysis.md - 问题分析文档
- plans/fix-plan-001.md - 修复计划文档

### B. 参考资料

- [CMake INTERFACE library 最佳实践](https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html)
- [Clang-Format 配置文件文档](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
- [Clang-Tidy 配置文件文档](https://clang.llvm.org/extra/clang-tidy/)
