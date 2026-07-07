# Proposal: Phase 5 B2 Architecture-Layer Schemas (C13)

> **STATUS: ACTIVE** 🟡
> **关联 Oracle 决议**: Architecture Reflection 2026-07-05 (session `ses_0ce717ac4ffejvLa2We0gzbuds`)
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五 (B2 阶段)
> **关联 handoff**: `docs/handoff/2026-07-05-week1-day1-day2-completion.md` §5.1-5.2
> **关联 ADR**: ADR-0021 (PDK 设计), ADR-0034 (Model Router plugin 范式)
> **前置依赖**: C12 ✅ archived (2026-07-04)
> **后续依赖**: C14 (pdk/llama_engine plugin), C15 (batching queue plugin)
> **最后更新**: 2026-07-05

## Why

Phase 5 Stage 1 全链 ship (C9-C12) 后，handoff §5 推荐的 B2 推理标准库 7 子图（engine/model/prefix_cache/kv_cache/decoding/batching）最初按"全部架构层实施"规划。

但架构反思（2026-07-05 Oracle session `ses_0ce717ac4ffejvLa2We0gzbuds`）揭示：**部分任务过度抽象为 plugin 反而增加复杂度**，正确的边界是：

| 类型 | 决策 | 理由 |
|---|---|---|
| 配置参数 (prefix_cache/kv_cache/decoding 标准部分) | **架构层 schema** | 引擎内部细节 / 标准 API 稳定 / 过度抽象 = 过度设计 |
| 可插拔实现 (engine/model/batching) | **PDK plugin** | 多后端并存 / 外部贡献 / 依赖隔离（ADR-0034 范式）|
| 高级采样算法 (mirostat_v2/typical_p 等) | **架构层 schema**（decoding.md 字符串选择，不提取上层接口）| 标准参数稳定，高级算法在 engine plugin 内部实现 |
| **结论**：handoff §5.1-5.3 推荐的"全部硬编码注册到 `src/common/tools/registry.cpp`"路径**固化僵硬**，应**分两层实施**：

- **本 change (C13)**: 4 个架构层 schema（prefix_cache + kv_cache + decoding + cloud_engine 占位），零 plugin，零 C++ 实现，纯 .md
- **C14**: pdk/llama_engine plugin 骨架 + B2.1/B2.2 实现
- **C15**: BatchingQueue 接口 + LlamaBatchingQueue reference + 第三方贡献流程 (后按 Adversarial Review 精简为 batching.md schema only)

这样既保留"灵活应对不同演进"（engine/model/batching plugin 化），又避免"过早抽象"（prefix_cache/kv_cache 留架构层）。

## What Changes

### 1. B2.3 prefix_cache.md schema（架构层配置）

**当前**: 占位（101 行 PLACEHOLDER，结构同 session.md 模板）
**目标**: 真实 schema，定义 DSL 层暴露的配置旋钮

```yaml
# lib/inference/prefix_cache.md
signature: "(enabled: bool, max_size: int) -> (config: json, status: string)"

## /configure
  type: tool_call
  tool: prefix_cache.configure
  arguments:
    enabled: "{{ inputs.enabled | default(true) }}"
    max_size: "{{ inputs.max_size | default(512) }}"  # pattern count cap
  output_keys: ["status", "active_patterns"]

# 实际实现在 engine plugin 内部 (llama.cpp 自带 prefix cache)
# 架构层仅暴露配置 schema
```

**注册方式**: 写入 `lib/inference/prefix_cache.md`，架构层工具 `prefix_cache.configure` 委托给当前激活的 engine plugin

### 2. B2.4 kv_cache.md schema（架构层 enum）

```yaml
# lib/inference/kv_cache.md
signature: "(evict_policy: string, max_size_gb: float) -> (config: json, status: string)"

## /configure
  type: tool_call
  tool: kv_cache.configure
  arguments:
    evict_policy: "{{ inputs.evict_policy | default('lru') }}"  # lru|lfu|fifo
    max_size_gb: "{{ inputs.max_size_gb | default(4.0) }}"
  output_keys: ["status", "active_policy", "current_size_gb"]
```

