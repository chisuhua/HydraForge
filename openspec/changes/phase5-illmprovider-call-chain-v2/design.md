## Context

### 当前状态(2026-07-06,main + phase5-inference-orchestration 分支)

HydraForge ILLMProvider 调用链经 3 轮 Oracle 审查(session `ses_0ca3dce4fffeck5vmAQMs6R94m`)+ 长期演进分析(session `ses_0c8f3f954ffeiw8s4X7xAW9hTJ`)后,识别 5 个长期演进风险:

**P0 ship gate 阻塞项**:
1. **Budget hole**:`node_executor.cpp:271-274`(execute_generate_subgraph)、`:447-463`(execute_yield)、`SimpleCognitiveOrchestrator` 三处直调 `llm_provider_->generate()` 绕过 `tool_registry_->set_cost_callback()`,**零计费**
2. **3 层链语义误用**:`OrchestrationILLMProvider::generate()` 经 `internal_registry_.call_tool("inference/generate", ...)` 走 ToolCoordinator 路径,LLM thought 误经 audit pipeline
3. **Cloud/Local 架构割裂**:ADR-0042 §4 决议 cloud 留核心,与 ADR-0005 §3 plugin-extensible 哲学矛盾

**P1 ship gate 阻塞项**:
4. **`available_models()` 默认空实现**导致 Router 静默失败(返回空列表,无编译期警告)
5. **Adversarial review 命名/SamplerStrategy/BatchingQueue 修正未反映到 ADR**(C14 proposal 仍引用 `llama_engine/`)

**ADR 演进状态**:
- 10 个新 ADR(0035/0038-0046)🔍 Proposed 等首次实施 commit → ✅ Approved
- 旧 ADR-0036(三层服务协议)/ ADR-0037(因果序)被本次 session renumber 改为 ADR-0045/0046(避免冲突)
- ADR-0042 §2 + §4 需修订(Phase 3 trigger 不可执行 + cloud plugin 化决议)

### 关键约束

- **C++20 + CMake 3.20+**:无 C++23 `std::expected` 可用,`Result<T,E>` 保留
- **跨 .so 边界安全**:`pdk_create_llm_provider` 返回 `shared_ptr<ILLMProvider>`(非 raw pointer,RAII 自动管理,解决 Sprint 17 C7 destruction order bug)
- **LlamaAdapterProvider 退役路径**:3 阶段(ADR-0042 §2),但 Phase 3 trigger 需修订
- **PluginInfo V2 ABI bump**:1104 字节(含 `dependencies[256]`),与 abi_version=1 共存
- **ToolCoordinator 审批范围**:仅 tool call,LLM generate 不经审批(ADR-0031 §决策 5 + ADR-0035 §1.1)
- **Agent 循环已实证**(代码 trace):ReAct 1 次 generate + 1 次 tool call/cycle;PlanExecute 2-3 次 generate/cycle;ForkJoin 0 次

### 关联决策与文档

- **依赖 ADR**:ADR-0001 / 0005 / 0034 / 0035 / 0041 / 0042 / 0045 / 0046
- **被依赖 OpenSpec**:`phase5-llama-engine-plugin`(C14,需同步命名更新)、`phase5-batching-queue-plugin`(C15,正交)
- **设计依据**:Oracle session `ses_0c8f3f954ffeiw8s4X7xAW9hTJ` 长期演进分析(session 续 `ses_0c8f3f954ffeiw8s4X7xAW9hTJ` D2 中间方案)

## Goals / Non-Goals

### Goals

1. **修 Budget Hole**(P0):3 处直调 LLM 路径全部计费,通过 Decorator 链实现
2. **实施 Dual Consumer Model**(P0):OrchestrationILLMProvider 保留但直连推理,Agent 循环绕开编排包装
3. **统一 Backend 插件机制**(P0):Cloud 与 Local 都走 PDK plugin 机制,`LLMProviderFactory` 仅作薄路由
4. **修订 ADR-0042**(P0):Phase 3 trigger 改 2 release cycles,Phase 2 `local` remap,deprecate `LlamaAdapter`+`LlamaAdapterProvider`
5. **`available_models()` pure virtual**(P1):强制 plugin 显式声明能力,避免 Router 静默失败
6. **命名统一**(P0):删 `llama_engine/` 残留引用,统一 `inference.*`
7. **文档同步**(P0):ADR-0001/0035/0042/0045 全面修订

