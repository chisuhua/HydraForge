### AgenticDSL '/taxonomy/axis2_domain/cmake_workflow'

# CMake Workflow — AgenticDSL 实现

> **轴分类**: 轴2-领域/工具
> **核心 DSL 特性**: tool_call, state, dsl_call, assert
> **对应 Superpowers 技能**: cmake_workflow

## /__meta__
execution_budget:
  max_llm_calls: 15
  max_tool_calls: 30
  max_total_nodes: 35

---

## /cmake/start
type: start
next: ["/cmake/detect_cmake_usage"]

## /cmake/detect_cmake_usage
type: user_input
prompt: |
  描述您需要完成的 CMake 任务：

  例如：
  - "添加一个新的子项目到现有 CMake 构建"
  - "修复 link error: cannot find -lfoo"
  - "配置交叉编译 toolchain"
  - "启用 Address Sanitizer"
input_variable: cmake_task
input_type: multiline
next: ["/cmake/analyze_task"]

## /cmake/analyze_task
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_analysis"]
prompt: |
  CMake 任务: {{cmake_task}}

  分析任务类型：
  - new_target: 新建目标（library/executable）
  - dependency: 添加依赖（find_package/external）
  - configuration: 配置问题（generator/platform）
  - cross_compile: 交叉编译
  - sanitizer: 调试工具启用

  输出:
  type: <类型>
  involves_external: true/false
  target_type: library/executable/header-only
  key_challenges: ["挑战列表"]
next: ["/cmake/check_existing"]

## /cmake/check_existing
type: tool_call
tool_name: glob
arguments:
  pattern: "**/CMakeLists.txt"
output_keys: ["cmake_files"]
next: ["/cmake/scan_existing"]

## /cmake/scan_existing
type: tool_call
tool_name: read_file
arguments:
  path: "CMakeLists.txt"
output_keys: ["root_cmake"]
next: ["/cmake/design_cmake"]

## /cmake/design_cmake
type: dsl_call
llm_tool: gpt-4
output_keys: ["cmake设计方案"]
prompt: |
  现有 CMake 结构:
  {{root_cmake}}
  发现的 CMake 文件: {{cmake_files}}

  任务: {{cmake_task}}
  分析: {{task_analysis}}

  生成 CMakeLists.txt 修改方案或新文件内容。
  遵循最佳实践：
  - 使用 target_* 命令而非全局命令
  - 使用 Generator Expression 处理平台差异
  - find_package 优先于 FetchContent
next: ["/cmake/apply_cmake"]

## /cmake/apply_cmake
type: tool_call
tool_name: write_file
arguments:
  path: "{{task_analysis.output_file|default:'CMakeLists.txt'}}"
  content: "{{cmake设计方案}}"
output_keys: ["cmake_written"]
next: ["/cmake/test_configure"]

## /cmake/test_configure
type: tool_call
tool_name: bash
arguments:
  command: "mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug 2>&1"
  timeout: "120"
output_keys: ["configure_output"]
next: ["/cmake/check_configure"]

## /cmake/check_configure
type: dsl_call
llm_tool: gpt-4
output_keys: ["configure_verdict"]
prompt: |
  CMake 配置输出:
  {{configure_output}}

  判断配置是否成功。
  如果失败，分析错误原因并给出修复建议。

  输出:
  success: true/false
  error_if_any: "错误描述"
  fix_suggestion: "修复建议"
next: ["/cmake/route_configure"]

## /cmake/route_configure
type: assert
condition: "{{configure_verdict.success}} == true"
on_failure: "/cmake/fix_cmake"
next: ["/cmake/test_build"]

## /cmake/fix_cmake
type: dsl_call
llm_tool: gpt-4
output_keys: ["fixed_cmake"]
prompt: |
  CMake 配置错误:
  {{configure_verdict.error_if_any}}
  修复建议: {{configure_verdict.fix_suggestion}}
  之前的设计: {{cmake设计方案}}

  生成修复后的 CMakeLists.txt。
next: ["/cmake/apply_fixed"]

## /cmake/apply_fixed
type: tool_call
tool_name: write_file
arguments:
  path: "{{task_analysis.output_file|default:'CMakeLists.txt'}}"
  content: "{{fixed_cmake}}"
output_keys: []
next: ["/cmake/test_configure"]

## /cmake/test_build
type: tool_call
tool_name: bash
arguments:
  command: "cd build && make -j$(nproc) 2>&1"
  timeout: "180"
output_keys: ["build_output"]
next: ["/cmake/judge_build"]

## /cmake/judge_build
type: dsl_call
llm_tool: gpt-4
output_keys: ["build_verdict"]
prompt: |
  构建输出:
  {{build_output}}

  判断构建是否成功。
  输出:
  success: true/false
  summary: "总结"
next: ["/cmake/route_build"]

## /cmake/route_build
type: assert
condition: "{{build_verdict.success}} == true"
on_failure: "/cmake/fix_build_error"
next: ["/cmake/summarize"]

## /cmake/fix_build_error
type: dsl_call
llm_tool: gpt-4
output_keys: ["fix_for_build"]
prompt: |
  构建错误:
  {{build_output}}

  生成修复方案。
next: ["/cmake/apply_build_fix"]

## /cmake/apply_build_fix
type: tool_call
tool_name: bash
arguments:
  command: "cd build && make 2>&1 | head -50"
  timeout: "60"
output_keys: ["retry_build"]
next: ["/cmake/judge_build"]

## /cmake/summarize
type: dsl_call
llm_tool: gpt-4
output_keys: ["summary"]
prompt: |
  CMake 任务: {{cmake_task}}
  最终状态: 成功

  生成总结报告。
next: ["/cmake/end"]

## /cmake/end
type: end

---

## /ideal_extension
type: comment
comment: |
  ## 理想 DSL 扩展：cmake_workflow 技能

  ### 1. build_system 节点（抽象构建系统）
  type: build_system
  command: cmake
  operations:
    - configure:
        source_dir: "{{source_dir}}"
        build_dir: "{{build_dir}}"
        options:
          CMAKE_BUILD_TYPE: Debug
          CMAKE_EXPORT_COMPILE_COMMANDS: ON
    - build:
        targets: ["all"]
        jobs: 8
    - test:
        arguments: "--output-on-failure"
  on_error:
    diagnose: true
    retry: 2

  ### 2. tool_template（工具调用模板）
  # 预定义 CMake 工具调用模式
  type: tool_template
  name: cmake_configure
  base_command: "cmake {{source}} -B {{build}}"
  templates:
    - pattern: "-DCMAKE_BUILD_TYPE={type}"
      choices: [Debug, Release, RelWithDebInfo, MinSizeRel]
    - pattern: "-DCMAKE_TOOLCHAIN_FILE={file}"
      required: false

  ### 3. config_generate（配置生成器）
  # 根据输入规格生成完整 CMake 配置
  type: config_generate
  template: cmake_library
  input:
    name: "mylib"
    sources: ["src/*.cpp"]
    dependencies: [Threads, Boost::filesystem]
    options:
      SANITIZE: address
  output: CMakeLists.txt_content
