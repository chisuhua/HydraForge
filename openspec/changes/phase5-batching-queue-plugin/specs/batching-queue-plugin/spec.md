# batching-schema Specification

> **Purpose**: 追踪 Phase 5 batching.md schema ship（C15 change 精简后产出）
> **STATUS: ACTIVE** 🟡 (精简版 — 仅 `batching.md` schema)
> **关联 design**: `openspec/changes/phase5-batching-queue-plugin/proposal.md`
> **关联 tasks**: `openspec/changes/phase5-batching-queue-plugin/tasks.md`
> **关联 decision**: `docs/adversarial-reviews/decisions-2026-07-07.md` D2
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五
> **最后更新**: 2026-07-07

## ADDED Requirements

### Requirement: lib-inference-batching-md-shipped

`lib/inference/batching.md` MUST 定义 `batching.submit_and_wait` 工具签名（架构层 schema）。

#### Scenario: batching.md schema

- **WHEN** 读取 `lib/inference/batching.md`
- **THEN** 文件存在
- **AND** YAML signature 包含 `(prompt: string, timeout_ms: int) -> (request_id: int, result: string)`
- **AND** 包含 `## /submit_and_wait` tool_call 节点引用 `batching.submit_and_wait` 工具
- **AND** 顶部包含 `⚠️ PLACEHOLDER` 标记
- **AND** 顶部说明：BatchingQueue 接口推迟到第二个推理后端出现时

#### Scenario: 默认 timeout

- **WHEN** 检查 batching.md 参数默认值
- **THEN** `timeout_ms` 默认 `30000` (30 秒)
