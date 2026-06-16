# Spec: ModelRouter Plugin Stub Capability Requirements

> **Sprint**: Sprint 0 (W1D3, 2026-06-16)
> **ADR**: docs/adr/plugin/adr-0034-model-router.md (plugin-candidate, K1 决策)
> **关联 plan**: .omo/plans/phase1-execution.md §Sprint 0

## ADDED Requirements

### REQ-MR-001: ModelCapability enum

Runtime 端 MUST 提供 `enum class ModelCapability` 含以下 5 个值:
- `Chat` — 对话生成
- `Completion` — 文本补全
- `Embedding` — 向量嵌入
- `ToolUse` — 工具调用
- `Vision` — 视觉输入

枚举值 MUST 保持稳定, 后续扩展需新增值, 不修改现有顺序。

### REQ-MR-002: ModelInfo struct

Runtime 端 MUST 提供 `struct ModelInfo` 含以下 4 个字段:
- `name: std::string` — 模型唯一标识
- `capabilities: std::vector<ModelCapability>` — 支持的能力列表
- `context_window: std::int64_t` (默认 0) — 上下文窗口大小 (tokens)
- `provider: std::string` (默认 `"unknown"`) — 提供方标识

### REQ-MR-003: available_models() virtual method

`ILLMProvider` MUST 提供 `virtual std::vector<ModelInfo> available_models() const` 方法。

- 默认实现 MUST 返回空 vector (`return {};`)
- Provider 实现 SHOULD override 此方法, 返回实际注册模型
- `MockLLMProvider` MUST override 返回 1 个 mock 模型: `name="mock-llm-v1"`, `capabilities={Chat, ToolUse}`, `context_window=4096`, `provider="mock"`

### REQ-MR-004: ModelRouterPolicy Plugin Stub

`examples/phase1_model_router_plugin/` MUST 提供 `ModelRouterPolicy::route(provider)` 静态方法:

- 输入: `const ILLMProvider&`
- 输出: `ILLMProvider::ModelInfo`
- 行为: 选择 `available_models()` 中**第一个支持 Chat** 的模型
- 异常: `std::runtime_error("no models available from provider")` 当 `available_models()` 为空
- 异常: `std::runtime_error("no Chat-capable model available")` 当无 Chat-capable 模型

### REQ-MR-005: Plugin Stub examples

Phase 1 MUST 提供 2 个独立可执行 Plugin Stub examples:
- `examples/phase1_model_router_plugin --mock` — 演示 ModelRouterPolicy 路由决策
- `examples/phase1_plugin_demo --mock` — 端到端 demo (MockLLMProvider + ModelRouterPolicy + generate())

两者 MUST 输出 `routed to model: mock-llm-v1`。

### REQ-MR-006: Unit Test Coverage

`tests/test_model_router_policy.cpp` MUST 包含 5 个 TEST_CASE:
1. `available_models() returns non-empty` — MockLLMProvider 默认非空 + 字段正确
2. `ModelRouterPolicy.route() selects first Chat-capable model` — 多模型场景选 Chat
3. `ModelRouterPolicy throws on empty model list` — 空列表异常
4. `ModelRouterPolicy throws when no Chat-capable model` — 无 Chat 模型异常
5. `ModelRouterPolicy route decision includes trace_id` — 路由决策可追溯 (Sprint 1a 透传 trace_id 前置)

## MODIFIED Requirements

无 — Sprint 0 仅新增能力, 不修改既有 API。

## REMOVED Requirements

无。

## 跨 Sprint 约束

- Sprint 0 实施 MUST NOT 复活 `IModelRouter`/`ModelRouter`/`DefaultModelRouter` Runtime 类 (K1 决策)
- Sprint 5 (PluginLoader) 实施 MUST 复用 `ModelRouterPolicy` 逻辑, 迁移到 PDK Plugin
- Sprint 5 验收 MUST 包含 "通过真实 .so 加载 phase1_model_router_plugin Policy 逻辑"