### Non-Goals

- **Multi-modal input/output**:`GenerationRequest.prompt` 保持 `std::string`,`GenerationResult.text` 保持 `std::string`。`ModelCapability::Vision` 枚举值保留但**无实现路径**,留 Phase 6 scoping ADR
- **PluginInfo v2 `dependencies[256]` 完整实施**:仅在 cloud plugin 阶段同步追加依赖检查 hook,不实施完整字段
- **Anthropic 原生 API 完整迁移**:CloudLLMAdapter 当前 OpenAI 兼容协议已 ship,Anthropic 用户通过 OpenAI 兼容代理或第三方 plugin
- **`Result<T,E>` → `std::expected` 重命名**:等 C++23 编译器基线(>2 年)再 `using Result = std::expected` 一行替换
- **Telemetry 基础设施**:为支持 ADR-0042 Phase 3 telemetry trigger 而新建 telemetry 系统,本 change 不做(已决议改 2 release cycles trigger)
- **新增 LLM 后端**(Vertex AI / Cohere / Bedrock 等):Phase 6+ backlog

## Decisions

### Decision 1: Dual Consumer Model(方案 E = A')

**决议**:保留 `OrchestrationILLMProvider` 实现 ILLMProvider,但内部从 `call_tool("inference/generate", ...)` 改为直连 `inference_provider_->generate()`。Agent 循环通过 `engine_->get_llm_provider()` 获取 raw ILLMProvider*,**绕开编排包装**。

**架构图**:

```
┌────────────────── Orchestration Plugin ──────────────────┐
│                                                           │
│  ┌────────────────────────────┐  ┌─────────────────────┐ │
│  │ OrchestrationILLMProvider  │  │ Agent Loops         │ │
│  │ (路由 + 会话管理)            │  │ (ReAct/PlanExec/    │ │
│  │                            │  │  ForkJoin)          │ │
│  │ 消费者: DSLEngine/          │  │                     │ │
│  │          NodeExecutor       │  │ 消费者: 循环自身      │ │
│  │                            │  │                     │ │
│  │ 直连推理 (no Tool dispatch)│  │ 直连推理(同样无包装) │ │
│  └──────────┬─────────────────┘  └──────────┬──────────┘ │
│             │                                 │            │
│             ▼                                 ▼            │
│        ┌──────────────────────────────────────────────┐    │
│        │  推理 Plugin ILLMProvider                   │    │
│        └────────────────────┬─────────────────────────┘    │
└────────────────────────────┼───────────────────────────────┘
                             ▼
                       llama_decode()
```

**理由**:
- Oracle 实证(代码 trace 2026-07-06,Phase 5 B2 pre-implementation 状态):Agent 循环 prompt 构建完全独立于编排层,thought context 不在编排层注入
  - `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h:121` — `agenticdsl::ILLMProvider* llm = engine_->get_llm_provider();`
  - `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h:203-205` — Plan phase prompt 构建(简单字符串拼接)
  - `include/agenticdsl/pdk/agent_loops/plan_execute_loop.h:251-255` — Verify phase prompt 构建
  - `src/modules/cognitive/simple_orchestrator.cpp:109-117` — ReAct prompt 构建(硬编码 system + user)
- Oracle 量化:Tool dispatch 开销 ~5μs vs llama_decode ~200ms = 0.0025%,**性能不是争论点**;但 LLM thought 经 ToolCoordinator audit pipeline 是**语义误用**(ADR-0031 §决策 5)
- 编排行 ILLMProvider 真实价值 = 路由 + 会话管理(ADR-0045 §2.2),不依赖 Tool dispatch 即可实现
- 净代码变化:-20 LOC(简化)

