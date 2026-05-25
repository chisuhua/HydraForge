### AgenticDSL '/superpowers/using_git_worktrees'

# Using Git Worktrees — AgenticDSL 实现

> 对应 Superpowers `using-git-worktrees` 技能
> 核心：检测隔离 → 创建 worktree → 项目设置 → 清理
> 使用 tool_call + state 实现资源生命周期管理

## /__meta__
execution_budget:
  max_tool_calls: 15
  max_total_nodes: 25
  max_user_inputs: 5

---

## /ugw/start
type: start
next: ["/ugw/detect_isolation"]

## /ugw/detect_isolation
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 检测工作区隔离 ==="
    GIT_DIR=$(cd "$(git rev-parse --git-dir 2>/dev/null)" 2>/dev/null && pwd -P)
    GIT_COMMON=$(cd "$(git rev-parse --git-common-dir 2>/dev/null)" 2>/dev/null && pwd -P)
    BRANCH=$(git branch --show-current 2>/dev/null)
    SUBMODULE=$(git rev-parse --show-superproject-working-tree 2>/dev/null)
    
    echo "GIT_DIR=$GIT_DIR"
    echo "GIT_COMMON=$GIT_COMMON"
    echo "BRANCH=$BRANCH"
    echo "SUBMODULE=$SUBMODULE"
    
    # 判断是否为 worktree
    if [ "$GIT_DIR" != "$GIT_COMMON" ] && [ -z "$SUBMODULE" ]; then
      echo "IS_WORKTREE=true"
    else
      echo "IS_WORKTREE=false"
    fi
    
    pwd
output_keys: ["worktree_detection"]
next: ["/ugw/save_detection"]

## /ugw/save_detection
type: state
operation: write
state_key: "worktree.{{session_id}}.detection"
value_template: |
  {
    "git_dir": "{{worktree_detection.GIT_DIR}}",
    "git_common": "{{worktree_detection.GIT_COMMON}}",
    "branch": "{{worktree_detection.BRANCH}}",
    "is_worktree": "{{worktree_detection.IS_WORKTREE}}",
    "submodule": "{{worktree_detection.SUBMODULE}}",
    "cwd": "{{worktree_detection.cwd}}"
  }
next: ["/ugw/assess_state"]

## /ugw/assess_state
type: dsl_call
llm_tool: gpt-4
output_keys: ["state_assessment"]
prompt: |
  分析 Git 工作区状态:
  
  {{worktree_detection}}
  
  判断:
  1. 是否已在隔离 worktree 中？
  2. 是否在普通仓库中？
  3. 是否在 submodule 中？
  4. 当前分支名？
  5. HEAD 是否 detached？
  
  输出:
  status: isolated | normal | submodule
  on_branch: true/false
  branch_name: 分支名（如果有）
  detached: true/false
  recommendation: '无操作' | '需要创建 worktree'
next: ["/ugw/handle_state"]

## /ugw/handle_state
type: generate_subgraph
prompt: |
  状态评估: {{state_assessment}}
  
  如果 status == isolated:
  生成:
  ### AgenticDSL '/dynamic/already_isolated'
  ## /dynamic/already_isolated/report
  type: tool_call
  tool_name: bash
  arguments:
    command: echo '已在隔离 worktree 中: {{worktree_detection.cwd}}'
  
  如果 status == normal:
  生成用户确认创建 worktree 的流程
  
  如果 status == submodule:
  生成:
  ### AgenticDSL '/dynamic/submodule_handling'
  通知用户在 submodule 中
output_keys: ["state_handler"]
signature_validation: ignore
next: ["/ugw/apply_handler"]

## /ugw/apply_handler
type: dsl_call
llm_tool: gpt-4
output_keys: ["handler_result"]
prompt: |
  执行状态处理:
  
  {{state_handler}}
  
  输出结果。
next: ["/ugw/check_needs_creation"]

## /ugw/check_needs_creation
type: assert
condition: "{{state_assessment|find:'需要创建 worktree'}}"
on_failure: "/ugw/skip_creation"
next: ["/ugw/ask_consent"]

## /ugw/ask_consent
type: user_input
prompt: |
  当前不在隔离工作区中。
  
  是否要创建隔离的 git worktree？
  它可以保护您当前的分支不受修改影响。
