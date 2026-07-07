# inference-config-schemas Specification

> **Purpose**: 追踪 Phase 5 B2 架构层 schema ship（C13 change 产出）
> **STATUS: ACTIVE** 🟡
> **关联 design**: `openspec/changes/phase5-b2-arch-schemas/proposal.md`
> **关联 tasks**: `openspec/changes/phase5-b2-arch-schemas/tasks.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五
> **最后更新**: 2026-07-05

## ADDED Requirements

### Requirement: prefix-cache-config-schema

`lib/inference/prefix_cache.md` MUST 定义 DSL 层 `prefix_cache.configure` 工具签名、参数 schema 和默认值。

#### Scenario: prefix_cache.configure 工具签名

- **WHEN** 读取 `lib/inference/prefix_cache.md` YAML signature
- **THEN** 包含 `(enabled: bool, max_size: int) -> (config: json, status: string)`
- **AND** `enabled` 字段默认值 `true`
- **AND** `max_size` 字段默认值 `512`
- **AND** 包含 `## /configure` tool_call 节点引用 `prefix_cache.configure` 工具

#### Scenario: prefix_cache.md 文档完整性

- **WHEN** 检查 `lib/inference/prefix_cache.md`
- **THEN** 文件存在
- **AND** 包含 1 个 tool_call 节点
- **AND** 顶部包含功能描述注释
- **AND** 包含占位说明（实际能力在 engine plugin 内部实施）

---

### Requirement: kv-cache-config-schema

`lib/inference/kv_cache.md` MUST 定义 DSL 层 `kv_cache.configure` 工具签名、`evict_policy` enum 和 `max_size_gb` 参数。

#### Scenario: kv_cache.configure 工具签名

- **WHEN** 读取 `lib/inference/kv_cache.md` YAML signature
- **THEN** 包含 `(evict_policy: string, max_size_gb: float) -> (config: json, status: string)`
- **AND** `evict_policy` enum 包含 `lru` / `lfu` / `fifo` 三个值，默认 `lru`
- **AND** `max_size_gb` 字段默认值 `4.0`
- **AND** 包含 `## /configure` tool_call 节点引用 `kv_cache.configure` 工具

#### Scenario: kv_cache.md 文档完整性

- **WHEN** 检查 `lib/inference/kv_cache.md`
- **THEN** 文件存在
- **AND** 与 prefix_cache.md 模板结构对齐

---

### Requirement: decoding-config-schema

`lib/inference/decoding.md` MUST 定义 DSL 层 `decoding.configure` 工具签名、5 种 sampler 选项和 `unsupported_warning` 字段。

#### Scenario: decoding.configure 工具签名

- **WHEN** 读取 `lib/inference/decoding.md` YAML signature
- **THEN** 包含 `(temperature: float, top_p: float, top_k: int, repeat_penalty: float, sampler: string) -> (config: json, status: string, unsupported_warning: string)`
- **AND** `temperature` 默认值 `0.7`，范围 `0.0-2.0`
- **AND** `top_p` 默认值 `0.9`，范围 `0.0-1.0`
- **AND** `top_k` 默认值 `40`
- **AND** `repeat_penalty` 默认值 `1.1`
- **AND** `sampler` 默认值 `greedy`

#### Scenario: 5 种 sampler 选项

- **WHEN** 检查 `sampler` 字段允许值
- **THEN** 包含 `greedy` / `temperature` / `mirostat_v1` / `mirostat_v2` / `typical_p` 五个值
- **AND** output_keys 包含 `unsupported_warning` 字段（用于 plugin 降级语义）

#### Scenario: decoding.md 文档完整性

- **WHEN** 检查 `lib/inference/decoding.md`
- **THEN** 文件存在
- **AND** 包含 1 个 tool_call 节点

---


### Requirement: cloud-engine-config-schema-placeholder

`lib/inference/cloud_engine.md` MUST 定义第三方 cloud engine plugin 接口契约 schema（含 PLACEHOLDER 标记）。

#### Scenario: cloud_engine.configure 工具签名

- **WHEN** 读取 `lib/inference/cloud_engine.md` YAML signature
- **THEN** 包含 `(provider: string, model: string, api_key_ref: string) -> (config: json, status: string)`
- **AND** `provider` 枚举值：`openai` / `anthropic` / `deepseek` / `qwen`
- **AND** `model` 字段为 string
- **AND** `api_key_ref` 字段引用 secret store 而非明文 API key

#### Scenario: PLACEHOLDER 标记

- **WHEN** 读取 `lib/inference/cloud_engine.md` 文件头
- **THEN** 包含 `> ⚠️ PLACEHOLDER` 标记
- **AND** 说明实现在 Phase 5 Stage 2+
- **AND** 说明第三方 plugin 按 schema 实现路径 (pdk/cloud_engine/openai/, pdk/cloud_engine/anthropic/, 等)

#### Scenario: cloud_engine.md 文档完整性

- **WHEN** 检查 `lib/inference/cloud_engine.md`
- **THEN** 文件存在
- **AND** 包含 1 个 tool_call 节点
- **AND** 与 decoding.md 模板结构对齐

---

### Requirement: lib-inference-coverage-improved

`lib/inference/` 子图覆盖率 MUST 从 ship + 2 占位 提升至 4 ship + 1 占位 + 2 待 C14。

#### Scenario: 覆盖率统计

- **WHEN** 运行 `ls lib/inference/*.md`
- **THEN** 包含 7 个文件：session.md (ship) + prefix_cache.md (ship) + kv_cache.md (ship) + decoding.md (ship) + cloud_engine.md (ship, PLACEHOLDER) + engine.md (PLACEHOLDER, 待 C14) + model.md (PLACEHOLDER, 待 C14)
- **AND** 4/7 真实 schema (session/prefix_cache/kv_cache/decoding)
- **AND** 1/7 真实 PLACEHOLDER schema (cloud_engine)
- **AND** 2/7 旧 PLACEHOLDER 保留 (engine/model, 待 C14 实施)

---

### Requirement: documentation-synced

3 处文档 MUST 同步本 change ship 状态：active-status / master plan / handoff。

#### Scenario: active-status.md 同步

- **WHEN** 读取 `docs/active-status.md` 活跃变更看板 Phase 5 行
- **THEN** 进度从 "3/7 ship + 2/7 占位" → "4/7 ship + 1/7 占位 + 2/7 待 C14"
- **AND** 实施日志追加 C13 ship 行（2026-07-05）

#### Scenario: master plan 同步

- **WHEN** 读取 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §5.4
- **THEN** 包含 B2 拆分说明（C13/C14/C15）
- **AND** §三 §四 表格添加 C13/C14/C15 行

#### Scenario: handoff 同步

- **WHEN** 读取 `docs/handoff/2026-07-05-week1-day1-day2-completion.md` §10.2
- **THEN** C13 标记 ✅ 完成
- **AND** §10.2 "⏳ B2.1+B2.2 engine/model 实施" 移除或标注 "→ C14 范围"