**被拒方案**:
- **A 原始**(砍掉编排 ILLMProvider):违背 ADR-0045 §2 设计意图,剥夺 DSLEngine/NodeExecutor 的路由 + 会话管理层
- **B**(Conditional Skip 2/3 层):DSLEngine 不该知道编排存在,重蹈 ADR-0019 §1.4 解耦反模式
- **C**(Reframe as Decorator):Agent 循环是 state machine 不是 decorator,强套会导致逻辑分裂
- **D**(ToolLayer Optional):同一接口两种语义 = API 反模式

### Decision 2: Decorator 模式修 Budget Hole

**决议**:实现 `ILLMProviderDecorator` 抽象 + 3 个具体装饰器(CostTracking / Compliance / RateLimit),DSLEngine 构造时按需包装。

**类签名**(伪码):

```cpp
// include/agenticdsl/contract/i_llm_provider_decorator.h
class ILLMProviderDecorator : public ILLMProvider {
public:
  explicit ILLMProviderDecorator(unique_ptr<ILLMProvider> inner);
  Result<GenerationResult, LLMError>
      generate(const GenerationRequest& req, std::stop_token token) override final;
  unique_ptr<IGenerationStream>
      generate_stream(const GenerationRequest& req, std::stop_token token) override final;
  vector<ModelInfo> available_models() const override final;
protected:
  // 子类必须 override 的钩子
  virtual Result<GenerationResult, LLMError>
      decorate_generate(const GenerationRequest&, Result<GenerationResult, LLMError>) = 0;
  unique_ptr<ILLMProvider> inner_;
};

// src/common/llm/cost_tracking_decorator.cpp
class CostTrackingDecorator : public ILLMProviderDecorator {
  shared_ptr<IBudgetController> budget_;
  // override decorate_generate: 提取 token count, 调 budget_->record_cost(tokens, model)
};
```

**部署位置**:`DSLEngine` 构造器中,在 set_llm_provider 之前包装:

```cpp
// engine.cpp:79 改为
auto base = provider_factory_->create(mock_config);
llm_provider_ = make_unique<CostTrackingDecorator>(
    make_unique<ComplianceDecorator>(move(base)),
    budget_controller_);
```

**理由**:
- GoF Decorator 模式,正交关注点独立可测
- 修 budget hole:3 处直调路径(NodeExecutor / SimpleCognitiveOrchestrator / PlanExecute)经装饰器链后**全部计费**
- 替代 ADR-0042 §决策 1 的 `pdk_destroy_llm_provider` 模式(shared_ptr RAII 自动管理)
- 与 ADR-0034 IModelRouter 协同:RoutingDecorator 是同类抽象

**被拒方案**:
- 在 ILLMProvider 接口加 `get_last_call_cost()`(违反 ISP,MockLLMProvider 也要实现空方法)
- 在 NodeExecutor 集中计费(每加一个 LLM 调用点都要改 NodeExecutor)

### Decision 3: Cloud Plugin 化(推 ADR-0042 §4)

**决议**:CloudLLMAdapter 从 `src/common/llm/` 移至 `pdk/cloud/` 作为 first-party plugin,所有 backend(cloud + local)统一走 PDK plugin 机制。

**目录结构**:

```
pdk/cloud/
├── CMakeLists.txt            # INTERFACE 库 hydraforge_pdk_cloud
├── README.md
├── include/
│   └── hydraforge/pdk/
│       └── cloud.h           # 公共 API
└── src/
    ├── cloud_plugin.cpp      # pdk_plugin_info / pdk_register_tools / pdk_create_llm_provider 导出
    └── cloud_adapter.cpp     # 移自 src/common/llm/cloud_adapter.cpp
```

**5 符号导出**(per ADR-0041):

```cpp
extern "C" {
  const PdkPluginInfo* pdk_plugin_info();
  int pdk_register_tools(PdkToolRegistry* reg);  // 空实现(cloud plugin 无工具,仅 ILLMProvider)
  std::shared_ptr<::agenticdsl::ILLMProvider> pdk_create_llm_provider(const PdkProviderConfig* cfg);
  bool pdk_plugin_init();
  void pdk_plugin_fini();
}
```

