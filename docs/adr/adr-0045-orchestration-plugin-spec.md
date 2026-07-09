# ADR-0045: 编排 PDK Plugin 规范

## 状态

🔍 Proposed (2026-07-06 — 架构方案讨论产出, 待 review; **2026-07-06 renumber**: 原编号 0036 因与旧 ADR-0036-三层服务协议冲突, 改为 0045)

## 领域

基座 / Orchestration / PDK Plugin

## 关联

- ADR-0035 (Inference Engine Plugin Spec) — 推理引擎 plugin 契约, 编排 plugin 的交互对象
- ADR-0046 (Plugin Communication Protocol) — 四通道通信架构
- ADR-0033 (Session Hierarchy) — UserSession/TaskSession/SubtaskSession 三层模型
- ADR-0031 (Execution Policy) — Plan/Agent/YOLO 三模式, IExecutionPolicy
- ADR-0001 (ILLMProvider Streaming) — ILLMProvider 流式接口
- ADR-0019 (IInteractionBus) — EventBus pub/sub, 事件 topic 约定
- ADR-0034 (Model Router) — IModelRouter 路由策略范式

---

## 背景

### 问题

HydraForge DSL engine 执行 workflow 时缺少**智能推理编排层**:

| 维度 | 现状 | 问题 |
|------|------|------|
| **模型选择** | 硬编码在 DSL 中 (`model: "gpt-4"`) | 无法根据任务类型/负载/成本动态切换模型 |
| **推理调度** | DSL engine 直接调用 ILLMProvider | 无负载均衡, 无排队, 无超时策略 |
| **Agent 策略** | SimpleCognitiveOrchestrator (Phase 0 单轮) | 缺少多轮 ReAct/ForkJoin/PlanExecute 集成推理 Plugin 的路径 |
| **性能感知** | 编排层不知推理引擎状态 | 无法基于 KV cache 使用率 / GPU 内存做路由决策 |

### 目标

定义**编排 PDK Plugin** 的:
1. 职责边界与推理 Plugin 的分工
2. 决策框架 (模型选择, 路由, 调度)
3. 与推理 Plugin 的交互协议
4. ILLMProvider 桥接策略

### 架构定位

```
HydraForge Framework
│
├── [推理引擎 Plugin] (ADR-0035)
│   └── 拥有 llama.cpp 资源, 暴露 tools + ILLMProvider
│
├── [编排 Plugin] ← 本文档定义
│   ├── 实现 ILLMProvider (by wrapping inference tools)
│   ├── Agent 循环 (ReAct/ForkJoin/PlanExecute)
│   ├── 模型路由决策 (调用 inference/get/models)
│   ├── 动态参数调节 (调用 inference/configure)
│   └── 监控推理引擎 (订阅 inference/lifecycle/* events)
│
└── DSL Workflow
    └── 通过编排 Plugin → 推理 Plugin 的全链路
```

---

## 决策

### 1. 编排 Plugin 的职责边界

| 职责 | 编排 Plugin | 推理 Plugin | HydraForge Core |
|------|:---:|:---:|:---:|
| **模型选择** (哪个模型处理此任务) | ✅ | ❌ (仅提供 model list) | ❌ |
| **推理执行** (decode, sampling) | ❌ | ✅ | ❌ |
| **提示词构建** (system prompt, chat template) | ✅ | ❌ (仅 tokenize) | ❌ |
| **Agent 循环** (ReAct, ForkJoin, PlanExecute) | ✅ | ❌ | ❌ |
| **工具调用编排** (ToolRegistry) | ✅ | ❌ | ❌ |
| **Session 管理** (UserSession/TaskSession) | ✅ | 部分 (SubtaskSession) | ✅ |
| **负载均衡** (多推理引擎分发) | ✅ | ❌ | ❌ |
| **超时/重试策略** | ✅ | ❌ (仅 abort_callback) | ❌ |
| **LLM 调用** (ILLMProvider interface) | ✅ (包装推理 plugin tools) | ✅ (实现 ILLMProvider) | ❌ |

### 2. ILLMProvider 桥接策略 (P0 fix @Oracle review; 2026-07-09 修订 per OpenSpec change `phase5-illmprovider-call-chain-v2` Decision 1 Dual Consumer Model)

