### AgenticDSL '/superpowers/test_driven_development'

# Test-Driven Development — AgenticDSL 实现

> 对应 Superpowers `test-driven-development` 技能
> 核心：红-绿-重构循环，每步验证

## /__meta__
execution_budget:
  max_llm_calls: 20
  max_tool_calls: 30
  max_total_nodes: 30
  max_depth: 15

---

## /tdd/start
type: start
next: ["/tdd/setup_context"]

## /tdd/setup_context
type: assign
assign:
  feature_description: "{{feature|default:'未指定功能'}}"
  test_framework: "{{test_framework|default:'catch2'}}"
  source_file: "{{source_file|default:'src/impl.cpp'}}"
  test_file: "{{test_file|default:'tests/test_impl.cpp'}}"
next: ["/tdd/phase_red"]

## /tdd/phase_red — RED: 写失败测试
type: dsl_call
llm_tool: gpt-4
output_keys: ["test_code"]
prompt: |
  == RED 阶段: 写失败测试 ==
  
  **铁律：不写实现代码，只写测试。**
  
  功能: {{feature_description}}
  测试框架: {{test_framework}}
  测试文件: {{test_file}}
  
  写一个最小化的测试，验证期望行为。
  
  要求:
  - 一个测试用例，只测一件事
  - 测试名称清晰地描述行为
  - 此时实现尚不存在，测试应该失败
  
  输出完整测试代码。
next: ["/tdd/save_test"]

## /tdd/save_test
type: tool_call
tool_name: read_file
arguments:
  path: "{{test_file}}"
output_keys: ["existing_test_content"]
next: ["/tdd/append_test"]

## /tdd/append_test
type: tool_call
tool_name: write_file
arguments:
  path: "{{test_file}}"
  content: "{{existing_test_content}}\n\n{{test_code}}"
output_keys: ["test_write_result"]
next: ["/tdd/verify_red"]

## /tdd/verify_red — 确认测试确实失败
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1"
  timeout: "60"
output_keys: ["test_output_red"]
next: ["/tdd/check_red"]

## /tdd/check_red
type: dsl_call
llm_tool: gpt-4
output_keys: ["red_verdict"]
prompt: |
  == 验证 RED 阶段 ==
  
  测试输出:
  {{test_output_red}}
  
  测试是否因为"功能未实现"而失败？
  （不是因编译错误或测试本身bug而失败）
  
  输出:
  red_pass: true/false
  reason: 原因说明
next: ["/tdd/handle_red"]

## /tdd/handle_red
type: assert
condition: "{{red_verdict|find:'red_pass: true'}}"
on_failure: "/tdd/fix_red"
next: ["/tdd/phase_green"]

## /tdd/fix_red
type: dsl_call
llm_tool: gpt-4
output_keys: ["red_fix"]
prompt: |
  RED 阶段未通过:
  
  {{red_verdict}}
  之前测试: {{test_code}}
  输出: {{test_output_red}}
  
  修复测试代码，使其正确失败。
  如果是编译问题，修复语法。
  如果测试逻辑错误，修复测试。
  
  输出修复后的测试代码。
next: ["/tdd/update_test"]

## /tdd/update_test
type: tool_call
tool_name: write_file
arguments:
  path: "{{test_file}}"
  content: "{{red_fix}}"
output_keys: ["updated"]
next: ["/tdd/verify_red"]

## /tdd/phase_green — GREEN: 实现最小代码
type: dsl_call
llm_tool: gpt-4
output_keys: ["implementation_code"]
prompt: |
  == GREEN 阶段: 实现最小代码 ==
  
  **铁律：只写刚好让测试通过的最小代码。**
  不要写多余的实现，不要提前优化。
  
  功能: {{feature_description}}
  测试: {{test_code}}
  源文件: {{source_file}}
  
  输出最小实现代码（只有通过测试需要的代码）。
  不要写额外功能。
next: ["/tdd/save_implementation"]

## /tdd/save_implementation
type: tool_call
tool_name: read_file
arguments:
  path: "{{source_file}}"
output_keys: ["existing_impl"]
next: ["/tdd/apply_implementation"]

## /tdd/apply_implementation
type: tool_call
tool_name: write_file
arguments:
  path: "{{source_file}}"
  content: "{{existing_impl}}\n\n{{implementation_code}}"
