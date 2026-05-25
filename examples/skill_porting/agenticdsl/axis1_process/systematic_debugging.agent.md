### AgenticDSL '/taxonomy/axis1_process/systematic_debugging'

# Systematic Debugging — AgenticDSL 实现

> **轴分类**: 轴1-流程/方法论
> **核心 DSL 特性**: state, dsl_call, tool_call, assert, loop
> **对应 Superpowers 技能**: systematic_debugging

## /__meta__
execution_budget:
  max_llm_calls: 25
  max_tool_calls: 40
  max_total_nodes: 50

---

## /debug/start
type: start
next: ["/debug/collect_symptoms"]

## /debug/collect_symptoms
type: user_input
prompt: |
  描述遇到的问题：

  请提供：
  1. 预期行为 vs 实际行为
  2. 错误信息（如有）
  3. 复现步骤
  4. 发生时间/频率
  5. 最近的相关变更
input_variable: symptom_report
input_type: multiline
next: ["/debug/classify_problem"]

## /debug/classify_problem
type: dsl_call
llm_tool: gpt-4
output_keys: ["problem_class"]
prompt: |
  问题报告: {{symptom_report}}

  将问题归类到以下类型之一：
  - crash: 崩溃/段错误/SIGSEGV
  - logic: 逻辑错误（结果不对）
  - memory: 内存泄漏/越界
  - deadlock: 死锁/活锁
  - performance: 性能问题
  - race: 竞态条件
  - other: 其他

  输出:
  type: <类型>
  severity: critical/high/medium/low
  initial_hypothesis: "初步猜测可能的原因"
next: ["/debug/init_debug_context"]

## /debug/init_debug_context
type: state
operation: write
state_key: "debug.{{session_id}}.context"
value_template: |
  {
    "symptom": {{symptom_report}},
    "problem_type": {{problem_class.type}},
    "severity": {{problem_class.severity}},
    "hypothesis": [],
    "verified_hypothesis": null,
    "fix_attempts": [],
    "phase": "investigation"
  }
next: ["/debug/gather_environment"]

## /debug/gather_environment
type: tool_call
tool_name: bash
arguments:
  command: "uname -a && gcc --version 2>/dev/null || clang --version 2>/dev/null || echo 'No compiler found'"
  timeout: "10"
output_keys: ["env_info"]
next: ["/debug/select_diagnostic"]

## /debug/select_diagnostic
type: dsl_call
llm_tool: gpt-4
output_keys: ["diagnostic_plan"]
prompt: |
  问题类型: {{problem_class.type}}
  环境信息: {{env_info}}

  根据问题类型，选择最合适的诊断工具和策略：

  - crash: gdb (core dump), address sanitizer
  - logic: 追加日志, 缩小输入范围
  - memory: valgrind, address sanitizer
  - deadlock: strace, gdb (thread apply all bt)
  - performance: perf, gprof
  - race: helgrind, tsan

  输出:
  tools: ["工具列表"]
  strategy: "诊断策略描述"
  commands: ["需要执行的命令"]
next: ["/debug/run_diagnostics"]

## /debug/run_diagnostics
type: fork
branches:
  - "/debug/run_tool_1"
  - "/debug/run_tool_2"
context_isolation: deep_copy
next: ["/debug/merge_diagnostics"]

## /debug/run_tool_1
type: tool_call
tool_name: bash
arguments:
  command: "{{diagnostic_plan.commands[0]}}"
  timeout: "120"
output_keys: ["diag_result_1"]
next: ["/debug/end_branch_1"]

## /debug/end_branch_1
type: end

## /debug/run_tool_2
type: tool_call
tool_name: bash
arguments:
  command: "{{diagnostic_plan.commands[1]}}"
  timeout: "120"
output_keys: ["diag_result_2"]
next: ["/debug/end_branch_2"]

## /debug/end_branch_2
type: end

## /debug/merge_diagnostics
type: dsl_call
llm_tool: gpt-4
output_keys: ["merged_analysis"]
prompt: |
  诊断结果 1: {{diag_result_1}}
  诊断结果 2: {{diag_result_2}}

  合并分析，输出：
  1. 发现的线索
  2. 可能的根因
  3. 下一步验证建议
next: ["/debug/form_hypothesis"]

## /debug/form_hypothesis
type: dsl_call
llm_tool: gpt-4
output_keys: ["hypothesis"]
prompt: |
  诊断分析: {{merged_analysis}}

  形成一个具体的、可验证的假设：
  - 假设内容
  - 验证方法
  - 如果假设成立，预期的修复方案

  输出:
  hypothesis: "假设描述"
  verification: "验证方法"
  expected_fix: "预期修复"
next: ["/debug/verify_hypothesis"]

## /debug/verify_hypothesis
type: tool_call
tool_name: bash
arguments:
  command: "{{hypothesis.verification}}"
  timeout: "60"
