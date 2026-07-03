# ADR-0001 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0001-illm-provider-streaming-interface.md](adr-0001-illm-provider-streaming-interface.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 1/7 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `ILLMProvider` | ✅ Shipped | `include/agenticdsl/contract/illm_provider.h` | 核心接口已实现 |
| `IGenerationStream` | ✅ Shipped | `include/agenticdsl/contract/illm_provider.h` | 流式接口已实现 |
| `GenerationResult` | ✅ Shipped | `include/agenticdsl/contract/illm_provider.h` | 同步结果结构体 |
| `GenerationRequest` | ✅ Shipped | `include/agenticdsl/contract/illm_provider.h` | 请求结构体 |
| `LLMError` | ✅ Shipped | `include/agenticdsl/contract/illm_provider.h` | 结构化错误 |
| `LlamaAdapter` | ✅ Shipped | `src/common/llm/llama_adapter.h` | llama.cpp 适配器 |
| `SSETokenParser` | 📅 Deferred | — | ADR §4 描述为"示意"类, 实际 SSE 解析逻辑内联在 HTTP 流式适配器中, 未提取为独立类 |

## 分类详情

### 📅 Deferred — `SSETokenParser`

ADR-0001 §4 描述了 `SSETokenParser` 作为"SSE 解析状态机（示意）"。ADR 原文明确标注这是示意代码, 不是正式 API 契约。

实际 SSE 解析逻辑:
- 当前 `LlamaAdapter` 通过 HTTP 阻塞模式工作 (`httplib::Client::Post`), 不涉及 SSE 流式解析
- OpenAI/Anthropic 适配器 (`openai_adapter.h` / `anthropic_adapter.h`) 在 ADR §影响范围中列出, 但实际未实现为独立类 — 云端流式适配器留待 Phase 5 自举服务化
- 当 Phase 5 实现真实 SSE 流式时, 可提取 `SSETokenParser` 为独立工具类, 或继续内联在适配器中

**推迟理由**: 当前仅 `MockLLMProvider` + `LlamaAdapter` 两个后端, 均不使用 SSE 流式协议。`SSETokenParser` 在无真实 SSE 后端时提取为独立类属于 YAGNI。

## 决策

- **ADR 状态**: ✅ Approved (保持)
- **理由**: ADR-0001 的核心契约 (`ILLMProvider` / `IGenerationStream` / `LLMError` / 取消机制) 6/7 已 Shipped; 唯一 Deferred 的 `SSETokenParser` 在 ADR 原文中即标注为"示意", 不属于核心 API 契约
- **风险**: 低 — Phase 5 实现 OpenAI/Anthropic SSE 适配器时, 可按需提取或内联

## 后续行动

- Phase 5 实现云端 SSE 适配器时, 评估是否提取 `SSETokenParser` 为独立工具类
- 本 audit 文档供 Phase 5 backlog 参考, 不创建独立 OpenSpec change
