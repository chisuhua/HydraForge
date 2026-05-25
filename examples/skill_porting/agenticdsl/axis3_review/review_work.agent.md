### AgenticDSL '/taxonomy/axis3_review/review_work'

# Review Work — AgenticDSL 实现

> **轴分类**: 轴3-审查/质量
> **核心 DSL 特性**: fork, join, dsl_call, assert, state
> **对应 Superpowers 技能**: review_work

## /__meta__
execution_budget:
  max_llm_calls: 50
  max_tool_calls: 60
  max_total_nodes: 80

---

## /review/start
type: start
next: ["/review/load_context"]

## /review/load_context
type: tool_call
tool_name: bash
arguments:
  command: "git diff --stat HEAD~5..HEAD 2>/dev/null || git log --oneline -10"
  timeout: "10"
output_keys: ["change_summary"]
next: ["/review/collect_artifacts"]

## /review/collect_artifacts
type: tool_call
tool_name: glob
arguments:
  pattern: "src/**/*.{cpp,h,hpp}"
output_keys: ["source_files"]
next: ["/review/init_review_state"]

## /review/init_review_state
type: state
operation: write
state_key: "review.{{session_id}}.state"
value_template: |
  {
    "change_summary": {{change_summary}},
    "source_files": {{source_files}},
    "reviews": {
      "goal_constraint": null,
      "code_quality": null,
      "security": null,
      "qa_execution": null,
      "context_mining": null
    },
    "all_passed": false
  }
next: ["/review/parallel_review"]

## /review/parallel_review
type: fork
branches:
  - "/review/review_goal_constraint"
  - "/review/review_code_quality"
  - "/review/review_security"
  - "/review/review_qa_execution"
  - "/review/review_context_mining"
context_isolation: deep_copy
next: ["/review/join_reviews"]

## /review/review_goal_constraint
type: dsl_call
llm_tool: gpt-4
output_keys: ["goal_constraint_review"]
prompt: |
  ## 审查维度 1：Goal/Constraint 验证

  变更摘要:
  {{change_summary}}

  源代码文件:
  {{source_files}}

  请验证：
  1. 实现是否满足原始需求？
  2. 是否满足项目约束（编码规范、架构约束）？
  3. 是否有未完成的 todo 或已知的 limitations？
  4. 测试覆盖是否充分？

  输出 JSON 格式:
  {
    "passed": true/false,
    "issues": ["问题列表，如果有"],
    "suggestions": ["建议列表，如果有"],
    "overall": "总结评价"
  }
next: ["/review/save_goal_constraint"]

## /review/save_goal_constraint
type: state
operation: merge
state_key: "review.{{session_id}}.state"
value_template: |
  {
    "reviews.goal_constraint": {{goal_constraint_review}}
  }
next: ["/review/end_branch_1"]

## /review/end_branch_1
type: end

## /review/review_code_quality
type: dsl_call
llm_tool: gpt-4
output_keys: ["code_quality_review"]
prompt: |
  ## 审查维度 2：Code Quality 验证

  源代码文件:
  {{source_files}}

  请验证：
  1. 代码可读性（命名、注释、结构）
  2. 设计模式是否合理
  3. 是否有重复代码可以提取
  4. 错误处理是否完善
  5. 内存管理是否安全（无泄漏、无悬挂指针）

  输出 JSON 格式:
  {
    "passed": true/false,
    "issues": ["问题列表"],
    "suggestions": ["建议列表"],
    "overall": "总结评价"
  }
next: ["/review/save_code_quality"]

## /review/save_code_quality
type: state
operation: merge
state_key: "review.{{session_id}}.state"
value_template: |
  {
    "reviews.code_quality": {{code_quality_review}}
  }
next: ["/review/end_branch_2"]

## /review/end_branch_2
type: end