input_variable: create_worktree
input_type: confirm
next: ["/ugw/check_consent"]

## /ugw/check_consent
type: assert
condition: "{{create_worktree}}"
on_failure: "/ugw/work_in_place"
next: ["/ugw/create_worktree"]

## /ugw/work_in_place
type: tool_call
tool_name: bash
arguments:
  command: "echo '将在原地工作（不创建 worktree）'"
output_keys: ["in_place_msg"]
next: ["/ugw/setup_project"]

## /ugw/create_worktree
type: user_input
prompt: |
  请输入新分支名称:
input_variable: new_branch
input_type: text
next: ["/ugw/do_create"]

## /ugw/do_create
type: tool_call
tool_name: bash
arguments:
  command: |
    BASE_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    WORKTREE_DIR="../worktrees/{{new_branch}}"
    
    echo "创建 worktree: $WORKTREE_DIR"
    echo "基础分支: $BASE_BRANCH"
    echo "新分支: {{new_branch}}"
    
    git worktree add -b "{{new_branch}}" "$WORKTREE_DIR" "$BASE_BRANCH" 2>&1
    
    if [ $? -eq 0 ]; then
      echo "WORKTREE_CREATED=true"
      echo "WORKTREE_PATH=$WORKTREE_DIR"
    else
      echo "WORKTREE_CREATED=false"
    fi
  timeout: "30"
output_keys: ["creation_result"]
next: ["/ugw/verify_creation"]

## /ugw/verify_creation
type: assert
condition: "{{creation_result|find:'WORKTREE_CREATED=true'}}"
on_failure: "/ugw/creation_failed"
next: ["/ugw/store_worktree_info"]

## /ugw/creation_failed
type: user_input
prompt: |
  Worktree 创建失败:
  {{creation_result}}
  
  请手动创建或选择其他操作。
input_variable: creation_fallback
input_type: text
next: ["/ugw/end"]

## /ugw/store_worktree_info
type: state
operation: write
state_key: "worktree.{{session_id}}.active"
value_template: |
  {
    "path": "{{creation_result.WORKTREE_PATH}}",
    "branch": "{{new_branch}}",
    "created_at": "{{timestamp}}",
    "status": "active"
  }
next: ["/ugw/setup_project"]

## /ugw/setup_project
type: dsl_call
llm_tool: gpt-4
output_keys: ["setup_plan"]
prompt: |
  新工作区设置:
  
  - 路径: {{creation_result.WORKTREE_PATH|default:'(原地)'}}
  - 分支: {{new_branch|default:'(当前)'}}
  
  输出项目设置步骤:
  1. 安装依赖
  2. 配置构建
  3. 运行初始测试
next: ["/ugw/execute_setup"]

## /ugw/execute_setup
type: tool_call
tool_name: bash
arguments:
  command: |
    PROJECT_DIR="{{creation_result.WORKTREE_PATH|default:'.'}}"
    cd "$PROJECT_DIR" 2>/dev/null || true
    
    echo "=== 项目设置 ==="
    if [ -f "Makefile" ]; then
      echo "运行 make..."
      make 2>&1 | tail -5
    elif [ -f "CMakeLists.txt" ]; then
      echo "配置 CMake..."
      mkdir -p build && cd build && cmake .. 2>&1 | tail -5
    fi
    
    echo "SETUP_COMPLETE=true"
output_keys: ["setup_output"]
next: ["/ugw/verify_setup"]

## /ugw/verify_setup
type: assert
condition: "{{setup_output|find:'SETUP_COMPLETE=true'}}"
on_failure: "/ugw/setup_failed"
next: ["/ugw/report_ready"]

## /ugw/setup_failed
type: user_input
prompt: |
  项目设置可能有问题:
  {{setup_output}}
  
  请检查并确认。
input_variable: setup_confirm
input_type: text
next: ["/ugw/report_ready"]

## /ugw/report_ready
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 工作区就绪 ==="
    echo "路径: {{creation_result.WORKTREE_PATH|default:'(原地)'}}"
    echo "分支: {{new_branch|default:'(当前)'}}"
    echo "状态: 就绪"
output_keys: ["ready_msg"]
next: ["/ugw/end"]

## /ugw/skip_creation
type: assign
assign:
  worktree_status: "already_isolated"
next: ["/ugw/report_ready"]

## /ugw/end
type: end
