# illmprovider-call-chain Specification

## Purpose

Phase 5 ILLMProvider 调用链 v2 架构 — 经 Oracle 长期演进分析(2026-07-06, session `ses_0c8f3f954ffeiw8s4X7xAW9hTJ`)识别 5 项决策(D1-D5 + D2' 中间方案 A'),实施 Dual Consumer Model(OrchestrationILLMProvider 直连推理 + Agent 循环 raw 直连)、Decorator 模式修 Budget Hole、Cloud Plugin 化统一 Backend 插件机制、ADR-0042 §2 + §4 修订、`available_models()` pure virtual、命名统一。

## ADDED Requirements

### Requirement: dual-consumer-model (REQ-ICC-001)

HydraForge ILLMProvider 调用链 MUST 支持 **Dual Consumer Model**: 同一推理 Plugin ILLMProvider 同时被两个 consumer 通过不同路径访问。

#### Scenario: DSLEngine/NodeExecutor 经编排 ILLMProvider

- **WHEN** `NodeExecutor::execute_generate_subgraph()` 调用 `llm_provider_->generate(req)`
- **THEN** 经 `OrchestrationILLMProvider::generate()` 处理(路由 + 会话管理)
- **AND** `OrchestrationILLMProvider` 内部直接调用 `inference_provider_->generate()`(共享 `shared_ptr<ILLMProvider>`,非 `call_tool`)
- **AND** **不**经过 `internal_registry_.call_tool("inference/generate", ...)`

#### Scenario: Agent 循环 raw 直连推理 Plugin ILLMProvider

- **WHEN** `PlanExecuteLoop::plan_phase()` / `verify_phase()` 调用 `engine_->get_llm_provider()->generate(req)`
- **THEN** `engine_->get_llm_provider()` MUST 返回 raw `ILLMProvider*`(推理 Plugin 实现,**非** OrchestrationILLMProvider 包装)
- **AND** **不**经过编排层包装(无路由、无会话管理)

#### Scenario: OrchestrationStream 流式聚合保留

- **WHEN** `OrchestrationILLMProvider::generate_stream()` 被调用
- **THEN** MUST 保留流式聚合策略(细粒度 / 聚合粒度 / 粗粒度)
- **AND** MUST 通过 `OrchestrationStream::next(token)` 内部从推理 Plugin 拉取 token,经聚合后返回上层

### Requirement: orchestration-illmprovider-direct-connection (REQ-ICC-002)

`OrchestrationILLMProvider::generate()` MUST 内部直连推理 Plugin ILLMProvider,绕开 `internal_registry_.call_tool("inference/generate", ...)` 路径。

#### Scenario: 直连 generate 调用

- **WHEN** DSL workflow 经 `OrchestrationILLMProvider::generate(req, token)` 调 LLM
- **THEN** MUST 顺序执行:(1) `router_.select(inference_provider_->available_models(), req)` 选择模型;(2) `ensure_session(selected_model)` 创建/复用会话;(3) `apply_per_request_config(req, model)` 应用 per-call 配置;(4) `inference_provider_->generate(tuned, token)` 直连推理
- **AND** **不**触发 `internal_registry_` 的 `call_tool` 函数调用
- **AND** **不**触发 `ToolCoordinator` 的 layer check / approval 检查
- **AND** **不**emit `orchestration.audit.internal.*` event(由推理 Plugin 自带 audit)

#### Scenario: 直连 generate_stream 保持流式

- **WHEN** DSL workflow 经 `OrchestrationILLMProvider::generate_stream(req, token)` 调流式 LLM
- **THEN** MUST 构造 `OrchestrationStream` 实例
- **AND** `OrchestrationStream` 内部 MUST 通过 `inference_provider_->generate_stream(req, token)` 获取推理 Plugin 流
- **AND** `OrchestrationStream::next(token)` MUST 按聚合策略(细粒度/聚合粒度/粗粒度)分块返回

#### Scenario: stop_token 传播

- **WHEN** 调用方在 `stop_token.stop_requested()` 触发取消
- **THEN** 直连路径 MUST 立即停止推理(传播 `stop_token` 到 `inference_provider_->generate/generate_stream`)
- **AND** 编排层 MUST 清理 `ensure_session()` 创建的临时会话状态

#### Scenario: verify_phase 重启语义(per Task 4.10)

