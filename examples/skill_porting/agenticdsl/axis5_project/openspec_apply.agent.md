### AgenticDSL '/taxonomy/axis5_project/openspec_apply'

# OpenSpec Apply — AgenticDSL 实现

> **轴分类**: 轴5-项目专用（仅限 HydraForge/AgenticDSL）
> **核心 DSL 特性**: state, fork, dsl_call, tool_call, assert
> **对应 Superpowers 技能**: openspec-apply-change

## /__meta__
execution_budget:
  max_llm_calls: 50
  max_tool_calls: 100
  max_total_nodes: 100

---

## /apply/start
type: start
next: ["/apply/load_change"]

## /apply/load_change
type: user_input
prompt: |
  请提供要实现的 OpenSpec change ID 或路径：

  例如：
  - "openspec/changes/abc123"
  - "abc123"
input_variable: change_id
input_type: text
next: ["/apply/verify_change"]

## /apply/verify_change
type: tool_call
tool_name: bash
arguments:
  command: "ls openspec/changes/{{change_id}}/CHANGE.yaml 2>/dev/null && cat openspec/changes/{{change_id}}/CHANGE.yaml || echo 'NOT_FOUND'"
  timeout: "10"
output_keys: ["change_manifest"]
next: ["/apply/check_approval"]

## /apply/check_approval
type: dsl_call
llm_tool: gpt-4
output_keys: ["approval_status"]
prompt: |
  Change manifest:
  {{change_manifest}}

  检查 change 是否已批准。
  输出:
  approved: true/false
  reason: "原因"
next: ["/apply/route_approval"]

## /apply/route_approval
type: assert
condition: "{{approval_status.approved}} == true"
on_failure: "/apply/require_approval"
next: ["/apply/load_tasks"]

## /apply/require_approval
type: dsl_call
llm_tool: gpt-4
output_keys: ["error_message"]
prompt: |
  Change 未批准: {{change_manifest}}

  生成错误消息，指导用户如何获得批准。
next: ["/apply/end_error"]

## /apply/end_error
type: end

## /apply/load_tasks
type: tool_call
tool_name: read_file
arguments:
  path: "openspec/changes/{{change_id}}/tasks/overview.md"
output_keys: ["tasks_overview"]
next: ["/apply/parse_tasks"]

## /apply/parse_tasks
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_list"]
prompt: |
  任务概述:
  {{tasks_overview}}

  解析任务列表，提取：
  - 任务 ID
  - 任务名称
  - 依赖关系
  - 验收标准

  输出 JSON 数组格式。
next: ["/apply/build_dependency"]

## /apply/build_dependency
type: dsl_call
llm_tool: gpt-4
output_keys: ["execution_plan"]
prompt: |
  任务列表:
  {{task_list}}

  分析依赖关系，构建执行计划：
  1. 哪些任务可以并行
  2. 哪些任务必须串行
  3. 拓扑排序结果

  输出执行计划。
next: ["/apply/init_apply_context"]

## /apply/init_apply_context
type: state
operation: write
state_key: "openspec.apply.{{session_id}}"
value_template: |
  {
    "change_id": {{change_id}},
    "tasks": {{task_list}},
    "execution_plan": {{execution_plan}},
    "status": "in_progress",
    "completed_tasks": [],
    "failed_tasks": []
  }
next: ["/apply/execute_parallel_tasks"]

## /apply/execute_parallel_tasks
type: fork
branches:
  - "/apply/execute_task_1"
  - "/apply/execute_task_2"
  - "/apply/execute_task_3"
context_isolation: deep_copy
next: ["/apply/merge_results"]

## /apply/execute_task_1
type: tool_call
tool_name: read_file
arguments:
  path: "openspec/changes/{{change_id}}/tasks/task-001.md"
output_keys: ["task_1_details"]
next: ["/apply/work_task_1"]

## /apply/work_task_1
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_1_implementation"]
prompt: |
  任务详情:
  {{task_1_details}}

  执行任务：
  1. 实现所需功能
  2. 写测试
  3. 更新文档（如需要）

  输出实现的代码变更说明。
next: ["/apply/verify_task_1"]

## /apply/verify_task_1
type: tool_call
tool_name: bash
arguments:
  command: "make build 2>&1 | tail -20"
  timeout: "120"
output_keys: ["task_1_build"]
next: ["/apply/run_task_1_tests"]

## /apply/run_task_1_tests
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1 | tail -30"
  timeout: "180"
output_keys: ["task_1_tests"]
next: ["/apply/judge_task_1"]

## /apply/judge_task_1
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_1_verdict"]
prompt: |
  任务 1 构建结果:
  {{task_1_build}}

  测试结果:
  {{task_1_tests}}

  判断任务是否成功完成。
  输出:
  success: true/false
  issues: ["问题列表"]
