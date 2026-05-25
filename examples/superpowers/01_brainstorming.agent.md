### AgenticDSL '/superpowers/brainstorming'

# Brainstorming — AgenticDSL 实现

> 对应 Superpowers `brainstorming` 技能
> 核心：探索用户意图 → 提问澄清 → 设计方案 → 写设计文档

## /__meta__
execution_budget:
  max_llm_calls: 30
  max_tool_calls: 10
  max_user_inputs: 50
  max_total_nodes: 40

---

## /brainstorm/start
type: start
next: ["/brainstorm/explore_context"]

## /brainstorm/explore_context
type: tool_call
tool_name: list_directory
arguments:
  path: "."
  recursive: "true"
output_keys: ["project_files"]
next: ["/brainstorm/init_state"]

## /brainstorm/init_state
type: state
operation: write
state_key: "brainstorm.{{session_id}}.state"
value_template: |
  {
    "project_context": {{project_files}},
    "clarifying_questions": [],
    "approaches": [],
    "design_doc_path": null
  }
next: ["/brainstorm/ask_intent"]

## /brainstorm/ask_intent
type: user_input
prompt: |
  项目结构已加载。请描述您想实现的功能或解决的问题：
input_variable: user_intent
input_type: multiline
next: ["/brainstorm/analyze_intent"]

## /brainstorm/analyze_intent
type: dsl_call
llm_tool: gpt-4
output_keys: ["intent_analysis"]
prompt: |
  用户想要: {{user_intent}}
  项目上下文: {{project_files}}

  分析:
  1. 核心目标是什么？
  2. 涉及哪些模块？
  3. 有什么隐含约束？
  4. 成功标准是什么？
next: ["/brainstorm/check_clarity"]

## /brainstorm/check_clarity
type: dsl_call
llm_tool: gpt-4
output_keys: ["clarity_check"]
prompt: |
  意图分析: {{intent_analysis}}

  判断需求是否足够清晰。
  如果不够清晰，输出需要澄清的问题（一次只问一个）。
  
  输出格式:
  is_clear: true/false
  question: "需要澄清的问题"
next: ["/brainstorm/route_clarity"]

## /brainstorm/route_clarity
type: assert
condition: "{{clarity_check|find:'is_clear: true'}}"
on_failure: "/brainstorm/ask_clarification"
next: ["/brainstorm/propose_approaches"]

## /brainstorm/ask_clarification
type: user_input
prompt: "{{clarity_check.question}}"
input_variable: clarification
input_type: text
next: ["/brainstorm/update_analysis"]

## /brainstorm/update_analysis
type: assign
assign:
  clarifying_questions: "{{clarifying_questions|append:{q: clarity_check.question, a: clarification}}}"
next: ["/brainstorm/analyze_intent"]

## /brainstorm/propose_approaches
type: dsl_call
llm_tool: gpt-4
output_keys: ["proposed_approaches"]
prompt: |
  基于需求和项目上下文，提出 2-3 种实现方案。
  
  需求: {{user_intent}}
  分析: {{intent_analysis}}
  项目上下文: {{project_files}}
  
  每种方案包括:
  - 方案名称
  - 核心思路
  - 优点
  - 缺点
  - 预估工作量
  
  推荐其中一个并说明理由。
next: ["/brainstorm/present_to_user"]

## /brainstorm/present_to_user
type: user_input
prompt: |
  ## 方案分析结果
  
  {{proposed_approaches}}
  
  请选择:
  1. 采用推荐方案继续
  2. 选择其他方案
  3. 需要调整
input_variable: user_choice
input_type: choice
options: ["采用推荐方案", "选择其他方案", "需要调整"]
next: ["/brainstorm/handle_choice"]

