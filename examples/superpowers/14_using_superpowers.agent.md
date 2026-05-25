### AgenticDSL '/superpowers/using_superpowers'

# Using Superpowers — AgenticDSL 实现

> 对应 Superpowers `using-superpowers` 技能
> 核心：发现可用技能 → 选择合适的技能 → 加载并执行
> 使用 state 实现技能注册表，generate_subgraph 实现技能自发现

## /__meta__
execution_budget:
  max_llm_calls: 15
  max_tool_calls: 10
  max_total_nodes: 25
  max_user_inputs: 5

---

## /usp/start
type: start
next: ["/usp/scan_skills"]

## /usp/scan_skills
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 扫描可用技能 ==="
    
    # 扫描各目录下的 SKILL.md
    for dir in ".opencode/skills" ".claude/skills" "~/.agents/skills" "~/.config/opencode/skills" "skills"; do
      if [ -d "$dir" ]; then
        echo "DIR:$dir"
        find "$dir" -name "SKILL.md" -maxdepth 2 2>/dev/null
      fi
    done
    
    # 扫描本项目的 .agent.md 文件
    find . -name "*.agent.md" -maxdepth 3 2>/dev/null | head -20
    
    echo "SCAN_COMPLETE"
  timeout: "10"
output_keys: ["skill_scan"]
next: ["/usp/check_scan"]

## /usp/check_scan
type: assert
condition: "{{skill_scan|find:'SCAN_COMPLETE'}}"
on_failure: "/usp/scan_failed"
next: ["/usp/parse_skills"]

## /usp/scan_failed
type: assign
assign:
  skill_scan: "DIR:skills\n扫描无结果"
next: ["/usp/parse_skills"]

## /usp/parse_skills
type: dsl_call
llm_tool: gpt-4
output_keys: ["skill_inventory"]
prompt: |
  解析技能扫描结果:
  
  {{skill_scan}}
  
  从扫描结果中提取技能列表。
  对于每个技能，提取:
  - 名称
  - 路径
  - 描述（如果能从路径推断）
  
  输出 JSON:
  [
    {"name": "brainstorming", "path": ".opencode/skills/brainstorming/SKILL.md", "description": "需求探索与设计"},
    ...
  ]
next: ["/usp/load_skill_index"]

## /usp/load_skill_index
type: state
operation: write
state_key: "superpowers.{{session_id}}.inventory"
value_template: |
  {{skill_inventory}}
next: ["/usp/present_skills"]

## /usp/present_skills
type: dsl_call
llm_tool: gpt-4
output_keys: ["formatted_menu"]
prompt: |
  格式化技能菜单:
  
  {{skill_inventory}}
  
  按类别分组并输出:
  
  ## 可用技能
  
  ### 流程类
  - brainstorming: 需求探索与设计
  - writing-plans: 生成实现计划
  - ...
  
  ### 开发类
  - test-driven-development: TDD 循环
  - ...
  
  ### 代码质量类
  - ...
next: ["/usp/ask_choice"]

## /usp/ask_choice
type: user_input
prompt: |
  可用技能:
  
  {{formatted_menu}}
  
  请选择要使用的技能（输入编号或名称）:
input_variable: skill_choice
input_type: text
next: ["/usp/find_skill"]

## /usp/find_skill
type: state
operation: read
state_key: "superpowers.{{session_id}}.inventory"
output_key: "inventory"
next: ["/usp/match_skill"]

## /usp/match_skill
type: dsl_call
llm_tool: gpt-4
output_keys: ["selected_skill"]
prompt: |
  匹配用户选择:
  
  用户输入: {{skill_choice}}
  技能列表: {{inventory}}
  
  找到最匹配的技能。
  如果找不到，返回 UNKNOWN。
  
  输出:
  match: true/false
  skill: {name, path, description}
  confidence: 匹配度
next: ["/usp/check_match"]

## /usp/check_match
type: assert
condition: "{{selected_skill|find:'match: true'}}"
on_failure: "/usp/no_match"
next: ["/usp/load_skill"]

## /usp/no_match
type: user_input
prompt: |
  未找到匹配技能: {{skill_choice}}
  
  是否:
  1. 重试选择
  2. 直接描述需求（动态生成技能）
input_variable: no_match_action
input_type: choice
options: ["重试选择", "动态生成"]
next: ["/usp/handle_no_match"]

## /usp/handle_no_match
type: assert
condition: "{{no_match_action}} == 重试选择"
on_failure: "/usp/dynamic_generate"
next: ["/usp/ask_choice"]

