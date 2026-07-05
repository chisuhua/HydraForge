# ADR Plugin 化候选清单

> 本目录存放**计划通过 Plugin（域插件）实现**的 ADR。
> 与 `docs/adr/` 根目录的 ADR 共用同一编号空间、状态词汇与工具链（参见 [docs/adr-management/STATUS-GLOSSARY.md](../adr-management/STATUS-GLOSSARY.md)）。

## 目录语义

| 维度 | 说明 |
|------|------|
| **与 `docs/adr/` 根目录的关系** | 编号连续、不重复（根目录 ADR-0021 PDK 设计 / ADR-0022 插件加载 → 本目录 ADR-0034 模型路由）。两者**一起**被 `tools/adr_lint.py` 与 `tools/adr_relationships.py` 扫描。 |
| **与 `docs/archive/adr/` 的关系** | 本目录是**活跃 ADR** 区域，不是归档区。归档的 ADR 永不进入本目录。 |
| **与 `docs/proposals/` 的关系** | `proposals/` 描述 AgenticDSL 语言演进方向；本目录描述**引擎能力的 plugin 化实施候选**。两者范围不重叠。 |
| **Plugin 候选标记** | 本目录下所有 ADR 的 frontmatter 必须包含 `plugin-candidate: true`。该标记为 ADR 工具链识别 plugin 化范围的唯一依据。 |

## 当前成员

| ADR | 议题 | 状态 | 计划实施方式 |
|-----|------|------|-------------|
| [adr-0034-model-router.md](./adr-0034-model-router.md) | IModelRouter 模型路由接口 | ✅ Approved (C7 ship, 2026-07-02) | 通过 Plugin SDK 加载第三方模型路由策略（参考 [ADR-0021](../adr-0021-pdk-design.md) PDK 设计） |

> ADR-0021 / ADR-0022 是 plugin 框架本身的 ADR（活跃在 `docs/adr/` 根目录），不属于本目录。
> 待 PDK 落地后，新的"plugin 化实施"ADR 优先放本目录，根目录保留纯架构 ADR。

## 维护规则

1. **加入条件**：ADR 内容中明确说明"通过 Plugin 实现"或"由第三方 Plugin 提供具体策略"。
2. **frontmatter 必须**：`plugin-candidate: true` + 标准 6 标签状态之一（参见 `STATUS-GLOSSARY.md`）。
3. **状态变更**：与根目录 ADR 同样规则——README、relationships.md、STATUS-GLOSSARY 同步更新。
4. **离开条件**：
   - 若 ADR 决定**不**通过 Plugin 实现 → `git mv` 移回 `docs/adr/` 根目录，并删除 `plugin-candidate` 标记。
   - 若 ADR 被废弃 → `git mv` 移至 `docs/archive/adr/`，并删除 `plugin-candidate` 标记。
5. **禁止重复归档**：本目录 ADR 不得同时存在于 `docs/archive/adr/`（避免双源真相）。

## 创建时间

2026-06-16，作为 project-organization 计划的 plugin 目录预留产出。
