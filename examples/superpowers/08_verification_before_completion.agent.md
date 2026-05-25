### AgenticDSL '/superpowers/verification_before_completion'

# Verification Before Completion — AgenticDSL 实现

> 对应 Superpowers `verification-before-completion` 技能
> 核心：证据先行，未验证前不声称完成
> 这是所有技能的安全门禁

## /__meta__
execution_budget:
  max_tool_calls: 15
  max_total_nodes: 20

---

## /vbc/start
type: start
next: ["/vbc/identify_claims"]

## /vbc/identify_claims
type: dsl_call
llm_tool: gpt-4
output_keys: ["claims_analysis"]
prompt: |
  分析当前工作状态，识别需要验证的所有声明。
  
  当前上下文: {{task_context|default:'无特定上下文'}}
  变更文件: {{changed_files|default:'未知'}}
  
  列出需要验证的项目:
  1. 测试是否通过？
  2. 构建是否成功？
  3. Lint 是否干净？
  4. 是否有未提交的变更？
  5. 功能是否按预期工作？
  
  输出 VerificationClaim 列表，每个包含:
  - claim: 声明内容
  - verification_command: 验证命令
  - success_criteria: 成功标准
next: ["/vbc/init_verification"]

## /vbc/init_verification
type: state
operation: write
state_key: "vbc.{{session_id}}.state"
value_template: |
  {
    "claims": {{claims_analysis}},
    "results": {},
    "all_passed": false,
    "started_at": "{{timestamp}}"
  }
next: ["/vbc/run_verifications"]

## /vbc/run_verifications — 逐一执行验证
type: fork
branches:
  - "/vbc/verify_tests"
  - "/vbc/verify_build"
  - "/vbc/verify_lint"
  - "/vbc/verify_git"
context_isolation: deep_copy
next: ["/vbc/join_verifications"]

## /vbc/verify_tests
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1; echo 'EXIT_CODE='$?"
  timeout: "120"
output_keys: ["test_output"]
next: ["/vbc/analyze_test"]

## /vbc/analyze_test
type: dsl_call
llm_tool: gpt-4
output_keys: ["test_verdict"]
prompt: |
  分析测试结果:
  
  {{test_output}}
  
  判断:
  - 全部通过 (PASS)
  - 有失败 (FAIL)
  - 部分通过 (PARTIAL)
  
  输出格式:
  verdict: PASS | FAIL | PARTIAL
  details: 详细分析
  failures: 失败列表（如果有）
next: ["/vbc/store_test_result"]

## /vbc/store_test_result
type: state
operation: write
state_key: "vbc.{{session_id}}.test_result"
value_template: |
  {
    "category": "tests",
    "output": "{{test_output}}",
    "verdict": {{test_verdict}}
  }
next: ["/vbc/end_test"]

## /vbc/end_test
type: end

## /vbc/verify_build
type: tool_call
tool_name: bash
arguments:
  command: "make 2>&1; echo 'BUILD_EXIT='$?"
  timeout: "120"
output_keys: ["build_output"]
next: ["/vbc/analyze_build"]

## /vbc/analyze_build
type: dsl_call
llm_tool: gpt-4
output_keys: ["build_verdict"]
prompt: |
  分析构建结果:
  
  {{build_output}}
  
  判断:
  - 构建成功 (PASS)
  - 构建失败 (FAIL)
  
  输出格式:
  verdict: PASS | FAIL
  details: 详细分析
next: ["/vbc/end_build"]

## /vbc/end_build
type: end

## /vbc/verify_lint
type: tool_call
tool_name: bash
arguments:
  command: |
    (clang-tidy --version 2>/dev/null && clang-tidy src/**/*.cpp 2>&1) || \
    (echo "Lint检查不可用" && exit 0)
output_keys: ["lint_output"]
next: ["/vbc/analyze_lint"]

## /vbc/analyze_lint
type: dsl_call
llm_tool: gpt-4
output_keys: ["lint_verdict"]
prompt: |
  分析 Lint 结果:
  
  {{lint_output}}
  
  输出:
  verdict: PASS | FAIL | NA
  issues: 问题列表
next: ["/vbc/end_lint"]

## /vbc/end_lint
type: end

## /vbc/verify_git
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== Git 状态 ==="
    git status --short 2>/dev/null || echo "NOT_A_GIT_REPO"
    echo ""
    echo "=== 未暂存的变更 ==="
    git diff --stat 2>/dev/null
    echo ""
    echo "=== 未提交的变更 ==="
    git diff --cached --stat 2>/dev/null
output_keys: ["git_output"]
next: ["/vbc/analyze_git"]

## /vbc/analyze_git
type: dsl_call
llm_tool: gpt-4
output_keys: ["git_verdict"]
prompt: |
  分析 Git 状态:
  
  {{git_output}}
  
  输出:
  clean: true/false
  uncommitted_count: 未提交文件数
  summary: 状态摘要
next: ["/vbc/end_git"]

## /vbc/end_git
type: end

## /vbc/join_verifications
type: join
wait_for:
  - "/vbc/end_test"
  - "/vbc/end_build"
  - "/vbc/end_lint"
  - "/vbc/end_git"
merge_strategy: deep_merge
next: ["/vbc/collect_results"]

## /vbc/collect_results
type: state
operation: read
state_key: "vbc.{{session_id}}.test_result"
output_key: "all_results"
next: ["/vbc/composite_verdict"]

## /vbc/composite_verdict
type: dsl_call
llm_tool: gpt-4
output_keys: ["final_verdict"]
prompt: |
  汇总所有验证结果:
  
  测试: {{test_verdict}}
  构建: {{build_verdict}}
  Lint: {{lint_verdict}}
  Git: {{git_verdict}}
  
  输出最终判定:
  - 是否全部通过
  - 未通过的项目
  - 是否可以声称完成
next: ["/vbc/check_verdict"]

## /vbc/check_verdict
type: assert
condition: "{{final_verdict|find:'全部通过|PASS'}}"
on_failure: "/vbc/report_failures"
next: ["/vbc/claim_completion"]

## /vbc/report_failures
type: user_input
prompt: |
  ⚠️ 验证未全部通过:
  
  {{final_verdict}}
  
  以下 Iron Law: **未验证通过前不能声称完成**
  
  是否要修复问题？
input_variable: fix_issues
input_type: confirm
next: ["/vbc/check_fix"]

## /vbc/check_fix
type: assert
condition: "{{fix_issues}}"
on_failure: "/vbc/end"
next: ["/vbc/run_verifications"]

## /vbc/claim_completion
type: user_input
prompt: |
  ✅ 全部验证通过!
  
  {{final_verdict}}
  
  证据链完整，可以声称完成。
  
  是否继续下一步（PR/Merge）？
input_variable: proceed
input_type: confirm
next: ["/vbc/end"]

## /vbc/end
type: end
