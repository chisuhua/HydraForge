# ADR-0007 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0007-context-compression.md](adr-0007-context-compression.md)
> **状态**: 🟡 Partial (audit 后保持)

## 状态

**📋 Audit** (impl-scope-audit 文档, 与 docs-code-drift-audit 配套使用)

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 🟡 Partial, 但 6/10 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `LayeredContext` | ✅ Shipped | `include/agenticdsl/types/layered_context.h` | 5-层结构化上下文 (ADR-0008) |
| `Context` | ✅ Shipped | `src/core/types/context.h` | 扁平上下文 (桥接到 LayeredContext) |
| `TraceRecord` | ✅ Shipped | `include/agenticdsl/types/trace_record.h` | 追踪记录 |
| `ToolResult` | ✅ Shipped | `src/core/types/tool_result.h` | 工具结果 (含 ErrorCode enum) |
| `ArchiveEntry` | 📅 Deferred | — | 归档条目结构未实现; `LayeredContext.archive` (L4) 提供 JSON 存储槽位但无结构化 entry |
| `CompressionTrigger` | 📅 Deferred | — | 压缩触发器未实现; ADR §触发条件 (token 阈值/turn 数) 未接入 |
| `ContextCompressor` | 📅 Deferred | — | LLM 压缩器未实现; ADR 明确标注"快照有, 无 LLM 压缩" |
| `ContextTurn` | 📅 Deferred | — | 对话轮次结构未实现; 当前上下文无 turn 粒度管理 |
| `Metadata` | 📅 Deferred | — | 元数据结构未独立实现; `LayeredContext.meta` (L5) 提供 JSON 槽位 |
| `ToolResultSummary` | 📅 Deferred | — | 工具结果摘要未实现; 当前 `ToolResult` 完整存储 |

## 分类详情

### 📅 Deferred (6 个) — LLM 上下文压缩未实施

ADR-0007 描述了 LLM 驱动的上下文压缩机制 (当上下文超过阈值时, 用 LLM 摘要历史对话)。当前实现状态:
- ✅ **快照能力**: `LayeredContext` 5-层结构 (system/recent/working/archive/meta) 已 Shipped
- ❌ **LLM 压缩**: `ContextCompressor` + `CompressionTrigger` + `ArchiveEntry` 均未实施
- ❌ **Turn 管理**: `ContextTurn` 对话轮次粒度未实现
- ❌ **摘要存储**: `ToolResultSummary` 未实现; `Metadata` 未独立结构化

**推迟理由**: ADR-0007 状态本身已标记 🟡 Partial ("快照有, 无 LLM 压缩")。LLM 压缩需要:
1. 真实 LLM 后端 (当前仅 MockLLMProvider + LlamaAdapter)
2. Token 计数器 (当前无 `IGenerationStream::usage_stats()`)
3. 压缩 prompt 模板 (未设计)

这些依赖留待 Phase 5 自举服务化 (真实 LLM 后端 + 预算控制)。

## 决策

- **ADR 状态**: 🟡 Partial (保持)
- **理由**: ADR-0007 已诚实标记 🟡 Partial; 6 个缺失类全部属于 LLM 压缩子系统, 与 ADR 状态一致
- **风险**: 中 — 长对话场景下上下文会无限增长, 但当前无真实 LLM 后端触发该问题

## 后续行动

- Phase 5 实现真实 LLM 后端 + token 计数后, 启动 `ContextCompressor` 实施
- `CompressionTrigger` 阈值 (token 数 / turn 数) 需与 `IBudgetController` 集成
- 本 audit 文档供 Phase 5 backlog 参考
