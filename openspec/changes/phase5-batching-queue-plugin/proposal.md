# Proposal: Phase 5 Batching Schema (C15 — 精简版)

> **STATUS: ACTIVE** 🟡 (精简版 — 按 Adversarial Review D2 决策，仅保留 `batching.md` schema)
> **关联 Oracle 决议**: Architecture Reflection 2026-07-05 (session `ses_0ce717ac4ffejvLa2We0gzbuds`)
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五 + §七
> **关联 decision**: `docs/adversarial-reviews/decisions-2026-07-07.md` D2 (推迟 BatchingQueue，仅保留 schema)
> **关联 ADR**: ADR-0021 (PDK 设计), ADR-0034 (plugin 范式)
> **前置依赖**: 无（本 change 已精简为 schema-only）
> **后续依赖**: BatchingQueue 接口推迟到第二个推理后端出现时
> **最后更新**: 2026-07-07

## Why

原 C15 提案包含 BatchingQueue PDK 接口（5 方法）+ LlamaBatchingQueue reference 实现 + 第三方贡献流程。Adversarial Review 调研 7 个主流推理引擎（vLLM/SGLang/llama.cpp/TRT-LLM/TGI/LMDeploy/lit-gpt）后确认**零项目有独立 BatchingQueue 接口**，batching 策略均内联在 engine scheduler/executor 中。

**决策**: BatchingQueue 接口 + 贡献流程推迟到第二个推理后端出现时再抽象。本 change 仅创建 `lib/inference/batching.md` schema 作为架构层占位。

## What Changes

### 1. lib/inference/batching.md schema（架构层占位）

**当前**: 不存在
**目标**: 40 行 PLACEHOLDER schema，定义 DSL 层 batching 工具签名

```yaml
# lib/inference/batching.md
> ⚠️ PLACEHOLDER — 实现在 Phase 5 Stage 2+
> BatchingQueue 接口推迟到第二个推理后端出现时再抽象

signature: "(prompt: string, timeout_ms: int) -> (request_id: int, result: string)"

## /submit_and_wait
  type: tool_call
  tool: batching.submit_and_wait
  arguments:
    prompt: "{{ inputs.prompt }}"
    timeout_ms: "{{ inputs.timeout_ms | default(30000) }}"
  output_keys: ["request_id", "result"]
```

### 2. 文档同步

- `docs/handoff/2026-07-05-week1-day1-day2-completion.md` §10.2 — C15 标记 ✅
- `docs/active-status.md` — Phase 5 Stage 1 进度更新
- `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五 — 标记 C15 精简

## What Does NOT Change

- **C12 YIELD/STREAM 实现** — 不修改
- **pdk/model_router/** — 已 ship C7，不修改
- **pdk/llama_engine/ (C14)** — C15 不新增文件
- **现有 LlamaAdapter** — 不修改

## Capabilities

### ADDED Requirements

- `lib-inference-batching-md-shipped`: `lib/inference/batching.md` MUST 定义 `batching.submit_and_wait` 工具签名（架构层 schema，顶部标注 PLACEHOLDER）

## Impact

**新增文件**:
- `lib/inference/batching.md` (~40 行，架构层 schema)

**修改文件**:
- `docs/handoff/2026-07-05-week1-day1-day2-completion.md` (~5 行)
- `docs/active-status.md` (~3 行)
- `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` (~15 行)

**总净增**: ~40 行 schema + ~20 行文档同步

**API 兼容性**: **零 breaking change**

**估时**: ~2 小时
- batching.md schema 编写: 1h
- 文档同步: 30min
- 验证: 30min

## Non-goals

- **不创建 BatchingQueue PDK 接口** — 推迟到第二个推理后端出现时
- **不实现 LlamaBatchingQueue** — 无 reference impl
- **不创建贡献流程文档** — 0 第三方贡献者时不写空文档
- **不修改 ADR-0021 §8** — ABI semver 政策非本 change 范围
- **不创建 batching_capability 工具** — 无 BatchingQueue 则不暴露

## 关联 change

- **前置**: 无（独立 schema-only change）
- **后续**: BatchingQueue 接口（推迟到第二个推理后端出现时）

## 验证标准

- [ ] `lib/inference/batching.md` 文件存在，包含 `batching.submit_and_wait` 工具签名
- [ ] 文件顶部包含 `⚠️ PLACEHOLDER` 标记
- [ ] `python3 tools/adr_lint.py` exit 0
- [ ] `python3 tools/docs_drift_audit.py` 0 DRIFT
- [ ] `openspec validate phase5-batching-queue-plugin` exit 0
