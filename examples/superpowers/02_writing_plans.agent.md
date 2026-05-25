### AgenticDSL '/superpowers/writing_plans'

# Writing Plans — AgenticDSL 实现

> 对应 Superpowers `writing-plans` 技能
> 核心：将需求拆解为任务 → 每个任务 TDD → 频繁提交

## /__meta__
execution_budget:
  max_llm_calls: 20
  max_tool_calls: 15
  max_total_nodes: 30

---

## /wp/start
type: start
next: ["/wp/load_spec"]

## /wp/load_spec
type: assign
assign:
  design_spec: "{{user_input|default:'未提供规格文档'}}"
  plan_date: "{{date}}"
  feature_name: "{{feature_name|default:'unnamed_feature'}}"
next: ["/wp/scope_check"]

## /wp/scope_check
type: dsl_call
llm_tool: gpt-4
output_keys: ["scope_analysis"]
prompt: |
  分析规格范围:
  
  {{design_spec}}
  
  是否涉及多个独立子系统？
  如果是，建议拆分为多个独立计划——每个计划产生可独立工作的软件。
  
  输出:
  subsystems: 子系统列表
  recommended_split: true/false
  reason: 拆分理由
next: ["/wp/check_split"]

## /wp/check_split
type: assert
condition: "{{scope_analysis|find:'recommended_split: false'}}"
on_failure: "/wp/suggest_split"
next: ["/wp/map_files"]

## /wp/suggest_split
type: user_input
prompt: |
  建议拆分为多个计划:
  
  {{scope_analysis}}
  
  是否继续拆分为独立子计划？
input_variable: approve_split
input_type: confirm
next: ["/wp/handle_split"]

## /wp/handle_split
type: assert
condition: "{{approve_split}}"
on_failure: "/wp/map_files"
next: ["/wp/end"]

## /wp/map_files
type: state
operation: read
state_key: "plan.{{session_id}}.file_map"
output_key: "existing_file_map"
next: ["/wp/analyze_structure"]

## /wp/analyze_structure
type: dsl_call
llm_tool: gpt-4
output_keys: ["file_structure"]
prompt: |
  分析实现 {{feature_name}} 所需的文件结构。
  
  规格: {{design_spec}}
  
  要求:
  - 每个文件一个清晰的职责
  - 小而专注，不搞大文件
  - 一起变化的文件放一起
  
  输出完整的文件映射:
  创建文件:
    - path/to/new_file.h: 职责描述
    - path/to/new_file.cpp: 职责描述
  修改文件:
    - path/to/existing.h: 修改内容
next: ["/wp/save_file_map"]

## /wp/save_file_map
type: state
operation: write
state_key: "plan.{{session_id}}.file_map"
value_template: |
  {{file_structure}}
next: ["/wp/create_tasks"]

## /wp/create_tasks
type: dsl_call
llm_tool: gpt-4
output_keys: ["task_breakdown"]
prompt: |
  基于文件结构，创建任务分解。
  
  文件结构: {{file_structure}}
  规格: {{design_spec}}
  
  要求:
  - 每个任务 2-5 分钟
  - 小步提交（每通过一个测试就 commit）
  - 每个任务包含: 写测试 → 看测试失败 → 实现 → 看测试通过 → 提交
  
  输出任务列表 (Markdown):
  ## Task 1: [名称]
  - 文件: ...
  - 步骤:
    1. 写测试
    2. 运行确认失败
    3. 实现最小代码
    4. 运行确认通过
    5. 提交
next: ["/wp/save_plan"]

## /wp/save_plan
type: tool_call
tool_name: write_file
arguments:
  path: "docs/superpowers/plans/{{plan_date}}-{{feature_name}}.md"
  content: |
    # {{feature_name}} 实现计划
    
    ## 概述
    目标: 基于 {{design_spec}}
    
    ## 文件结构
    {{file_structure}}
    
    ## 任务
    {{task_breakdown}}
    
    ## 验证
    - 所有测试通过
    - 构建无警告
    - 代码审查
output_keys: ["plan_path"]
next: ["/wp/report_done"]

## /wp/report_done
type: tool_call
tool_name: bash
arguments:
  command: "echo '计划已保存到 {{plan_path}}'"
output_keys: ["done_message"]
next: ["/wp/end"]

## /wp/end
type: end
