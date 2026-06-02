# Skill: brainstorming

**分类**: 轴1-流程/方法论
**触发词**: "我想实现...", "我要添加...", "创建一个...", "设计一下..."

## When to Use

在开始任何创意工作（创建功能、构建组件、添加行为或修改现有行为）之前，**必须**使用此技能。

## What It Does

作为思考伙伴，帮助用户：
1. 探索真实意图（而非表面需求）
2. 澄清模糊点
3. 发现隐藏约束
4. 评估多种方案
5. 形成完整设计方案

## How It Works

```
用户提出想法
    ↓
探索问题空间（提问、挑战假设、重构问题）
    ↓
调查代码库（如相关）
    ↓
比较方案（建立对比表、权衡取舍）
    ↓
提出路径建议（如被要求）
    ↓
形成设计文档
```

## Core Principles

- **好奇而非指令** — 自然提问，不走脚本
- **开放线程** — 提出多个有趣方向，让用户跟随共鸣
- **可视化** — 自由使用 ASCII 图表
- **适应性强** — 跟随有趣线索，适时转向
- **耐心** — 不急于下结论，让问题形态自然浮现
- **务实** — 在真正相关时调查代码库

## Hard Gate

在呈现设计并获得用户批准之前，**不得**：
- 调用任何实现技能
- 编写任何代码
- 搭架任何项目
- 采取任何实现行动

## AgenticDSL Example

> 2026-06-03 更新：原 `axis1_process/brainstorming.agent.md` 已被移除。
> 当前 brainstorming 技能未提供 AgenticDSL 实现示例，仅作为 SKILL.md 标准格式的参考。
> 完整的 AgenticDSL 工作流模式参见 `examples/skill_porting/ideal_dsl/03_workflow_skill.md`。

## Ideal DSL Extension

**参考**: `../../ideal_dsl/03_workflow_skill.md`

流程类技能的理想 DSL 扩展提案：
- `type: skill_invoke` — 原生技能调用节点
- `skill_compose` — 技能链式组合
- 标准 start → explore → clarify → propose → design → end 模式
