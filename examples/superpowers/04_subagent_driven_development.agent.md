### AgenticDSL '/superpowers/subagent_driven_development'

# Subagent-Driven Development — AgenticDSL 实现

> 对应 Superpowers `subagent-driven-development` 技能
> 核心：每个任务派发独立子 agent → 两阶段 review（spec compliance → quality）
> 使用 Fork/Join 实现并行派发

## /__meta__
execution_budget:
  max_llm_calls: 100
  max_tool_calls: 80
  max_total_nodes: 80
  max_depth: 20

---

## /sdd/start
type: start
next: ["/sdd/load_plan"]

## /sdd/load_plan
type: tool_call
tool_name: read_file
arguments:
  path: "{{plan_path}}"
output_keys: ["plan_content"]
next: ["/sdd/parse_tasks"]

## /sdd/parse_tasks
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_list"]
prompt: |
  解析计划中的任务列表:
  
  {{plan_content}}
  
  输出 JSON 数组，每个元素:
  {
    "id": "task_1",
    "name": "任务名",
    "files": ["需要修改的文件"],
    "description": "任务描述",
    "dependencies": []  # 依赖的任务 ID
  }
next: ["/sdd/topological_sort"]

## /sdd/topological_sort
type: dsl_call
llm_tool: gpt-4
output_keys: ["execution_order"]
prompt: |
  对任务按拓扑排序:
  
  {{task_list}}
  
  要求:
  - 按依赖关系排序
  - 没有依赖的任务可以并行
  - 输出分组列表: [[并行任务组1], [并行任务组2], ...]
next: ["/sdd/init_state"]

## /sdd/init_state
type: state
operation: write
state_key: "sdd.{{session_id}}.progress"
value_template: |
  {
    "execution_order": {{execution_order}},
    "results": {},
    "review_results": {},
    "current_group": 0
  }
next: ["/sdd/process_group"]

## /sdd/process_group — 并行处理一组任务
type: fork
branches: ["/sdd/task_dispatcher", "/sdd/coordination"]
context_isolation: deep_copy
next: ["/sdd/join_group"]

## /sdd/task_dispatcher
type: dsl_call
llm_tool: gpt-4
output_keys: ["dispatched_tasks"]
prompt: |
  当前任务组从索引 {{current_group}}。
  执行顺序: {{execution_order}}
  
  获取当前组的所有独立任务。
  返回: 当前组的任务列表。
next: ["/sdd/execute_in_parallel"]

## /sdd/execute_in_parallel
type: fork
branches: ["/sdd/execute_task_a", "/sdd/execute_task_b", "/sdd/execute_task_c"]
context_isolation: deep_copy
next: ["/sdd/join_parallel"]

## /sdd/execute_task_a
type: generate_subgraph
prompt: |
  执行任务: {{dispatched_tasks[0]}}
  
  生成独立的执行子图，包含:
  1. 读取需要修改的文件
  2. LLM 分析现有代码
  3. 实现修改
  4. 运行测试
  
  输出: 任务执行结果
output_keys: ["result_a"]
signature_validation: ignore
next: ["/sdd/review_a_spec"]

## /sdd/review_a_spec
type: dsl_call
llm_tool: gpt-4
output_keys: ["spec_review_a"]
prompt: |
  规格合规审查:
  
  任务: {{dispatched_tasks[0]}}
  执行结果: {{result_a}}
  
  检查是否完全满足规格要求。
  输出: "PASS" 或具体问题。
next: ["/sdd/review_a_quality"]

## /sdd/review_a_quality
type: dsl_call
llm_tool: gpt-4
output_keys: ["quality_review_a"]
prompt: |
  代码质量审查:
  
  任务: {{dispatched_tasks[0]}}
  实现: {{result_a}}
  
  检查:
  - 代码质量
  - 错误处理
  - 性能问题
  - 安全性
  
  输出: "PASS" 或具体发现。
next: ["/sdd/store_result_a"]

## /sdd/store_result_a
type: state
operation: write
state_key: "sdd.{{session_id}}.task_a_result"
value_template: |
  {
    "task": {{dispatched_tasks[0]}},
    "result": {{result_a}},
    "spec_review": "{{spec_review_a}}",
    "quality_review": "{{quality_review_a}}"
  }
next: ["/sdd/end_task_a"]

## /sdd/end_task_a
type: end

## /sdd/execute_task_b
type: generate_subgraph
prompt: |
  执行任务: {{dispatched_tasks[1]}}
  
  生成独立的执行子图。
