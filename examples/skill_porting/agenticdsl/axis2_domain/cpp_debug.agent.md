### AgenticDSL '/taxonomy/axis2_domain/cpp_debug'

# C++ Debug — AgenticDSL 实现

> **轴分类**: 轴2-领域/工具
> **核心 DSL 特性**: tool_call, dsl_call, fork, state, assert
> **对应 Superpowers 技能**: cpp_debug

## /__meta__
execution_budget:
  max_llm_calls: 20
  max_tool_calls: 50
  max_total_nodes: 45

---

## /cppdbg/start
type: start
next: ["/cppdbg/classify_problem"]

## /cppdbg/classify_problem
type: user_input
prompt: |
  描述 C++ 运行时问题：

  例如：
  - "程序在处理大文件时崩溃，输出 Segmentation fault"
  - "程序卡死，没有输出"
  - "内存不断增长，最终 OOM"
  - "输出结果不正确，逻辑错误"
input_variable: problem_desc
input_type: multiline
next: ["/cppdbg/detect_problem_type"]

## /cppdbg/detect_problem_type
type: dsl_call
llm_tool: gpt-4
output_keys: ["problem_type"]
prompt: |
  问题描述: {{problem_desc}}

  将问题分类到以下类型之一：
  - crash: 崩溃/段错误
  - deadlock: 死锁/卡死
  - memory_leak: 内存泄漏
  - out_of_bounds: 越界访问
  - dangling_ptr: 悬挂指针
  - logic_error: 逻辑错误
  - undefined_behavior: 未定义行为

  输出:
  type: <类型>
  severity: critical/high/medium
  likely_tools: ["建议的诊断工具"]
next: ["/cppdbg/prepare_binary"]

## /cppdbg/prepare_binary
type: tool_call
tool_name: bash
arguments:
  command: "ls -la src/*.cpp CMakeLists.txt 2>/dev/null || find . -name '*.cpp' -o -name 'CMakeLists.txt' | head -20"
  timeout: "10"
output_keys: ["project_structure"]
next: ["/cppdbg/build_debug"]

## /cppdbg/build_debug
type: tool_call
tool_name: bash
arguments:
  command: "mkdir -p build-debug && cd build-debug && cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-g -O0' 2>&1 && make -j$(nproc) 2>&1"
  timeout: "180"
output_keys: ["debug_build"]
next: ["/cppdbg/route_by_type"]

## /cppdbg/route_by_type
type: switch
input: "{{problem_type.type}}"
cases:
  crash: "/cppdbg/diagnose_crash"
  deadlock: "/cppdbg/diagnose_deadlock"
  memory_leak: "/cppdbg/diagnose_memory"
  out_of_bounds: "/cppdbg/diagnose_bounds"
  dangling_ptr: "/cppdbg/diagnose_ptr"
  logic_error: "/cppdbg/diagnose_logic"
  undefined_behavior: "/cppdbg/diagnose_undefined"
default: "/cppdbg/manual_diagnosis"

## /cppdbg/diagnose_crash
type: fork
branches:
  - "/cppdbg/run_gdb"
  - "/cppdbg/run_asan"
context_isolation: deep_copy
next: ["/cppdbg/merge_crash_results"]

## /cppdbg/run_gdb
type: tool_call
tool_name: bash
arguments:
  command: "cd build-debug && gdb -batch -ex 'run' -ex 'bt' -ex 'info registers' ./your_binary 2>&1 || echo 'GDB run failed'"
  timeout: "60"
output_keys: ["gdb_output"]
next: ["/cppdbg/end_gdb"]

## /cppdbg/end_gdb
type: end

## /cppdbg/run_asan
type: tool_call
tool_name: bash
arguments:
  command: "cd build-debug && cmake . -DCMAKE_CXX_FLAGS='-g -O0 -fsanitize=address -fno-omit-frame-pointer' 2>&1 && make clean && make 2>&1 && ASAN_OPTIONS='detect_leaks=1' ./your_binary 2>&1 | head -100"
  timeout: "180"
output_keys: ["asan_output"]
next: ["/cppdbg/end_asan"]

## /cppdbg/end_asan
type: end

## /cppdbg/merge_crash_results
type: dsl_call
llm_tool: gpt-4
output_keys: ["crash_analysis"]
prompt: |
  GDB 输出:
  {{gdb_output}}

  ASAN 输出:
  {{asan_output}}

  分析崩溃根因，输出：
  - likely_location: "最可能的崩溃位置"
  - root_cause: "根因"
  - fix_suggestion: "修复建议"
next: ["/cppdbg/apply_fix"]

## /cppdbg/diagnose_deadlock
type: tool_call
tool_name: bash
arguments:
  command: "cd build-debug && gdb -batch -ex 'thread apply all bt' -ex 'info threads' ./your_binary 2>&1"
  timeout: "60"
output_keys: ["thread_output"]
next: ["/cppdbg/analyze_deadlock"]

## /cppdbg/analyze_deadlock
type: dsl_call
llm_tool: gpt-4
output_keys: ["deadlock_analysis"]
prompt: |
  线程状态:
  {{thread_output}}

  分析可能的死锁原因。
  输出:
  likely_locks: ["涉及的锁"]
  root_cause: "根因"
  fix_suggestion: "修复建议"
next: ["/cppdbg/apply_fix"]

## /cppdbg/diagnose_memory
type: tool_call
tool_name: bash
arguments:
  command: "cd build-debug && valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./your_binary 2>&1 | head -150"
  timeout: "300"
output_keys: ["valgrind_output"]
next: ["/cppdbg/analyze_memory"]

