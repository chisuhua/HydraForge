### AgenticDSL '/taxonomy/axis3_review/receiving_code_review'

# Receiving Code Review — AgenticDSL 实现

> **轴分类**: 轴3-审查/质量（跨轴：也属于流程）
> **核心 DSL 特性**: dsl_call, user_input, state, assert, tool_call
> **对应 Superpowers 技能**: receiving_code_review

## /__meta__
execution_budget:
  max_llm_calls: 20
  max_tool_calls: 30
  max_total_nodes: 40

---

## /rcr/start
type: start
next: ["/rcr/load_feedback"]

## /rcr/load_feedback
type: user_input
prompt: |
  请粘贴 code review 反馈内容：

  包括：
  - reviewer 的所有评论
  - 指出的具体问题
  - 建议的修改方式
input_variable: review_feedback
input_type: multiline
next: ["/rcr/parse_feedback"]

## /rcr/parse_feedback
type: dsl_call
llm_tool: gpt-4
output_keys: ["parsed_feedback"]
prompt: |
  Code review 反馈:

  {{review_feedback}}

  解析反馈，识别：
  1. 每条具体意见
  2. 意见类型（bug/风格/设计/讨论）
  3. 是否明确要求修改

  输出 JSON 数组:
  [
    {
      "id": 1,
      "comment": "反馈内容",
      "type": "bug|style|design|discussion|question",
      "requires_change": true/false,
      "severity": "critical|high|medium|low"
    }
  ]
next: ["/rcr/validate_each"]

## /rcr/validate_each
type: fork
branches:
  - "/rcr/validate_item_1"
  - "/rcr/validate_item_2"
  - "/rcr/validate_item_3"
context_isolation: deep_copy
next: ["/rcr/merge_validations"]

## /rcr/validate_item_1
type: dsl_call
llm_tool: gpt-4
output_keys: ["validation_1"]
prompt: |
  Review 反馈 #1:

  {{parsed_feedback[0]}}

  请技术验证：
  1. 这个问题是否真实存在？
  2. 如果修改，会引入其他问题吗？
  3. 是否有理由不完全采纳这条反馈？

  输出:
  {
    "agreed": true/false,
    "reasoning": "验证理由",
    "proposed_response": "如果不同意，如何回应reviewer"
  }
next: ["/rcr/end_item_1"]

## /rcr/end_item_1
type: end

## /rcr/validate_item_2
type: dsl_call
llm_tool: gpt-4
output_keys: ["validation_2"]
prompt: |
  Review 反馈 #2:

  {{parsed_feedback[1]}}

  请技术验证：
  1. 这个问题是否真实存在？
  2. 如果修改，会引入其他问题吗？
  3. 是否有理由不完全采纳这条反馈？

  输出:
  {
    "agreed": true/false,
    "reasoning": "验证理由",
    "proposed_response": "如果不同意，如何回应reviewer"
  }
next: ["/rcr/end_item_2"]

## /rcr/end_item_2
type: end

## /rcr/validate_item_3
type: dsl_call
llm_tool: gpt-4
output_keys: ["validation_3"]
prompt: |
  Review 反馈 #3:

  {{parsed_feedback[2]}}

  请技术验证：
  1. 这个问题是否真实存在？
  2. 如果修改，会引入其他问题吗？
  3. 是否有理由不完全采纳这条反馈？

  输出:
  {
    "agreed": true/false,
    "reasoning": "验证理由",
    "proposed_response": "如果不同意，如何回应reviewer"
  }
next: ["/rcr/end_item_3"]

## /rcr/end_item_3
type: end

## /rcr/merge_validations
type: dsl_call
llm_tool: gpt-4
output_keys: ["response_plan"]
prompt: |
  解析的反馈:
  {{parsed_feedback}}

  验证结果:
  - Item 1: {{validation_1}}
  - Item 2: {{validation_2}}
  - Item 3: {{validation_3}}

  汇总：
  1. 哪些反馈同意修改
  2. 哪些反馈需要与 reviewer 讨论
  3. 如何与 reviewer 沟通

  输出响应计划。
next: ["/rcr/classify_actions"]