**Factory 路由变更**(`src/common/llm/llm_provider_factory.cpp`):

```cpp
// 旧: 直接 new
if (backend == "openai" || ...) return cloud_factory->create(config);

// 新: dlopen plugin
if (backend == "openai" || ...) {
  static auto plugin_loader = CloudPluginLoader("pdk_cloud");
  return plugin_loader.load_provider(config);
}
```

**理由**:
- 5 年视角下统一机制 vs 两套机制,前者总成本 < 后者
- cloud 路径补全 lifecycle hooks(`pdk_plugin_init/fini`,支持连接池、key rotation)
- 内部 gateway 代理、自定义 auth 等部署需求可由第三方 plugin 满足
- 与推理 plugin 共享 ABI(`pdk_plugin_info` / `pdk_create_llm_provider`),架构对称

**被拒方案**:
- 保持 cloud 在核心(ADR-0042 §4 原决议):与 ADR-0005 §3 plugin-extensible 哲学矛盾,无法支持自定义 auth
- 仅 plugin 化"未来 cloud",保留现有 CloudLLMAdapter:两套机制永久共存,维护税高

### Decision 4: ADR-0042 §2 + §4 修订

**决议**:Phase 3 trigger 改 2 release cycles,Phase 2 `local` remap(不删除),同时 deprecate `LlamaAdapter`+`LlamaAdapterProvider`。

**修订对比表**:

| 项 | 原 ADR-0042 §2/§4 | 修订后 |
|---|---|---|
| Phase 1 trigger | "本 ADR Approved 即生效" | 不变 |
| Phase 2 trigger | "推理 Plugin ✅ + 1 release cycle" | 不变 |
| Phase 2 措施 | "移除 `local` → LlamaAdapterProvider 映射" | **改为 remap `local` → InferencePlugin**(用户配置零改动) |
| Phase 3 trigger | "Telemetry 30 天零实例化" | **改为 Phase 2 后 2 release cycles** |
| Phase 3 措施 | "删除 `LlamaAdapterProvider`" | **同时删除 `LlamaAdapter`(底层 HTTP 包装)** |
| §4 决议 | "cloud 留 HTTP 客户端在核心" | **推翻**:cloud plugin 化(per Decision 3) |

**Phase 2/3 时间线明确定义**(消除歧义):

| Phase | 触发时间 | 同步信号 |
|---|---|---|
| **Phase 1** | 本 ADR Approved 后立即 | 当前 OpenSpec change `phase5-illmprovider-call-chain-v2` ship |
| **Phase 2** | 推理 Plugin(`pdk/inference_engine/`)✅ Approved + **下一个 minor release**(per ADR-0042 §2 trigger) | 跟踪信号:`docs/adr/adr-0035-inference-engine-plugin-spec.md` 状态从 🔍 Proposed → ✅ Approved + `openspec/changes/phase5-llama-engine-plugin/` archived |
| **Phase 3** | Phase 2 ship 后**再 2 个 minor release**(估算 ~6-12 个月,per 项目 release cadence) | 跟踪信号:`git log --grep="v0\."` 计数 + OpenSpec change `phase5-illmprovider-call-chain-v2` 标记为 Phase 3 ready |

**注意**:
- "release cycle" 指 HydraForge minor release(估算 3-6 个月一次),非 minor commit 或 patch release
- Phase 2 实际接入点已经在本 change 中预留(`Task 5.12c`),无需新 OpenSpec change
- Phase 3 删除 `LlamaAdapterProvider` + `LlamaAdapter` 需要新建独立 OpenSpec change(2027+)

**`[[deprecated]]` 标注**(非 breaking):

```cpp
// src/common/llm/llama_adapter_provider.h
class [[deprecated("Use pdk/inference_engine/ plugin instead, see ADR-0042 §2")]]
    LlamaAdapterProvider : public ILLMProvider {
  // ...
};

// src/common/llm/llama_adapter.h
class [[deprecated("LlamaAdapter is deprecated; use pdk/inference_engine/ plugin, see ADR-0042 §2")]]
    LlamaAdapter {
  // ...
};
```