next: ["/apply/route_task_1"]

## /apply/route_task_1
type: assert
condition: "{{task_1_verdict.success}} == true"
on_failure: "/apply/fix_task_1"
next: ["/apply/mark_task_1_done"]

## /apply/fix_task_1
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_1_fix"]
prompt: |
  任务 1 问题:
  {{task_1_verdict.issues}}

  生成修复方案。
next: ["/apply/apply_task_1_fix"]

## /apply/apply_task_1_fix
type: tool_call
tool_name: bash
arguments:
  command: "echo 'Applying fix: {{task_1_fix}}'"
  timeout: "10"
output_keys: ["fix_applied"]
next: ["/apply/verify_task_1"]

## /apply/mark_task_1_done
type: state
operation: merge
state_key: "openspec.apply.{{session_id}}"
value_template: |
  {
    "completed_tasks": {{completed_tasks|append:"task-001"}}
  }
next: ["/apply/end_task_1"]

## /apply/end_task_1
type: end

## /apply/execute_task_2
type: tool_call
tool_name: read_file
arguments:
  path: "openspec/changes/{{change_id}}/tasks/task-002.md"
output_keys: ["task_2_details"]
next: ["/apply/work_task_2"]

## /apply/work_task_2
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_2_implementation"]
prompt: |
  任务详情:
  {{task_2_details}}

  执行任务。
next: ["/apply/verify_task_2"]

## /apply/verify_task_2
type: tool_call
tool_name: bash
arguments:
  command: "make build 2>&1 | tail -20"
  timeout: "120"
output_keys: ["task_2_build"]
next: ["/apply/run_task_2_tests"]

## /apply/run_task_2_tests
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1 | tail -30"
  timeout: "180"
output_keys: ["task_2_tests"]
next: ["/apply/judge_task_2"]

## /apply/judge_task_2
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_2_verdict"]
prompt: |
  任务 2 构建结果:
  {{task_2_build}}

  测试结果:
  {{task_2_tests}}

  判断任务是否成功完成。
  输出:
  success: true/false
  issues: ["问题列表"]
next: ["/apply/route_task_2"]

## /apply/route_task_2
type: assert
condition: "{{task_2_verdict.success}} == true"
on_failure: "/apply/fix_task_2"
next: ["/apply/mark_task_2_done"]

## /apply/fix_task_2
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_2_fix"]
prompt: |
  任务 2 问题:
  {{task_2_verdict.issues}}

  生成修复方案。
next: ["/apply/apply_task_2_fix"]

## /apply/apply_task_2_fix
type: tool_call
tool_name: bash
arguments:
  command: "echo 'Applying fix: {{task_2_fix}}'"
  timeout: "10"
output_keys: []
next: ["/apply/verify_task_2"]

## /apply/mark_task_2_done
type: state
operation: merge
state_key: "openspec.apply.{{session_id}}"
value_template: |
  {
    "completed_tasks": {{completed_tasks|append:"task-002"}}
  }
next: ["/apply/end_task_2"]

## /apply/end_task_2
type: end

## /apply/execute_task_3
type: tool_call
tool_name: read_file
arguments:
  path: "openspec/changes/{{change_id}}/tasks/task-003.md"
output_keys: ["task_3_details"]
next: ["/apply/work_task_3"]

## /apply/work_task_3
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_3_implementation"]
prompt: |
  任务详情:
  {{task_3_details}}

  执行任务。
next: ["/apply/verify_task_3"]

## /apply/verify_task_3
type: tool_call
tool_name: bash
arguments:
  command: "make build 2>&1 | tail -20"
  timeout: "120"
output_keys: ["task_3_build"]
next: ["/apply/run_task_3_tests"]

## /apply/run_task_3_tests
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1 | tail -30"
  timeout: "180"
output_keys: ["task_3_tests"]
next: ["/apply/judge_task_3"]

## /apply/judge_task_3
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_3_verdict"]
prompt: |
  任务 3 构建结果:
  {{task_3_build}}

  测试结果:
  {{task_3_tests}}

  判断任务是否成功完成。
  输出:
  success: true/false
  issues: ["问题列表"]
next: ["/apply/route_task_3"]

## /apply/route_task_3
type: assert
condition: "{{task_3_verdict.success}} == true"
on_failure: "/apply/fix_task_3"
next: ["/apply/mark_task_3_done"]

## /apply/fix_task_3
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_3_fix"]
prompt: |
  任务 3 问题:
  {{task_3_verdict.issues}}

  生成修复方案。
next: ["/apply/apply_task_3_fix"]

## /apply/apply_task_3_fix
type: tool_call
tool_name: bash
arguments:
  command: "echo 'Applying fix: {{task_3_fix}}'"
  timeout: "10"
output_keys: []
next: ["/apply/verify_task_3"]