## /rcr/classify_actions
type: dsl_call
llm_tool: gpt-4
output_keys: ["action_plan"]
prompt: |
  Review 反馈: {{review_feedback}}
  响应计划: {{response_plan}}

  将反馈分类为：
  - agree_and_fix: 同意，直接修改
  - agree_but_different_approach: 同意，但用不同方式实现
  - disagree_with_explanation: 不同意，需要解释
  - need_clarification: 需要进一步澄清
  - out_of_scope: 超出本次 PR scope

  为每条反馈指定操作类型。
next: ["/rcr/init_tracker"]

## /rcr/init_tracker
type: state
operation: write
state_key: "rcr.{{session_id}}.tracker"
value_template: |
  {
    "feedback": {{parsed_feedback}},
    "validations": [{{validation_1}}, {{validation_2}}, {{validation_3}}],
    "actions": {{action_plan}},
    "status": "in_progress",
    "agreed_count": 0,
    "disagreed_count": 0
  }
next: ["/rcr/route_to_actions"]

## /rcr/route_to_actions
type: switch
input: "{{action_plan[0].action}}"
cases:
  agree_and_fix: "/rcr/apply_fix"
  agree_but_different: "/rcr/apply_different"
  disagree: "/rcr/discuss_disagreement"
  clarify: "/rcr/ask_clarification"
  out_of_scope: "/rcr/explain_scope"
default: "/rcr/continue_next"

## /rcr/apply_fix
type: tool_call
tool_name: bash
arguments:
  command: "echo 'Apply fix for: {{action_plan[0].comment}}' && git diff {{action_plan[0].file}} 2>/dev/null || echo 'No diff available'"
  timeout: "10"
output_keys: ["fix_result"]
next: ["/rcr/mark_done"]

## /rcr/apply_different
type: tool_call
tool_name: bash
arguments:
  command: "echo 'Apply different approach for: {{action_plan[0].comment}}'"
  timeout: "10"
output_keys: ["different_result"]
next: ["/rcr/mark_done"]

## /rcr/discuss_disagreement
type: user_input
prompt: |
  你选择不采纳以下反馈：

  {{action_plan[0].comment}}

  请提供你的技术理由，assistant 将帮你生成回应。
input_variable: disagreement_reason
input_type: multiline
next: ["/rcr/mark_discussion"]

## /rcr/mark_discussion
type: state
operation: merge
state_key: "rcr.{{session_id}}.tracker"
value_template: |
  {
    "disagreed_count": {{disagreed_count}} + 1
  }
next: ["/rcr/continue_next"]

## /rcr/ask_clarification
type: user_input
prompt: |
  关于以下反馈需要澄清：

  {{action_plan[0].comment}}

  请描述你需要澄清的问题。
input_variable: clarification_question
input_type: multiline
next: ["/rcr/mark_clarification"]

## /rcr/mark_clarification
type: state
operation: merge
state_key: "rcr.{{session_id}}.tracker"
value_template: |
  {
    "clarification_needed": true
  }
next: ["/rcr/continue_next"]

## /rcr/explain_scope
type: dsl_call
llm_tool: gpt-4
output_keys: ["scope_response"]
prompt: |
  以下反馈被标记为超出 scope：

  {{action_plan[0].comment}}

  生成礼貌的解释，说明这个反馈超出本次 PR 范围，建议作为 follow-up 处理。
next: ["/rcr/mark_scope"]

## /rcr/mark_scope
type: state
operation: merge
state_key: "rcr.{{session_id}}.tracker"
value_template: |
  {
    "out_of_scope_count": {{out_of_scope_count|default:0}} + 1
  }
next: ["/rcr/continue_next"]

## /rcr/mark_done
type: state
operation: merge
state_key: "rcr.{{session_id}}.tracker"
value_template: |
  {
    "agreed_count": {{agreed_count}} + 1
  }
next: ["/rcr/continue_next"]

## /rcr/continue_next
type: dsl_call
llm_tool: gpt-4
output_keys: ["next_action"]
prompt: |
  当前处理到第 {{loop.index|default:1}} 条反馈。
  还有多少条反馈未处理？

  判断是否还有更多反馈需要处理。
next: ["/rcr/route_to_actions"]