**理由**:
- Telemetry 基础设施不存在,Phase 3 trigger 不可执行(Oracle 验证)
- 保留 `local` 配置 key 让用户配置零改动(向后兼容)
- 同时 deprecate `LlamaAdapter` 关闭 escape hatch(直接 `new LlamaAdapter` 绕过 factory)
- Phase 2/3 时间线明确定义,实施者无需猜测 trigger 边界

### Decision 5: `available_models()` Pure Virtual

**决议**:`available_models()` 从虚方法(默认空)改为纯虚方法。

**修改**:

```cpp
// src/common/llm/llm_types.h:157
// 旧
virtual std::vector<ModelInfo> available_models() const { return {}; }
// 新
virtual std::vector<ModelInfo> available_models() const = 0;  // REQ-ICC-005
```

**修改 4 个实现显式 override**:
- `MockLLMProvider::available_models()` → 返回 1 个 mock-llm-v1(per existing test fixture)
- `CloudLLMAdapter::available_models()` → 返回 config.model
- `LlamaAdapterProvider::available_models()` → 返回 llama_config.model
- 新 `InferencePlugin::available_models()` → 返回 loaded gguf models

**理由**:
- 强制 plugin 显式声明能力,避免 Router 静默失败(返回空列表,无警告)
- 5 个 provider 实现必须 override,5 处修改 <1 天
- 与 ADR-0034 IModelRouter 集成校验更可靠

### Decision 6: 命名统一(ADR-0035 §2)

**决议**:删 `llama_engine/` 残留引用,统一 `inference.*`(13 个工具)。

**修订文件**:
- `docs/adr/adr-0035-inference-engine-plugin-spec.md` §2 工具命名表
- `openspec/changes/phase5-llama-engine-plugin/proposal.md` 同步更新
- `openspec/changes/phase5-b2-arch-schemas/proposal.md` 检查同步

**理由**:
- Adversarial review 2026-07-06 已 flag 但未实施
- 避免 C14 ship 后发现命名不一致 → 回炉改造
- 与 ADR-0043 (PDK Tool 命名 slash 约定) 一致

### Decision 7: ADR 文档同步修订

**决议**:同步修订 4 个 ADR 文档状态行,加 renumber/decision note。

| ADR | 修订 |
|---|---|
| ADR-0001 | 加 `available_models()` pure virtual 决议;记 `is_available()`/`name()` deferred indefinitely;记 `Result<T,E>` → `std::expected` 等 C++23 |
| ADR-0035 §1.1 | 三层消费链图重画(Dual Consumer Model) |
| ADR-0035 §2 | 工具命名统一到 `inference.*` |
| ADR-0042 §2 | Phase 3 trigger + `local` remap + deprecate 范围 |
| ADR-0042 §4 | 推翻"cloud 留核心",改为 plugin 化 |
| ADR-0045 §2 | 直连而非 call_tool |
| ADR-0045 §6 | 删"双 IToolRegistry"架构 |
| ADR-0035/0038 | 加 SamplerStrategy/BatchingQueue deferred 注记 |

## Risks / Trade-offs