### 3. B2.5 decoding.md schema（架构层标准 schema + plugin hook）

```yaml
# lib/inference/decoding.md
signature: "(temperature: float, top_p: float, top_k: int, repeat_penalty: float, sampler: string) -> (config: json, status: string, unsupported_warning: string)"

## /apply
  type: tool_call
  tool: decoding.configure
  arguments:
    temperature: "{{ inputs.temperature | default(0.7) }}"  # 0.0-2.0
    top_p: "{{ inputs.top_p | default(0.9) }}"              # 0.0-1.0
    top_k: "{{ inputs.top_k | default(40) }}"                # int
    repeat_penalty: "{{ inputs.repeat_penalty | default(1.1) }}"  # float
    sampler: "{{ inputs.sampler | default('greedy') }}"  # greedy|temperature|mirostat_v1|mirostat_v2|typical_p
  output_keys: ["status", "active_sampler", "unsupported_warning"]
```

### 4. cloud_engine.md schema（第三方 plugin 占位）

```yaml
# lib/inference/cloud_engine.md
> ⚠️ PLACEHOLDER — 实现在 Phase 5 Stage 2+
> 第三方 plugin 按 schema 实现 (pdk/cloud_engine/openai/, pdk/cloud_engine/anthropic/, 等)

signature: "(provider: string, model: string, api_key_ref: string) -> (config: json, status: string)"

## /configure
  type: tool_call
  tool: cloud_engine.configure
  arguments:
    provider: "{{ inputs.provider }}"          # openai|anthropic|deepseek|qwen
    model: "{{ inputs.model }}"
    api_key_ref: "{{ inputs.api_key_ref }}"    # 引用 secret store 而非明文
  output_keys: ["status", "provider", "model"]
```

### 5. lib/inference 当前状态收尾

**当前**:
- `session.md` 107 行 — 完整 ship
- `engine.md` 101 行 — PLACEHOLDER (B2.1 待 C14 实施)
- `model.md` 108 行 — PLACEHOLDER (B2.2 待 C14 实施)

**本 change 后**:
- `prefix_cache.md` — 真实 schema
- `kv_cache.md` — 真实 schema
- `decoding.md` — 真实 schema
- `cloud_engine.md` — 真实 schema (占位标记)
- engine.md / model.md 仍为 PLACEHOLDER（待 C14 实施）

### 6. 文档同步

