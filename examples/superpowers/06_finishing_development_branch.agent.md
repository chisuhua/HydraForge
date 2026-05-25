### AgenticDSL '/superpowers/finishing_development_branch'

# Finishing a Development Branch — AgenticDSL 实现

> 对应 Superpowers `finishing-a-development-branch` 技能
> 核心：验证测试 → 检测环境 → 呈现选项 → 执行选择 → 清理

## /__meta__
execution_budget:
  max_tool_calls: 20
  max_total_nodes: 25
  max_user_inputs: 5

---

## /fdb/start
type: start
next: ["/fdb/verify_tests"]

## /fdb/verify_tests
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1; echo 'EXIT_CODE='$?"
  timeout: "120"
output_keys: ["test_output"]
next: ["/fdb/check_tests"]

## /fdb/check_tests
type: tool_call
tool_name: bash
arguments:
  command: |
    result="{{test_output}}"
    exit_code=$(echo "$result" | grep "EXIT_CODE=" | cut -d= -f2)
    failures=$(echo "$result" | grep -i "FAILED" | wc -l)
    if [ "$exit_code" = "0" ] && [ "$failures" = "0" ]; then
      echo "TESTS_PASSED"
    else
      echo "TESTS_FAILED: $failures failures"
    fi
output_keys: ["test_status"]
next: ["/fdb/handle_test_result"]

## /fdb/handle_test_result
type: assert
condition: "{{test_status|find:'TESTS_PASSED'}}"
on_failure: "/fdb/test_failure"
next: ["/fdb/detect_env"]

## /fdb/test_failure
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 测试失败 ==="
    echo "{{test_output}}" | head -50
    echo ""
    echo "必须先修复测试才能继续合并/PR"
output_keys: ["failure_detail"]
next: ["/fdb/ask_fix"]

## /fdb/ask_fix
type: user_input
prompt: |
  {{failure_detail}}
  
  是否修复测试？
input_variable: fix_tests
input_type: confirm
next: ["/fdb/check_fix"]

## /fdb/check_fix
type: assert
condition: "{{fix_tests}}"
on_failure: "/fdb/end"
next: ["/fdb/verify_tests"]

## /fdb/detect_env
type: tool_call
tool_name: bash
arguments:
  command: |
    GIT_DIR=$(cd "$(git rev-parse --git-dir)" 2>/dev/null && pwd -P)
    GIT_COMMON=$(cd "$(git rev-parse --git-common-dir)" 2>/dev/null && pwd -P)
    BRANCH=$(git branch --show-current 2>/dev/null)
    SUBMODULE=$(git rev-parse --show-superproject-working-tree 2>/dev/null)
    
    echo "GIT_DIR=$GIT_DIR"
    echo "GIT_COMMON=$GIT_COMMON"
    echo "BRANCH=$BRANCH"
    echo "SUBMODULE=$SUBMODULE"
output_keys: ["env_info"]
next: ["/fdb/classify_env"]

## /fdb/classify_env
type: dsl_call
llm_tool: gpt-4
output_keys: ["env_classification"]
prompt: |
  分析 Git 环境:
  
  {{env_info}}
  
  判断:
  - 是否在 worktree 中
  - 分支名是什么
  - 是否 detached HEAD
  
  输出:
  env_type: worktree | normal | detached
  branch: 分支名
  needs_cleanup: true/false
next: ["/fdb/find_base_branch"]

## /fdb/find_base_branch
type: tool_call
tool_name: bash
arguments:
  command: |
    BASE=$(git merge-base HEAD main 2>/dev/null || git merge-base HEAD master 2>/dev/null)
    if [ -n "$BASE" ]; then
      echo "base_branch=$(git name-rev --name-only "$BASE" 2>/dev/null | sed 's/^[^~]*//' || echo 'main')"
    else
      echo "base_branch=unknown"
    fi
output_keys: ["base_info"]
next: ["/fdb/save_state"]

## /fdb/save_state
type: state
operation: write
state_key: "fdb.{{session_id}}.state"
value_template: |
  {
    "env": {{env_classification}},
    "base_branch": "{{base_info}}",
    "test_status": "passed"
  }
next: ["/fdb/present_options"]

## /fdb/present_options
type: dsl_call
llm_tool: gpt-4
output_keys: ["options_menu"]
prompt: |
  基于 Git 环境呈现选项:
  
  环境: {{env_classification}}
  基础分支: {{base_info}}
  
  如果是正常仓库或 named worktree:
  选项:
  1. Merge 回 base 分支
  2. Push 并创建 PR
  3. 保留当前分支
  4. 丢弃当前工作
  
  如果是 detached HEAD:
  选项:
  1. Push 为新分支并创建 PR
  2. 保留现状
  3. 丢弃当前工作
  
  输出格式化的菜单。
next: ["/fdb/ask_choice"]

## /fdb/ask_choice
type: user_input
prompt: |
  {{options_menu}}
  
  请选择:
input_variable: user_choice
input_type: choice
options: ["Merge", "Push + PR", "保留", "丢弃"]
next: ["/fdb/execute_choice"]

## /fdb/execute_choice
type: generate_subgraph
prompt: |
  用户选择了: {{user_choice}}
  环境: {{env_classification}}
  分支: {{env_info.BRANCH}}
  基础分支: {{base_info}}
  
  生成对应的执行图:
  
  如果 Merge:
  ### AgenticDSL '/dynamic/merge'
  包含: git checkout {{base}}, git merge {{branch}}, git push
  
  如果 Push + PR:
  ### AgenticDSL '/dynamic/pr'
  包含: git push -u origin {{branch}}, gh pr create
  
  如果 保留:
  ### AgenticDSL '/dynamic/keep'
  包含: echo "分支已保留"
  
  如果 丢弃:
  ### AgenticDSL '/dynamic/discard'
  包含: git worktree remove (如果是 worktree)
output_keys: ["action_graph"]
signature_validation: ignore
next: ["/fdb/execute_action"]

## /fdb/execute_action
type: dsl_call
llm_tool: gpt-4
output_keys: ["action_result"]
prompt: |
  执行用户选择的动作。
  
  选择: {{user_choice}}
  动作图: {{action_graph}}
  
  输出执行结果。
next: ["/fdb/cleanup"]

## /fdb/cleanup
type: dsl_call
llm_tool: gpt-4
output_keys: ["cleanup_plan"]
prompt: |
  环境: {{env_classification}}
  执行结果: {{action_result}}
  
  判断是否需要清理:
  - worktree 需要移除
  - 临时文件需要删除
  
  输出清理计划或 "NO_CLEANUP"。
next: ["/fdb/check_cleanup"]

## /fdb/check_cleanup
type: assert
condition: "{{cleanup_plan|find:'NO_CLEANUP'}}"
on_failure: "/fdb/perform_cleanup"
next: ["/fdb/update_state"]

## /fdb/perform_cleanup
type: tool_call
tool_name: bash
arguments:
  command: "{{cleanup_plan.command}}"
output_keys: ["cleanup_result"]
next: ["/fdb/update_state"]

## /fdb/update_state
type: state
operation: write
state_key: "fdb.{{session_id}}.final"
value_template: |
  {
    "action": "{{user_choice}}",
    "result": {{action_result}},
    "cleanup": "{{cleanup_plan}}",
    "status": "completed"
  }
next: ["/fdb/done_report"]

## /fdb/done_report
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 分支收尾完成 ==="
    echo "操作: {{user_choice}}"
    echo "结果: {{action_result}}"
output_keys: ["done"]
next: ["/fdb/end"]

## /fdb/end
type: end
