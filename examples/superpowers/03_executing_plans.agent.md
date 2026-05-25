### AgenticDSL '/superpowers/executing_plans'

# Executing Plans — AgenticDSL 实现

> 对应 Superpowers `executing-plans` 技能
> 核心：加载计划 → 审查 → 执行任务序列 → 完成

## /__meta__
execution_budget:
  max_llm_calls: 5
  max_tool_calls: 50
  max_total_nodes: 60

---

## /ep/start
type: start
next: ["/ep/load_plan"]

## /ep/load_plan
type: tool_call
tool_name: read_file
arguments:
  path: "{{plan_path}}"
output_keys: ["plan_content"]
next: ["/ep/verify_plan"]

## /ep/verify_plan
type: assert
condition: "{{plan_content|length}} > 10"
on_failure: "/ep/plan_error"
next: ["/ep/review_plan"]

## /ep/plan_error
type: tool_call
tool_name: bash
arguments:
  command: "echo '错误: 计划文件为空或不存在: {{plan_path}}'"
output_keys: ["error_msg"]
next: ["/ep/end"]

## /ep/review_plan
type: dsl_call
llm_tool: gpt-4
output_keys: ["plan_review"]
prompt: |
  审查以下实现计划:
  
  {{plan_content}}
  
  检查:
  1. 是否有不清楚的指令？
  2. 是否有缺失的上下文？
  3. 依赖是否明确？
  4. 验证步骤是否完整？
  
  有问题则输出问题列表；无问题输出 "PLAN_OK"。
next: ["/ep/check_review"]

## /ep/check_review
type: assert
condition: "{{plan_review|find:'PLAN_OK'}}"
on_failure: "/ep/raise_concerns"
next: ["/ep/parse_tasks"]

## /ep/raise_concerns
type: tool_call
tool_name: bash
arguments:
  command: "echo '计划存在以下问题:\n{{plan_review}}'"
output_keys: ["concerns"]
next: ["/ep/ask_user"]

## /ep/ask_user
type: user_input
prompt: |
  计划审查发现问题:
  {{plan_review}}
  
  是否继续执行？
input_variable: continue_anyway
input_type: confirm
next: ["/ep/check_continue"]

## /ep/check_continue
type: assert
condition: "{{continue_anyway}}"
on_failure: "/ep/end"
next: ["/ep/parse_tasks"]

## /ep/parse_tasks
type: generate_subgraph
prompt: |
  从计划中提取任务列表并生成执行图:
  
  {{plan_content}}
  
  输出:
  ### AgenticDSL '/dynamic/task_executor'
  ## /dynamic/task_executor/task_1
  type: tool_call
  tool_name: bash
  arguments:
    command: echo "执行 Task 1"
  ...
  
  每个任务应有验证步骤。
output_keys: ["execution_graph_path"]
signature_validation: ignore
next: ["/ep/init_tracker"]

## /ep/init_tracker
type: state
operation: write
state_key: "exec.{{session_id}}.progress"
value_template: |
  {
    "plan_path": "{{plan_path}}",
    "current_task": 0,
    "total_tasks": "{{task_count}}",
    "completed_tasks": [],
    "failed_tasks": [],
    "status": "running"
  }
next: ["/ep/execute_tasks"]

## /ep/execute_tasks
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_result"]
prompt: |
  执行计划中的下一个任务。
  
  计划: {{plan_content}}
  当前进度: {{execution_state}}
  
  确定下一个要执行的任务并输出命令。
next: ["/ep/run_task"]

## /ep/run_task
type: tool_call
tool_name: bash
arguments:
  command: "{{task_result.command}}"
  timeout: "120"
output_keys: ["task_output"]
next: ["/ep/verify_task"]

## /ep/verify_task
type: dsl_call
llm_tool: gpt-4
output_keys: ["verification"]
prompt: |
  验证任务执行结果:
  
  任务: {{task_result}}
  输出: {{task_output}}
  
  是否成功？如果没有成功，错误是什么？
  
  输出: "PASS" 或 "FAIL: 原因"
next: ["/ep/check_task"]

## /ep/check_task
type: assert
condition: "{{verification|find:'PASS'}}"
on_failure: "/ep/handle_failure"
next: ["/ep/update_progress"]

## /ep/handle_failure
type: tool_call
tool_name: bash
arguments:
  command: "echo '任务失败: {{verification}}'"
output_keys: ["failure_msg"]
next: ["/ep/ask_continue"]

## /ep/ask_continue
type: user_input
prompt: |
  任务执行失败:
  {{verification}}
  
  是否:
  1. 重试当前任务
  2. 跳过继续
  3. 停止执行
input_variable: failure_action
input_type: choice
options: ["重试", "跳过", "停止"]
next: ["/ep/route_failure"]

## /ep/route_failure
type: generate_subgraph
prompt: |
  用户选择: {{failure_action}}
  
  如果 "重试": 生成跳转到任务重试的图
  如果 "跳过": 生成更新进度并继续的图
  如果 "停止": 生成结束执行的图
output_keys: ["failure_path"]
signature_validation: ignore
next: ["/ep/execute_route"]

## /ep/execute_route
type: dsl_call
llm_tool: gpt-4
output_keys: ["route_result"]
prompt: |
  执行失败处理路由: {{failure_path}}
next: ["/ep/check_should_stop"]

## /ep/check_should_stop
type: assert
condition: "{{failure_action}} == 停止"
on_failure: "/ep/execute_tasks"
next: ["/ep/completion_report"]

## /ep/update_progress
type: state
operation: write
state_key: "exec.{{session_id}}.progress"
value_template: |
  {
    "current_task": {{current_task|add:1}},
    "completed_tasks": {{completed_tasks|append:task_result.name}}
  }
next: ["/ep/check_more_tasks"]

## /ep/check_more_tasks
type: tool_call
tool_name: bash
arguments:
  command: |
    remaining=$(grep -c "^## Task" "{{plan_path}}" 2>/dev/null || echo "0")
    completed=$(cat <<'STATE' | python3 -c "import sys,json; d=json.load(sys.stdin); print(len(d.get('completed_tasks',[])))" 2>/dev/null || echo "0")
    echo "remaining=$((remaining - completed))"
output_keys: ["task_count_result"]
next: ["/ep/has_more"]

## /ep/has_more
type: assign
assign:
  remaining: "{{task_count_result|extract:'remaining=(\\d+)'}}"
next: ["/ep/check_remaining"]

## /ep/check_remaining
type: assert
condition: "{{remaining}} > 0"
on_failure: "/ep/completion_report"
next: ["/ep/execute_tasks"]

## /ep/completion_report
type: state
operation: read
state_key: "exec.{{session_id}}.progress"
output_key: "final_state"
next: ["/ep/summarize"]

## /ep/summarize
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 执行报告 ==="
    echo "计划: {{plan_path}}"
    echo "完成: {{final_state.completed_tasks|length}} 个任务"
    echo "失败: {{final_state.failed_tasks|length}} 个任务"
    echo "状态: {{final_state.status}}"
output_keys: ["summary"]
next: ["/ep/transition"]

## /ep/transition
type: tool_call
tool_name: bash
arguments:
  command: "echo '执行完成。下一步: 调用 finishing-a-development-branch 技能'"
output_keys: ["done"]
next: ["/ep/end"]

## /ep/end
type: end