## /rcr/generate_summary
type: dsl_call
llm_tool: gpt-4
output_keys: ["summary"]
prompt: |
  Review 响应汇总:

  反馈: {{parsed_feedback}}
  验证结果: [{{validation_1}}, {{validation_2}}, {{validation_3}}]
  行动计划: {{action_plan}}

  生成最终的 review 响应总结，包含：
  1. 已同意并修改的内容
  2. 需要讨论的内容及理由
  3. 超出 scope 的内容
  4. 需要的澄清
next: ["/rcr/present_summary"]

## /rcr/present_summary
type: user_input
prompt: |
  Review 响应总结:

  {{summary}}

  请审核这个响应计划，确认后 assistant 将：
  1. 应用所有同意的修改
  2. 生成与 reviewer 沟通的消息
input_variable: plan_confirmed
input_type: confirm
next: ["/rcr/route_confirmation"]

## /rcr/route_confirmation
type: assert
condition: "{{plan_confirmed}} == true"
on_failure: "/rcr/revise_plan"
next: ["/rcr/apply_all_agreed"]

## /rcr/revise_plan
type: dsl_call
llm_tool: gpt-4
output_keys: ["revised_plan"]
prompt: |
  用户对响应计划有修改意见。

  请基于反馈更新响应计划。
next: ["/rcr/update_and_present"]

## /rcr/update_and_present
type: state
operation: merge
state_key: "rcr.{{session_id}}.tracker"
value_template: |
  {
    "actions": {{revised_plan}}
  }
next: ["/rcr/present_summary"]

## /rcr/apply_all_agreed
type: tool_call
tool_name: bash
arguments:
  command: "echo 'Applying all agreed changes...' && git status"
  timeout: "10"
output_keys: ["apply_result"]
next: ["/rcr/generate_response_message"]

## /rcr/generate_response_message
type: dsl_call
llm_tool: gpt-4
output_keys: ["response_message"]
prompt: |
  已完成的修改: {{apply_result}}

  生成给 reviewer 的回复消息，包含：
  1. 感谢 reviewer 的反馈
  2. 已做的修改
  3. 不同意的理由（如有）
  4. 超出 scope 的说明（如有）
  5. 需要的澄清（如有）
next: ["/rcr/finalize"]

## /rcr/finalize
type: tool_call
tool_name: write_file
arguments:
  path: "docs/reviews/{{timestamp}}-review-response.md"
  content: |
    # Code Review 响应报告

    ## 审查反馈
    {{review_feedback}}

    ## 响应摘要
    {{summary}}

    ## 回复消息
    {{response_message}}

    ## 后续行动
    - 已修改: {{agreed_count}} 项
    - 需讨论: {{disagreed_count}} 项
    - 需澄清: {{clarification_needed|default:false}}
    - 超出 scope: {{out_of_scope_count|default:0}} 项
output_keys: ["response_doc"]
next: ["/rcr/end"]

## /rcr/end
type: end

---

## /ideal_extension
type: comment
comment: |
  ## 理想 DSL 扩展：receiving_code_review 技能

  ### 1. review_response 节点
  # Review 响应节点
  type: review_response
  input:
    feedback: review_feedback
    code_context: changed_files
  output:
    validated: validated_issues
    response_plan: action_plan
  validation:
    agree_threshold: 0.7
    require_explanation_on_disagree: true

  ### 2. feedback_validation 节点
  # 验证每条反馈的有效性
  type: feedback_validation
  feedback_item: "{{feedback_item}}"
  validate_against:
    - code: "{{source_code}}"
    - tests: "{{test_files}}"
    - constraints: "{{project_constraints}}"
  output:
    agreed: boolean
    reasoning: string
    alternative_approach: string (optional)

  ### 3. dispute_resolution 节点
  # 争议解决机制
  type: dispute_resolution
  disputed_feedback: "{{feedback_item}}"
  author_position: "{{author_argument}}"
  resolution_mode: technical_review|community_consensus|defer_to_reviewer
  output:
    resolution: accepted|rejected|deferred
    explanation: string

  ### 4. skill_invoke（调用审查技能）
  type: skill_invoke
  skill: "receiving_code_review"
  input:
    feedback: "{{review_comments}}"
    changes: "{{code_diff}}"
  output:
    response_plan: action_plan
    response_message: reply_to_reviewer