| 风险 | 等级 | 影响 | 缓解措施 |
|---|---|---|---|
| **Cloud plugin 化破坏现有 CloudLLMAdapter 用户** | 🟠 | "openai"/"anthropic" 等 6 个 provider 字符串路由从核心代码变 dlopen,二进制兼容性需验证 | Factory 内置 CloudPluginLoader,首次 dlopen 后缓存 handle;CloudLLMAdapter 行为 100% 保持(单元测试覆盖) |
| **`available_models()` 变 pure virtual 破坏外部 ILLMProvider 实现** | 🟡 | 任何第三方 ILLMProvider 子类必须 override | 在 ADR-0001 标注;在 PR review 时检查;给出 stub 示例 |
| **OrchestrationILLMProvider 直连后,future 编排逻辑需 Tool audit 时缺乏路径** | 🟡 | 如果未来编排层想加"LLM call 也走 audit event"(目前不需要),需重新设计 | 流式聚合 OrchestrationStream 已 emit lifecycle event;如未来需要 audit,从 OrchestrationILLMProvider 内部 emit `orchestration.audit.internal.*` 即可(per ADR-0045 §6.3) |
| **Decorator 链引入额外间接层** | 🟢 | 性能开销 ~50-100ns/层,可忽略 | 装饰器链深度限制 ≤ 3 层(默认 CostTracking + Compliance);监控 emit |
| **Phase 3 deprecation trigger 改 2 release cycles 后,部分用户卡在旧 API** | 🟡 | "local" + "llama" 配置用户在 release N+2 后编译失败 | `[[deprecated]]` 警告给 2 release cycles;文档迁移指南;`local_legacy` 隐藏 alias 临时回退 |
| **Multi-modal ADR 留 Phase 6,Phase 5+1 年 vision 需求激增** | 🟡 | 2027-2028 重做 ILLMProvider 接口 | 本 change 末加 ADR-0001 v2 stub:`GenerationRequest.prompt` 后续扩展为 `std::variant<std::string, MediaChunk>` |
| **Cloud plugin 化引入 dlopen 路径,如果 .so 找不到或 ABI 不匹配** | 🟡 | 用户迁移到新版本后启动失败 | Factory 路由兜底:dlopen 失败时降级到 Mock provider(永不返回 nullptr,日志 ERROR) |
| **Anthropic 协议 bug 未修**(路由到不存在的实现) | 🟡 | "anthropic" 配置用户实际拿到 OpenAI 兼容(可能不工作) | Decision 3 plugin 化后,Anthropic 在 cloud plugin 内部显式分支(`if (provider == "anthropic") use_anthropic_protocol()`);MVP 仍 OpenAI 兼容 |

## Migration Plan

### 阶段 1 (Day 1-2):Decorator + budget hole 修复

1. 创建 `ILLMProviderDecorator` 接口
2. 实现 `CostTrackingDecorator`(修 P0 budget hole)
3. DSLEngine 构造时包装
4. 跑现有测试 + 新增 `test_cost_tracking_decorator.cpp`(3 处计费断言)
5. 验证:三处直调路径全部计费(`IBudgetController::record_llm_call` 调用次数 == LLM 调用次数)

### 阶段 2 (Day 3-5):Dual Consumer Model

1. 修订 ADR-0045 §2 + §6
2. 重构 `OrchestrationILLMProvider::generate()` 为直连
3. 验证 Agent 循环仍能调 LLM(`plan_execute_loop.h:121` 用 `engine_->get_llm_provider()` + `simple_orchestrator.cpp:118` 用 `llm_->generate()`,均为 raw ILLMProvider* 直连)
4. 新增 `test_orchestration_dual_consumer.cpp`
5. 验证:流式聚合 `OrchestrationStream` 仍 emit lifecycle events

### 阶段 3 (Day 6-10):Cloud Plugin 化

1. 创建 `pdk/cloud/` 目录结构
2. 移 CloudLLMAdapter 代码
3. 导出 5 符号(`pdk_plugin_info` / `pdk_register_tools` / `pdk_create_llm_provider` / `pdk_plugin_init` / `pdk_plugin_fini`)
4. 修改 `LLMProviderFactory::create()` cloud 路由为 dlopen
5. 新增 `test_cloud_llm_plugin.cpp`
6. 验证:6 个 provider 字符串(`openai`/`anthropic`/`deepseek`/`qwen`/`moonshot`/`custom`)行为保持

### 阶段 4 (Day 11-12):ADR 修订 + `available_models()` pure virtual

1. 修订 ADR-0001/0035/0042/0045
2. 修改 `llm_types.h:157` `available_models()` 改 pure virtual
3. 修改 4 个实现显式 override
4. 新增 `test_available_models_pure_virtual.cpp`
5. 同步 OpenSpec `phase5-llama-engine-plugin/proposal.md` 命名

### 阶段 5 (Day 13):ADR-0042 §2 退役路径 + deprecate 标注

