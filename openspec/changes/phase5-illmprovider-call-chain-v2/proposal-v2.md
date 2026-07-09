> **VERSION**: v2 (修订版) | **STATUS**: 📋 Proposal ✅ ready for review | **SUPERSEDES**: v1 (proposal.md)
>
> v2 消除 v1 中 5 项歧义、修正 3 项不一致、解决 2 项已知风险。详见各节标注。

## §0 目标澄清

**本 change 不修改 `ILLMProvider::generate()` 签名。**

v1 proposal 及 `docs/active-status.md` 第 94 行曾出现 "ILLMProvider 接口升级 (`generate()` → `generate(IRuntimeContext)`)" 的提法，该提法已在 active-status 中删除。本 change 的 `ILLMProvider` 接口变更仅限于:

- **D5**: `available_models()` 从默认空实现改为 **pure virtual**（标注为 BREAKING change to ADR-0001）
- **其他**: `generate()` / `generate_stream()` 签名完全不变

**"接口升级"的实际含义**是: `OrchestrationILLMProvider::generate()` 内部实现从 `call_tool` 改为直连推理 Plugin（Dual Consumer Model，D2'），而非 ILLMProvider 接口签名的修改。

---

## Why

HydraForge 当前 ILLMProvider 调用链存在 5 个长期演进风险，经 Oracle 长期分析(2026-07-06, session `ses_0c8f3f954ffeiw8s4X7xAW9hTJ`)识别为 P0 ship gate:

1. **Budget hole**: `node_executor.cpp:271-274`(execute_generate_subgraph)、`:447-463`(execute_yield)、`SimpleCognitiveOrchestrator` 三处 LLM 调用绕过 `tool_registry_->set_cost_callback()`,**零计费**。Phase 5 ship 前必须修复。
2. **3 层 ILLMProvider 链的语义误用**: 编排 Plugin 的 `OrchestrationILLMProvider::generate()` 内部经 `internal_registry_.call_tool("inference/generate", ...)` 走 ToolCoordinator 路径，但 LLM "thought" 不应经过 audit pipeline(ADR-0031 §决策 5)。
3. **Cloud 与 Local 路径架构割裂**: ADR-0042 §4 决议 cloud 留核心 HTTP 客户端，与 ADR-0005 §3 的 plugin-extensible 哲学矛盾，且无法支持内部 gateway 代理 / 自定义 auth 等部署需求。
4. **ADR-0042 Phase 3 退役 trigger 不可执行**: "Telemetry 30 天零实例化" 在当前仓库无 telemetry 基础设施情况下无法评估。
5. **Adversarial review 推荐的 SamplerStrategy/BatchingQueue/命名修正未反映到 ADR**: C14 proposal 仍引用 `llama_engine/` namespace，与新 ADR-0035 `inference.*` 不一致。

## What Changes

本 change 基于 Oracle 长期分析 5 项决策(D1-D5)+ D2 中间方案 A'(Dual Consumer Model)，实施以下变更:

### D1: Cloud plugin 化(P0)

- **BREAKING**: `CloudLLMAdapter` 从核心代码 `src/common/llm/` 移至 `pdk/cloud/` 作为 first-party plugin
- **Plugin .so 命名**: `libhydraforge_pdk_cloud.so`（与 C14 `libhydraforge_pdk_llama_engine.so` 并列）
- `LLMProviderFactory` 路由 "openai/anthropic/deepseek/qwen/moonshot/custom" → 改为 dlopen plugin + `pdk_create_llm_provider()` 工厂符号
- `anthropic` 协议从 "路由到不存在的实现" 修复为 first-party plugin 内显式分支（避免当前 CloudLLMAdapter 仅 OpenAI 兼容的 bug）
- **"local" 配置 remap**: 映射到 C14 的 `llama_engine` plugin（不是本 change 的 `inference_engine`）

### D2': Dual Consumer Model(P0)

