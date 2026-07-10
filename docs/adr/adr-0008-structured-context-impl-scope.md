# ADR-0008 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0008-structured-context.md](adr-0008-structured-context.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

**📋 Audit** (impl-scope-audit 文档, 与 docs-code-drift-audit 配套使用)

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 7/9 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `LayeredContext` | ✅ Shipped | `include/agenticdsl/types/layered_context.h` | 5-层结构化上下文核心实现 |
| `Context` | ✅ Shipped | `src/core/types/context.h` | 扁平上下文 (桥接到 LayeredContext, `to_context` / `from_context` 双向桥接) |
| `SystemLayer` | 🔁 Evolved | `include/agenticdsl/types/layered_context.h` (`LayeredContext::system`) | L1 系统层, 实现为 `LayeredContext` 的 `system` JSON 字段 (非独立类) |
| `WorkingLayer` | 🔁 Evolved | `include/agenticdsl/types/layered_context.h` (`LayeredContext::working`) | L3 工作层, 同上 |
| `MetaLayer` | 🔁 Evolved | `include/agenticdsl/types/layered_context.h` (`LayeredContext::meta`) | L5 元数据层, 同上 |
| `ArchiveEntry` | 📅 Deferred | — | 归档条目结构未实现 (同 ADR-0007); `LayeredContext.archive` (L4) 提供 JSON 槽位 |
| `ContextTurn` | 📅 Deferred | — | 对话轮次结构未实现 (同 ADR-0007) |
| `ContextMigrations` | 📅 Deferred | — | 上下文迁移工具未实现; 当前仅 v1 格式 |
| `StateTools` | 📅 Deferred | — | 状态工具集未实现; `LayeredContext::can_read` / `can_write` 提供基础 layer 级权限 |

## 分类详情

### 🔁 Evolved — Layer 类型 → LayeredContext JSON 字段

ADR-0008 原始描述可能涉及 `SystemLayer` / `WorkingLayer` / `MetaLayer` 等独立类。实际实现选择了更简洁的方案: `LayeredContext` 结构体直接包含 5 个 `nlohmann::json` 字段 (`system` / `recent` / `working` / `archive` / `meta`), 每个字段代表一层。

设计理由 (layered_context.h 注释):
- 仅使用 `nlohmann::json` 作为存储, 不引入 STL 容器
- 路径导航使用 `json::operator[]` + 点分递归
- 越界或类型不匹配返回 null JSON
- L1 `system` 只读保护通过 `navigate()` 内部 guard 实现

独立 Layer 类会增加类型系统复杂度, 当前 JSON 字段方案已满足 5-层语义划分需求。

### 📅 Deferred (4 个)

- **`ArchiveEntry` / `ContextTurn`**: 同 ADR-0007, LLM 压缩子系统未实施
- **`ContextMigrations`**: 上下文格式迁移工具 YAGNI (当前仅 v1)
- **`StateTools`**: 高层状态工具集 (dsl.md §4.1.4) 未实现; `LayeredContext::can_read` / `can_write` 提供基础权限, 复杂权限由 `IExecutionPolicy` (ADR-0031) 处理

## 决策

- **ADR 状态**: ✅ Approved (保持)
- **理由**: ADR-0008 核心契约 (5-层结构化上下文 `LayeredContext`) 已 Shipped; 7 个缺失类中 3 个 Evolved (Layer 类型 → JSON 字段), 4 个 Deferred (压缩子系统 / 迁移工具 / 状态工具, 与 ADR-0007 重叠)
- **风险**: 低 — 5-层语义划分已通过 `LayeredContext` 完整实现

## 后续行动

- `ArchiveEntry` / `ContextTurn` 随 ADR-0007 LLM 压缩子系统一并实施
- `StateTools` 留待 Phase 5 状态管理精细化
- 本 audit 文档供 Phase 5 backlog 参考
