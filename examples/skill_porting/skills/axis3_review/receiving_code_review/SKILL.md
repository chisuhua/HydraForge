# Skill: receiving_code_review

**分类**: 轴3-审查/质量（跨轴：也属于流程）
**触发词**: "code review feedback", "review comments", "review 反馈", "收到评审意见"

## When to Use

在收到 code review 反馈后、**实施修改之前**激活此技能。

## What It Does

对 review 反馈进行技术验证，防止盲目执行：
1. **评估** — 每条反馈是否合理
2. **验证** — 反馈的问题是否真实存在
3. **优先级** — 哪些必须改，哪些可以讨论
4. **计划** — 如何响应（修改/解释/反驳）

## Core Principles

- **技术验证优先** — 不要因为 reviewer 的权威就盲目执行
- **最小化修改** — 只改反馈中明确指出的问题
- **保留上下文** — 解释为什么某些反馈不适用
- **记录决策** — 重要的 review 决策要形成文档

## Anti-Patterns (Blocked)

- ❌ 盲目执行所有 review 反馈（不验证就改）
- ❌ 忽略 review 反馈（不响应）
- ❌ 过度修改（scope creep）
- ❌ 反驳时没有技术依据

## When to Disagree

有技术依据时可以反驳 reviewer：
- 反馈基于误解
- 反馈与项目规范冲突
- 反馈的实现方式有更优解
- 反馈超出本次 PR scope

## AgenticDSL Example

**对应文件**: `../../agenticdsl/axis3_review/receiving_code_review.agent.md`

该文件展示了如何用 AgenticDSL 实现 review 反馈响应工作流，包含：
- `dsl_call` — 分析 review 反馈
- `user_input` — 与 reviewer 交互澄清
- `state` — 维护修改状态
- `assert` — 验证修改是否满足反馈

## Ideal DSL Extension

**参考**: `../../ideal_dsl/05_review_skill.md`

审查类技能的理想 DSL 扩展提案：
- `type: review_response` — review 响应节点
- `feedback_validation` — 反馈验证逻辑
- `dispute_resolution` — 争议解决机制
