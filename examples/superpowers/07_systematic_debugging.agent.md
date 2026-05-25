### AgenticDSL '/superpowers/systematic_debugging'

# Systematic Debugging — AgenticDSL 实现

> 对应 Superpowers `systematic-debugging` 技能
> 核心：根因调查 → 分析 → 修复 → 验证（四阶段）

## /__meta__
execution_budget:
  max_llm_calls: 15
  max_tool_calls: 20
  max_total_nodes: 30

---

## /dbg/start
type: start
next: ["/dbg/collect_info"]

## /dbg/collect_info
type: assign
assign:
  error_description: "{{error|default:'未知错误'}}"
  test_command: "{{test_cmd|default:'make test'}}"
  debug_phase: "root_cause"
  findings: "{}"
next: ["/dbg/phase_1_root_cause"]

## /dbg/phase_1_root_cause — 第一阶段：根因调查
type: dsl_call
llm_tool: gpt-4
output_keys: ["phase_1_output"]
prompt: |
  == 阶段 1/4: 根因调查 ==
  
  错误: {{error_description}}
  
  ### 1.1 读取错误信息
  - 解读错误信息
  - 分析堆栈跟踪
  - 注意行号和文件
  
  ### 1.2 检查最近变更
  需要检查 git diff 和最近的提交。
  
  ### 1.3 如果是多组件系统
  在每个组件边界注入诊断:
  - 记录进入组件的数据
  - 记录离开组件的数据
  - 检查环境/配置传播
  - 检查每层的状态
  
  请按以上流程输出调查结果。
  
  IMPORTANT: 只有完成这个阶段的完整调查后，才能进入修复阶段。
next: ["/dbg/check_error_context"]

## /dbg/check_error_context
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 最近 Git 变更 ==="
    git log --oneline -10 2>/dev/null || echo "无 git 历史"
    echo ""
    echo "=== 差异 ==="
    git diff HEAD~5..HEAD --stat 2>/dev/null || echo "无法获取 diff"
output_keys: ["git_context"]
next: ["/dbg/synthesize_root_cause"]

## /dbg/synthesize_root_cause
type: dsl_call
llm_tool: gpt-4
output_keys: ["root_cause_analysis"]
prompt: |
  综合分析根因:
  
  错误: {{error_description}}
  调查: {{phase_1_output}}
  Git 变更: {{git_context}}
  
  输出:
  1. 根因（精确的代码位置）
  2. 触发条件
  3. 影响范围
  4. 验证方法（如何确认根因正确）
  
  **规则：在定位到确凿根因之前，绝对不能进入修复阶段。**
next: ["/dbg/verify_root_cause"]

## /dbg/verify_root_cause
type: user_input
prompt: |
  根因分析完成:
  {{root_cause_analysis}}
  
  根因是否准确？需要补充信息吗？
input_variable: root_cause_confirmed
input_type: text
next: ["/dbg/phase_2_analyze"]

## /dbg/phase_2_analyze — 第二阶段：分析修复方案
type: dsl_call
llm_tool: gpt-4
output_keys: ["fix_design"]
prompt: |
  == 阶段 2/4: 设计修复方案 ==
  
  根因: {{root_cause_analysis}}
  用户确认: {{root_cause_confirmed}}
  
  设计修复方案:
  1. 最小化修复（只改必要代码）
  2. 考虑副作用
  3. 添加回归测试
  
  输出:
  - 修改方案
  - 需要修改的文件
  - 测试计划
  - 风险评估
next: ["/dbg/phase_3_implement"]

## /dbg/phase_3_implement — 第三阶段：实施修复
type: dsl_call
llm_tool: gpt-4
output_keys: ["implementation"]
prompt: |
  == 阶段 3/4: 实施修复 ==
  
  方案: {{fix_design}}
  
  输出具体代码修改（明确到行）。
  
  格式:
  文件: path/to/file
  旧代码: ...
  新代码: ...
next: ["/dbg/save_findings"]

## /dbg/save_findings
type: state
operation: write
state_key: "debug.{{session_id}}.findings"
value_template: |
  {
    "root_cause": {{root_cause_analysis}},
    "fix_design": {{fix_design}},
    "implementation": {{implementation}},
    "phase": "implemented"
  }
next: ["/dbg/phase_4_verify"]

## /dbg/phase_4_verify — 第四阶段：验证
type: tool_call
tool_name: bash
arguments:
  command: "{{test_command}} 2>&1"
  timeout: "120"
output_keys: ["verification_output"]
next: ["/dbg/check_verification"]

## /dbg/check_verification
type: dsl_call
llm_tool: gpt-4
output_keys: ["verdict"]
prompt: |
  == 阶段 4/4: 验证结果 ==
  
  测试输出:
  {{verification_output}}
  
  判断:
  - 修复是否解决了问题？
  - 是否有回归？
  - 需要进一步修改吗？
  
  输出:
  status: PASS | FAIL | PARTIAL
  details: 详细验证结果
next: ["/dbg/handle_verdict"]

## /dbg/handle_verdict
type: assert
condition: "{{verdict|find:'status: PASS'}}"
on_failure: "/dbg/handle_fix_failure"
next: ["/dbg/final_report"]

## /dbg/handle_fix_failure
type: dsl_call
llm_tool: gpt-4
output_keys: ["revision"]
prompt: |
  修复未通过验证。
  
  错误: {{verdict}}
  分析: {{root_cause_analysis}}
  之前方案: {{fix_design}}
  之前实现: {{implementation}}
  
  分析失败原因并输出修正方案。
next: ["/dbg/check_retry_count"]

## /dbg/check_retry_count
type: assign
assign:
  retry_count: "{{retry_count|default:0|add:1}}"
next: ["/dbg/max_retries"]

## /dbg/max_retries
type: assert
condition: "{{retry_count}} < 3"
on_failure: "/dbg/escalate"
next: ["/dbg/phase_3_implement"]

## /dbg/escalate
type: user_input
prompt: |
  经过 3 次修复尝试仍未通过验证。
  需要人工介入。
  
  最新状态: {{revision}}
  
  是否继续？
input_variable: escalate_choice
input_type: confirm
next: ["/dbg/check_escalate"]

## /dbg/check_escalate
type: assert
condition: "{{escalate_choice}}"
on_failure: "/dbg/end"
next: ["/dbg/phase_2_analyze"]

## /dbg/final_report
type: state
operation: write
state_key: "debug.{{session_id}}.final"
value_template: |
  {
    "root_cause": {{root_cause_analysis}},
    "fix": {{implementation}},
    "verification": {{verdict}},
    "status": "resolved"
  }
next: ["/dbg/report"]

## /dbg/report
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 调试完成 ==="
    echo "根因: {{root_cause_analysis.root_cause}}"
    echo "状态: 已修复"
    echo "验证: {{verdict}}"
output_keys: ["debug_report"]
next: ["/dbg/end"]

## /dbg/end
type: end