## /apply/mark_task_3_done
type: state
operation: merge
state_key: "openspec.apply.{{session_id}}"
value_template: |
  {
    "completed_tasks": {{completed_tasks|append:"task-003"}}
  }
next: ["/apply/end_task_3"]

## /apply/end_task_3
type: end

## /apply/merge_results
type: dsl_call
llm_tool: gpt-4
output_keys: ["merge_summary"]
prompt: |
  任务执行结果:
  - Task 1: {{task_1_verdict}}
  - Task 2: {{task_2_verdict}}
  - Task 3: {{task_3_verdict}}

  汇总执行结果，判断整体是否成功。
next: ["/apply/route_merge"]

## /apply/route_merge
type: assert
condition: "{{merge_summary.success}} == true"
on_failure: "/apply/handle_failures"
next: ["/apply/final_verification"]

## /apply/handle_failures
type: state
operation: merge
state_key: "openspec.apply.{{session_id}}"
value_template: |
  {
    "status": "failed",
    "failed_tasks": {{failed_tasks}}
  }
next: ["/apply/present_failure"]

## /apply/present_failure
type: user_input
prompt: |
  Change 实现失败。

  失败的任务:
  {{merge_summary.failed_tasks}}

  请选择：
  - 修复问题后重试
  - 放弃这个 change
input_variable: failure_decision
input_type: text
next: ["/apply/route_failure"]

## /apply/route_failure
type: switch
input: "{{failure_decision}}"
cases:
  retry: "/apply/execute_parallel_tasks"
  abort: "/apply/end_aborted"
default: "/apply/present_failure"]

## /apply/end_aborted
type: end

## /apply/final_verification
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1 | tail -50"
  timeout: "300"
output_keys: ["final_test_results"]
next: ["/apply/judge_final"]

## /apply/judge_final
type: dsl_call
llm_tool: gpt-4
output_keys: ["final_verdict"]
prompt: |
  最终测试结果:
  {{final_test_results}}

  执行最终验证，确认所有验收标准满足。
  输出:
  verified: true/false
  summary: "验证总结"
next: ["/apply/route_final"]

## /apply/route_final
type: assert
condition: "{{final_verdict.verified}} == true"
on_failure: "/apply/final_fix"
next: ["/apply/update_change_status"]

## /apply/final_fix
type: dsl_call
llm_tool: gpt-4
output_keys: ["final_fix"]
prompt: |
  最终验证失败:
  {{final_verdict.summary}}

  生成修复方案。
next: ["/apply/apply_final_fix"]

## /apply/apply_final_fix
type: tool_call
tool_name: bash
arguments:
  command: "echo 'Applying final fix'"
  timeout: "10"
output_keys: []
next: ["/apply/final_verification"]

## /apply/update_change_status
type: state
operation: merge
state_key: "openspec.apply.{{session_id}}"
value_template: |
  {
    "status": "completed",
    "completed_at": "{{timestamp}}"
  }
next: ["/apply/generate_report"]

## /apply/generate_report
type: tool_call
tool_name: write_file
arguments:
  path: "openspec/changes/{{change_id}}/IMPLEMENTATION.yaml"
  content: |
    id: {{change_id}}
    status: completed
    completed_at: {{timestamp}}

    completed_tasks: {{completed_tasks}}
    failed_tasks: {{failed_tasks}}

    final_verification: {{final_verdict.summary}}
output_keys: ["report_path"]
next: ["/apply/present_success"]

## /apply/present_success
type: user_input
prompt: |
  Change 实现成功！

  完成的 tasks: {{completed_tasks}}

  报告已保存: {{report_path}}

  下一步建议：
  - 运行完整的测试套件
  - 发起 code review
  - 使用 openspec-archive 归档
input_variable: next_action
input_type: confirm
next: ["/apply/end"]

## /apply/end
type: end

---

## /ideal_extension
type: comment
comment: |
  ## 理想 DSL 扩展：openspec_apply 技能

  ### 1. openspec_execute 节点
  # OpenSpec 执行节点
  type: openspec_execute
  change_id: "{{change_id}}"
  strategy: parallel|sequential
  max_parallel: 3
  on_task_failure:
    mode: abort|continue|retry
    max_retries: 2
  output:
    completed_tasks: [task_array]
    failed_tasks: [task_array]
    status: completed|failed

  ### 2. spec_compliance 节点
  # 规格合规验证
  type: spec_compliance
  spec_file: "{{spec_path}}"
  implementation: "{{changed_files}}"
  check:
    - api_signature
    - data_format
    - error_handling
    - performance
  output:
    compliant: boolean
    violations: [violation_array]

  ### 3. skill_invoke（调用项目技能）
  type: skill_invoke
  skill: "openspec_apply"
  input:
    change_id: "{{change_id}}"
  output:
    status: completed
    completed_tasks: [tasks]
    report: implementation_report
