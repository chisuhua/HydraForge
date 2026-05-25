### AgenticDSL '/superpowers/receiving_code_review'

# Receiving Code Review — AgenticDSL 实现

> 对应 Superpowers `receiving-code-review` 技能
> 核心：收到审核意见 → 理解 → 验证 → 评估 → 回应 → 实施

## /__meta__
execution_budget:
  max_llm_calls: 10
  max_tool_calls: 10
  max_total_nodes: 20
  max_user_inputs: 5

---

## /rcr/start
type: start
next: ["/rcr/collect_feedback"]

## /rcr/collect_feedback
type: user_input
prompt: |
  请输入收到的代码审查反馈:
  
  （逐条粘贴，或用列表形式）
input_variable: review_feedback
input_type: multiline
next: ["/rcr/init_review"]

## /rcr/init_review
type: state
operation: write
state_key: "review.{{session_id}}.state"
value_template: |
  {
    "feedback": "{{review_feedback}}",
    "source": "{{review_source|default:'human_partner'}}",
    "status": "received",
    "items": []
  }
next: ["/rcr/read_feedback"]

## /rcr/read_feedback
type: dsl_call
llm_tool: gpt-4
output_keys: ["parsed_feedback"]
prompt: |
  == 步骤 1/6: 完整阅读 ==
  
  收到的反馈:
  {{review_feedback}}
  
  先完整阅读，不做任何反应。
  
  逐条解析:
  1. 每条反馈的具体要求是什么？
  2. 涉及哪些文件？
  3. 是功能问题还是代码风格问题？
  
  输出结构化的反馈列表:
  - item_1: {description, files, type, is_clear}
  - item_2: ...
  
  标记不清晰的条目。
next: ["/rcr/check_clarity"]

## /rcr/check_clarity
type: dsl_call
llm_tool: gpt-4
output_keys: ["clarity_check"]
prompt: |
  == 步骤 2/6: 理解需求 ==
  
  反馈条目:
  {{parsed_feedback}}
  
  对于每个条目，用自己的话重述要求:
  - "我理解这个要求是要...对吗？"
  
  如果任何条目不清晰，标记为 UNCLEAR。
  不要在不理解的情况下开始实施。
  
  输出:
  - 清晰的条目: 重述确认
  - 不清晰的条目: 列出具体疑问
next: ["/rcr/check_unclear"]

## /rcr/check_unclear
type: assert
condition: "{{clarity_check|find:'UNCLEAR'}}"
on_failure: "/rcr/verify_items"
next: ["/rcr/ask_clarification"]

## /rcr/ask_clarification
type: user_input
prompt: |
  以下条目不清晰，需要澄清:
  
  {{clarity_check}}
  
  请提供更多信息:
input_variable: clarification
input_type: multiline
next: ["/rcr/update_feedback"]

## /rcr/update_feedback
type: dsl_call
llm_tool: gpt-4
output_keys: ["updated_feedback"]
prompt: |
  更新反馈:
  
  原始: {{parsed_feedback}}
  澄清: {{clarification}}
  
  输出更新后的完整反馈列表。
next: ["/rcr/verify_items"]

## /rcr/verify_items — 步骤 3/6: 验证
type: dsl_call
llm_tool: gpt-4
output_keys: ["verification_check"]
prompt: |
  == 步骤 3/6: 验证 ==
  
  反馈:
  {{clarity_check|default:parsed_feedback}}
  
  对每条建议验证:
  1. 在代码库上下文中是否合理？
  2. 是否会破坏现有功能？
  3. 为什么当前的实现是这样的？
  4. 在所有平台/版本上是否都有效？
  
  输出每条建议的技术评估。
next: ["/rcr/evaluate_items"]

## /rcr/evaluate_items — 步骤 4/6: 评估
type: dsl_call
llm_tool: gpt-4
output_keys: ["evaluation"]
prompt: |
  == 步骤 4/6: 技术评估 ==
  
  反馈验证结果:
  {{verification_check}}
  
  对每条反馈做出决定:
  - ACCEPT: 技术合理，应该实施
  - REJECT: 有技术理由不采纳
  - DEFER: 需要进一步讨论
  
  对于 REJECT 的，准备技术反驳理由。
  对于 DEFER 的，说明需要什么信息。
