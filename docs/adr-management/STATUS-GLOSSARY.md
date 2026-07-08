# ADR 状态词汇表（Status Glossary）

> 本文档定义 HydraForge 项目所有 ADR 文件、状态表格、关系图中使用的状态标签。
> 创建于 2026-06-12，作为 project-organization 计划 Stage 1 / Task 2 的产出。
> 2026-06-16 移至 `docs/adr-management/`，与 `relationships.md` 同处 ADR 管理目录。
> 所有 ADR 状态字段、README 表格、SPECS-ALIGNMENT、relationships.md 必须使用本词汇表中的标签。

## 6 个标准标签

| 标签 | 英文 | 语义 | 使用场景 |
|------|------|------|----------|
| ✅ Approved | Approved | ADR 已批准 + 代码已落地 | ADR-0001, ADR-0003, ADR-0005, ADR-0008, ADR-0009, ADR-0019, ADR-0021, ADR-0022, ADR-0023, ADR-0034 |
| 🟡 Partial | Partial | 接口/部分方法已实现，框架可见 | ADR-0007, ADR-0020, ADR-0031 |
| ❌ Not Implemented | Not Implemented | ADR 已批准但代码无对应实现 | ADR-0002, ADR-0010-0018, ADR-0032 |
| ⛔ Superseded | Superseded | 被新 ADR 替代 | ADR-0006（被 ADR-0020 替代）, ADR-0036（被 ADR-0045 替代, 2026-07-08 软归档） |
| 🔍 Proposed | Proposed | 仅是提案，未到实施阶段 | ADR-0030 |
| 📋 Reserved | Reserved | 编号预留，无内容 | (无活跃 ADR，未来用于占位) |

## 已废弃词汇（迁移对照表）

下列旧词汇在本词汇表建立后**禁止使用**。本表为一次性迁移参考。

| 旧词汇 | 新标签 |
|--------|--------|
| 已批准 / Approved | ✅ Approved |
| 部分实施 / Partially Implemented | 🟡 Partial |
| 未实施 / Not Implemented | ❌ Not Implemented |
| 已废弃 / Superseded / 已替代 | ⛔ Superseded |
| 提议中 / Proposed | 🔍 Proposed |

## 维护规则

1. 新 ADR 起草时必须从这 6 个标签中选一个。
2. 任何 ADR 状态变更时，README、relationships.md、SPECS-ALIGNMENT.md 必须同步。
   - **同步方向**: From `## 状态` 字段 → STATUS-GLOSSARY 状态表 (单向)，任何 ADR 状态变更时 MUST 在同一次 commit 中同步。
3. 不再创建新的状态标签；如需扩展，先在本表添加定义再使用。