output_keys: ["verification_result"]
next: ["/debug/judge_hypothesis"]

## /debug/judge_hypothesis
type: dsl_call
llm_tool: gpt-4
output_keys: ["verdict"]
prompt: |
  假设: {{hypothesis.hypothesis}}
  验证命令输出: {{verification_result}}

  判断假设是否成立。
  输出:
  confirmed: true/false
  reasoning: "判断理由"
next: ["/debug/route_verdict"]

## /debug/route_verdict
type: assert
condition: "{{verdict.confirmed}} == true"
on_failure: "/debug/refine_hypothesis"
next: ["/debug/implement_fix"]

## /debug/refine_hypothesis
type: dsl_call
llm_tool: gpt-4
output_keys: ["refined_hypothesis"]
prompt: |
  原假设未确认: {{hypothesis.hypothesis}}
  验证结果: {{verification_result}}
  之前的诊断分析: {{merged_analysis}}

  形成新的假设或调整验证策略。
next: ["/debug/update_context"]

## /debug/update_context
type: state
operation: merge
state_key: "debug.{{session_id}}.context"
value_template: |
  {
    "hypothesis": {{hypothesis|append:refined_hypothesis}},
    "phase": "investigation"
  }
next: ["/debug/verify_hypothesis"]

## /debug/implement_fix
type: dsl_call
llm_tool: gpt-4
output_keys: ["fix_proposal"]
prompt: |
  确认的假设: {{hypothesis.hypothesis}}

  基于确认的假设，提出最小化修复方案：
  - 只改问题本身，不改周围代码
  - 解释为什么这个修复能解决问题
next: ["/debug/apply_fix"]

## /debug/apply_fix
type: tool_call
tool_name: bash
arguments:
  command: "echo 'FIX: {{fix_proposal}}' > /tmp/fix_proposal.txt && cat /tmp/fix_proposal.txt"
  timeout: "10"
output_keys: ["fix_applied"]
next: ["/debug/verify_fix"]

## /debug/verify_fix
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1 || echo 'Build/test failed'"
  timeout: "120"
output_keys: ["test_result"]
next: ["/debug/judge_fix"]

## /debug/judge_fix
type: dsl_call
llm_tool: gpt-4
output_keys: ["fix_verdict"]
prompt: |
  修复应用结果: {{fix_applied}}
  测试结果: {{test_result}}

  判断修复是否成功（问题不再复现，测试通过）。
  输出:
  success: true/false
  summary: "总结"
next: ["/debug/route_fix_result"]

## /debug/route_fix_result
type: assert
condition: "{{fix_verdict.success}} == true"
on_failure: "/debug/revert_and_iterate"
next: ["/debug/document_fix"]

## /debug/revert_and_iterate
type: tool_call
tool_name: bash
arguments:
  command: "git checkout -- . && echo 'Reverted'"
  timeout: "10"
output_keys: ["reverted"]
next: ["/debug/update_context"]

## /debug/document_fix
type: tool_call
tool_name: write_file
arguments:
  path: "docs/debug/{{timestamp}}-fix.md"
  content: |
    # 调试报告

    ## 问题
    {{symptom_report}}

    ## 根因
    {{hypothesis.hypothesis}}

    ## 修复
    {{fix_proposal}}

    ## 验证
    {{test_result}}

    ## 教训
    {{fix_verdict.summary}}
output_keys: ["fix_doc_path"]
next: ["/debug/end"]

## /debug/end
type: end

---

## /ideal_extension
type: comment
comment: |
  ## 理想 DSL 扩展：systematic_debugging 技能

  ### 1. hypothesis 节点类型
  # 结构化假设验证
  type: hypothesis
  description: "{{hypothesis.description}}"
  verify_with:
    tool: "gdb"
    command: "gdb -batch -ex run -ex bt {{binary}}"
  expected: "Should show SIGSEGV in foo()"
  on_confirm: "/fix/implement"
  on_reject: "/debug/refine"

  ### 2. diagnostic_tool 内置节点
  # 调试工具集成
  type: diagnostic_tool
  tool: gdb
  args:
    binary: "{{binary}}"
    core: "{{core_file}}"
  output_format: structured
  timeout: 60

  ### 3. 循环节点（Loop）
  # investigation 阶段循环直到假设被确认或穷尽
  type: loop
  max_iterations: 10
  condition: "hypothesis.confirmed == false && attempts < max"
  body:
    - /debug/refine_hypothesis
    - /debug/verify_hypothesis
  until: "/debug/implement_fix"

  ### 4. skill_invoke 调试技能
  type: skill_invoke
  skill: "cpp_debug"
  input:
    symptom: "{{symptom_report}}"
    problem_type: "{{problem_class.type}}"
  output:
    root_cause: root_cause
    fix: fix_proposal
