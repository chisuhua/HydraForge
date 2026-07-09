## Why

HydraForge 当前 ILLMProvider 调用链存在 5 个长期演进风险,经 Oracle 长期分析(2026-07-06, session `ses_0c8f3f954ffeiw8s4X7xAW9hTJ`)识别为 P0 ship gate:

1. **Budget hole**: `node_executor.cpp:271-274`(execute_generate_subgraph)、`:447-463`(execute_yield)、`SimpleCognitiveOrchestrator` 三处 LLM 调用绕过 `tool_registry_->set_cost_callback()`,**零计费**。Phase 5 ship 前必须修复。
2. **3 层 ILLMProvider 链的语义误用**: 编排 Plugin 的 `OrchestrationILLMProvider::generate()` 内部经 `internal_registry_.call_tool("inference/generate", ...)` 走 ToolCoordinator 路径,但 LLM "thought" 不应经过 audit pipeline(ADR-0031 §决策 5)。
3. **Cloud 与 Local 路径架构割裂**: ADR-0042 §4 决议 cloud 留核心 HTTP 客户端,与 ADR-0005 §3 的 plugin-extensible 哲学矛盾,且无法支持内部 gateway 代理 / 自定义 auth 等部署需求。
4. **ADR-0042 Phase 3 退役 trigger 不可执行**: "Telemetry 30 天零实例化" 在当前仓库无 telemetry 基础设施情况下无法评估。
5. **Adversarial review 推荐的 SamplerStrategy/BatchingQueue/命名修正未反映到 ADR**: C14 proposal 仍引用 `llama_engine/` namespace,与新 ADR-0035 `inference.*` 不一致。

## What Changes

本 change 基于 Oracle 长期分析 5 项决策(D1-D5)+ D2 中间方案 A'(Dual Consumer Model),实施以下变更:

### D1: Cloud plugin 化(P0)
- **BREAKING**:`CloudLLMAdapter` 从核心代码 `src/common/llm/` 移至 `pdk/cloud/` 作为 first-party plugin
- `LLMProviderFactory` 路由"openai/anthropic/deepseek/qwen/moonshot/custom" → 改为 dlopen plugin + `pdk_create_llm_provider()` 工厂符号
- `anthropic` 协议从"路由到不存在的实现"修复为 first-party plugin 内显式分支(避免当前 CloudLLMAdapter 仅 OpenAI 兼容的 bug)

### D2': Dual Consumer Model(P0)
- **修改 ADR-0045 §2**: `OrchestrationILLMProvider::generate()` 内部从 `internal_registry_.call_tool("inference/generate", ...)` 改为直连 `inference_provider_->generate()`(共享 `shared_ptr<ILLMProvider>`)
- **修改 ADR-0045 §6**: 删"双 IToolRegistry"架构(internal + external bypass),改为单一 external registry,编排 Plugin 不再区分内/外调用
- Agent 循环(ReAct/PlanExecute/ForkJoin)通过 `engine_->get_llm_provider()` 获取 raw ILLMProvider*,绕开编排包装器
- 编排行 ILLMProvider 保留作为路由 + 会话管理层,服务 DSLEngine/NodeExecutor 外部消费者
- `OrchestrationStream` 流式聚合(细粒度/聚合/粗粒度)保留

### D3: ILLMProviderDecorator + CostTrackingDecorator(P0)
- **新增** `include/agenticdsl/contract/i_llm_provider_decorator.h`:`ILLMProviderDecorator` 接口
- **新增** `src/common/llm/cost_tracking_decorator.{h,cpp}`: 包装 generate/generate_stream,记录 token + 调 `IBudgetController`
- **新增** `src/common/llm/compliance_decorator.{h,cpp}`: prompt/completion 扫描(MVP: 仅日志,Phase 6 接 PII 检测)
- **新增** `src/common/llm/rate_limit_decorator.{h,cpp}`: 多租户 token-bucket 限流(默认未启用)
- DSLEngine 构造时按需包装 Decorator 链,部署在 NodeExecutor 之前
- 修 budget hole: 三处直调 LLM 路径经装饰器链后**全部计费**

### D4: ADR-0042 §2 + §4 修订(P0)
- **修改 ADR-0042 §2 Phase 3 trigger**: "Telemetry 30 天零实例化" → "Phase 2 完成后 2 release cycles"
- **修改 ADR-0042 §2 Phase 2**: `"local"` 配置从"删除映射"改为"remap 到 InferencePlugin"(用户配置零改动)
- **修改 ADR-0042 §2 deprecation scope**: 同时 deprecate `LlamaAdapterProvider` 和 `LlamaAdapter`(底层 HTTP 包装)
- **修改 ADR-0042 §4**: 推翻原"cloud 留核心"决议,与 D1 一致
- **新增** `src/common/llm/llama_adapter_provider.h` 添加 `[[deprecated("Use pdk/inference_engine/ plugin instead, see ADR-0042 §2")]]`

