### AgenticDSL '/taxonomy/axis1_process/brainstorming'

# Brainstorming — AgenticDSL 实现

> **轴分类**: 轴1-流程/方法论
> **核心 DSL 特性**: user_input, dsl_call, state, assert, fork, skill_invoke(理想)
> **对应 Superpowers 技能**: brainstorming

## /__meta__
execution_budget:
  max_llm_calls: 30
  max_tool_calls: 20
  max_user_inputs: 50
  max_total_nodes: 40

---

## /brainstorm/start
type: start
next: ["/brainstorm/load_context"]

## /brainstorm/load_context
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
    "design_doc_path": null,
    "phase": "intent_gathering"
  }
next: ["/brainstorm/ask_intent"]

## /brainstorm/ask_intent
type: user_input
prompt: |
  项目结构已加载。请描述您想实现的功能或解决的问题：

  例如：
  - "我想添加一个用户认证模块"
  - "我想优化数据库查询性能"
  - "我想重构订单处理流程"
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

  分析：
  1. 核心目标是什么？
  2. 涉及哪些模块/文件？
  3. 有什么隐含约束？
  4. 成功标准是什么？
  5. 有什么已知风险？
next: ["/brainstorm/check_clarity"]

## /brainstorm/check_clarity
type: dsl_call
llm_tool: gpt-4
output_keys: ["clarity_check"]
prompt: |
  意图分析: {{intent_analysis}}

  判断需求是否足够清晰可以进入方案设计阶段。
  如果不够清晰，输出需要澄清的问题（一次只问一个）。

  输出格式:
  is_clear: true/false
  question: "需要澄清的问题（如果不够清晰）"
  confidence: 0.0-1.0
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
next: ["/brainstorm/update_intent"]

## /brainstorm/update_intent
type: state
operation: merge
state_key: "brainstorm.{{session_id}}.state"
value_template: |
  {
    "clarifying_questions": {{clarifying_questions|append:{q: clarity_check.question, a: clarification}}},
    "original_intent": "{{user_intent}}"
  }
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
  - 缺点/风险
  - 预估工作量 (小/中/大)

  推荐其中一个并说明理由。
next: ["/brainstorm/present_approaches"]

## /brainstorm/present_approaches
type: user_input
prompt: |
  提出了以下方案：

  {{proposed_approaches}}

  请选择您偏好的方案，或提出修改意见。
input_variable: approach_choice
input_type: text
next: ["/brainstorm/build_design"]

## /brainstorm/build_design
type: dsl_call
llm_tool: gpt-4
output_keys: ["design_doc"]
prompt: |
  选中的方案: {{approach_choice}}

  生成完整设计文档，包含：
  1. 概述
  2. 详细设计
  3. 实现步骤
  4. 测试策略
  5. 风险和缓解措施
next: ["/brainstorm/save_design"]

## /brainstorm/save_design
type: tool_call
tool_name: write_file
arguments:
  path: "docs/superpowers/specs/{{timestamp}}-design.md"
  content: "{{design_doc}}"
output_keys: ["design_path"]
next: ["/brainstorm/confirm_design"]

## /brainstorm/confirm_design
type: user_input
prompt: |
  设计文档已保存到: {{design_path}}

  请审核设计文档，确认是否进入实现阶段。
input_variable: design_approved
input_type: confirm
next: ["/brainstorm/route_approval"]

## /brainstorm/route_approval
type: assert
condition: "{{design_approved}} == true"
on_failure: "/brainstorm/revise_design"
next: ["/brainstorm/skill_transition"]

## /brainstorm/revise_design
type: dsl_call
llm_tool: gpt-4
output_keys: ["revised_design"]
prompt: |
  用户对设计文档的修改意见: {{design_approved}}

  请基于反馈更新设计文档。
next: ["/brainstorm/save_revised"]

## /brainstorm/save_revised
type: tool_call
tool_name: write_file
arguments:
  path: "{{design_path}}"
  content: "{{revised_design}}"
output_keys: []
next: ["/brainstorm/confirm_design"]

## /brainstorm/skill_transition
type: dsl_call
llm_tool: gpt-4
output_keys: ["next_skill"]
prompt: |
  设计阶段完成。

  下一步应该是什么？
  - 如果需要实现 → "writing_plans"
  - 如果需要探索更多问题 → "brainstorming"（再次）
  - 如果需要并发调查 → "dispatching_parallel_agents"

  输出下一步技能名称。
next: ["/brainstorm/end"]

## /brainstorm/end
type: end

---

## /ideal_extension
type: comment
comment: |
  ## 理想 DSL 扩展：brainstorming 技能

  ### 1. skill_invoke 节点（原生技能调用）
  # 当前：通过 dsl_call + prompt 模拟技能调用
  # 理想：直接声明式调用
  type: skill_invoke
  skill: "brainstorming"
  input:
    user_intent: "{{user_intent}}"
    project_context: "{{project_files}}"
  output:
    design_doc: design_doc
    next_skill: next_skill

  ### 2. skill_compose（技能链式组合）
  # 多个技能按顺序组合，自动传递上下文
  type: skill_compose
  skills:
    - skill: "brainstorming"
      output_alias: "design_doc"
    - skill: "writing_plans"
      input:
        spec: "{{design_doc}}"
    - skill: "subagent_driven_development"
      input:
        plan: "{{plan_output}}"

  ### 3. 交互式分支（Interactive Branch）
  # 用户选择分支方向，声明式路由
  type: user_branch
  prompt: "请选择方案："
  options:
    - label: "方案A"
      next: "/approach_a"
    - label: "方案B"
      next: "/approach_b"
    - label: "修改设计"
      next: "/brainstorm/revise_design"