**关键决策**: 编排 Plugin 实现 `ILLMProvider` 接口, 内部**直连**推理 Plugin 的 ILLMProvider (共享 `shared_ptr<ILLMProvider>`), 不再通过 `internal_registry_.call_tool("inference/generate", ...)` 走 Tool dispatch。

#### 2.1 Dual Consumer Model (与 ADR-0035 §1.1 一致)

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

**关键洞察**: 
- **原方案 (2026-07-06)**: DSLEngine → 编排 ILLMProvider → `internal_registry_.call_tool("inference/generate", ...)` → 推理 Plugin (LLM thought 经 ToolCoordinator audit pipeline — **语义误用**, 违反 ADR-0031 §决策 5)
- **修订后 (2026-07-09)**: DSLEngine → 编排 ILLMProvider (直连 `inference_provider_->generate()`) → llama_decode(); Agent 循环平行路径直连推理 Provider (绕开编排包装)
- 推理 Plugin 的 ILLMProvider 是**内部接口** (仅编排 Plugin + Agent 循环使用), 不直接暴露给 DSLEngine/SimpleCognitiveOrchestrator 的对外消费路径
- **编排层 ILLMProvider 真实价值** = 路由 + 会话管理 (ADR-0045 §2.2), 不依赖 Tool dispatch 即可实现
- 净代码变化: -20 LOC (简化)

#### 2.2 generate() 桥接 (同步) — 直连推理 Provider

```cpp
Result<GenerationResult, LLMError>
OrchestrationILLMProvider::generate(const GenerationRequest& req, std::stop_token token) override {
  // 1. 决策层: 模式感知 + 路由 + 配置
  auto status = inference_provider_->call_tool("inference/get/status", {});  // via tool (查询)
  auto models = inference_provider_->available_models();
  auto selected_model = router_->select(models, task_context);
  auto sess_id = ensure_session(selected_model);

  // 2. 直连推理 (no Tool dispatch — 不经 ToolCoordinator audit pipeline)
  //    shared_ptr<ILLMProvider> 引用计数保证并发安全
  auto result = inference_provider_->generate(req, token);

  // 3. 响应包装
  if (result) return result;
  return Result::failure(result.error());
}
```

#### 2.3 generate_stream() 桥接 (流式) (P0 fix @Oracle review; 2026-07-09 修订: 直连模式)

```cpp
std::unique_ptr<IGenerationStream>
OrchestrationILLMProvider::generate_stream(const GenerationRequest& req, std::stop_token token) override {
  // 1. 决策同上
  ...

  // 2. 直连推理 Plugin 的流式接口, 返回编排层自己的 IGenerationStream 包装
  auto inner_stream = inference_provider_->generate_stream(req, token);
  return std::make_unique<OrchestrationStream>(std::move(inner_stream), token);
}

class OrchestrationStream : public IGenerationStream {
  // 内部聚合策略: N 个 inner stream chunk → 1 个 semantic chunk
  std::optional<std::string> next(std::stop_token token) override {
    // 拉取下一个 inner stream chunk (直连, 不经 Tool dispatch)
    // 可能的聚合: 多个 token 组成 1 个 semantic chunk (字数/换行边界)
    ...
  }
};
```

**两种实现策略** (可配置):
- **细粒度**: 每个 token 推一次 `next()` — 高响应, 高开销
- **聚合粒度**: N 个 token / 换行 / 句号边界聚合 — 平衡响应与开销 (推荐默认)
- **粗粒度**: 整段生成完后一次性返回 — 无流式意义, 用 `generate()` 即可

**stop_token 传播**: `OrchestrationStream::next(token)` 内部 token 直接传给下游的 `inference/generate/stream` tool。`stop_token.stop_requested()` 触发 plugin 内部 llama_set_abort_callback, 后台线程退出。

### 3. Agent 循环集成

编排 Plugin 通过 `DEFINE_AGENT` 宏 (PDK, ADR-0021) 定义 Agent 循环:

