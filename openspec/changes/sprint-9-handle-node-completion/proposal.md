## Why

Sprint 9 step 1 已 ship 3 个 commit (`40008a5` `ce4358b` `bd936af`) — NodeResult 类型 + handle_node_completion stub + spec.md 修正 + handle_node_completion 完整函数体。但这些 commit 在 ship 时未创建 backing OpenSpec change,违反项目治理史(AGENTS.md 显示每个 sprint 必须配 change)。本次回填仅为治理一致性,**无新代码变更**,仅 spec 跟踪。

## What Changes

- 添加 `2026-06-24-sprint-9-handle-node-completion` change 目录,引用已 ship 的 3 commit
- 无新代码 / 无 ADR 变更 / 无 spec 实质内容变更
- 任务全部 [x](回填),ship 后立即 archive

## Capabilities

无(本 change 是治理回填,无 spec-level 行为变化)。

## Impact

**修改文件**: 无(本 change 不修改代码,仅添加 proposal.md + tasks.md)
**API 稳定性**: 无影响
**依赖变更**: 无
**测试影响**: 无
**风险域**: 🟢 低(纯治理回填)
