# Skill: openspec_propose

**分类**: 轴5-项目专用（仅限 HydraForge/AgenticDSL 项目）
**触发词**: "propose", "提议", "我想改变", "new feature", "improvement"

## When to Use

当用户想描述他们想构建的东西，并获得包含设计、规格和任务的完整提案时激活此技能。

## What It Does

一步生成完整的 OpenSpec change 提案：
1. **探索** — 理解用户真实意图
2. **设计** — 创建详细设计方案
3. **规格** — 定义技术规格
4. **任务** — 分解为可执行任务

## OpenSpec Change 结构

```
change/
├── proposal.md      # 提议书
├── design.md        # 详细设计
├── spec.md          # 技术规格
└── tasks/          # 任务分解
    ├── task-001.md
    └── task-002.md
```

## When NOT to Use

- 如果用户只想讨论想法 → 使用 `openspec-explore`
- 如果用户只想做小改动 → 直接修改代码
- 如果用户已经有完整设计 → 使用 `openspec-apply-change`

## AgenticDSL Example

**对应文件**: `../../agenticdsl/axis5_project/openspec_propose.agent.md`

该文件展示了如何用 AgenticDSL 实现 OpenSpec propose 工作流，包含：
- `dsl_call` — 生成 proposal/design/spec
- `state` — 维护 change 状态
- `tool_call` — 写文件创建 change 目录

## Ideal DSL Extension

**参考**: `../../ideal_dsl/04_domain_skill.md`

项目专用技能可以受益于：
- `type: openspec_change` — 原生 OpenSpec change 节点
- `type: task_decompose` — 任务分解节点
