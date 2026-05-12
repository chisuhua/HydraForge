# HydraForge 实施计划

**文档编号:** PLAN-001
**版本:** 1.0
**日期:** 2026-05-11
**状态:** 草稿
**基于:** docs/superpowers/spec/SPEC-001-problem-analysis-2026-05-11.md, docs/superpowers/spec/SPEC-002-fix-solution-2026-05-11.md

---

## 1. 计划概述

本文档为 HydraForge 项目修复提供详细的实施计划，包括任务分解、依赖关系、验收标准和时间估算。

---

## 2. 已完成工作

| 任务 | 状态 | 完成日期 |
|------|------|----------|
| T-001: 移除 llama.cpp submodule | ✅ 已完成 | 2026-05-11 |
| T-002: 修复 CMakeLists.txt 重复源文件 | ✅ 已完成 | 2026-05-11 |

---

## 3. 待实施任务

### 阶段 1: 构建系统修复 (P2)

#### T-003: 现代化 CMake 配置

**任务描述:**
将废弃的 `include_directories()` 和 `link_directories()` 改为 INTERFACE library 模式。

**输入:** SPEC-002-fix-solution.md §3.1
**输出:**
- 新增 `agenticdsl_includes` INTERFACE library
- 修改 `CMakeLists.txt`
- 移除全局 `include_directories()` 和 `link_directories()`

**子任务:**
1. 创建 `agenticdsl_includes` INTERFACE library
2. 配置 `target_include_directories()` 使用 generator expressions
3. 配置 `target_link_libraries()` 传递依赖
4. 更新所有 target 链接 `agenticdsl_includes`
5. 移除全局 `include_directories()` 调用
6. 移除 `link_directories()` 调用

**依赖:** T-002
**预计工时:** 45 分钟
**验收标准:**
```bash
# 构建成功
rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)

# 无 include_directories 警告
# CMake 输出无 "include_directories" 废弃警告
```

---

### 阶段 2: 代码质量工具 (P2)

#### T-004: 添加 .clang-format 配置

**任务描述:**
创建代码格式化配置文件。

**输入:** SPEC-002-fix-solution.md §3.2.1
**输出:**
- 新建 `.clang-format` 文件

**内容:**
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

**依赖:** 无
**预计工时:** 15 分钟
**验收标准:**
```bash
# 检查格式
clang-format --dry-run --ferror-threshold=1 src/modules/executor/node_executor.cpp

# 无错误退出
echo $? == 0
```

---

#### T-005: 添加 .clang-tidy 配置

**任务描述:**
创建静态分析配置文件。

**输入:** SPEC-002-fix-solution.md §3.2.2
**输出:**
- 新建 `.clang-tidy` 文件

**内容:**
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

**依赖:** T-004
**预计工时:** 15 分钟
**验收标准:**
```bash
# 生成 compile_commands.json
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

# 运行静态分析
clang-tidy -p build src/modules/executor/node_executor.cpp 2>&1 | head -20
```

---

#### T-006: 生成 compile_commands.json

**任务描述:**
配置 CMake 生成 compile_commands.json 以支持 LSP 和 clang-tidy。

**输入:** SPEC-002-fix-solution.md §3.2.3
**输出:**
- 修改 `CMakeLists.txt` 添加 `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)`

**依赖:** 无 (独立于 T-003)
**预计工时:** 5 分钟
**验收标准:**
```bash
ls -la build/compile_commands.json
# 文件存在且非空
```

---

### 阶段 3: 代码规范 (P2)

#### T-007: 统一 Header 宏命名

**任务描述:**
将所有头文件保护宏统一为 `AGENTICDSL_<MODULE>_<FILE>_H` 格式。

**输入:** SPEC-002-fix-solution.md §3.3
**输出:**
- 修改以下文件的宏定义:
  - `src/common/tools/registry.h` - `COMMON_TOOLS_REGISTRY_H` → `AGENTICDSL_COMMON_TOOLS_REGISTRY_H`
  - `src/core/types/node.h` - `AGENTICDSL_COMMON_TYPES_NODE_H` → `AGENTICDSL_CORE_TYPES_NODE_H`
  - `src/core/types/context.h` - `AGENTICDSL_TYPES_CONTEXT_H` → `AGENTICDSL_CORE_TYPES_CONTEXT_H`
  - `src/core/types/common.h` - `AGENTICDSL_TYPES_COMMON_H` → `AGENTICDSL_CORE_TYPES_COMMON_H`
  - `src/modules/scheduler/resource_manager.h` - `AGENTICDSL_SCHEDULER_RESOURCE_MANAGER_H` → `AGENTICDSL_MODULES_SCHEDULER_RESOURCE_MANAGER_H`

**子任务:**
1. 修改 `COMMON_TOOLS_REGISTRY_H` → `AGENTICDSL_COMMON_TOOLS_REGISTRY_H`
2. 修改 `AGENTICDSL_COMMON_TYPES_NODE_H` → `AGENTICDSL_CORE_TYPES_NODE_H` (注意: 当前是 COMMON 不是 TYPES)
3. 修改 `AGENTICDSL_TYPES_CONTEXT_H` → `AGENTICDSL_CORE_TYPES_CONTEXT_H`
4. 修改 `AGENTICDSL_TYPES_COMMON_H` → `AGENTICDSL_CORE_TYPES_COMMON_H`
5. 修改 `AGENTICDSL_SCHEDULER_RESOURCE_MANAGER_H` → `AGENTICDSL_MODULES_SCHEDULER_RESOURCE_MANAGER_H`
6. 验证构建