- **修改 ADR-0045 §2**: `OrchestrationILLMProvider::generate()` 内部从 `internal_registry_.call_tool("inference/generate", ...)` 改为直连 `inference_provider_->generate()`(共享 `shared_ptr<ILLMProvider>`)
- **修改 ADR-0045 §6**: 删 "双 IToolRegistry"架构(internal + external bypass)，改为单一 external registry，编排 Plugin 不再区分内/外调用
- Agent 循环(ReAct/PlanExecute/ForkJoin)通过 `engine_->get_llm_provider()` 获取 raw ILLMProvider*，绕开编排包装器
- 编排层 ILLMProvider 保留作为路由 + 会话管理层，服务 DSLEngine/NodeExecutor 外部消费者
- `OrchestrationStream` 流式聚合(细粒度/聚合/粗粒度)保留

### D3: ILLMProviderDecorator + CostTrackingDecorator(P0)

- **新增** `include/agenticdsl/contract/i_llm_provider_decorator.h`:`ILLMProviderDecorator` 接口
- **新增** `src/common/llm/cost_tracking_decorator.{h,cpp}`: 包装 generate/generate_stream，记录 token + 调 `IBudgetController`
  - **Streaming token 计数精度** (R5 风险解决): `prompt_tokens` 从 `GenerationRequest.params` 或 plugin metadata 获取；`completion_tokens` 从 chunk 数按 plugin 约定换算，或 plugin per-chunk metadata；最终调用 `budget_->record_llm_call(prompt_tokens + completion_tokens, model_name)`
- **新增** `src/common/llm/compliance_decorator.{h,cpp}`: prompt/completion 扫描(MVP: 仅日志，Phase 6 接 PII 检测)
- **新增** `src/common/llm/rate_limit_decorator.{h,cpp}`: 多租户 token-bucket 限流(默认未启用)
- DSLEngine 构造时按需包装 Decorator 链，部署在 NodeExecutor 之前
- 修 budget hole: 三处直调 LLM 路径经装饰器链后**全部计费**

### D4: ADR-0042 §2 + §4 修订(P0)

- **修改 ADR-0042 §2 Phase 3 trigger**: "Telemetry 30 天零实例化" → "Phase 2 完成后 2 release cycles"
- **修改 ADR-0042 §2 Phase 2**: `"local"` 配置从 "删除映射"改为 "remap 到 InferencePlugin"（用户配置零改动）
- **修改 ADR-0042 §2 deprecation scope**: 同时 deprecate `LlamaAdapterProvider` 和 `LlamaAdapter`（底层 HTTP 包装）
- **修改 ADR-0042 §4**: 推翻原 "cloud 留核心" 决议，与 D1 一致
- **新增** `src/common/llm/llama_adapter_provider.h` 添加 `[[deprecated("Use pdk/inference_engine/ plugin instead, see ADR-0042 §2")]]`
- **新增** `src/common/llm/llama_adapter.h` 添加 `[[deprecated("LlamaAdapter is deprecated; use pdk/inference_engine/ plugin, see ADR-0042 §2")]]`（R3 风险解决: LlamaAdapter 作为 Phase 1 临时层明确）
- **注意**: C14 fallback 阶段，LlamaAdapter 作为 Phase 1 临时层保留（明确标注 `[[deprecated]]` 但不删除）

### D5: `available_models()` pure virtual(P1)

- **BREAKING change to ADR-0001**: `ILLMProvider::available_models()` 从默认空实现改为 pure virtual
- 第三方 `ILLMProvider` 子类必须 override（否则编译失败）
- 修改 4 个现有实现(MockLLMProvider/CloudLLMAdapter/LlamaAdapterProvider/新 InferencePlugin)显式 override
- **与 v1 的区别**: v2 明确标注此变更的 BREAKING 性质，并在 ADR-0001 修订流程中记录

### 命名统一(P0)

- **修改 ADR-0035 §2 工具命名表**: 删 `llama_engine/` 残留引用，统一 `inference.*`
- **修改 OpenSpec `phase5-llama-engine-plugin/proposal.md`**: 引用同步更新
- **新增 ADR-0035/0038 注记**: SamplerStrategy + BatchingQueue deferred(per adversarial review 2026-07-06)