output_keys: ["impl_write"]
next: ["/tdd/verify_green"]

## /tdd/verify_green — 确认测试通过
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1"
  timeout: "60"
output_keys: ["test_output_green"]
next: ["/tdd/check_green"]

## /tdd/check_green
type: dsl_call
llm_tool: gpt-4
output_keys: ["green_verdict"]
prompt: |
  == 验证 GREEN 阶段 ==
  
  测试输出:
  {{test_output_green}}
  
  所有测试是否通过？
  
  输出:
  green_pass: true/false
  details: 详情
next: ["/tdd/handle_green"]

## /tdd/handle_green
type: assert
condition: "{{green_verdict|find:'green_pass: true'}}"
on_failure: "/tdd/fix_green"
next: ["/tdd/phase_refactor"]

## /tdd/fix_green
type: dsl_call
llm_tool: gpt-4
output_keys: ["green_fix"]
prompt: |
  GREEN 阶段测试未通过:
  
  {{green_verdict}}
  测试: {{test_code}}
  实现: {{implementation_code}}
  输出: {{test_output_green}}
  
  修复实现代码，让测试通过。
  仍然保持最小实现。
next: ["/tdd/apply_green_fix"]

## /tdd/apply_green_fix
type: tool_call
tool_name: write_file
arguments:
  path: "{{source_file}}"
  content: "{{green_fix}}"
output_keys: ["fix_applied"]
next: ["/tdd/verify_green"]

## /tdd/phase_refactor — REFACTOR: 保持测试通过
type: dsl_call
llm_tool: gpt-4
output_keys: ["refactored_code"]
prompt: |
  == REFACTOR 阶段: 重构 ==
  
  当前实现:
  {{implementation_code}}
  
  **规则: 重构后测试必须仍然通过。**
  
  改进:
  - 代码质量
  - 命名
  - 提取公共逻辑
  - 添加注释
  
  但不要改变功能。
  输出重构后的代码。
next: ["/tdd/apply_refactor"]

## /tdd/apply_refactor
type: tool_call
tool_name: write_file
arguments:
  path: "{{source_file}}"
  content: "{{refactored_code}}"
output_keys: ["refactor_write"]
next: ["/tdd/verify_refactor"]

## /tdd/verify_refactor — 确认重构后测试仍通过
type: tool_call
tool_name: bash
arguments:
  command: "make test 2>&1"
  timeout: "60"
output_keys: ["test_output_refactor"]
next: ["/tdd/check_refactor"]

## /tdd/check_refactor
type: assert
condition: "{{test_output_refactor|find:'PASSED|OK|0 failures'}}"
on_failure: "/tdd/revert_refactor"
next: ["/tdd/commiit"]

## /tdd/revert_refactor
type: dsl_call
llm_tool: gpt-4
output_keys: ["revert_msg"]
prompt: |
  重构导致测试失败! 需要回退到绿色状态。
  
  输出: 原始实现代码（重构前的版本）。
  
  教训: 重构必须保持测试通过。
next: ["/tdd/restore_green"]

## /tdd/restore_green
type: tool_call
tool_name: write_file
arguments:
  path: "{{source_file}}"
  content: "{{implementation_code}}"
output_keys: ["restored"]
next: ["/tdd/verify_green"]

## /tdd/commiit
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== TDD 循环完成 ==="
    echo "功能: {{feature_description}}"
    echo "RED: 测试已失败 → GREEN: 实现通过 → REFACTOR: 完成"
    echo "建议提交信息: 'feat: {{feature_description}}'"
output_keys: ["commit_msg"]
next: ["/tdd/ask_next"]

## /tdd/ask_next
type: user_input
prompt: |
  TDD 循环完成。
  
  {{commit_msg}}
  
  是否:
  1. 继续下一个 TDD 循环
  2. 提交当前变更
input_variable: next_action
input_type: choice
options: ["下一个循环", "提交变更"]
next: ["/tdd/route_next"]

## /tdd/route_next
type: assert
condition: "{{next_action}} == 提交变更"
on_failure: "/tdd/phase_red"
next: ["/tdd/end"]

## /tdd/end
type: end