**依赖:** T-003
**预计工时:** 20 分钟
**验收标准:**
```bash
# 验证所有宏统一
grep -r "#ifndef AGENTICDSL" src/ | wc -l
# 应覆盖所有 .h 文件

# 构建验证
make -j$(nproc)
ctest --output-on-failure
```

---

### 阶段 4: 文档修复 (P3)

#### T-008: 修复 AGENTS.md 文档

**任务描述:**
更新 `src/modules/executor/AGENTS.md`，移除不存在的节点类型引用。

**输入:** SPEC-002-fix-solution.md §3.4
**输出:**
- 修改 `src/modules/executor/AGENTS.md`

**修改内容:**
1. 将 NODE TYPES 表格中的 `llm_call` 行移除 (实际 enum 中是 DSL_CALL)
2. 移除 `loop` 行 (不存在于 enum 中)
3. 将 `dsl_node` 改为 `dsl_call` (与 enum 一致)
4. 更新为 10 种节点类型

**修改后表格:**
```
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
```

**依赖:** 无
**预计工时:** 10 分钟
**验收标准:**
```bash
# 无过时引用
grep -E "llm_call|loop" src/modules/executor/AGENTS.md
# 应无输出
```

---

### 阶段 5: 代码清理 (P3)

#### T-009: 清理 Commented-Out 代码

**任务描述:**
删除 `node_executor.cpp` 中无用的注释代码。

**输入:** SPEC-002-fix-solution.md §3.5
**输出:**
- 修改 `src/modules/executor/node_executor.cpp`

**修改内容:**
删除以下注释代码 (第 21-24 行):
```cpp
//auto resources_ctx = ResourceManager::instance().get_resources_context();
//if (!resources_ctx.empty()) {
//    context_with_resources["resources"] = resources_ctx;
//}
```

**依赖:** 无
**预计工时:** 5 分钟
**验收标准:**
```bash
# 无注释代码残留
grep -n "//auto resources" src/modules/executor/node_executor.cpp
# 应无输出

# 构建验证
make
```

---

## 4. 任务依赖图

```
T-001 ✅ ──┐
T-002 ✅ ──┤
           ├── T-003 ──┬── T-007 ──┐
           │           └── T-006 ──┤
           │                        ├── T-008
T-004 ─────┤                        │
T-005 ─────┴────────────────────────┴── T-009
```

---

## 5. 时间估算

| 阶段 | 任务 | 预计工时 | 累计 |
|------|------|----------|------|
| 阶段 1 | T-003: CMake 现代化 | 45 min | 45 min |
| 阶段 2 | T-004: .clang-format | 15 min | 60 min |
| 阶段 2 | T-005: .clang-tidy | 15 min | 75 min |
| 阶段 2 | T-006: compile_commands | 5 min | 80 min |
| 阶段 3 | T-007: Header 宏命名 | 20 min | 100 min |
| 阶段 4 | T-008: AGENTS.md | 10 min | 110 min |
| 阶段 5 | T-009: 清理注释代码 | 5 min | 115 min |

**总计: ~2 小时**

---

## 6. 验收标准

### 6.1 构建验收

```bash
# 完整构建流程
rm -rf build
mkdir build && cd build
cmake .. -DAGENTICDSL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure
```

**验收标准:** 所有测试通过，构建无警告

### 6.2 代码质量验收

```bash
# 格式检查
clang-format --dry-run --ferror-threshold=1 src/ 2>&1 | grep -v "Skipping"

# 静态分析
clang-tidy -p build src/modules/executor/node_executor.cpp 2>&1 | grep -E "error|warning" | head -10
```

**验收标准:** 无格式错误，无严重警告

### 6.3 文档验收

```bash
# 文档一致性
grep -r "llm_call\|loop" src/modules/executor/AGENTS.md
# 无输出
```

---

## 7. 风险与缓解

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| CMake 更改破坏构建 | 低 | 高 | 每次修改后完整构建验证 |
| Header 宏变更引入编译错误 | 低 | 高 | 保留备份，git stash 快速回滚 |
| clang-format 改变过多文件 | 中 | 低 | 使用 `--dry-run` 预检 |

---

## 8. 里程碑

| 里程碑 | 日期 | 条件 |
|--------|------|------|
| M1: 构建系统就绪 | +45 min | T-003 完成 |
| M2: 代码质量工具就绪 | +80 min | T-004, T-005, T-006 完成 |
| M3: 代码规范统一 | +100 min | T-007 完成 |
| M4: 全部完成 | +115 min | 所有任务完成 |

---

## 9. 附录

### A. 相关文档

| 文档 | 路径 |
|------|------|
| 问题分析 | `docs/superpowers/spec/SPEC-001-problem-analysis-2026-05-11.md` |
| 修复方案 | `docs/superpowers/spec/SPEC-002-fix-solution-2026-05-11.md` |
| 实施计划 | `docs/superpowers/plan/PLAN-001-implementation-2026-05-11.md` |

### B. 命令参考

```bash
# 快速验证构建
rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)

# 运行测试
ctest --output-on-failure

# 格式检查
clang-format --dry-run --ferror-threshold=1 src/

# 静态分析
clang-tidy -p build src/ 2>&1 | head -50
```
