### AgenticDSL '/superpowers/requesting_code_review'

# Requesting Code Review — AgenticDSL 实现

> 对应 Superpowers `requesting-code-review` 技能
> 核心：完成实现后发起代码审查，使用 Fork 并行检查多个维度

## /__meta__
execution_budget:
  max_llm_calls: 20
  max_tool_calls: 20
  max_total_nodes: 35

---

## /reqr/start
type: start
next: ["/reqr/collect_changes"]

## /reqr/collect_changes
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 变更概览 ==="
    git diff --stat HEAD 2>/dev/null || echo "NO_CHANGES"
    echo ""
    echo "=== 详细变更 ==="
    git diff HEAD 2>/dev/null | head -200
output_keys: ["changes"]
next: ["/reqr/check_has_changes"]

## /reqr/check_has_changes
type: assert
condition: "{{changes|find:'NO_CHANGES'}}"
on_failure: "/reqr/init_review"
next: ["/reqr/no_changes"]

## /reqr/no_changes
type: user_input
prompt: |
  没有检测到变更。
  
  请指定要审查的代码范围或文件:
input_variable: manual_files
input_type: text
next: ["/reqr/init_review"]

## /reqr/init_review
type: state
operation: write
state_key: "review_req.{{session_id}}.state"
value_template: |
  {
    "changes": "{{changes}}",
    "status": "in_progress",
    "dimensions": {}
  }
next: ["/reqr/parallel_review"]

## /reqr/parallel_review — 并行执行多个审查维度
type: fork
branches:
  - "/reqr/review_goal"
  - "/reqr/review_quality"
  - "/reqr/review_security"
  - "/reqr/review_test"
context_isolation: deep_copy
next: ["/reqr/join_reviews"]

## /reqr/review_goal — 维度 1: 目标与约束审查
type: dsl_call
llm_tool: gpt-4
output_keys: ["goal_review"]
prompt: |
  [审查维度 1/4: 目标与约束]
  
  变更内容:
  {{changes}}
  
  目标/需求: {{requirements|default:'未指定'}}
  
  检查:
  1. 实现是否满足原始需求？
  2. 是否过度实现（YAGNI 违规）？
  3. 是否在范围内？
  
  输出审查结果。
next: ["/reqr/save_goal"]

## /reqr/save_goal
type: state
operation: write
state_key: "review_req.{{session_id}}.goal"
value_template: |
  {{goal_review}}
next: ["/reqr/end_goal"]

## /reqr/end_goal
type: end

## /reqr/review_quality — 维度 2: 代码质量审查
type: dsl_call
llm_tool: gpt-4
output_keys: ["quality_review"]
prompt: |
  [审查维度 2/4: 代码质量]
  
  变更内容:
  {{changes}}
  
  检查:
  1. 代码是否符合项目规范？
  2. 命名是否合理？
  3. 是否有重复代码（DRY）？
  4. 错误处理是否完整？
  5. 是否有不必要的复杂性？
  6. 边界情况是否处理？
  
  输出审查结果。
next: ["/reqr/end_quality"]

## /reqr/end_quality
type: end

## /reqr/review_security — 维度 3: 安全审查
type: dsl_call
llm_tool: gpt-4
output_keys: ["security_review"]
prompt: |
  [审查维度 3/4: 安全]
  
  变更内容:
  {{changes}}
  
  检查:
  1. 是否有输入验证？
  2. 是否有注入风险？
  3. 敏感信息是否泄露？
  4. 权限检查是否完整？
  5. 是否有内存安全问题（C++）？
  
  输出审查结果。
next: ["/reqr/end_security"]

## /reqr/end_security
type: end

## /reqr/review_test — 维度 4: 测试审查
type: dsl_call
llm_tool: gpt-4
output_keys: ["test_review"]
prompt: |
  [审查维度 4/4: 测试覆盖]
  
  变更内容:
  {{changes}}
  
  检查:
  1. 是否有对应的测试？
  2. 测试是否覆盖了关键路径？
  3. 是否覆盖了边界情况？
  4. 是否覆盖了错误路径？
  5. 测试命名是否清晰？
  
  此外，运行测试并检查结果。
  
  输出审查结果。
next: ["/reqr/run_tests"]

## /reqr/run_tests
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1"
  timeout: "60"
output_keys: ["test_output"]
next: ["/reqr/end_test"]

## /reqr/end_test
type: end

## /reqr/join_reviews
type: join
wait_for:
  - "/reqr/end_goal"
  - "/reqr/end_quality"
  - "/reqr/end_security"
  - "/reqr/end_test"