### Cloud Plugin ABI 符号格式(P0)

- **统一为 C7 数据符号**: Cloud plugin 使用与 C7 Model Router plugin 一致的符号导出格式，**非函数版本**
  ```cpp
  extern "C" const hydraforge::PluginInfo pdk_plugin_info;
  ```
- 而非 `extern "C" const PdkPluginInfo* pdk_plugin_info()`（函数版本）
- 与其他导出符号(`pdk_create_llm_provider`, `pdk_register_tools`, `pdk_plugin_init`, `pdk_plugin_fini`)共存

### 文档修订(P0)

- **修改 ADR-0045**: §2 + §6 大幅简化(-30% 行数)
- **修改 ADR-0035 §1.1**: 三层消费链图重画(Dual Consumer Model)
- **修改 ADR-0042**: §2 + §4 全面修订(见 D4)
- **修改 ADR-0001**: `available_models()` pure virtual 决议；记 `is_available()`/`name()` deferred indefinitely；记 `Result<T,E>` → `std::expected` 等 C++23 基线

### 非目标(Non-goals)

- **不做 multi-modal input/output**(`GenerationRequest.prompt` 保持 `std::string`, `GenerationResult.text` 保持 `std::string`)。`ModelCapability::Vision` 枚举值保留但**无实现路径**。Multi-modal scoping 留 Phase 6。
- **不做 PluginInfo v2 完整实施**(`dependencies[256]` 拓扑排序)。ADR-0041 §1.5 已设计但本次 change 不实施完整字段，仅在 cloud plugin 阶段同步追加依赖检查。
- **不做 Anthropic 原生 API 协议完整迁移**。CloudLLMAdapter 当前 OpenAI 兼容协议已 ship，Anthropic 用户通过 OpenAI 兼容代理或第三方 plugin。
- **不做 Result<T,E> → std::expected 重命名**。等 C++23 编译器基线达成后(>2 年)再 `using Result = std::expected` 一行替换。
- **❌ 不实施 `generate(IRuntimeContext)` 接口升级** — 本 change 不修改 `generate()` 签名。`ILLMProvider` 接口升级的提法已从 `docs/active-status.md` 删除。本 change 唯一接口变更是 `available_models()` → pure virtual(D5)。

## Capabilities

### New Capabilities

