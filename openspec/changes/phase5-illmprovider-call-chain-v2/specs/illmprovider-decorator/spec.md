# illmprovider-decorator Specification

## Purpose

`ILLMProviderDecorator` 抽象接口 + 3 个具体装饰器(`CostTracking` / `Compliance` / `RateLimit`)— 通过 GoF Decorator 模式修 Phase 5 Budget Hole(3 处 LLM 直调路径零计费),并将 cost / observability / governance 作为正交关注点独立实现、可测、可组合。Phase 5 ship gate。

## ADDED Requirements

### Requirement: illmprovider-decorator-interface (REQ-IPD-001)

`ILLMProviderDecorator` MUST 是继承 `ILLMProvider` 的装饰器基类,提供内部 `unique_ptr<ILLMProvider> inner_` 持有与转发逻辑,子类 MUST override 装饰钩子方法。

#### Scenario: 类签名定义

- **WHEN** 检查 `include/agenticdsl/contract/i_llm_provider_decorator.h`
- **THEN** MUST 声明 `class ILLMProviderDecorator : public ILLMProvider`
- **AND** MUST 持有 `std::unique_ptr<ILLMProvider> inner_` 成员
- **AND** MUST override `generate()` / `generate_stream()` / `available_models()` 为 `final` 并转发到 `inner_`
- **AND** MUST 提供 protected virtual 钩子方法 `decorate_generate(req, inner_result)` / `decorate_generate_stream(req, inner_stream)` / `decorate_available_models(inner_models)`

#### Scenario: 装饰器链深度限制

- **WHEN** 装饰器链通过 `std::move` 多次嵌套构造
- **THEN** DSLEngine MUST 限制最大层数 ≤ 4(含 inner_ 在内,即最多 3 个装饰器 + 1 个 inner_ = 4 层 ILLMProvider)
- **AND** 超出层数 MUST 抛 `std::runtime_error("decorator chain too deep")`(如 CostTracking→Compliance→RateLimit→inner_=4 层 OK;再加一层=5 层 throw)

#### Scenario: 装饰器可独立 mock 测试

- **WHEN** 单元测试 `ILLMProviderDecorator` 子类
- **THEN** MUST 能用 `MockILLMProvider` 注入 `inner_` 构造
- **AND** MUST 验证 `decorate_*` 钩子被调用
- **AND** MUST 验证 `inner_` 方法被转发

### Requirement: cost-tracking-decorator (REQ-IPD-002)

`CostTrackingDecorator` MUST 包装 ILLMProvider,在 `generate()` 和 `generate_stream()` 完成后记录 token 消耗到 `IBudgetController`。

#### Scenario: 同步 generate 计费

- **WHEN** `CostTrackingDecorator::generate(req, token)` 被调用
- **THEN** MUST 调用 `inner_->generate(req, token)` 获取 `Result<GenerationResult, LLMError>`
- **AND** 若结果有值(`has_value()`),MUST 提取 `prompt_tokens` + `completion_tokens`
- **AND** MUST 调用 `budget_->record_llm_call(tokens, model_name)`
- **AND** MUST 返回原 `Result`(装饰器不修改业务返回值)

#### Scenario: 流式 generate_stream 计费

- **WHEN** `CostTrackingDecorator::generate_stream(req, token)` 被调用
- **THEN** MUST 调用 `inner_->generate_stream(req, token)` 获取 `unique_ptr<IGenerationStream>`
- **AND** MUST 包装流对象,在 `next()` 返回 `std::nullopt`(流结束)时统计累积 token
- **AND** MUST 调用 `budget_->record_llm_call(accumulated_tokens, model_name)`

#### Scenario: 错误结果不计费

- **WHEN** `inner_->generate()` 返回 `Result::failure(error)`
- **THEN** MUST NOT 调用 `budget_->record_llm_call`(避免失败请求被计费)
- **AND** MUST 透传 error 给上层

#### Scenario: budget hole 修复

- **WHEN** 集成测试跑 100 次 NodeExecutor execute_generate_subgraph + 100 次 execute_yield + 100 次 SimpleCognitiveOrchestrator react
- **THEN** `IBudgetController::record_llm_call` MUST 被调用次数 == 300(每次 LLM 调用都计费,**非**仅 total_cost_usd > 0)
- **AND** 每次 `record_llm_call` 调用 MUST 携带非零 token 数(Mock 响应需包含显式 prompt_tokens/completion_tokens)

### Requirement: compliance-decorator (REQ-IPD-003)

`ComplianceDecorator` MUST 包装 ILLMProvider,在 `generate()` / `generate_stream()` 调用前后 emit compliance log events 到 `IInteractionBus`(MVP 仅日志,不做 PII 检测)。

#### Scenario: prompt hash 记录

