### AgenticDSL '/superpowers/writing_skills'

# Writing Skills — AgenticDSL 实现

> 对应 Superpowers `writing-skills` 技能
> 核心：TDD 应用于文档编写，测试场景驱动技能构建
> 使用 generate_subgraph 实现技能自举

## /__meta__
execution_budget:
  max_llm_calls: 25
  max_tool_calls: 15
  max_total_nodes: 35
  max_user_inputs: 10

---

## /ws/start
type: start
next: ["/ws/identify_need"]

## /ws/identify_need
type: user_input
prompt: |
  为什么要创建这个技能？
  
  描述您遇到的问题或需要标准化的流程:
input_variable: skill_need
input_type: multiline
next: ["/ws/analyze_need"]

## /ws/analyze_need
type: dsl_call
llm_tool: gpt-4
output_keys: ["skill_analysis"]
prompt: |
  分析技能需求:
  
  {{skill_need}}
  
  判断:
  1. 这是技能(Skill)还是配置(Config)？
     - 技能: 可复用的技术、模式、工具、参考指南
     - 非技能: 一次性解决方案、标准实践
  2. 应用范围（跨项目还是项目特定）？
  3. 是否已有类似技能？
  
  输出分析结论。
next: ["/ws/check_skill_worthy"]

## /ws/check_skill_worthy
type: assert
condition: "{{skill_analysis|find:'技能'}}"
on_failure: "/ws/not_skill"
next: ["/ws/create_test_scenario"]

## /ws/not_skill
type: user_input
prompt: |
  这似乎更适合作为项目配置而非独立技能:
  {{skill_analysis}}
  
  是否仍要创建技能？
input_variable: force_skill
input_type: confirm
next: ["/ws/check_force"]

## /ws/check_force
type: assert
condition: "{{force_skill}}"
on_failure: "/ws/end"
next: ["/ws/create_test_scenario"]

## /ws/create_test_scenario — TDD RED 阶段
type: dsl_call
llm_tool: gpt-4
output_keys: ["test_scenario"]
prompt: |
  == RED 阶段: 创建测试场景 ==
  
  技能需求: {{skill_need}}
  分析: {{skill_analysis}}
  
  创建一个测试场景（pressure scenario）:
  - 描述一个具体场景
  - 在场景中，agent 应该按特定方式行为
  
  这个场景用于验证技能的有效性:
  - 没有技能时: agent 应该犯错或遗漏关键步骤
  - 有技能时: agent 应该正确遵循流程
  
  输出测试场景描述。
next: ["/ws/baseline_test"]

## /ws/baseline_test — 验证技能不存在时的行为
type: dsl_call
llm_tool: gpt-4
output_keys: ["baseline_behavior"]
prompt: |
  模拟 agent 在*没有*技能时的行为:
  
  测试场景:
  {{test_scenario}}
  
  预测 agent 会如何响应？
  会犯什么错误？会遗漏什么步骤？
  
  输出基线行为分析。
next: ["/ws/create_skill_content"]

## /ws/create_skill_content — GREEN 阶段
type: dsl_call
llm_tool: gpt-4
output_keys: ["skill_content"]
prompt: |
  == GREEN 阶段: 编写技能文档 ==
  
  技能需求: {{skill_need}}
  测试场景: {{test_scenario}}
  
  编写一个完整的 SKILL.md。
  
  格式:
  ---
  name: <技能名>
  description: <一句话描述>
  ---
  
  # <技能名>
  
  ## 概述
  
  ## 流程
  
  ## 检查项
  
  ---
  
  要求:
  - 清晰的步骤
  - 可验证的检查项
  - 反面示例（常见错误）
  - 前置条件
next: ["/ws/save_skill"]

## /ws/save_skill
type: user_input
prompt: |
  技能草稿完成。
  
  请指定保存路径:
  （默认: .opencode/skills/<skill_name>/SKILL.md）
input_variable: skill_path
input_type: text
next: ["/ws/write_skill"]

## /ws/write_skill
type: tool_call
tool_name: write_file
arguments:
  path: "{{skill_path}}"
  content: "{{skill_content}}"
output_keys: ["saved_skill"]
next: ["/ws/save_test_scenario"]

## /ws/save_test_scenario
type: state
operation: write
state_key: "skill.{{session_id}}.test"
value_template: |
  {
    "scenario": {{test_scenario}},
    "baseline": {{baseline_behavior}},
    "skill_path": "{{skill_path}}"
  }
next: ["/ws/verify_skill"]

## /ws/verify_skill — 验证技能有效性
type: dsl_call
llm_tool: gpt-4
output_keys: ["skill_verification"]
prompt: |
  == REFACTOR 阶段: 验证技能 ==
  
  测试场景:
  {{test_scenario}}
  
  技能内容:
  {{skill_content}}
  
  模拟 agent 在*拥有*此技能后:
  1. 是否会遵循流程？
  2. 是否会避免之前的错误？
  3. 是否有遗漏的边界情况？
  4. 是否足够明确没有歧义？
  
  输出验证结果和改进建议。
next: ["/ws/apply_improvements"]

## /ws/apply_improvements
type: dsl_call
llm_tool: gpt-4
output_keys: ["improved_skill"]
prompt: |
  根据验证结果改进技能:
  
  原始: {{skill_content}}
  验证: {{skill_verification}}
  
  修复问题，堵住漏洞，保持合规性。
  输出改进后的完整 SKILL.md。
next: ["/ws/save_improved"]

## /ws/save_improved
type: tool_call
tool_name: write_file
arguments:
  path: "{{skill_path}}"
  content: "{{improved_skill}}"
output_keys: ["saved_improved"]
next: ["/ws/final_verify"]

## /ws/final_verify
type: dsl_call
llm_tool: gpt-4
output_keys: ["final_verdict"]
prompt: |
  最终验证:
  
  测试场景:
  {{test_scenario}}
  
  基线行为（无技能）:
  {{baseline_behavior}}
  
  最终技能:
  {{improved_skill}}
  
  确认技能是否:
  - 覆盖了基线行为中的错误
  - 流程清晰可执行
  - 有明确的检查项
  - 无歧义
  
  输出: "VERIFIED" 或改进建议
next: ["/ws/check_verification"]

## /ws/check_verification
type: assert
condition: "{{final_verdict|find:'VERIFIED'}}"
on_failure: "/ws/apply_improvements"
next: ["/ws/save_final_state"]

## /ws/save_final_state
type: state
operation: write
state_key: "skill.{{session_id}}.final"
value_template: |
  {
    "skill_path": "{{skill_path}}",
    "scenario": {{test_scenario}},
    "verified": true,
    "created_at": "{{timestamp}}"
  }
next: ["/ws/report_complete"]

## /ws/report_complete
type: tool_call
tool_name: bash
arguments:
  command: |
    echo "=== 技能编写完成 ==="
    echo "技能: {{skill_path}}"
    echo "状态: 已验证"
    echo ""
    echo "建议提交信息: 'feat(skill): 添加 {{skill_path}} 技能'"
output_keys: ["report"]
next: ["/ws/end"]

## /ws/end
type: end