- **WHEN** `PlanExecuteLoop::verify_phase()` 返回 **retryable failure**(per ADR-0045 §2.4 错误分类)
- **THEN** `PlanExecuteLoop` MUST 重启主循环从 `plan_phase()` 开始新轮迭代
- **AND** MUST NOT throw exception
- **AND** `retry_count` MUST 递增
- **AND** 若 `retry_count >= MAX_RETRIES` 则 MUST 终止循环并 return `failure`

- **WHEN** `PlanExecuteLoop::verify_phase()` 返回 **non-retryable failure**
- **THEN** `PlanExecuteLoop` MUST 终止循环并 return `failure`
- **AND** MUST NOT throw exception

### Requirement: agent-loop-raw-illmprovider-access (REQ-ICC-003)

Agent 循环(ReAct / PlanExecute / ForkJoin) MUST 通过 `engine_->get_llm_provider()` 获取 `ILLMProvider*`,绕开**编排包装**(OrchestrationILLMProvider),但仍经过 Decorator 链(CostTracking/Compliance/RateLimit)。

> **语义澄清**: "raw" 指"非 OrchestrationILLMProvider 包装",**不是**"无任何装饰器"。Agent 循环仍需经过 Decorator 链以保证计费/合规/限流。`engine_->get_llm_provider()` 返回 Decorator 链最外层或推理 Plugin provider(若无编排包装)。

#### Scenario: PlanExecuteLoop 直连

- **WHEN** `PlanExecuteLoop::plan_phase()` / `verify_phase()` 调 LLM
- **THEN** MUST 调 `engine_->get_llm_provider()->generate(req)`
- **AND** `engine_->get_llm_provider()` MUST 返回非 OrchestrationILLMProvider 的 ILLMProvider(Decorator 链最外层或推理 Plugin provider)
- **AND** `engine_->get_llm_provider()` 签名 MUST 保持不变(向后兼容)

#### Scenario: SimpleCognitiveOrchestrator 直连

- **WHEN** `SimpleCognitiveOrchestrator::react_once()` 调 LLM
- **THEN** MUST 调 `llm_->generate(req)`(成员 `ILLMProvider* llm_`)
- **AND** `llm_` MUST 指向非 OrchestrationILLMProvider 的 ILLMProvider(经 Decorator 链)
- **AND** **不**经过 `OrchestrationILLMProvider`

#### Scenario: ForkJoinLoop 不直连

- **WHEN** `ForkJoinLoop::run(branches, ctx)` 执行
- **THEN** MUST NOT route LLM generation through `OrchestrationILLMProvider`(per Dual Consumer Model direct-call semantics)
- **AND** Branch handler 内部如果需要 LLM,各自独立通过 `engine_->get_llm_provider()` 获取 raw `ILLMProvider*` 直连
- **AND** MUST NOT 经过编排包装的 `OrchestrationILLMProvider::generate()`(避免 fork 分支共享编排层 session/router 状态导致 race)

### Requirement: illmprovider-pure-virtual-available-models (REQ-ICC-004)

`ILLMProvider::available_models()` MUST 是 **pure virtual** 方法,任何 ILLMProvider 子类 MUST override。

#### Scenario: 接口变 pure virtual

- **WHEN** 检查 `src/common/llm/llm_types.h` 中 `ILLMProvider` 接口定义
- **THEN** `available_models() const` MUST 声明为 `virtual std::vector<ModelInfo> available_models() const = 0`
- **AND** MUST NOT 包含默认实现(无 `{ return {}; }`)

#### Scenario: 5 个实现显式 override

- **WHEN** 编译以下 5 个 ILLMProvider 实现
- **THEN** MUST 各自显式 `override` `available_models()`:
  - `MockLLMProvider` — 返回 1 个 mock-llm-v1
  - `CloudLLMAdapter` (在 `pdk/cloud/src/`)— 返回 `config.model`
  - `LlamaAdapterProvider` — 返回 `llama_config.model`
  - `LlamaAdapterProvider` deprecated 后 — 返回空 + warning log
  - `InferencePlugin` (在 `pdk/inference_engine/src/`)— 返回 loaded gguf models 列表

#### Scenario: 编译失败兜底

- **WHEN** 第三方 ILLMProvider 子类未 override `available_models()`
- **THEN** MUST 编译失败(纯虚方法未实现)
- **AND** 编译错误信息 MUST 包含 `available_models` 字串

### Requirement: tool-naming-inference-prefix (REQ-ICC-005)

推理 Plugin 暴露的所有 DSL workflow 工具 MUST 使用 `inference/` 前缀(slash 分隔, per ADR-0043)。

