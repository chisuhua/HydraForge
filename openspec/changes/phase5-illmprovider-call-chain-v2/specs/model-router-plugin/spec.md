# Spec: model-router-plugin (Phase 5 增量 — RouterDecorator 集成)

## MODIFIED Requirements

### Requirement: available_models() virtual method (REQ-MR-003)

`ILLMProvider` MUST 提供 `virtual std::vector<ModelInfo> available_models() const` 方法,该方法 MUST 是 **pure virtual**(无默认实现)。任何 ILLMProvider 子类 MUST override,否则编译失败。

#### Scenario: 5 个 Provider 实现显式 override

- **WHEN** 编译以下 5 个 ILLMProvider 实现
- **THEN** MUST 各自显式 `override` `available_models()`:
  - `MockLLMProvider` — 返回 1 个 `mock-llm-v1`(`capabilities={Chat, ToolUse}`, `context_window=4096`)
  - `CloudLLMAdapter` (在 `pdk/cloud/src/`)— 返回 `config.model` 对应的 `ModelInfo`
  - `LlamaAdapterProvider` (deprecated) — 返回 `llama_config.model` 对应的 `ModelInfo`
  - `InferencePlugin` (在 `pdk/inference_engine/src/`)— 返回 loaded gguf models 列表
  - 任何第三方 ILLMProvider — 必须显式 override(否则编译失败)

#### Scenario: 编译时强制 override

- **WHEN** 第三方 ILLMProvider 子类未 override `available_models()`
- **THEN** MUST 编译失败(纯虚方法未实现)
- **AND** 编译错误信息 MUST 包含 `available_models` 字串
- **AND** Phase 5+ 任何新 Plugin MUST override(per REQ-ICC-004)

#### Scenario: Router 静默失败防护

- **WHEN** Plugin Provider 返回空 `available_models()`(override 但返回空 vector)
- **THEN** ModelRouter MUST 抛出 `ModelRoutingError(NoViableModel)`
- **AND** MUST NOT 静默返回默认 model(避免选错模型)

#### Scenario: 默认空实现删除

- **WHEN** 检查 `src/common/llm/llm_types.h:157` 中 `available_models()` 定义
- **THEN** MUST NOT 含 `{ return {}; }` 默认实现
- **AND** MUST 是 `virtual std::vector<ModelInfo> available_models() const = 0`

## ADDED Requirements

### Requirement: router-decorator-integration-point (REQ-MR-IPD-001)

`OrchestrationILLMProvider` MUST 通过 `router_.select(models, req)` 调用 `IModelRouter`,实现路由选择。`RouterDecorator` 不作为独立类,直接复用 ADR-0034 现有 `IModelRouter` 抽象。

#### Scenario: OrchestrationILLMProvider 路由调用点

- **WHEN** `OrchestrationILLMProvider::generate(req, token)` 被调用
- **THEN** MUST 调用 `router_->select(inference_provider_->available_models(), req)` 获取 `ModelInfo`
- **AND** MUST 验证 `ModelInfo.provider` 与 `req` 配置的 `provider` 字符串匹配
- **AND** 若 `available_models()` 返回空 vector,MUST 返回 `Result::failure(LLMError::Code::InvalidRequest, "no models available")`(防止 Router 静默失败,per REQ-MR-003 场景 "Router 静默失败防护")

#### Scenario: Router 注入构造

- **GIVEN** 编排 Plugin 构造时注入 `shared_ptr<IModelRouter> router`
- **WHEN** 编排 Plugin 启动
- **THEN** MUST 存储 `router_` 成员
- **AND** MUST NOT 隐式创建默认 Router(用户必须显式注入)

#### Scenario: Router 策略复用

- **GIVEN** 现有 `CostModelRouterPolicy` / `QualityModelRouterPolicy` / `LatencyModelRouterPolicy` (per ADR-0034 Phase 1+2 ship)
- **WHEN** 编排 Plugin 启动
- **THEN** MUST 复用现有 3 个 Router 策略(从 `pdk/model_router/` plugin 加载)
- **AND** MUST NOT 重新实现路由策略

### Requirement: orchestration-router-passthrough (REQ-MR-IPD-002)

编排 Plugin MUST 在构造时验证 router 与 inference provider 的 provider 字符串兼容性,避免路由到不支持的 backend。

#### Scenario: provider 字符串匹配验证

- **GIVEN** `inference_provider_` 是 cloud plugin 实例(`provider = "openai"`)
- **AND** `router_` 是 `CostModelRouterPolicy` 实例(支持 multi-provider)
- **WHEN** 编排 Plugin 构造
- **THEN** MUST 验证 `router_->supported_providers()` 包含 `"openai"`
- **AND** 若不包含,MUST emit WARNING 日志 + 继续启动(允许 router 跳过不识别的 provider)

#### Scenario: 路由失败兜底

- **WHEN** `router_->select(models, req)` 返回 `ModelRoutingError(NoViableModel)`
- **THEN** MUST 透传 error 给上层 `OrchestrationILLMProvider::generate()`
- **AND** MUST NOT fallback 到任意 model(避免静默选错)