### D5: `available_models()` pure virtual(P1)
- **修改 ILLMProvider**: `available_models()` 从默认空实现改为 pure virtual
- 修改 4 个实现(MockLLMProvider/CloudLLMAdapter/LlamaAdapterProvider/新 InferencePlugin)显式 override

### 命名统一(P0)
- **修改 ADR-0035 §2 工具命名表**: 删 `llama_engine/` 残留引用,统一 `inference.*`
- **修改 OpenSpec `phase5-llama-engine-plugin/proposal.md`**: 引用同步更新
- **新增 ADR-0035/0038 注记**: SamplerStrategy + BatchingQueue deferred(per adversarial review 2026-07-06)

### 文档修订(P0)
- **修改 ADR-0045**: §2 + §6 大幅简化(-30% 行数)
- **修改 ADR-0035 §1.1**: 三层消费链图重画(Dual Consumer Model)
- **修改 ADR-0042**: §2 + §4 全面修订(见 D4)
- **修改 ADR-0001**: `available_models()` pure virtual 决议;记 `is_available()`/`name()` deferred indefinitely;记 `Result<T,E>` → `std::expected` 等 C++23 基线

### 非目标(Non-goals)
- **不做 multi-modal input/output**(GenerationRequest.prompt 保持 std::string,GenerationResult.text 保持 std::string)。`ModelCapability::Vision` 枚举值保留但**无实现路径**。Multi-modal scoping 留 Phase 6。
- **不做 PluginInfo v2 完整实施**(`dependencies[256]` 拓扑排序)。ADR-0041 §1.5 已设计但本次 change 不实施完整字段,仅在 cloud plugin 阶段同步追加依赖检查。
- **不做 Anthropic 原生 API 协议完整迁移**。CloudLLMAdapter 当前 OpenAI 兼容协议已 ship,Anthropic 用户通过 OpenAI 兼容代理或第三方 plugin。
- **不做 Result<T,E> → std::expected 重命名**。等 C++23 编译器基线达成后(>2 年)再 `using Result = std::expected` 一行替换。

## Capabilities

