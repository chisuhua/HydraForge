### AgenticDSL '/superpowers/dispatching_parallel_agents'

# Dispatching Parallel Agents — AgenticDSL 实现

> 对应 Superpowers `dispatching-parallel-agents` 技能
> 核心：多个独立问题域 → 并行派发 → 汇总结果
> 使用 Fork + 独立分支实现真并行

## /__meta__
execution_budget:
  max_llm_calls: 30
  max_tool_calls: 30
  max_total_nodes: 50

---

## /dpa/start
type: start
next: ["/dpa/collect_failures"]

## /dpa/collect_failures
type: user_input
prompt: |
  描述需要并行调查的多个问题（每行一个）:
  
  例如:
  - test_a: 用户认证测试失败
  - test_b: 数据库连接测试超时
  - test_c: API 响应格式错误
input_variable: failure_input
input_type: multiline
next: ["/dpa/parse_problems"]

## /dpa/parse_problems
type: dsl_call
llm_tool: gpt-4
output_keys: ["problems"]
prompt: |
  将用户输入解析为独立问题域:
  
  {{failure_input}}
  
  输出 JSON 数组:
  [
    {
      "id": "domain_a",
      "name": "问题A",
      "description": "描述",
      "files": ["相关文件路径"],
      "independent": true
    }
  ]
  
  标记哪些是相互独立的（可并行调查）。
next: ["/dpa/classify_domains"]

## /dpa/classify_domains
type: dsl_call
llm_tool: gpt-4
output_keys: ["classified_domains"]
prompt: |
  分析问题域，识别:
  
  {{problems}}
  
  1. 哪些问题完全独立（可并行）
  2. 哪些问题相关（需串行）
  3. 分组建议
  
  输出分组方案。
next: ["/dpa/init_investigation"]

## /dpa/init_investigation
type: state
operation: write
state_key: "dpa.{{session_id}}.state"
value_template: |
  {
    "problems": {{problems}},
    "classification": {{classified_domains}},
    "results": {},
    "start_time": "{{timestamp}}"
  }
next: ["/dpa/parallel_investigate"]

## /dpa/parallel_investigate — 并行派发所有独立调查
type: fork
branches:
  - "/dpa/investigate_1"
  - "/dpa/investigate_2"
  - "/dpa/investigate_3"
  - "/dpa/investigate_4"
context_isolation: deep_copy
next: ["/dpa/join_investigations"]

## /dpa/investigate_1
type: dsl_call
llm_tool: gpt-4
output_keys: ["finding_1"]
prompt: |
  独立调查问题域 1:
  
  {{classified_domains}}
  
  输出: 问题域1 的根因分析。
next: ["/dpa/fix_1"]

## /dpa/fix_1
type: dsl_call
llm_tool: gpt-4
output_keys: ["fix_1"]
prompt: |
  基于根因分析提出修复方案:
  
  问题域1 分析: {{finding_1}}
  
  输出最小化修复方案（只改问题域1，不改其他代码）。
next: ["/dpa/end_1"]

## /dpa/end_1
type: end

## /dpa/investigate_2
type: dsl_call
llm_tool: gpt-4
output_keys: ["finding_2"]
prompt: |
  独立调查问题域 2:
  
  {{classified_domains}}
  
  输出: 问题域2 的根因分析。
next: ["/dpa/fix_2"]

## /dpa/fix_2
type: dsl_call
llm_tool: gpt-4
output_keys: ["fix_2"]
prompt: |
  基于根因分析提出修复方案:
  
  问题域2 分析: {{finding_2}}
  
  输出最小化修复方案（只改问题域2）。
next: ["/dpa/end_2"]

## /dpa/end_2
type: end

## /dpa/investigate_3
type: dsl_call
llm_tool: gpt-4
output_keys: ["finding_3"]
prompt: |
  独立调查问题域 3（如果存在）:
  
  {{classified_domains}}
  
  如果问题域不足 3 个，输出 "NO_DOMAIN"。
next: ["/dpa/end_3"]

## /dpa/end_3
type: end

## /dpa/investigate_4
type: dsl_call
llm_tool: gpt-4
output_keys: ["finding_4"]
prompt: |
  独立调查问题域 4（如果存在）:
  
  {{classified_domains}}
  
  如果问题域不足 4 个，输出 "NO_DOMAIN"。
next: ["/dpa/end_4"]

## /dpa/end_4
type: end

## /dpa/join_investigations
type: join
wait_for:
  - "/dpa/end_1"
  - "/dpa/end_2"
  - "/dpa/end_3"
  - "/dpa/end_4"
merge_strategy: deep_merge
next: ["/dpa/aggregate_findings"]

## /dpa/aggregate_findings
type: state
operation: read
state_key: "dpa.{{session_id}}.state"
output_key: "investigation_state"
next: ["/dpa/synthesize"]

## /dpa/synthesize
type: dsl_call
llm_tool: gpt-4
output_keys: ["final_report"]
prompt: |
  汇总并行调查结果:
  
  问题域1: {{finding_1}} → 修复: {{fix_1}}
  问题域2: {{finding_2}} → 修复: {{fix_2}}
  问题域3: {{finding_3}}
  问题域4: {{finding_4}}
  
  输出最终综合报告:
  1. 每个问题的根因
  2. 修复方案
  3. 相互影响分析
next: ["/dpa/save_results"]

## /dpa/save_results
type: state
operation: write
state_key: "dpa.{{session_id}}.results"
value_template: |
  {
    "findings": {
      "domain_1": {"root_cause": {{finding_1}}, "fix": {{fix_1}}},
      "domain_2": {"root_cause": {{finding_2}}, "fix": {{fix_2}}}
    },
    "report": {{final_report}}
  }
next: ["/dpa/present_report"]

## /dpa/present_report
type: user_input
prompt: |
  == 并行调查完成 ==
  
  {{final_report}}
  
  是否应用修复方案？
input_variable: apply_fixes
input_type: confirm
next: ["/dpa/check_apply"]

## /dpa/check_apply
type: assert
condition: "{{apply_fixes}}"
on_failure: "/dpa/end"
next: ["/dpa/apply_parallel"]

## /dpa/apply_parallel
type: fork
branches: ["/dpa/apply_domain_1", "/dpa/apply_domain_2"]
context_isolation: deep_copy
next: ["/dpa/join_apply"]

## /dpa/apply_domain_1
type: tool_call
tool_name: bash
arguments:
  command: "echo '应用问题域1的修复: {{fix_1}}'"
output_keys: ["apply_1"]
next: ["/dpa/end_apply_1"]

## /dpa/end_apply_1
type: end

## /dpa/apply_domain_2
type: tool_call
tool_name: bash
arguments:
  command: "echo '应用问题域2的修复: {{fix_2}}'"
output_keys: ["apply_2"]
next: ["/dpa/end_apply_2"]

## /dpa/end_apply_2
type: end

## /dpa/join_apply
type: join
wait_for: ["/dpa/end_apply_1", "/dpa/end_apply_2"]
merge_strategy: last_write_wins
next: ["/dpa/verify"]

## /dpa/verify
type: tool_call
tool_name: bash
arguments:
  command: "make test"
output_keys: ["test_output"]
next: ["/dpa/report_verification"]

## /dpa/report_verification
type: user_input
prompt: |
  验证结果:
  {{test_output}}
  
  全部通过了吗？
input_variable: tests_pass
input_type: confirm
next: ["/dpa/end"]

## /dpa/end
type: end