#### Scenario: 13 个推理工具命名

- **WHEN** 推理 Plugin 注册工具到 `IToolRegistry`
- **THEN** 工具名 MUST 全部以 `inference/` 开头
- **AND** MUST NOT 出现 `llama_engine/` 前缀(per adversarial review 2026-07-06 命名修正)
- **AND** 命名清单 MUST 包含:
  - `inference/engine/init`
  - `inference/model/{load, unload, list, switch}`
  - `inference/session/{create, destroy}`
  - `inference/generate`
  - `inference/generate/stream`
  - `inference/configure` (L3a 动态配置)
  - `inference/sampler/configure` (L3b 采样策略)
  - `inference/get/{status, models}`

#### Scenario: OpenSpec C14 同步

- **WHEN** OpenSpec change `phase5-llama-engine-plugin/proposal.md` 引用推理 Plugin 工具
- **THEN** MUST 使用 `inference/*` 命名
- **AND** MUST NOT 出现 `llama_engine/*` 残留引用
- **AND** 同步修订 C14 proposal.md

### Requirement: orchestration-single-registry (REQ-ICC-006)

编排 Plugin MUST 仅持有 **单一 `IToolRegistry`**(不再区分 internal / external),`internal_registry_` 与 `external_registry_` 双架构 MUST 删除。

#### Scenario: 单一 registry 引用

- **WHEN** 编排 Plugin 构造时注入 `IToolRegistry&`
- **THEN** MUST 存储为单一成员 `registry_`(无 internal/external 区分)
- **AND** Agent 循环 MUST NOT 调用 `internal_registry_.call_tool("inference/generate", ...)`(已删除此路径)
- **AND** Agent 循环 MUST NOT 通过 registry 路径调 LLM(走 raw `engine_->get_llm_provider()`)

#### Scenario: 工具调用经外部 registry

- **WHEN** Agent 循环需要调外部工具(非 LLM,如 `inference/get/status`)
- **THEN** MUST 通过 `registry_.call_tool(...)` 调用
- **AND** 经 ToolCoordinator 走 approval pipeline(per ADR-0031 §决策 5)
- **AND** 不绕过审批(无 internal 豁免)

#### Scenario: ADR-0045 §6 简化

- **WHEN** 检查 `docs/adr/adr-0045-orchestration-plugin-spec.md` §6 内容
- **THEN** MUST NOT 包含 "内部调用 vs 外部调用 — ToolCoordinator 豁免机制" 章节
- **AND** MUST NOT 包含 "internal_registry" / "external_registry" 字段描述
- **AND** MUST 简化为 "单一 IToolRegistry + ToolCoordinator 集成"

### Requirement: deferred-sampler-batching-strategy (REQ-ICC-007)

`SamplerStrategy` 和 `BatchingQueue` 接口 MUST 标记为 **deferred**,per adversarial review 2026-07-06。

#### Scenario: SamplerStrategy deferred 注释

- **WHEN** 检查 `docs/adr/adr-0035-inference-engine-plugin-spec.md` §4 配置分层
- **THEN** MUST 含 "SamplerStrategy 接口 deferred 到 Phase 6+ (per adversarial review)" 注释
- **AND** 当前实现 MUST 仅支持 basic sampler 链(L3a + L3b 静态)

#### Scenario: BatchingQueue deferred 注释

- **WHEN** 检查 `docs/adr/adr-0038-dynamic-config-interface.md`
- **THEN** MUST 含 "BatchingQueue 接口 deferred 到第二个推理 backend 实现时 (per adversarial review)" 注释
- **AND** 当前实现 MUST 仅支持单请求 FIFO loop(per OpenSpec `phase5-batching-queue-plugin` reference impl)

---

### Requirement: set-llm-provider-decorator-rewrapping (REQ-ICC-008)

`DSLEngine::set_llm_provider()` MUST 在设置新 provider 时重新包装 Decorator 链,避免自定义 provider 绕过计费。

#### Scenario: set_llm_provider 触发重新包装

- **WHEN** 用户调用 `engine->set_llm_provider(custom_provider)`
- **THEN** DSLEngine MUST 先 `move` 旧 provider,再按 REQ-IPD-005 顺序包装 Decorator 链
- **AND** `engine->get_llm_provider()` MUST 返回新链的最外层装饰器(含 CostTrackingDecorator)
- **AND** 该行为 MUST 与构造器中初始包装一致(共享同一 `decorate_provider()` 私有方法)