## /review/review_security
type: dsl_call
llm_tool: gpt-4
output_keys: ["security_review"]
prompt: |
  ## 审查维度 3：Security 验证

  源代码文件:
  {{source_files}}

  请验证：
  1. 输入验证（所有外部输入是否经过验证）
  2. 权限控制（是否有不必要的权限提升）
  3. 敏感信息处理（密码、密钥、token）
  4. 常见漏洞（SQL injection, XSS, buffer overflow）
  5. 资源限制（是否有拒绝服务风险）

  输出 JSON 格式:
  {
    "passed": true/false,
    "issues": ["安全问题列表"],
    "suggestions": ["安全建议列表"],
    "overall": "总结评价"
  }
next: ["/review/save_security"]

## /review/save_security
type: state
operation: merge
state_key: "review.{{session_id}}.state"
value_template: |
  {
    "reviews.security": {{security_review}}
  }
next: ["/review/end_branch_3"]

## /review/end_branch_3
type: end

## /review/review_qa_execution
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1 || echo 'Test command not found'"
  timeout: "120"
output_keys: ["test_results"]
next: ["/review/save_qa_results"]

## /review/save_qa_results
type: dsl_call
llm_tool: gpt-4
output_keys: ["qa_review"]
prompt: |
  ## 审查维度 4：Hands-on QA 执行

  测试结果:
  {{test_results}}

  变更摘要:
  {{change_summary}}

  请验证：
  1. 测试是否全部通过？
  2. 是否有测试覆盖了新功能？
  3. 是否有回归测试？
  4. 边界条件是否被测试？

  输出 JSON 格式:
  {
    "passed": true/false,
    "test_coverage": "评估",
    "issues": ["问题列表"],
    "suggestions": ["建议列表"],
    "overall": "总结评价"
  }
next: ["/review/save_qa"]

## /review/save_qa
type: state
operation: merge
state_key: "review.{{session_id}}.state"
value_template: |
  {
    "reviews.qa_execution": {{qa_review}}
  }
next: ["/review/end_branch_4"]

## /review/end_branch_4
type: end

## /review/review_context_mining
type: dsl_call
llm_tool: gpt-4
output_keys: ["context_review"]
prompt: |
  ## 审查维度 5：Context Mining

  变更摘要:
  {{change_summary}}

  请从以下角度挖掘上下文：
  1. 是否有相关的历史 issue 或 PR？
  2. 是否有相关的设计文档？
  3. 是否有相关的技术债务？
  4. 是否影响其他模块？

  输出 JSON 格式:
  {
    "passed": true/false,
    "related_contexts": ["相关上下文列表"],
    "issues": ["问题列表"],
    "suggestions": ["建议列表"],
    "overall": "总结评价"
  }
next: ["/review/save_context"]

## /review/save_context
type: state
operation: merge
state_key: "review.{{session_id}}.state"
value_template: |
  {
    "reviews.context_mining": {{context_review}}
  }
next: ["/review/end_branch_5"]

## /review/end_branch_5
type: end

## /review/join_reviews
type: dsl_call
llm_tool: gpt-4
output_keys: ["joined_verdict"]
prompt: |
  ## 汇总 5 路审查结果

  1. Goal/Constraint 验证:
  {{reviews.goal_constraint}}

  2. Code Quality 验证:
  {{reviews.code_quality}}

  3. Security 验证:
  {{reviews.security}}

  4. QA Execution:
  {{reviews.qa_execution}}

  5. Context Mining:
  {{reviews.context_mining}}

  判断是否所有审查都通过。
  如果有任何一路未通过，列出所有需要修复的问题。

  输出 JSON 格式:
  {
    "all_passed": true/false,
    "failed_reviews": ["未通过的审查名称"],
    "all_issues": ["所有需要修复的问题汇总"],
    "overall_summary": "总体评价"
  }
next: ["/review/judge_reviews"]

## /review/judge_reviews
type: assert
condition: "{{joined_verdict.all_passed}} == true"
on_failure: "/review/collect_feedback"
next: ["/review/generate_report"]

## /review/collect_feedback
type: state
operation: merge
state_key: "review.{{session_id}}.state"
value_template: |
  {
    "all_passed": false,
    "failed_issues": {{joined_verdict.all_issues}}
  }
