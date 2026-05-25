# Skill: openspec_apply

**分类**: 轴5-项目专用（仅限 HydraForge/AgenticDSL 项目）
**触发词**: "implement", "apply change", "开始实现", "execute plan"

## When to Use

当存在已批准的 OpenSpec change，需要开始实现时激活此技能。

## What It Does

从 OpenSpec change 执行实现：
1. **验证** — 确认 change 已批准
2. **分解** — 将任务分解为可执行单元
3. **派发** — 并行执行独立任务
4. **验证** — 确保实现符合规格
5. **归档** — 完成后的归档流程

## 与其他技能的关系

```
openspec-propose → openspec-apply → openspec-archive
     ↑                  ↓
     └───── (可能需要回退到 propose)
```

## Hard Gate

在 `openspec-propose` 阶段完成并获得批准之前，**不得**激活此技能。

## AgenticDSL Example

**对应文件**: `../../agenticdsl/axis5_project/openspec_apply.agent.md`

该文件展示了如何用 AgenticDSL 实现 OpenSpec apply 工作流，包含：
- `state` — 维护 change 执行上下文
- `fork` — 并行执行独立任务
- `dsl_call` — LLM 辅助实现
- `verification-before-completion` 集成

## Ideal DSL Extension

**参考**: `../../ideal_dsl/05_review_skill.md`

项目专用技能结合审查模式：
- `type: openspec_execute` — OpenSpec 执行节点
- `type: spec_compliance` — 规格合规验证
