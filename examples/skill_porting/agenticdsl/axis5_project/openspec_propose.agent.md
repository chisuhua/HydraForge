### AgenticDSL '/taxonomy/axis5_project/openspec_propose'

# OpenSpec Propose — AgenticDSL 实现

> **轴分类**: 轴5-项目专用（仅限 HydraForge/AgenticDSL）
> **核心 DSL 特性**: dsl_call, state, tool_call, user_input
> **对应 Superpowers 技能**: openspec-propose

## /__meta__
execution_budget:
  max_llm_calls: 30
  max_tool_calls: 40
  max_total_nodes: 50

---

## /propose/start
type: start
next: ["/propose/collect_idea"]

## /propose/collect_idea
type: user_input
prompt: |
  描述你想提议的变更：

  请详细说明：
  1. 你想实现什么？
  2. 为什么需要这个变更？
  3. 你期望的结果是什么？

  例如：
  - "我想给 DSL 添加并行任务执行支持"
  - "我想优化 LLM 调用性能"
  - "我想添加一个新的工具注册机制"
input_variable: change_idea
input_type: multiline
next: ["/propose/init_change"]

## /propose/init_change
type: state
operation: write
state_key: "openspec.change.{{session_id}}"
value_template: |
  {
    "id": "{{session_id}}",
    "idea": {{change_idea}},
    "status": "proposing",
    "created_at": "{{timestamp}}"
  }
next: ["/propose/generate_proposal"]

## /propose/generate_proposal
type: dsl_call
llm_tool: gpt-4
output_keys: ["proposal"]
prompt: |
  用户想要的变更:

  {{change_idea}}

  生成 OpenSpec proposal，包含：

  1. **概述**
     - 变更名称
     - 简要描述
     - 目标和动机

  2. **详细描述**
     - 当前状态（问题是什么）
     - 期望状态（解决方案是什么）
     - 影响范围

  3. **替代方案**
     - 为什么不选择其他方案
     - 权衡取舍

  4. **成功标准**
     - 如何衡量变更成功
     - 验收条件

  输出完整的 proposal markdown。
next: ["/propose/save_proposal"]

## /propose/save_proposal
type: tool_call
tool_name: bash
arguments:
  command: "mkdir -p openspec/changes/{{session_id}}/tasks"
  timeout: "5"
output_keys: ["change_dir"]
next: ["/propose/write_proposal"]

## /propose/write_proposal
type: tool_call
tool_name: write_file
arguments:
  path: "openspec/changes/{{session_id}}/proposal.md"
  content: "{{proposal}}"
output_keys: ["proposal_path"]
next: ["/propose/generate_design"]

## /propose/generate_design
type: dsl_call
llm_tool: gpt-4
output_keys: ["design"]
prompt: |
  Proposal:
  {{proposal}}

  基于 proposal 生成详细设计文档，包含：

  1. **架构设计**
     - 新的模块/类/接口
     - 数据结构变化
     - API 设计

  2. **实现计划**
     - 需要的代码变更
     - 新增文件
     - 修改文件

  3. **依赖关系**
     - 依赖哪些组件
     - 其他组件是否受影响

  4. **风险评估**
     - 实现风险
     - 兼容性风险
     - 性能影响
next: ["/propose/write_design"]

## /propose/write_design
type: tool_call
tool_name: write_file
arguments:
  path: "openspec/changes/{{session_id}}/design.md"
  content: "{{design}}"
output_keys: ["design_path"]
next: ["/propose/generate_spec"]

## /propose/generate_spec
type: dsl_call
llm_tool: gpt-4
output_keys: ["spec"]
prompt: |
  设计:
  {{design}}

  生成技术规格文档，包含：

  1. **接口规格**
     - 公共 API 列表
     - 参数和返回值
     - 错误码

  2. **数据类型**
     - 新增数据结构
     - 格式和约束

  3. **配置项**
     - 新增配置项
     - 默认值
     - 验证规则

  4. **测试策略**
     - 单元测试
     - 集成测试
     - 边界条件
next: ["/propose/write_spec"]

## /propose/write_spec
type: tool_call
tool_name: write_file
arguments:
  path: "openspec/changes/{{session_id}}/spec.md"
  content: "{{spec}}"
