# Skill: cmake_workflow

**分类**: 轴2-领域/工具
**触发词**: "CMakeLists.txt", "cmake configure", "cmake 错误", "link error", "find_package"

## When to Use

在以下场景激活此技能：
- 编写或重构 `CMakeLists.txt`
- 配置 out-of-source 构建
- 选择生成器（Ninja/Make/VS）
- 使用 `target_link_libraries` 管理依赖
- 通过 `find_package` 或 FetchContent 集成外部包
- 启用 sanitizers、设置 toolchain 文件
- 导出 CMake 包

## What It Does

提供 CMake 构建系统的最佳实践和工作流：
- 标准 CMake 配置模式
- 依赖管理策略
- 跨平台配置
- 常见错误排查

## How It Works

```
需求分析 → CMake 配置设计 → 编写 CMakeLists.txt → 验证构建 → 调试（如有）
```

## Core Patterns

### 推荐模式
```cmake
# ✅ 使用 target_include_directories（禁止全局 include）
target_include_directories(my_target PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ✅ 使用 Generator Expression 处理平台差异
target_compile_options(my_target PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall>
    $<$<CXX_COMPILER_ID:Clang>:-Weverything>
)

# ✅ find_package 优先于 FetchContent（更稳定）
find_package(Threads REQUIRED)
target_link_libraries(my_target PRIVATE Threads::Threads)
```

### 反模式（Blocked）
```cmake
# ❌ 禁止 include_directories()（全局污染）
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# ❌ 禁止 link_directories()（隐式依赖）
link_directories(${CMAKE_BINARY_DIR}/lib)
```

## AgenticDSL Example

**对应文件**: `../../agenticdsl/axis2_domain/cmake_workflow.agent.md`

该文件展示了如何用 AgenticDSL 实现 CMake 工作流，包含：
- `tool_call` — 执行 cmake 命令
- `state` — 维护 CMake 配置上下文
- `dsl_call` — 生成 CMakeLists.txt 内容
- `assert` — 验证构建结果

## Ideal DSL Extension

**参考**: `../../ideal_dsl/04_domain_skill.md`

领域/工具类技能的理想 DSL 扩展提案：
- `type: tool_template` — 预定义工具调用模板
- `type: config_generate` — 配置生成器节点
- `build_system` 节点 — 抽象构建系统操作