next: ["/rcr/check_rejected"]

## /rcr/check_rejected
type: assert
condition: "{{evaluation|find:'REJECT'}}"
on_failure: "/rcr/prepare_response"
next: ["/rcr/prepare_pushback"]

## /rcr/prepare_pushback
type: dsl_call
llm_tool: gpt-4
output_keys: ["pushback"]
prompt: |
  准备技术反驳:
  
  要反驳的条目:
  {{evaluation}}
  
  格式:
  - 指出问题: ...
  - 技术理由: ...
  - 建议替代方案: ...
  
  不要使用:
  - "你说得对"（表演性同意）
  - "好主意"
  - 情绪化回应
  
  直接陈述技术事实。
next: ["/rcr/prepare_response"]

## /rcr/prepare_response — 步骤 5/6: 回应
type: dsl_call
llm_tool: gpt-4
output_keys: ["response"]
prompt: |
  == 步骤 5/6: 准备回应 ==
  
  评估结果: {{evaluation}}
  反驳（如果有）: {{pushback|default:'无'}}
  
  对每条反馈准备回应:
  
  对于 ACCEPT:
  - 技术确认（不是表演性同意）
  - 直接开始说明实施计划
  
  对于 REJECT:
  - 技术理由
  - 建议替代方案
  
  对于 DEFER:
  - 需要的信息
  - 后续步骤
next: ["/rcr/present_response"]

## /rcr/present_response
type: user_input
prompt: |
  以下是审核反馈的处理方案:
  
  {{response}}
  
  请确认是否可以按此方案实施？
input_variable: approve_response
input_type: confirm
next: ["/rcr/check_approval"]

## /rcr/check_approval
type: assert
condition: "{{approve_response}}"
on_failure: "/rcr/adjust"
next: ["/rcr/implement_fixes"]

## /rcr/adjust
type: user_input
prompt: "请说明需要如何调整："
input_variable: adjustments
input_type: multiline
next: ["/rcr/prepare_response"]

## /rcr/implement_fixes — 步骤 6/6: 逐个实施
type: dsl_call
llm_tool: gpt-4
output_keys: ["implementation_plan"]
prompt: |
  == 步骤 6/6: 实施 ==
  
  获批准的修改:
  {{response}}
  
  逐个实施，每项独立测试。
  
  输出实施顺序和每项的具体修改。
next: ["/rcr/execute_fixes"]

## /rcr/execute_fixes
type: generate_subgraph
prompt: |
  按计划实施修复:
  
  {{implementation_plan}}
  
  生成可执行的 DSL 图，每个修复作为独立步骤。
output_keys: ["fix_graph"]
signature_validation: ignore
next: ["/rcr/verify_fixes"]

## /rcr/verify_fixes
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1"
  timeout: "60"
output_keys: ["test_result"]
next: ["/rcr/check_fixes"]

## /rcr/check_fixes
type: assert
condition: "{{test_result|find:'PASSED|0 failures'}}"
on_failure: "/rcr/rollback"
next: ["/rcr/final_report"]

## /rcr/rollback
type: dsl_call
llm_tool: gpt-4
output_keys: ["rollback_plan"]
prompt: |
  修改导致测试失败:
  
  测试: {{test_result}}
  修改: {{implementation_plan}}
  
  逐个排查哪些修改导致失败，输出回退方案。
next: ["/rcr/end"]

## /rcr/final_report
type: state
operation: write
state_key: "review.{{session_id}}.final"
value_template: |
  {
    "feedback": "{{review_feedback}}",
    "accepted": {{evaluation}},
    "implemented": true,
    "tests_pass": true
  }
next: ["/rcr/report"]

## /rcr/report
type: tool_call
tool_name: bash
arguments:
  command: "echo '审核反馈处理完毕，所有修改已实施，测试通过。'"
output_keys: ["done"]
next: ["/rcr/end"]

## /rcr/end
type: end