1. 修改 `LlamaAdapterProvider` 构造函数加 `[[deprecated]]`
2. 修改 `LlamaAdapter` 构造函数加 `[[deprecated]]`
3. 修改 `LLMProviderFactory::create()` "local" / "llama" 路由 → remap 到新 InferencePlugin(Phase 2 真实执行点)
4. 验证:用户 `provider: "local"` 配置 0 改动仍工作

### Rollback 策略

- **阶段 1 失败**(Decorator 引入 bug):git revert commit,`LLMProviderFactory` 暂时不包装 Decorator,budget hole 临时接受
- **阶段 2 失败**(Dual Consumer 破坏 Agent 循环):保留 ADR-0045 §2 原 call_tool 实现,git revert
- **阶段 3 失败**(Cloud plugin 化引入 dlopen 兼容问题):保留 CloudLLMAdapter 在 `src/common/llm/` 双位置,factory 路由保留旧核心路径作为 fallback
- **阶段 4 失败**(`available_models()` pure virtual 编译失败):回退到默认空实现,加 `// FIXME: enforce override in 6 months` 注释
- **阶段 5 失败**(deprecate 编译警告噪音过大):移除 `[[deprecated]]`,改用运行时警告

每个阶段 ship 独立 commit,可独立 revert。**Phase 5 ship gate** = 阶段 1-4 全部完成 + 零回归。

## Open Questions

1. **Q1: RouterDecorator 与 OrchestrationILLMProvider 的边界**
   - 当前决议:RouterDecorator 是 ADR-0034 IModelRouter 注入点,OrchestrationILLMProvider 内部调用 `router_.select()`;RouterDecorator **不是** 独立类(直接复用 ADR-0034 现有抽象)
   - 待定:是否需要独立 RouterDecorator 类作为 `ILLMProviderDecorator` 子类?

2. **Q2: ComplianceDecorator MVP 范围**
   - 当前决议:MVP 仅日志(emit `compliance.log` event 到 IInteractionBus),不做 PII 检测
   - 待定:Phase 6+ 是否需要 hash-only 模式(仅记录 hash,不存原始 prompt)?

3. **Q3: RateLimitDecorator 默认禁用 vs 默认启用**
   - 当前决议:默认禁用(单租户场景无意义);多租户 SaaS 场景 opt-in
   - 待定:`tenant_id` 来源(`GenerationRequest` 扩展字段?env var?config?)

4. **Q4: `local_legacy` 隐藏 alias 是否提供**
   - 当前决议:Phase 3 release 内提供(临时回退)
   - 待定:Phase 3 release 后是否完全删除(零迁移路径)

5. **Q5: Cloud plugin 的 Anthropic 协议实施深度**
   - 当前决议:MVP 仍 OpenAI 兼容(因为 httplib 限制),后续接 httplib::Client::Post stream chunk callback 后升级
   - 待定:是否在 cloud plugin 阶段就引入 native Anthropic protocol adapter?

6. **Q6: `pdk_register_tools` 在 cloud plugin 中应注册什么工具**
   - 当前决议:空实现(cloud plugin 不暴露 DSL workflow 工具)
   - 待定:Phase 6+ 是否添加 `cloud/{provider}/list_models` 工具给 DSL workflow 用?

7. **Q7: ADR-0001 v2(multi-modal) scoping ADR 何时启动**
   - 当前决议:Phase 6 启动,本次 change 不阻塞
   - 待定:是否在本 change 中预留 `GenerationRequest` 扩展点(如 `extra_fields` map)?

---

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
| 不删除失败测试 | ✅ 新增 5 个测试,0 删除 |

## 引用

- Oracle 长期演进分析 session:`ses_0c8f3f954ffeiw8s4X7xAW9hTJ`
- 探索 agent session:`ses_0c8f71030ffeFmhDwKSnmZn3oc`
- 关联 ADR:ADR-0001 / 0005 / 0031 / 0033 / 0034 / 0035 / 0041 / 0042 / 0045 / 0046
- 关联 OpenSpec:`phase5-llama-engine-plugin`(C14,需同步)、`phase5-batching-queue-plugin`(C15,正交)
- 关联 handoff:`docs/handoff/2026-07-06-architecture-completion.md`