## /cppdbg/analyze_memory
type: dsl_call
llm_tool: gpt-4
output_keys: ["memory_analysis"]
prompt: |
  Valgrind 输出:
  {{valgrind_output}}

  分析内存泄漏。
  输出:
  leak_summary: "泄漏摘要"
  leak_location: "泄漏位置"
  fix_suggestion: "修复建议"
next: ["/cppdbg/apply_fix"]

## /cppdbg/diagnose_bounds
type: tool_call
tool_name: bash
arguments:
  command: "cd build-debug && cmake . -DCMAKE_CXX_FLAGS='-g -O0 -fsanitize=address -fno-omit-frame-pointer' 2>&1 && make clean && make 2>&1 && ./your_binary 2>&1 | head -100"
  timeout: "180"
output_keys: ["bounds_output"]
next: ["/cppdbg/analyze_bounds"]

## /cppdbg/analyze_bounds
type: dsl_call
llm_tool: gpt-4
output_keys: ["bounds_analysis"]
prompt: |
  越界检测输出:
  {{bounds_output}}

  分析越界访问位置和原因。
  输出:
  violation_location: "违规位置"
  root_cause: "根因"
  fix_suggestion: "修复建议"
next: ["/cppdbg/apply_fix"]

## /cppdbg/diagnose_ptr
type: tool_call
tool_name: bash
arguments:
  command: "cd build-debug && cmake . -DCMAKE_CXX_FLAGS='-g -O0 -fsanitize=address' 2>&1 && make && ./your_binary 2>&1 | head -100"
  timeout: "180"
output_keys: ["ptr_output"]
next: ["/cppdbg/analyze_ptr"]

## /cppdbg/analyze_ptr
type: dsl_call
llm_tool: gpt-4
output_keys: ["ptr_analysis"]
prompt: |
  指针诊断输出:
  {{ptr_output}}

  分析悬挂指针问题。
  输出:
  issue_location: "问题位置"
  root_cause: "根因"
  fix_suggestion: "修复建议"
next: ["/cppdbg/apply_fix"]

## /cppdbg/diagnose_logic
type: user_input
prompt: |
  请提供：
  1. 预期输出
  2. 实际输出
  3. 相关代码文件路径
input_variable: logic_details
input_type: multiline
next: ["/cppdbg/analyze_logic"]

## /cppdbg/analyze_logic
type: dsl_call
llm_tool: gpt-4
output_keys: ["logic_analysis"]
prompt: |
  逻辑错误详情:
  {{logic_details}}

  分析逻辑错误。
  输出:
  bug_location: "bug 位置"
  root_cause: "根因"
  fix_suggestion: "修复建议"
next: ["/cppdbg/apply_fix"]

## /cppdbg/diagnose_undefined
type: tool_call
tool_name: bash
arguments:
  command: "cd build-debug && cmake . -DCMAKE_CXX_FLAGS='-g -O0 -fsanitize=undefined' 2>&1 && make && ./your_binary 2>&1 | head -100"
  timeout: "180"
output_keys: ["ubsan_output"]
next: ["/cppdbg/analyze_undefined"]

## /cppdbg/analyze_undefined
type: dsl_call
llm_tool: gpt-4
output_keys: ["ub_analysis"]
prompt: |
  UBsan 输出:
  {{ubsan_output}}

  分析未定义行为。
  输出:
  ub_location: "UB 位置"
  root_cause: "根因"
  fix_suggestion: "修复建议"
next: ["/cppdbg/apply_fix"]

## /cppdbg/manual_diagnosis
type: dsl_call
llm_tool: gpt-4
output_keys: ["manual_analysis"]
prompt: |
  问题描述: {{problem_desc}}

  提供手动诊断建议和命令。
next: ["/cppdbg/end"]

## /cppdbg/apply_fix
type: dsl_call
llm_tool: gpt-4
output_keys: ["final_fix"]
prompt: |
  问题类型: {{problem_type.type}}
  分析结果:
  {{crash_analysis|default:deadlock_analysis|default:memory_analysis|default:bounds_analysis|default:ptr_analysis|default:logic_analysis|default:ub_analysis|default:manual_analysis}}

  生成最终的最小化修复代码。
next: ["/cppdbg/verify_fix"]

## /cppdbg/verify_fix
type: tool_call
tool_name: bash
arguments:
  command: "cd build-debug && make 2>&1 && echo 'Build successful'"
  timeout: "120"
output_keys: ["verify_result"]
next: ["/cppdbg/end"]

## /cppdbg/end
type: end

---

## /ideal_extension
type: comment
comment: |
  ## 理想 DSL 扩展：cpp_debug 技能

  ### 1. debugger_session 节点
  # 托管 gdb/lldb 会话
  type: debugger_session
  debugger: gdb
  binary: "{{binary}}"
  commands:
    - "run {{args}}"
    - "bt"
    - "info registers"
    - "frame {{frame}}"
  timeout: 60
  output_format: structured
  on_crash:
    dump_core: true
    analyze: true

  ### 2. memory_analysis 节点
  type: memory_analysis
  tool: valgrind
  options:
    leak_check: full
    show_leak_kinds: all
    track_origins: yes
  input_binary: "{{binary}}"
  output:
    leak_count: integer
    leak_bytes: integer
    leak_locations: array

  ### 3. stack_trace 节点
  # 结构化堆栈跟踪解析
  type: stack_trace
  source: "{{gdb_output}}"
  parse_format: gdb
  resolve_symbols: true
  output:
    crash_frame: integer
    function_names: array
    file_locations: array

  ### 4. skill_invoke（调用调试技能）
  type: skill_invoke
  skill: "cpp_debug"
  input:
    symptom: "{{problem_desc}}"
    problem_type: "{{problem_type.type}}"
    binary: "{{binary}}"
  output:
    root_cause: root_cause
    fix: final_fix
