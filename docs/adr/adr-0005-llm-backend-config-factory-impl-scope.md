# ADR-0005 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0005-llm-backend-config-factory.md](adr-0005-llm-backend-config-factory.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

**📋 Audit** (impl-scope-audit 文档, 与 docs-code-drift-audit 配套使用)

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 9/14 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `LLMProviderFactory` | ✅ Shipped | `src/common/llm/llm_provider_factory.h` | 路由式工厂 (Sprint 18 P1.T1 从零构建) |
| `ILLMProvider` | ✅ Shipped | `include/agenticdsl/contract/illm_provider.h` | Provider 抽象接口 |
| `LlamaAdapter` | ✅ Shipped | `src/common/llm/llama_adapter.h` | llama.cpp 适配器 |
| `MockLLMProvider` | ✅ Shipped | `src/common/llm/mock_provider.h` | 测试用 Mock |
| `IProviderFactory` | ✅ Shipped | `include/agenticdsl/contract/iprovider_factory.h` | 工厂抽象接口 (Sprint 18 P1.T1) |
| `LLMConfig` | ✅ Shipped | `src/common/llm/llm_config.h` | 统一 per-call struct (非 ADR 草稿的 backends map) |
| `ProviderCreator` | 🔁 Evolved | — | Creator 注册模式被函数式 `LLMProviderFactory` 内部路由替代 |
| `OpenAICreator` | 🔁 Evolved | — | 同上; OpenAI 路由内联在 `LLMProviderFactory::create()` |
| `AnthropicCreator` | 🔁 Evolved | — | 同上; Anthropic 路由内联 |
| `LlamaCreator` | 🔁 Evolved | — | 同上; Llama 路由内联 |
| `VertexAICreator` | 📅 Deferred | — | ADR §长期扩展 Phase 2+ 后端, 未实施 |
| `BackendConfig` | 🔁 Evolved | `src/common/llm/llm_config.h` (`LLMConfig`) | ADR 草稿的 `BackendConfig` (type/model/api_key_env/...) 合并进统一 `LLMConfig` struct |
| `ConfigValidationResult` | 📅 Deferred | — | 配置验证逻辑未提取为独立结构体; 当前 `LLMConfig` 解析时内联校验 |
| `ConfigVersion` | 📅 Deferred | — | 配置版本迁移机制未实施 (YAGNI — 当前仅 v1 配置) |
| `HarnessEngine` | 📅 Deferred | — | ADR §4 描述的多 Agent 生命周期管理器; 实际由 `DSLEngine` + `CognitiveWorker` 分摊 |

## 分类详情

### 🔁 Evolved — Creator 注册模式 → 函数式路由

ADR-0005 §3 描述了 `ProviderCreator` 抽象基类 + `OpenAICreator` / `AnthropicCreator` / `LlamaCreator` 三个具体实现的注册模式。实际实现 (Sprint 18 P1.T1) 走了更简洁的函数式路由:

```cpp
// 实际实现 (LLMProviderFactory::create)
std::unique_ptr<ILLMProvider> LLMProviderFactory::create(const LLMConfig& config) {
  // 基于 config.provider 字段路由到 mock_factory / cloud_factory / llama_factory
}
```

内部持有 3 个 `IProviderFactory` 实例 (`mock_factory` / `cloud_factory` / `llama_factory`), 每个 factory 封装对应后端的创建逻辑。Creator 注册模式被简化为固定路由表。

**演进理由** (ADR-0005 §状态变更日志 2026-06-17 明确记录):
- ADR §3 设计草图未实现为编译代码
- OpenSpec change `2026-06-15-residual-engine-h-decoupling` 从零构建 (5-7 天)
- 避免 YAGNI: 不实现 CloudProviderFactory/LlamaProviderFactory 独立类

### 🔁 Evolved — `BackendConfig` → `LLMConfig`

ADR-0005 §3 的 `BackendConfig` (type/model/api_key_env/api_base/timeout/retries) 在实际实现中合并进统一的 `LLMConfig` struct (`src/common/llm/llm_config.h:28`), 包含 provider/api_url/model/max_tokens/temperature 等 per-call 字段。ADR 草稿的 `LLMConfig` (default_backend + backends map) 未实现。

### 📅 Deferred (4 个)

- **`VertexAICreator`**: ADR §长期扩展 Phase 2+ 后端 (Vertex AI / Gemini), 当前无需求
- **`ConfigValidationResult`**: 配置验证未提取为独立结构体; `LLMConfig` 解析时内联校验
- **`ConfigVersion`**: 配置版本迁移机制 YAGNI (当前仅 v1)
- **`HarnessEngine`**: 多 Agent 生命周期管理器; 实际由 `DSLEngine` (单引擎) + `CognitiveWorker` (per-agent 隔离) + `DomainWorkerPool` (并发任务) 分摊

## 决策

- **ADR 状态**: ✅ Approved (保持)
- **理由**: ADR-0005 核心契约 (`LLMProviderFactory` + `IProviderFactory` + `LLMConfig` + 配置驱动工厂) 已 Shipped; 9 个缺失类中 5 个 Evolved (Creator 模式 → 函数式路由, `BackendConfig` → `LLMConfig`), 4 个 Deferred (Phase 2+ 扩展 / YAGNI)
- **风险**: 低 — Creator 注册模式的演进路径在 ADR §状态变更日志中已明确记录

## 后续行动

- Phase 5+ 新增 Vertex AI / Grok 等后端时, 评估是否需要恢复 Creator 注册模式 (或继续函数式路由)
- `ConfigValidationResult` / `ConfigVersion` 留待配置复杂度增长时实施
- 本 audit 文档供 Phase 5 backlog 参考