- `illmprovider-call-chain`: 定义 Phase 5 ILLMProvider 调用链 v2 架构 — Dual Consumer Model(D2')、Agent 循环直连、OrchestrationILLMProvider 路由 + 会话职责边界、Plugin Loader 集成、错误传播路径。**不涉及 `generate()` 签名修改**。
- `illmprovider-decorator`: 定义 `ILLMProviderDecorator` 抽象 + CostTrackingDecorator(修 budget hole，含 streaming token 计数精度)+ ComplianceDecorator(MVP 日志)+ RateLimitDecorator(默认禁用)
- `cloud-llm-plugin`: 定义 CloudLLMAdapter 作为 first-party plugin(`pdk/cloud/`)的导出符号 + 工厂符号 + Anthropic 协议分支 + PluginInfo 配置。ABI 符号统一为 C7 数据符号。

### Modified Capabilities

- `plugin-loader`: 添加 `pdk_create_llm_provider` dlsym 查找 + abi_version 检查(cloud + inference plugin 均需)+ 5 符号完整支持
- `model-router-plugin`: 添加 RouterDecorator 集成点(编排 ILLMProvider 内部调用 `router_.select()`)+ `RouterDecorator` 类规范

### REQ-ICC-005 工具集限定(v2 明确化)

C16 实施以下 `inference/` 工具集:
| 工具名 | 实施方 |
|--------|:------:|
| `inference/generate` | **C16** (cloud plugin 内部生成) |
| `inference/generate/stream` | **C16** (streaming) |
| `inference/session/create` | **C16** (cloud session 生命周期) |
| `inference/session/destroy` | **C16** |
| `inference/configure` | **C16** (cloud plugin 动态配置) |

C14 独立实施（不与 C16 重叠）:
| 工具名 | 实施方 |
|--------|:------:|
| `inference/engine/init` | **C14** (llama_engine plugin) |
| `inference/engine/generate` | **C14** |
| `inference/engine/stream` | **C14** |
| `inference/engine/status` | **C14** |
| `inference/model/load` | **C14** |
| `inference/model/unload` | **C14** |
| `inference/model/list` | **C14** |
| `inference/model/switch` | **C14** |

二者不重叠。C16 不涉及 engine/model 生命周期管理工具。

## Impact

### 受影响代码

| 模块 | 文件 | 变更类型 |
|---|---|---|
| 核心 | `src/core/engine.{h,cpp}` | 构造时包装 Decorator 链；`run()` 传 raw ILLMProvider* 给 NodeExecutor |
| 核心 | `src/modules/executor/node_executor.{h,cpp}` | 受装饰器链包装后所有 `llm_provider_->generate()` 自动计费 |
| 核心 | `src/modules/cognitive/simple_orchestrator.cpp` | 同上 |
| 核心 | `src/common/llm/llm_provider_factory.cpp` | cloud 路由改为 dlopen plugin；`local` remap 到 InferencePlugin |
| 核心 | `src/common/llm/llm_types.h` | `available_models()` 变 pure virtual(**BREAKING to ADR-0001**) |
| 核心 | `src/common/llm/mock_provider.{h,cpp}` | override `available_models()` |
| 核心 | `src/common/llm/llama_adapter_provider.{h,cpp}` | `[[deprecated]]` 标注 |
| 核心 | `src/common/llm/llama_adapter.{h,cpp}` | `[[deprecated]]` 标注 |
| Plugin | `pdk/cloud/{CMakeLists.txt,src/,include/}` | 新建 first-party cloud plugin |
| Plugin | `pdk/llama_engine/{CMakeLists.txt,src/,include/}` | 新建推理 plugin(依赖 OpenSpec `phase5-llama-engine-plugin` C14) |
| Plugin | `include/agenticdsl/contract/i_llm_provider_decorator.h` | 新建抽象接口 |
| Plugin | `src/common/llm/cost_tracking_decorator.{h,cpp}` | 新建 |
| Plugin | `src/common/llm/compliance_decorator.{h,cpp}` | 新建 |
| Plugin | `src/common/llm/rate_limit_decorator.{h,cpp}` | 新建 |
| Plugin | `include/agenticdsl/plugin/pdk_provider_config.h` | 新建 PdkProviderConfig 纯 POD struct(跨 .so ABI 安全) |
| Plugin | `include/agenticdsl/pdk/llm_provider_decorator.h` | 新建 PDK helper |
| Plugin | `include/agenticdsl/pdk/router_decorator.h` | 新建 PDK helper |
| Adapter | `src/common/llm/cloud_adapter.{h,cpp}` | 移至 `pdk/cloud/src/` 作为 first-party plugin 实现 |
| ADR | `docs/adr/adr-0001-*.md` | 加 pure virtual + BREAKING 注记 + deferred 注记 |
| ADR | `docs/adr/adr-0035-*.md` | §1.1 图重画 + §2 命名统一 |
| ADR | `docs/adr/adr-0042-*.md` | §2 + §4 全面修订 |
| ADR | `docs/adr/adr-0045-*.md` | §2 + §6 大幅简化 |
| OpenSpec | `openspec/changes/phase5-llama-engine-plugin/proposal.md` | 同步 ADR-0035 §2 命名 |
| 测试 | `tests/test_cost_tracking_decorator.cpp` | 新建(4 test cases: 同步计费/流式计费/错误不计费/集成 budget hole) |
| 测试 | `tests/test_cost_tracking_streaming_precision.cpp` | 新建(1 test case: streaming 累积精度) |
| 测试 | `tests/test_decorator_chain_depth.cpp` | 新建(1 test case: 深度限制) |
| 测试 | `tests/test_illmprovider_decorator.cpp` | 新建(综合装饰器链) |
| 测试 | `tests/test_compliance_decorator.cpp` | 新建(3 test cases) |
| 测试 | `tests/test_rate_limit_decorator.cpp` | 新建(3 test cases) |
| 测试 | `tests/test_orchestration_dual_consumer.cpp` | 新建(5 test cases) |
| 测试 | `tests/test_cloud_llm_plugin.cpp` | 新建(5 test cases: 5 符号导出/OpenAI 路由/Anthropic 协议/dlopen 兜底/Lifecycle) |
| 测试 | `tests/test_available_models_pure_virtual.cpp` | 新建(4 test cases) |
| 测试 | `tests/test_plugin_loader_v2.cpp` | 新建(5 test cases: 5 符号查找/lifecycle 顺序/拓扑排序/循环依赖检测/abi_version 向后兼容) |

### 受影响 API

| API | 影响 |
|---|---|
| `ILLMProvider::available_models()` | **变 pure virtual**(BREAKING — 任何子类必须 override，标注为 BREAKING change to ADR-0001) |
| `DSLEngine::set_llm_provider(unique_ptr<ILLMProvider>)` | 签名不变，但内部改为先包装 Decorator 链再 set |
| `LLMProviderFactory::create()` | cloud 路由改为 dlopen，行为保持但内部实现变 |
| `LlamaAdapterProvider` 构造函数 | `[[deprecated]]` 警告(非 breaking) |
| `LlamaAdapter` 构造函数 | `[[deprecated]]` 警告(非 breaking) |
| `OrchestrationILLMProvider::generate()` | 内部直连而非 call_tool(行为等价但性能更优) |
| 新 `pdk_create_llm_provider()` | cloud plugin 导出符号 |
| Cloud plugin `pdk_plugin_info` | 使用 C7 数据符号格式(`extern "C" const PluginInfo`) |

### 受影响依赖
- 无外部依赖新增/删除
- llama.cpp / httplib / nlohmann_json 依赖不变
- pdk/ 子目录新增 2 个 plugin(`cloud` + `llama_engine`)，`pdk/CMakeLists.txt` 同步更新

### Ship Gate 影响
- **P0**: CostTrackingDecorator + budget hole 修复必须 Phase 5 ship 前完成
- **P0**: Cloud plugin 化是 Phase 5 B2 实施路径 D(PoC)的对称(cloud 也走 plugin 机制)
- **P1**: `available_models()` pure virtual 可与 P0 并行 ship，不阻塞

### 测试计数(v2 修正)

- **Baseline**: ~64 ctest（after C13/C14/C15 ship，零 regression）
- **C16 新增**: ~30 测试（decorator 4 + cloud 5 + dual_consumer 5 + streaming_budget 1 + chain_depth 1 + compliance 3 + rate_limit 3 + available_models 4 + plugin_loader_v2 5 + 其他 4）
- **总计**: 64 + 30 = **~94 测试 PASS 零回归**

> **v2 修正说明**: v1 曾声称 "90 测试 (61 baseline + 29 new)"，与实际 34 个 test case 不一致。v2 重新核算为 ~64 baseline + ~30 new = ~94。

### 估时(v2 修正)

| 阶段 | v1 估时 | v2 估时 | 原因 |
|:----:|:-------:|:-------:|------|
| 阶段 1 (Decorator + budget hole) | 1-2d | 2-3d | streaming 精度测试 + 装饰器深度限制 + set_llm_provider 重新包装 |
| 阶段 2 (Dual Consumer Model) | 1-2d | 2-3d | 新建 OrchestrationILLMProvider 类 + 移除 internal_registry_ |
| 阶段 3 (Cloud Plugin 化) | 2-3d | 3-5d | Anthropic 真实协议实现 + SSE + CloudPluginLoader + factory 重构 |
| 阶段 4 (ADR + available_models) | 0.5-1d | 2-3d | 全代码库 ILLMProvider 子类扫描 + 编译失败兜底 + Router 静默失败防护 |
| 阶段 5 (deprecate 标注) | 0.5d | 0.5-1d | 不变 |
| **总计** | **3.5-4d** | **9-13d** | 低估了 Anthropic 协议实施(2-3d)、available_models 全代码库修复(1-2d)、PluginLoader 扩展(1-2d) |

> **v2 修正说明**: v1 估时 3.5-4d 低估。v2 修正为 9-13d,经 Metis 审查(2026-07-09)确认:Anthropic 协议实现(2-3d)、available_models 全代码库子类扫描+修复(1-2d)、PluginLoader 扩展+lifecycle+拓扑排序(1-2d)、集成回归(1-2d)均被低估。PdkProviderConfig 纯 POD 定义额外增加跨 .so ABI 工作。

### 关联 ADR / OpenSpec Changes
- **依赖**: ADR-0035(推理 plugin 规范)、ADR-0045(编排 plugin 规范)、ADR-0042(ILLMProvider 演进)、ADR-0041(PluginLoader 生命周期)
- **被依赖**: OpenSpec `phase5-llama-engine-plugin`(C14，需同步更新命名)、OpenSpec `phase5-batching-queue-plugin`(C15，与本 change 正交)
- **关联 ADR-0001**(ILLMProvider 流式接口)——**BREAKING change: available_models() → pure virtual**
- **关联 ADR-0005**(LLM 后端配置与工厂，需添加 plugin 路径注释)
- **关联 ADR-0031**(执行策略，确认编排 ILLMProvider 不经 ToolCoordinator 的语义澄清)
- **关联 ADR-0034**(IModelRouter，RouterDecorator 集成点)
- **关联 ADR-0033**(Session Hierarchy，ensure_session() 在编排 ILLMProvider 内的职责)
- **关联 ADR-0019**(IInteractionBus，订阅 inference.* events)

## Migration Plan

### 阶段 1 (Day 1-2.5): Decorator + budget hole 修复

1. 创建 `ILLMProviderDecorator` 接口
2. 实现 `CostTrackingDecorator`(含 streaming token 计数精度)
3. DSLEngine 构造时包装
4. 跑现有测试 + 新增 `test_cost_tracking_decorator.cpp`(4 处计费断言)
5. 新增 `test_cost_tracking_streaming_precision.cpp`(streaming 累积精度)
6. 新增 `test_decorator_chain_depth.cpp`(深度限制)
7. 验证: 三处直调路径全部计费(`IBudgetController::record_llm_call` 调用次数 == LLM 调用次数)

### 阶段 2 (Day 3-5): Dual Consumer Model

1. 修订 ADR-0045 §2 + §6
2. 重构 `OrchestrationILLMProvider::generate()` 为直连
3. 验证 Agent 循环仍能调 LLM(`plan_execute_loop.h:121` 用 `engine_->get_llm_provider()` + `simple_orchestrator.cpp:118` 用 `llm_->generate()`，均为 raw ILLMProvider* 直连)
4. 新增 `test_orchestration_dual_consumer.cpp`
5. 验证: 流式聚合 `OrchestrationStream` 仍 emit lifecycle events

### 阶段 3 (Day 6-10): Cloud Plugin 化

1. 创建 `pdk/cloud/` 目录结构
2. 移 CloudLLMAdapter 代码
3. 导出 5 符号(`pdk_plugin_info` 数据符号 / `pdk_register_tools` / `pdk_create_llm_provider` / `pdk_plugin_init` / `pdk_plugin_fini`)
4. 修改 `LLMProviderFactory::create()` cloud 路由为 dlopen
5. 新增 `test_cloud_llm_plugin.cpp`
6. 验证: 6 个 provider 字符串(`openai`/`anthropic`/`deepseek`/`qwen`/`moonshot`/`custom`)行为保持

### 阶段 4 (Day 11-12.5): ADR 修订 + `available_models()` pure virtual

1. 修订 ADR-0001/0035/0042/0045
2. 修改 `llm_types.h` `available_models()` 改 pure virtual (**BREAKING change to ADR-0001**)
3. 修改 4 个现有实现显式 override
4. 新增 `test_available_models_pure_virtual.cpp`
5. 同步 OpenSpec `phase5-llama-engine-plugin/proposal.md` 命名

### 阶段 5 (Day 12.5-13): ADR-0042 §2 退役路径 + deprecate 标注

1. 修改 `LlamaAdapterProvider` 构造函数加 `[[deprecated]]`
2. 修改 `LlamaAdapter` 构造函数加 `[[deprecated]]`
3. 修改 `LLMProviderFactory::create()` "local" / "llama" 路由 → remap 到新 InferencePlugin(Phase 2 真实执行点)
4. 验证: 用户 `provider: "local"` 配置 0 改动仍工作

### Rollback 策略

- **阶段 1 失败**(Decorator 引入 bug): git revert commit，`LLMProviderFactory` 暂时不包装 Decorator，budget hole 临时接受
- **阶段 2 失败**(Dual Consumer 破坏 Agent 循环): 保留 ADR-0045 §2 原 call_tool 实现，git revert
- **阶段 3 失败**(Cloud plugin 化引入 dlopen 兼容问题): 保留 CloudLLMAdapter 在 `src/common/llm/` 双位置，factory 路由保留旧核心路径作为 fallback
- **阶段 4 失败**(`available_models()` pure virtual 编译失败): 回退到默认空实现，加 `// BREAKING: enforce in ADR-0001 v2` 注释
- **阶段 5 失败**(deprecate 编译警告噪音过大): 移除 `[[deprecated]]`，改用运行时警告

每个阶段 ship 独立 commit，可独立 revert。**Phase 5 ship gate** = 阶段 1-4 全部完成 + 零回归。

## 架构合规性检查

| 合规项 | 状态 |
|---|---|
| C++20 标准 | ✅ 全部新代码使用 C++20 特性(`std::stop_token`, `std::jthread`, `std::shared_ptr`) |
| CMake 3.20+ | ✅ 新 `pdk/cloud/CMakeLists.txt` 遵循 `pdk/model_router/` 模式 |
| 2 空格缩进 | ✅ 所有新文件遵循 |
| 中文注释 | ✅ |
| `target_include_directories()` 而非全局 | ✅ 不引入 `include_directories()` |
| 无 `as any` / `@ts-ignore` | ✅ C++ 编译期类型检查 |
| 无空 catch | ✅ 错误处理用 `Result<T,E>` 而非异常 |
| 不删除失败测试 | ✅ 新增 ~30 测试，0 删除 |
| BREAKING change 声明 | ✅ D5 `available_models()` pure virtual 显式标注 "BREAKING change to ADR-0001" |
| Stream token 计数精度 | ✅ CostTrackingDecorator 含 streaming 精度测试(零误差) |

## 引用

- Oracle 长期演进分析 session: `ses_0c8f3f954ffeiw8s4X7xAW9hTJ`
- 探索 agent session: `ses_0c8f71030ffeFmhDwKSnmZn3oc`
- 关联 ADR: ADR-0001 / 0005 / 0031 / 0033 / 0034 / 0035 / 0041 / 0042 / 0045 / 0046
- 关联 OpenSpec: `phase5-llama-engine-plugin`(C14，需同步)、`phase5-batching-queue-plugin`(C15，正交)
- 关联 handoff: `docs/handoff/2026-07-06-architecture-completion.md`
- Metis 审查 session: `ses_0c48fd9fcffeLx2Xp1RgzygLz1`
- Adversarial review decisions: `docs/adversarial-reviews/decisions-2026-07-07.md`
- v1 proposal (superseded): `openspec/changes/phase5-illmprovider-call-chain-v2/proposal.md`