next: ["/review/present_feedback"]

## /review/present_feedback
type: user_input
prompt: |
  审查发现问题，需要修复：

  {{joined_verdict.all_issues}}

  请修复这些问题后重新提交审查。
input_variable: revision_ready
input_type: confirm
next: ["/review/route_revision"]

## /review/route_revision
type: assert
condition: "{{revision_ready}} == true"
on_failure: "/review/abort_review"
next: ["/review/parallel_review"]

## /review/abort_review
type: dsl_call
llm_tool: gpt-4
output_keys: ["abort_summary"]
prompt: |
  用户选择终止审查。

  生成审查终止总结。
next: ["/review/end"]

## /review/generate_report
type: state
operation: merge
state_key: "review.{{session_id}}.state"
value_template: |
  {
    "all_passed": true,
    "overall_summary": {{joined_verdict.overall_summary}}
  }
next: ["/review/finalize_report"]

## /review/finalize_report
type: tool_call
tool_name: write_file
arguments:
  path: "docs/reviews/{{timestamp}}-review-report.md"
  content: |
    # 审查报告

    ## 审查时间
    {{timestamp}}

    ## 变更摘要
    {{change_summary}}

    ## 审查结果

    ### 1. Goal/Constraint 验证
    **通过**: {{reviews.goal_constraint.passed}}
    {{reviews.goal_constraint.overall}}

    ### 2. Code Quality 验证
    **通过**: {{reviews.code_quality.passed}}
    {{reviews.code_quality.overall}}

    ### 3. Security 验证
    **通过**: {{reviews.security.passed}}
    {{reviews.security.overall}}

    ### 4. QA Execution
    **通过**: {{reviews.qa_execution.passed}}
    {{reviews.qa_execution.overall}}

    ### 5. Context Mining
    **通过**: {{reviews.context_mining.passed}}
    {{reviews.context_mining.overall}}

    ## 总体结论
    {{joined_verdict.overall_summary}}
output_keys: ["report_path"]
next: ["/review/end"]

## /review/end
type: end

---

## /ideal_extension
type: comment
comment: |
  ## 理想 DSL 扩展：review_work 技能

  ### 1. review_gate 节点（审查门禁）
  # 所有子审查通过才算通过
  type: review_gate
  name: "implementation_review"
  reviews:
    - type: oracle
      dimension: goal_constraint
      prompt: "验证实现满足需求..."
    - type: oracle
      dimension: code_quality
      prompt: "验证代码质量..."
    - type: oracle
      dimension: security
      prompt: "验证安全性..."
    - type: agent
      dimension: qa_execution
      tasks: ["run_tests", "verify_coverage"]
    - type: agent
      dimension: context_mining
      tasks: ["check_git_history", "check_related_docs"]
  pass_threshold: 1.0
  on_pass:
    next: "/merge/approve"
  on_fail:
    next: "/review/collect_feedback"
    output: failed_reviews

  ### 2. parallel_review 节点
  # 并行执行多个审查
  type: parallel_review
  reviews:
    - /review/review_goal_constraint
    - /review/review_code_quality
    - /review/review_security
    - /review/review_qa_execution
    - /review/review_context_mining
  isolation: deep_copy
  sync_at: "/review/join_reviews"

  ### 3. review_criteria（标准化审查标准）
  type: review_criteria
  dimension: code_quality
  criteria:
    - name: readability
      weight: 0.3
      check: "命名清晰、注释充分、结构合理"
    - name: design
      weight: 0.3
      check: "设计模式合理、模块化良好"
    - name: error_handling
      weight: 0.2
      check: "错误处理完善"
    - name: memory_safety
      weight: 0.2
      check: "无内存泄漏、无悬挂指针"
  scoring: weighted_average
  pass_threshold: 0.8

  ### 4. skill_invoke（调用审查技能）
  type: skill_invoke
  skill: "review_work"
  input:
    changes: "{{git_diff}}"
    files: "{{source_files}}"
  output:
    report: review_report
    passed: all_passed