```cpp
DEFINE_AGENT(InferenceOrchestrator, LoopType::PlanExecute,
  // 1. 查询推理引擎状态 (通过 inference_provider_ 直连)
  auto models = inference_provider_->available_models();

  // 2. 模型选择决策 (ADR-0034 IModelRouter 注入)
  auto selected = router_->route(RoutingContext{models, task_context});

  // 3. 按需调整推理参数 (L3a 动态配置 — 通过 tool_registry_, 经 ToolCoordinator 审批)
  tool_registry_->call_tool("inference/configure",
    {{"n_threads", "8"}, {"prefer", "latency"}});

  // 4. 执行推理 (直连推理 Provider, 不经 ToolCoordinator)
  auto result = inference_provider_->generate(
    GenerationRequest{.prompt = prompt, .params = sampler_config}, token);

  // 5. 工具调用 (走 tool_registry_, 经 ToolCoordinator 审批)
  auto tool_result = tool_registry_->call_tool("selected_tool", args);
  ...
);
```

**注入说明 (2026-07-09 修订)**:
- `router_` 为 ADR-0034 `IModelRouter`, 通过 `std::shared_ptr<IModelRouter>` 注入编排 Plugin 构造函数
- `inference_provider_` 为 `std::shared_ptr<ILLMProvider>` (推理 Plugin), 直连 generate/generate_stream, LLM thought 不经 ToolCoordinator
- `tool_registry_` 为单一 `IToolRegistry` (经 ToolCoordinator 装饰), 用于面向 DSL workflow 的工具调用

### 4. 事件驱动的动态调节 (P0 fix @Oracle review; 2026-07-09 修订: 直连模式)

编排 Plugin 订阅推理引擎的 lifecycle events:

```cpp
// 用 weak_ptr<this> 防止 Plugin 卸载后的 use-after-free
std::weak_ptr<OrchestrationPlugin> weak_self = shared_from_this();

bus_->subscribe_topic("inference.lifecycle.context_overflow",
  [weak_self](const ToolResult& ev) {
    auto self = weak_self.lock();
    if (!self) return;  // Plugin 已卸载, 跳过
    // KV cache 满 → 减小 n_ctx 或切换模型 (通过 tool_registry_, 经 ToolCoordinator)
    self->tool_registry_->call_tool("inference/configure",
      {{"n_ctx", "4096"}});
  });

bus_->subscribe_topic("inference.error.OOM",
  [weak_self](const ToolResult& ev) {
    auto self = weak_self.lock();
    if (!self) return;
    self->tool_registry_->call_tool("inference/configure",
      {{"prefer", "memory_saving"}});
  });
}
```

**生命周期**:
- 编排 Plugin 构造时 subscribe
- 编排 Plugin 析构时 unsubscribe (持 token)
- 卸载顺序保证: 见 §6 + ADR-0022 destruction order

### 5. 编排 Plugin 注册的工具

编排 Plugin 向 DSL workflow 暴露的工具:

| 工具名 | 功能 | 映射到推理 Plugin |
|--------|------|-------------------|
| `orchestration/route` | 根据任务描述选择推理模型 | `inference/get/models` + ADR-0034 IModelRouter 策略 |
| `orchestration/execute` | 执行一次 Agent 任务 | `inference/generate` + Agent loop |
| `orchestration/status` | 查询编排状态 (当前模型, 任务队列) | `inference/get/status` |
| `orchestration/configure` | 动态调节编排策略 (路由偏好, 超时阈值) | 不需要映射 (编排自身状态) |

**路由策略复用**: `orchestration/route` 内部直接调用 ADR-0034 `IModelRouter::route()`, 不引入新的路由抽象。Strategy 实现由 C7 模式 router plugin 提供。

---

### 6. Tool 调用架构 — 单一 IToolRegistry + ToolCoordinator 集成 (2026-07-09 修订: 删除双 registry 模式)

**修订说明 (2026-07-09, per OpenSpec change `phase5-illmprovider-call-chain-v2` Decision 1 + Task 7.8)**: 原 "双 IToolRegistry" 架构 (`internal_registry_` + `external_registry_` — 见 2026-07-06 版 §6) 被废弃。编排 Plugin 现使用**单一 IToolRegistry**, LLM generate 路径直连 `inference_provider_->generate()` (不经 ToolCoordinator), 仅 DSL workflow 的 tool call 经 ToolCoordinator 审批。

#### 6.1 调用语义 (修订后)