- **WHEN** `ComplianceDecorator::generate(req, token)` 被调用
- **THEN** MUST 计算 `prompt_hash = std::hash<std::string>{}(req.prompt)`
- **AND** MUST emit `compliance.log` event 到 `IInteractionBus`,payload 含 `{prompt_hash, model, tenant_id, timestamp}`
- **AND** MUST NOT 在 payload 中存原始 prompt 文本(避免 secret 泄露,per ADR-0031 §决策 7)

#### Scenario: completion hash 记录

- **WHEN** `inner_->generate()` 返回成功结果
- **THEN** MUST 计算 `completion_hash = std::hash<std::string>{}(result.text)`
- **AND** MUST emit `compliance.log` event,payload 含 `{completion_hash, prompt_hash, model, timestamp}`

#### Scenario: 流式 completion hash

- **WHEN** `ComplianceDecorator::generate_stream(req, token)` 流结束
- **THEN** MUST 计算流累积 `completion_hash`(把所有 chunk 拼起来再 hash)
- **AND** MUST emit 单个 `compliance.log` event

#### Scenario: MVP 默认禁用

- **WHEN** DSLEngine 构造时未显式启用 ComplianceDecorator
- **THEN** MUST NOT 自动包装 ComplianceDecorator(避免无意义的日志噪音)
- **AND** Phase 6+ 启用 PII 检测后再 opt-in 启用

### Requirement: rate-limit-decorator (REQ-IPD-004)

`RateLimitDecorator` MUST 包装 ILLMProvider,在 `generate()` 前检查 token bucket 配额(多租户场景)。

#### Scenario: 默认禁用

- **WHEN** DSLEngine 构造时未显式启用 RateLimitDecorator
- **THEN** MUST NOT 自动包装 RateLimitDecorator(单租户场景无意义)

#### Scenario: opt-in 启用 + 配额检查

- **GIVEN** DSLEngine opt-in 启用 RateLimitDecorator(多租户部署)
- **AND** tenant_id "tenant-A" 配置 token bucket `10000 tokens/minute`
- **WHEN** 调用 `generate(req)` 且 `req` 含 `tenant_id = "tenant-A"`
- **THEN** MUST 检查 token bucket 剩余配额
- **AND** 若配额不足,MUST 返回 `Result::failure(LLMError::Code::RateLimited, "tenant quota exceeded")`
- **AND** 若配额充足,MUST 转发到 `inner_->generate(req, token)`

#### Scenario: 流式生成配额扣减

- **WHEN** `RateLimitDecorator::generate_stream(req, token)` 流开始
- **THEN** MUST 预扣 token bucket 的 `max_tokens`(从 `req.params.max_tokens` 读取)
- **AND** 流结束后 MUST 按实际 token 消耗调整(退还剩余)
- **AND** 流被中断 MUST 按已消耗 token 扣减(不退还)

### Requirement: decorator-deployment-order (REQ-IPD-005)

DSLEngine 构造时 MUST 按特定顺序部署装饰器链,顺序变更 MUST NOT 影响业务行为但 MUST 影响审计完整性。

#### Scenario: 默认部署顺序

- **WHEN** DSLEngine 构造器初始化 `llm_provider_`
- **THEN** MUST 按以下顺序包装(从外到内):
  1. **CostTrackingDecorator**(最外层,保证所有 LLM 调用被计费,即使 inner 抛错)
  2. **ComplianceDecorator**(若 opt-in 启用,记录 prompt/completion hash)
  3. **RateLimitDecorator**(若 opt-in 启用,在 generate 前检查配额)
  4. **inner_**(推理 Plugin ILLMProvider 或 OrchestrationILLMProvider)
- **AND** 装饰器链总深度 MUST ≤ 4(含 inner_)

#### Scenario: 装饰器链测试

- **WHEN** 集成测试构造 4 个嵌套装饰器(CostTracking → Compliance → RateLimit → inner)
- **THEN** CostTracking MUST 收到最终的 `Result` (含 success 或 error)
- **AND** Compliance MUST 在 generate 前收到 `req`
- **AND** RateLimit MUST 在 generate 前检查配额
- **AND** 业务返回值 MUST 与无装饰器时**完全一致**

#### Scenario: 装饰器链性能开销

- **WHEN** 跑 10000 次 generate,每次均带装饰器链
- **THEN** 总开销 MUST < 100ms(每个调用 < 10μs)
- **AND** 相对 llama_decode ~200ms 开销 MUST < 0.005%

#### Scenario: set_llm_provider 重新包装

- **WHEN** 用户调用 `engine->set_llm_provider(custom_provider)`
- **THEN** DSLEngine MUST 重新包装 Decorator 链(按 REQ-IPD-005 顺序)
- **AND** 先 `move` 旧 provider 的生命周期由新链接管
- **AND** 确保 `get_llm_provider()` 返回新链的最外层装饰器(含 CostTrackingDecorator,保证自定义 provider 也不绕过计费)