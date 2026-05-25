# Skill: frontend_ui_workflow

**分类**: 轴4-UI/前端
**触发词**: "UI", "界面", "按钮", "组件", "CSS", "layout", "设计稿"

## When to Use

在以下场景激活此技能：
- 构建或修改 UI 组件
- 实现页面布局
- 添加动画效果
- 还原设计稿
- 前端技术选型（React/Vue/原生 HTML+CSS）

## What It Does

作为设计师出身的开发者，即使没有设计稿也能实现美观的 UI：
- 从自然语言描述生成 UI
- 应用现代 CSS 实践（Flexbox/Grid/CSS Variables）
- 响应式设计
- 动画和交互

## Core Principles

- **Design-first** — 先考虑用户体验，再实现
- **Progressive enhancement** — 基础功能优先，增强在后
- **Mobile-first** — 从小屏幕开始，逐步增强
- **Accessible** — WCAG 合规，语义化 HTML

## AgenticDSL Example

**对应文件**: `../../agenticdsl/axis4_frontend/frontend_ui_workflow.agent.md`

该文件展示了如何用 AgenticDSL 实现前端 UI 工作流，包含：
- `user_input` — 收集 UI 需求
- `dsl_call` — 生成 HTML/CSS/JS
- `tool_call` — 生成文件
- `fork` — 并行生成多组件

## Ideal DSL Extension

**参考**: `../../ideal_dsl/04_domain_skill.md`

领域/工具类（UI 专项）的理想 DSL 扩展：
- `type: ui_component` — UI 组件节点
- `type: style_sheet` — 样式表生成器
- `visual_test` 节点 — 视觉测试验证