| 调用类型 | 路径 | ToolCoordinator |
|---------|------|:---:|
| **LLM generate** (编排 → 推理) | `inference_provider_->generate()` (直连 shared_ptr) | ❌ 不经审批 |
| **LLM generate_stream** (编排 → 推理) | `inference_provider_->generate_stream()` (直连) | ❌ 不经审批 |
| **编排 layer 查询** (编排 → inference/get/*) | `inference_provider_->call_tool()` 或直接 `available_models()` | ❌ ReadOnly 无需审批 |
| **DSL workflow tool call** | 单一 `tool_registry_->call_tool()` (经 ToolCoordinator 装饰) | ✅ 走审批 |

#### 6.2 审计补偿

编排 Plugin 内部 LLM 调用时**主动 emit** 审计事件:

```cpp
// 编排 Plugin 内部 wrapper
auto orchestrated_generate = [this](const GenerationRequest& req, std::stop_token token) {
  // 1. emit 编排审计事件 (区别于 ToolCoordinator 的外部 audit)
  bus_->emit("orchestration.audit.llm.generate", ToolResult{
    .meta = {{"source", "orchestration"}, {"policy_mode", current_mode_}}
  });
  // 2. 实际调用 (直连, 不经 ToolRegistry)
  return inference_provider_->generate(req, token);
};
```

**审计事件命名**:
- 外部 tool call (经 ToolCoordinator): `tool.audit.invoked` / `tool.audit.completed` / `tool.audit.denied` (ADR-0031 §决策 7)
- 编排内部 LLM 调用: `orchestration.audit.llm.generate` / `orchestration.audit.llm.stream`

#### 6.3 依赖注入约定 (修订后)

```cpp
// DSLEngine 构造时
auto orchestrator = OrchestrationPlugin::create(
  tool_registry,         // 单一 IToolRegistry (经 ToolCoordinator 装饰)
  inference_provider,    // shared_ptr<ILLMProvider> (推理 Plugin, 直连)
  bus,
  router                 // ADR-0034 IModelRouter
);
```

**与 ADR-0046 通信协议的协调**: 编排 Plugin 通过 IInteractionBus 订阅 `inference.lifecycle.*` / `inference.error.*` events 进行动态调节 (见 §4)。ToolCoordinator 审批范围限于 DSL workflow 的 tool call, LLM 调用不在审批范围。

---

### 7. 测试策略 (P1 fix @Oracle review; 2026-07-09 修订: 单一 registry)

| # | 测试名 | 覆盖 |
|---|--------|------|
| 1 | `illmprovider_direct_generate` | 编排 ILLMProvider 直连 inference_provider_->generate() (不经 call_tool) |
| 2 | `orchestration_audit_emit` | 每次内部 LLM invoke 都 emit orchestration.audit.llm.* |
| 3 | `illmprovider_dual_consumer` | DSLEngine → 编排 ILLMProvider + Agent 循环平行路径 → 推理 ILLMProvider |
| 4 | `illmprovider_generate_stream_aggregation` | 编排 generate_stream N token → 1 semantic chunk 策略 |
| 5 | `router_integration_cost_quality_latency` | IModelRouter 3 策略注入编排 Plugin |
| 6 | `event_subscriber_weak_ptr_safety` | Plugin 卸载后 callback 不触发 use-after-free |
| 7 | `orchestration_configure_tool` | orchestration/configure 动态调节内部策略 |
| 8 | `dual_plugin_lifecycle_synergy` | 双 Plugin 加载/卸载顺序协同 |

---

## 替代方案

### Option A: 编排逻辑内嵌到 DSLEngine (非 Plugin)

**被拒绝理由**: 违反 PDK 解耦原则 (ADR-0021)。编排策略 (路由算法, Agent 循环) 应可独立迭代, 不受 HydraForge core 发布节奏限制。

### Option B: 推理 Plugin 直接实现编排逻辑

**被拒绝理由**: 违反单一职责。推理引擎的职责是"算", 编排的职责是"怎么算"。耦合后无法支持多推理引擎 (cloud + local) 统一编排。

---

## 实施顺序

1. 编排 Plugin 骨架 (pdk_register_tools + 基本路由)
2. ILLMProvider 包装层 (调用 inference tools)
3. Agent 循环集成 (React → PlanExecute → ForkJoin)
4. 事件驱动调节 (订阅 inference lifecycle events)
5. 编排 DSL tools 暴露

---

*创建日期*: 2026-07-06
*Oracle 审查*: ses_0ca3dce4fffeck5vmAQMs6R94m
*依赖*: ADR-0035 (推理引擎 Plugin 规范), ADR-0046 (插件间通信协议)