output_keys: ["result_b"]
signature_validation: ignore
next: ["/sdd/review_b_spec"]

## /sdd/review_b_spec
type: dsl_call
llm_tool: gpt-4
output_keys: ["spec_review_b"]
prompt: |
  规格合规审查:
  
  任务: {{dispatched_tasks[1]}}
  执行结果: {{result_b}}
  
  检查是否完全满足规格要求。
  输出: "PASS" 或具体问题。
next: ["/sdd/review_b_quality"]

## /sdd/review_b_quality
type: dsl_call
llm_tool: gpt-4
output_keys: ["quality_review_b"]
prompt: |
  代码质量审查:
  
  任务: {{dispatched_tasks[1]}}
  实现: {{result_b}}
  
  输出: "PASS" 或具体发现。
next: ["/sdd/end_task_b"]

## /sdd/end_task_b
type: end

## /sdd/execute_task_c
type: dsl_call
llm_tool: gpt-4
output_keys: ["result_c"]
prompt: |
  执行第三个任务（如果存在）: {{dispatched_tasks[2]}}
  
  如果不存在，输出 "NO_TASK"。
next: ["/sdd/check_c"]

## /sdd/check_c
type: assert
condition: "{{result_c|find:'NO_TASK'}}"
on_failure: "/sdd/review_c"
next: ["/sdd/end_task_c"]

## /sdd/review_c
type: dsl_call
llm_tool: gpt-4
output_keys: ["review_c"]
prompt: |
  审查任务结果:
  任务: {{dispatched_tasks[2]}}
  结果: {{result_c}}
  
  输出审查结论。
next: ["/sdd/end_task_c"]

## /sdd/end_task_c
type: end

## /sdd/coordination
type: dsl_call
llm_tool: gpt-4
output_keys: ["coordination_note"]
prompt: |
  协调当前任务组的执行。
  
  检查组内任务之间是否有需要协调的共享资源或依赖。
  如果有，记录注意事项。
next: ["/sdd/end_coordination"]

## /sdd/end_coordination
type: end

## /sdd/join_parallel
type: join
wait_for: ["/sdd/end_task_a", "/sdd/end_task_b", "/sdd/end_task_c"]
merge_strategy: deep_merge
next: ["/sdd/aggregate_results"]

## /sdd/aggregate_results
type: state
operation: read
state_key: "sdd.{{session_id}}.task_a_result"
output_key: "all_results"
next: ["/sdd/summarize_group"]

## /sdd/summarize_group
type: dsl_call
llm_tool: gpt-4
output_keys: ["group_summary"]
prompt: |
  汇总当前组执行结果:
  
  {{all_results}}
  
  输出:
  1. 成功任务
  2. 失败任务及修复建议
  3. 进入下一组的条件
next: ["/sdd/check_group_failures"]

## /sdd/check_group_failures
type: assert
condition: "{{group_summary|find:'FAIL'}}"
on_failure: "/sdd/advance_group"
next: ["/sdd/fix_failures"]

## /sdd/fix_failures
type: dsl_call
llm_tool: gpt-4
output_keys: ["fix_plan"]
prompt: |
  修复失败任务:
  
  {{group_summary}}
  
  生成修复方案。
next: ["/sdd/execute_fixes"]

## /sdd/execute_fixes
type: generate_subgraph
prompt: |
  {{fix_plan}}
output_keys: ["fix_result"]
signature_validation: ignore
next: ["/sdd/advance_group"]

## /sdd/advance_group
type: assign
assign:
  current_group: "{{current_group|add:1}}"
next: ["/sdd/check_last_group"]

## /sdd/check_last_group
type: tool_call
tool_name: bash
arguments:
  command: |
    python3 -c "
    import json
    order = json.loads('{{execution_order}}')
    current = {{current_group}}
    print('has_more' if current < len(order) else 'complete')
    "
output_keys: ["group_status"]
next: ["/sdd/has_more_groups"]

## /sdd/has_more_groups
type: assert
condition: "{{group_status}} == has_more"
on_failure: "/sdd/final_summary"
next: ["/sdd/process_group"]

## /sdd/final_summary
type: state
operation: read
state_key: "sdd.{{session_id}}.progress"
output_key: "final_progress"
next: ["/sdd/complete_report"]

## /sdd/complete_report
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 子驱动开发完成 ==="
    echo "总计: {{final_progress.results|length}} 个任务"
output_keys: ["report"]
next: ["/sdd/end"]

## /sdd/end
type: end