### New Capabilities
- `illmprovider-call-chain`: 定义 Phase 5 ILLMProvider 调用链 v2 架构 — Dual Consumer Model(D2')、Agent 循环直连、OrchestrationILLMProvider 路由 + 会话职责边界、Plugin Loader 集成、错误传播路径
- `illmprovider-decorator`: 定义 `ILLMProviderDecorator` 抽象 + CostTrackingDecorator(修 budget hole)+ ComplianceDecorator(MVP 日志)+ RateLimitDecorator(默认禁用)
- `cloud-llm-plugin`: 定义 CloudLLMAdapter 作为 first-party plugin(`pdk/cloud/`)的导出符号 + 工厂符号 + Anthropic 协议分支 + PluginInfo 配置

### Modified Capabilities
- `plugin-loader`: 添加 `pdk_create_llm_provider` dlsym 查找 + abi_version 检查(cloud + inference plugin 均需)+ 5 符号完整支持
- `model-router-plugin`: 添加 RouterDecorator 集成点(编排 ILLMProvider 内部调用 `router_.select()`)+ `RouterDecorator` 类规范

## Impact

### 受影响代码

| 模块 | 文件 | 变更类型 |
|---|---|---|
| 核心 | `src/core/engine.{h,cpp}` | 构造时包装 Decorator 链;`run()` 传 raw ILLMProvider* 给 NodeExecutor |
| 核心 | `src/modules/executor/node_executor.{h,cpp}` | 受装饰器链包装后所有 `llm_provider_->generate()` 自动计费 |
| 核心 | `src/modules/cognitive/simple_orchestrator.cpp` | 同上 |
| 核心 | `src/common/llm/llm_provider_factory.cpp` | cloud 路由改为 dlopen plugin;`local` remap 到 InferencePlugin |
| 核心 | `src/common/llm/llm_types.h` | `available_models()` 变 pure virtual |
| 核心 | `src/common/llm/mock_provider.{h,cpp}` | override `available_models()` |
| 核心 | `src/common/llm/llama_adapter_provider.{h,cpp}` | `[[deprecated]]` 标注 |
| 核心 | `src/common/llm/llama_adapter.{h,cpp}` | `[[deprecated]]` 标注 |
| Plugin | `pdk/cloud/{CMakeLists.txt,src/,include/}` | 新建 first-party cloud plugin |
| Plugin | `pdk/inference_engine/{CMakeLists.txt,src/,include/}` | 新建推理 plugin(依赖 OpenSpec `phase5-llama-engine-plugin`) |
| Plugin | `include/agenticdsl/contract/i_llm_provider_decorator.h` | 新建抽象接口 |
| Plugin | `src/common/llm/cost_tracking_decorator.{h,cpp}` | 新建 |
| Plugin | `src/common/llm/compliance_decorator.{h,cpp}` | 新建 |
| Plugin | `src/common/llm/rate_limit_decorator.{h,cpp}` | 新建 |
| Plugin | `include/agenticdsl/pdk/llm_provider_decorator.h` | 新建 PDK helper |
| Plugin | `include/agenticdsl/pdk/router_decorator.h` | 新建 PDK helper |
| Adapter | `src/common/llm/cloud_adapter.{h,cpp}` | 移至 `pdk/cloud/src/` 作为 first-party plugin 实现 |
| ADR | `docs/adr/adr-0001-*.md` | 加 pure virtual + deferred 注记 |
| ADR | `docs/adr/adr-0035-*.md` | §1.1 图重画 + §2 命名统一 |
| ADR | `docs/adr/adr-0042-*.md` | §2 + §4 全面修订 |
| ADR | `docs/adr/adr-0045-*.md` | §2 + §6 大幅简化 |
| OpenSpec | `openspec/changes/phase5-llama-engine-plugin/proposal.md` | 同步 ADR-0035 §2 命名 |
| 测试 | `tests/test_cost_tracking_decorator.cpp` | 新建 |
| 测试 | `tests/test_illmprovider_decorator.cpp` | 新建 |
| 测试 | `tests/test_orchestration_dual_consumer.cpp` | 新建 |
| 测试 | `tests/test_cloud_llm_plugin.cpp` | 新建 |
| 测试 | `tests/test_available_models_pure_virtual.cpp` | 新建 |

### 受影响 API

| API | 影响 |
|---|---|
| `ILLMProvider::available_models()` | 变 pure virtual(breaking — 任何子类必须 override) |
| `DSLEngine::set_llm_provider(unique_ptr<ILLMProvider>)` | 签名不变,但内部改为先包装 Decorator 链再 set |
| `LLMProviderFactory::create()` | cloud 路由改为 dlopen,行为保持但内部实现变 |
| `LlamaAdapterProvider` 构造函数 | `[[deprecated]]` 警告(非 breaking) |
| `LlamaAdapter` 构造函数 | `[[deprecated]]` 警告(非 breaking) |
| `OrchestrationILLMProvider::generate()` | 内部直连而非 call_tool(行为等价但性能更优) |
| 新 `pdk_create_llm_provider()` | cloud plugin 导出符号 |

### 受影响依赖
- 无外部依赖新增/删除
- llama.cpp / httplib / nlohmann_json 依赖不变
- pdk/ 子目录新增 2 个 plugin(`cloud` + `inference_engine`),`pdk/CMakeLists.txt` 同步更新

### Ship Gate 影响
- **P0**: CostTrackingDecorator + budget hole 修复必须 Phase 5 ship 前完成
- **P0**: Cloud plugin 化是 Phase 5 B2 实施路径 D(PoC)的对称(cloud 也走 plugin 机制)
- **P1**: `available_models()` pure virtual 可与 P0 并行 ship,不阻塞

### 关联 ADR / OpenSpec Changes
- **依赖**: ADR-0035(推理 plugin 规范)、ADR-0045(编排 plugin 规范)、ADR-0042(ILLMProvider 演进)、ADR-0041(PluginLoader 生命周期)
- **被依赖**: OpenSpec `phase5-llama-engine-plugin`(C14,需同步更新命名)、OpenSpec `phase5-batching-queue-plugin`(C15,与本 change 正交)
- **关联 ADR-0001**(ILLMProvider 流式接口)、**ADR-0005**(LLM 后端配置与工厂,需添加 plugin 路径注释)
- **关联 ADR-0031**(执行策略,确认编排 ILLMProvider 不经 ToolCoordinator 的语义澄清)
- **关联 ADR-0034**(IModelRouter,RouterDecorator 集成点)
- **关联 ADR-0033**(Session Hierarchy,ensure_session() 在编排 ILLMProvider 内的职责)
- **关联 ADR-0019**(IInteractionBus,订阅 inference.* events)