## /brainstorm/handle_choice
type: generate_subgraph
prompt: |
  用户选择: {{user_choice}}
  方案: {{proposed_approaches}}
  
  如果用户接受方案，生成:
  ### AgenticDSL '/dynamic/design_phase'
  ## /dynamic/design_phase/present_design
  type: dsl_call
  ...
  output_keys: ["design"]
  
  如果需要调整，生成:
  ### AgenticDSL '/dynamic/adjust'
  ## /dynamic/adjust/ask_details
  type: user_input
  prompt: 请提供调整方向
  input_variable: adjustments
  ...
output_keys: ["dynamic_path"]
signature_validation: ignore
next: ["/brainstorm/execute_phase"]

## /brainstorm/execute_phase
type: dsl_call
llm_tool: gpt-4
output_keys: ["phase_result"]
prompt: |
  执行当前阶段: {{dynamic_path}}
  当前状态: {{proposed_approaches}}, {{user_choice}}
next: ["/brainstorm/write_design_doc"]

## /brainstorm/write_design_doc
type: dsl_call
llm_tool: gpt-4
output_keys: ["design_doc"]
prompt: |
  编写设计文档:
  
  功能: {{user_intent}}
  方案: {{proposed_approaches}}
  最终设计: {{phase_result}}
  
  输出格式: Markdown，包含:
  - 概述
  - 设计决策
  - 架构图
  - 接口定义
  - 数据流
  - 风险与缓解
next: ["/brainstorm/save_doc"]

## /brainstorm/save_doc
type: tool_call
tool_name: write_file
arguments:
  path: "docs/superpowers/specs/{{date}}-{{session_id}}-design.md"
  content: "{{design_doc}}"
output_keys: ["saved_path"]
next: ["/brainstorm/self_review"]

## /brainstorm/self_review
type: dsl_call
llm_tool: gpt-4
output_keys: ["review_result"]
prompt: |
  自我审查设计文档:
  
  {{design_doc}}
  
  检查:
  1. 是否有占位符或 TBD？
  2. 是否有矛盾？
  3. 范围是否明确？
  4. 是否有遗漏？
  
  输出问题列表（如果有）。
next: ["/brainstorm/check_review"]

## /brainstorm/check_review
type: assert
condition: "{{review_result|length}} < 50"
on_failure: "/brainstorm/fix_review"
next: ["/brainstorm/user_review"]

## /brainstorm/fix_review
type: dsl_call
llm_tool: gpt-4
output_keys: ["fixed_doc"]
prompt: |
  修复以下问题:
  {{review_result}}
  
  原始文档: {{design_doc}}
  
  输出修复后的完整文档。
next: ["/brainstorm/save_fixed"]

## /brainstorm/save_fixed
type: tool_call
tool_name: write_file
arguments:
  path: "{{saved_path}}"
  content: "{{fixed_doc}}"
output_keys: ["resaved"]
next: ["/brainstorm/user_review"]

## /brainstorm/user_review
type: user_input
prompt: |
  设计文档已保存到 {{saved_path}}。
  
  请审查文档，确认是否可以进入实现阶段？
input_variable: user_approval
input_type: confirm
next: ["/brainstorm/check_approval"]

## /brainstorm/check_approval
type: assert
condition: "{{user_approval}}"
on_failure: "/brainstorm/ask_revisions"
next: ["/brainstorm/save_state"]

## /brainstorm/ask_revisions
type: user_input
prompt: "请说明需要哪些修改："
input_variable: revisions
input_type: multiline
next: ["/brainstorm/write_design_doc"]

## /brainstorm/save_state
type: state
operation: write
state_key: "brainstorm.{{session_id}}.final"
value_template: |
  {
    "intent": {{user_intent}},
    "design_doc": {{design_doc}},
    "saved_path": "{{saved_path}}",
    "approved": true
  }
next: ["/brainstorm/transition_to_plan"]

## /brainstorm/transition_to_plan
type: tool_call
tool_name: bash
arguments:
  command: "echo '设计完成。下一步: 调用 writing-plans 生成实现计划'"
output_keys: ["transition_message"]
next: ["/brainstorm/end"]

## /brainstorm/end
type: end