- `docs/handoff/2026-07-05-week1-day1-day2-completion.md` §10.2 标记本 change 完成
- `docs/active-status.md` Phase 5 Stage 1 进度从 "3/7 ship + 2/7 占位" 更新
- `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §5.4 调整 B2 拆分为 C13/C14/C15

## What Does NOT Change

- **B2.1 engine 实施** — 移至 C14 (plugin)
- **B2.2 model 实施** — 移至 C14 (plugin)
- **B2.6 batching 实施** — 移至 C15 (plugin)
- **TSan race fix** — 立即修复（独立 commit，不在本 change 范围）
- **现有 lib/inference/session.md** — 已 ship，不动
- **现有 pdk/model_router/** — 已 ship（C7），不动
- **任何 C++ 实现** — 本 change 是 schema-only ship

## Capabilities

### ADDED Requirements

- `prefix-cache-schema-defined`: `lib/inference/prefix_cache.md` MUST 定义 DSL 层 prefix_cache.configure 工具签名和参数 schema
- `kv-cache-schema-defined`: `lib/inference/kv_cache.md` MUST 定义 DSL 层 kv_cache.configure 工具签名、evict_policy enum 和 max_size_gb 参数
- `decoding-schema-defined`: `lib/inference/decoding.md` MUST 定义 DSL 层 decoding.configure 工具签名、5 种 sampler 选项 (greedy/temperature/mirostat_v1/mirostat_v2/typical_p) 和 unsupported_warning 字段
- `cloud-engine-schema-placeholder`: `lib/inference/cloud_engine.md` MUST 定义第三方 cloud engine plugin 接口契约 schema（顶部 PLACEHOLDER 标记 + Stage 2+ 实施说明）
- `lib-inference-coverage-improved`: lib/inference/ 子图覆盖率 MUST 从 "3/7 ship (1 真实 + 2 占位) + 4 缺失" → "4/7 ship + 3 待 C14/C15 实施"

## Impact

**修改文件**:
- `lib/inference/prefix_cache.md` (新, +50 行)
- `lib/inference/kv_cache.md` (新, +45 行)
- `lib/inference/decoding.md` (新, +60 行)
- `lib/inference/cloud_engine.md` (新, +55 行)
- `docs/handoff/2026-07-05-week1-day1-day2-completion.md` (~5 行更新 §10.2)
- `docs/active-status.md` (~3 行更新 Phase 5 进度)
- `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` (~10 行调整 §5.4)
- `openspec/changes/phase5-b2-arch-schemas/{proposal,tasks,specs}.md` (本 change artifacts)

**零代码逻辑变更**：本 change 仅 schema + 头文件接口声明 + 文档同步

**API 兼容性**: **零 breaking change**（无 .cpp/.h 实现变更）

**估时**: 0.5-1 天 (Day 1 工作量)
- 4 个 .md schema 编写: 3h
- 文档同步: 1h
- 验证 (ctest + adr_lint + docs_drift): 30min

## Non-goals

- **不实施任何 C++ 引擎代码**（B2.1/B2.2 engine/model 留 C14 plugin 层实施）
- **不实施 batching queue**（留 C15 plugin 层）
- **不修改现有 schema**（session.md / engine.md / model.md 保持）
- **不创建新测试**（schema-only ship 无 C++ 逻辑可测）
- **不实施 TSan race fix**（独立 commit 立即修复，不阻塞本 change）
- **不创建 pdk/llama_engine/**（留 C14）

## 关联 change

- **前置**: C9 (ADR impl-scope audit) ✅ + C10 (Lazy ModuleState) ✅ + C11 (SessionRegistry) ✅ + C12 (YIELD/STREAM) ✅
- **后续**: C14 (pdk/llama_engine plugin — B2.1/B2.2)
- **后续**: C15 (BatchingQueue plugin — B2.6 + PR 模板) — 后按 Adversarial Review 决策精简

## 验证标准

- [ ] ctest baseline 64/64 PASS 零回归（schema-only ship 无 C++ 变更）
- [ ] `python3 tools/adr_lint.py` exit 0（无新 ADR 文件）
- [ ] `python3 tools/docs_drift_audit.py` 0 DRIFT（schema 引用文档同步）
- [ ] `openspec validate phase5-b2-arch-schemas` exit 0
- [ ] 4 个 .md schema 文件通过 Markdown 渲染（无语法错误）
- [ ] lib/inference/ 子图覆盖率: 1/7 ship → 4/7 ship（session + prefix_cache + kv_cache + decoding）+ 1/7 占位（cloud_engine）+ 2/7 待 C14（engine/model）

## Oracle 决策依据

**会话**: `ses_0ce717ac4ffejvLa2We0gzbuds` (2026-07-05)
**关键判据**:
1. 实现多样性 (≥2 等价实现并存) — prefix_cache 是引擎内部细节 → 架构层
2. 外部贡献预期 (社区/团队贡献) — cloud_engine 预期外部贡献 → 架构层占位契约
3. ABI 契约稳定性 (6+ 月) — sampler parameters 是 LLM 标准 API → 架构层

**对比 plugin 层 (C14/C15)**:
- engine/model/batching 是多后端并存 → plugin 层
- 高级采样算法可演进 → plugin extension hook（架构层声明接口 + C14 出 reference）