merge_strategy: deep_merge
next: ["/reqr/aggregate"]

## /reqr/aggregate
type: state
operation: read
state_key: "review_req.{{session_id}}.goal"
output_key: "all_reviews"
next: ["/reqr/synthesize"]

## /reqr/synthesize
type: dsl_call
llm_tool: gpt-4
output_keys: ["final_review"]
prompt: |
  汇总所有审查维度:
  
  === 目标/约束 ===
  {{goal_review}}
  
  === 代码质量 ===
  {{quality_review}}
  
  === 安全 ===
  {{security_review}}
  
  === 测试 ===
  {{test_review}}
  测试输出: {{test_output}}
  
  输出综合审查报告:
  1. 通过项
  2. 问题项（按严重程度排序）
  3. 必须修复的阻塞项
  4. 建议项
  5. 总体评估
next: ["/reqr/check_blockers"]

## /reqr/check_blockers
type: assert
condition: "{{final_review|find:'阻塞'}}"
on_failure: "/reqr/report_clean"
next: ["/reqr/identify_blockers"]

## /reqr/identify_blockers
type: dsl_call
llm_tool: gpt-4
output_keys: ["blockers"]
prompt: |
  提取阻塞项:
  
  {{final_review}}
  
  列出所有必须修复的阻塞问题。
next: ["/reqr/present_review"]

## /reqr/present_review
type: user_input
prompt: |
  == 代码审查报告 ==
  
  {{final_review}}
  
  阻塞项:
  {{blockers|default:'无'}}
  
  是否修复阻塞项后继续？
input_variable: fix_blockers
input_type: confirm
next: ["/reqr/check_fix_blockers"]

## /reqr/check_fix_blockers
type: assert
condition: "{{fix_blockers}}"
on_failure: "/reqr/report_issues"
next: ["/reqr/generate_fixes"]

## /reqr/generate_fixes
type: dsl_call
llm_tool: gpt-4
output_keys: ["fix_actions"]
prompt: |
  生成修复阻塞项的具体方案:
  
  {{blockers}}
  
  每个修复输出:
  文件: 路径
  问题: 问题描述
  修复: 具体代码修改
next: ["/reqr/apply_fixes"]

## /reqr/apply_fixes
type: generate_subgraph
prompt: |
  {{fix_actions}}
  
  生成可执行 DSL 图逐个实施修复。
output_keys: ["fix_graph"]
signature_validation: ignore
next: ["/reqr/reverify"]

## /reqr/reverify
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1"
  timeout: "60"
output_keys: ["retest"]
next: ["/reqr/check_reverify"]

## /reqr/check_reverify
type: assert
condition: "{{retest|find:'PASSED|0 failures'}}"
on_failure: "/reqr/generate_fixes"
next: ["/reqr/report_clean"]

## /reqr/report_clean
type: user_input
prompt: |
  审查完成。
  
  {{final_review}}
  
  是否:
  1. 创建 PR
  2. 提交变更
  3. 继续修改
input_variable: review_complete_action
input_type: choice
options: ["创建 PR", "提交变更", "继续修改"]
next: ["/reqr/route_action"]

## /reqr/report_issues
type: user_input
prompt: |
  审查发现问题但未修复:
  
  {{final_review}}
  
  请确认是否仍要提交？
input_variable: force_submit
input_type: confirm
next: ["/reqr/check_force"]

## /reqr/check_force
type: assert
condition: "{{force_submit}}"
on_failure: "/reqr/generate_fixes"
next: ["/reqr/route_action"]

## /reqr/route_action
type: generate_subgraph
prompt: |
  用户选择: {{review_complete_action|default:'提交变更'}}
  
  生成对应的执行图:
  - "创建 PR": git push + gh pr create
  - "提交变更": git commit
  - "继续修改": 回到开发
output_keys: ["action_graph"]
signature_validation: ignore
next: ["/reqr/execute"]

## /reqr/execute
type: dsl_call
llm_tool: gpt-4
output_keys: ["action_result"]
prompt: |
  执行动作: {{review_complete_action|default:'提交变更'}}
  动作图: {{action_graph}}
  
  输出执行结果。
next: ["/reqr/save_final"]

## /reqr/save_final
type: state
operation: write
state_key: "review_req.{{session_id}}.final"
value_template: |
  {
    "review": {{final_review}},
    "action": "{{review_complete_action}}",
    "status": "completed"
  }
next: ["/reqr/end"]

## /reqr/end
type: end