## /usp/load_skill
type: tool_call
tool_name: read_file
arguments:
  path: "{{selected_skill.skill.path}}"
output_keys: ["skill_content"]
next: ["/usp/check_skill_loaded"]

## /usp/check_skill_loaded
type: assert
condition: "{{skill_content|length}} > 10"
on_failure: "/usp/load_failed"
next: ["/usp/present_skill"]

## /usp/load_failed
type: assign
assign:
  skill_content: "错误: 无法加载技能文件 {{selected_skill.skill.path}}"
next: ["/usp/present_skill"]

## /usp/present_skill
type: user_input
prompt: |
  已加载技能: {{selected_skill.skill.name}}
  
  路径: {{selected_skill.skill.path}}
  
  技能内容:
  ---
  {{skill_content|truncate:1000}}
  ---
  
  是否执行此技能？
input_variable: execute_skill
input_type: confirm
next: ["/usp/check_execute"]

## /usp/check_execute
type: assert
condition: "{{execute_skill}}"
on_failure: "/usp/ask_choice"
next: ["/usp/execute_skill"]

## /usp/execute_skill - 动态生成技能执行图
type: generate_subgraph
prompt: |
  执行技能: {{selected_skill.skill.name}}
  
  技能内容:
  {{skill_content}}
  
  根据技能描述生成执行 DSL。
  
  例如，如果技能是 brainstorming，生成:
  ### AgenticDSL '/dynamic/execute_brainstorming'
  ## /dynamic/execute_brainstorming/start
  type: start
  ...
  
  如果技能是 TDD，生成 TDD 循环 DSL。
  
  生成的 DSL 应忠实反映技能的工作流。
output_keys: ["skill_execution_graph"]
signature_validation: ignore
next: ["/usp/run_skill"]

## /usp/run_skill
type: dsl_call
llm_tool: gpt-4
output_keys: ["skill_result"]
prompt: |
  执行技能 DSL:
  
  {{skill_execution_graph}}
  
  输出执行结果。
next: ["/usp/dynamic_generate"]

## /usp/dynamic_generate
type: user_input
prompt: |
  请描述您想完成的任务:
  
  （AgenticDSL 将动态生成并执行对应的技能）
input_variable: task_description
input_type: multiline
next: ["/usp/create_skill_on_the_fly"]

## /usp/create_skill_on_the_fly
type: generate_subgraph
prompt: |
  根据用户需求动态创建技能:
  
  任务描述: {{task_description}}
  
  分析需求并生成:
  1. 一个标准的 .agent.md 文件
  2. 包含完整的执行流程
  
  输出格式:
  ### AgenticDSL '/dynamic/custom_skill'
  ## /dynamic/custom_skill/start
  type: start
  ...
output_keys: ["custom_skill_graph"]
signature_validation: ignore
next: ["/usp/execute_custom"]

## /usp/execute_custom
type: dsl_call
llm_tool: gpt-4
output_keys: ["custom_result"]
prompt: |
  执行动态生成的技能:
  
  {{custom_skill_graph}}
  
  输出执行结果。
next: ["/usp/save_custom_skill"]

## /usp/save_custom_skill
type: user_input
prompt: |
  动态技能执行完成。
  
  结果: {{custom_result}}
  
  是否保存此技能供以后使用？
input_variable: save_skill
input_type: confirm
next: ["/usp/check_save"]

## /usp/check_save
type: assert
condition: "{{save_skill}}"
on_failure: "/usp/end"
next: ["/usp/persist_skill"]

## /usp/persist_skill
type: user_input
prompt: "请输入技能名称:"
input_variable: new_skill_name
input_type: text
next: ["/usp/save_to_file"]

## /usp/save_to_file
type: tool_call
tool_name: write_file
arguments:
  path: ".opencode/skills/{{new_skill_name}}/SKILL.md"
  content: "{{custom_skill_graph}}"
output_keys: ["saved_path"]
next: ["/usp/update_inventory"]

## /usp/update_inventory
type: state
operation: write
state_key: "superpowers.{{session_id}}.inventory"
value_template: |
  {{inventory|append:{name: new_skill_name, path: saved_path}}}
next: ["/usp/report"]

## /usp/report
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== Superpowers 技能系统 ==="
    echo "已加载技能: {{inventory|length}} 个"
    echo "已执行: {{selected_skill.skill.name|default:'动态技能'}}"
    echo "新保存: {{new_skill_name|default:'无'}}"
output_keys: ["final_report"]
next: ["/usp/end"]

## /usp/end
type: end