output_keys: ["spec_path"]
next: ["/propose/generate_tasks"]

## /propose/generate_tasks
type: dsl_call
llm_tool: gpt-4
output_keys: ["tasks"]
prompt: |
  设计:
  {{design}}

  将实现分解为具体任务，每个任务：

  1. 任务名称
  2. 详细描述
  3. 验收标准
  4. 预计工作量
  5. 依赖任务

  生成 5-10 个任务。
next: ["/propose/write_tasks"]

## /propose/write_tasks
type: tool_call
tool_name: bash
arguments:
  command: "echo '{{tasks}}' | head -100"
  timeout: "5"
output_keys: ["tasks_content"]
next: ["/propose/save_tasks"]

## /propose/save_tasks
type: tool_call
tool_name: write_file
arguments:
  path: "openspec/changes/{{session_id}}/tasks/overview.md"
  content: "{{tasks}}"
output_keys: ["tasks_path"]
next: ["/propose/generate_manifest"]

## /propose/generate_manifest
type: tool_call
tool_name: write_file
arguments:
  path: "openspec/changes/{{session_id}}/CHANGE.yaml"
  content: |
    id: {{session_id}}
    status: proposed
    created_at: {{timestamp}}

    files:
      proposal: {{proposal_path}}
      design: {{design_path}}
      spec: {{spec_path}}
      tasks: {{tasks_path}}

    summary: |
      {{proposal|truncate:200}}
next: ["/propose/present_change"]

## /propose/present_change
type: user_input
prompt: |
  OpenSpec change 已生成！

  目录: openspec/changes/{{session_id}}/

  文件:
  - proposal.md — 提议书
  - design.md — 详细设计
  - spec.md — 技术规格
  - tasks/overview.md — 任务分解
  - CHANGE.yaml — Change 清单

  请审核这些文档，确认后可以进入实现阶段（使用 openspec-apply 技能）。
input_variable: change_approved
input_type: confirm
next: ["/propose/route_approval"]

## /propose/route_approval
type: assert
condition: "{{change_approved}} == true"
on_failure: "/propose/revise_change"
next: ["/propose/update_status"]

## /propose/revise_change
type: user_input
prompt: |
  请描述需要修改的内容：
input_variable: revision
input_type: multiline
next: ["/propose/apply_revision"]

## /propose/apply_revision
type: dsl_call
llm_tool: gpt-4
output_keys: ["revised_proposal"]
prompt: |
  原 proposal:
  {{proposal}}

  修改需求:
  {{revision}}

  更新 proposal。
next: ["/propose/overwrite_proposal"]

## /propose/overwrite_proposal
type: tool_call
tool_name: write_file
arguments:
  path: "{{proposal_path}}"
  content: "{{revised_proposal}}"
output_keys: []
next: ["/propose/present_change"]

## /propose/update_status
type: state
operation: merge
state_key: "openspec.change.{{session_id}}"
value_template: |
  {
    "status": "approved",
    "approved_at": "{{timestamp}}"
  }
next: ["/propose/end"]

## /propose/end
type: end

---

## /ideal_extension
type: comment
comment: |
  ## 理想 DSL 扩展：openspec_propose 技能

  ### 1. openspec_change 节点
  # OpenSpec change 节点
  type: openspec_change
  action: propose
  input:
    idea: "{{change_idea}}"
    context: "{{project_context}}"
  output:
    change_dir: "openspec/changes/{{session_id}}"
    status: proposed|approved|rejected
  auto_generate:
    - proposal
    - design
    - spec
    - tasks

  ### 2. task_decompose 节点
  # 任务分解节点
  type: task_decompose
  design: "{{design_document}}"
  constraints:
    max_tasks: 10
    min_task_size: 1h
    max_task_size: 8h
  output:
    tasks: [task_array]
    dependencies: dependency_graph

  ### 3. skill_invoke（调用项目技能）
  type: skill_invoke
  skill: "openspec_propose"
  input:
    idea: "{{user_idea}}"
  output:
    change_id: session_id
    files: [proposal, design, spec, tasks]
    status: approved
