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

### 2. ILLMProvider 桥接策略 (P0 fix @Oracle review)

**关键决策**: 编排 Plugin 实现 `ILLMProvider` 接口, 内部通过调用推理 Plugin 的 `inference/generate` tool 来实现。

#### 2.1 三层消费链 (与 ADR-0035 §1.1 一致)

```
DSLEngine / SimpleCognitiveOrchestrator
  │
  │ ILLMProvider::generate(req, token)
  ▼
[编排 Plugin (implements ILLMProvider)] ← DSLEngine 唯一可见的 ILLMProvider
  │
  │ internal_registry.call_tool("inference/generate", args) (豁免 ToolCoordinator)
  ▼
[推理引擎 Plugin (also implements ILLMProvider)] ← 仅编排 Plugin 使用, 内部接口
  │
  │ llama_decode() → sampler → tokens
  ▼
返回 GenerationResult
```

**关键洞察**: 推理 Plugin 的 ILLMProvider 是**内部接口**, 不直接暴露给 DSLEngine/SimpleCognitiveOrchestrator。**编排 Plugin 的 ILLMProvider 是 DSLEngine 唯一可见的 LLM provider**。

#### 2.2 generate() 桥接 (同步)

```cpp
Result<GenerationResult, LLMError>
OrchestrationILLMProvider::generate(const GenerationRequest& req, std::stop_token token) override {
  // 1. 决策层: 模式感知 + 路由 + 配置
  auto status = internal_registry_.call_tool("inference/get/status", {});
  auto models = internal_registry_.call_tool("inference/get/models", {});
  auto selected_model = router_.select(models, task_context);
  auto sess_id = ensure_session(selected_model);

  // 2. 调用推理 Plugin (内部豁免 ToolCoordinator, 走 §6 内部 API)
  auto args = build_args(req, sess_id, selected_model);
  auto tool_result = internal_registry_.call_tool("inference/generate", args);

  // 3. 响应包装
  if (tool_result.ok()) return Result::success(tool_result.data);
  return Result::failure(map_tool_error_to_llm_error(tool_result));
}
```

#### 2.3 generate_stream() 桥接 (流式) (P0 fix @Oracle review)

```cpp
std::unique_ptr<IGenerationStream>
OrchestrationILLMProvider::generate_stream(const GenerationRequest& req, std::stop_token token) override {
  // 1. 决策同上
  ...

  // 2. 创建编排层自己的 IGenerationStream 包装
  return std::make_unique<OrchestrationStream>(internal_registry_, req, sess_id, token);
}

class OrchestrationStream : public IGenerationStream {
  // 内部聚合策略: N 个 inference/generate/stream tool chunk → 1 个 semantic chunk
  std::optional<std::string> next(std::stop_token token) override {
    // 拉取下一个 inference stream chunk (通过内部循环)
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
  // 1. 查询推理引擎状态 (通过 internal_registry, 豁免 ToolCoordinator)
  auto status = internal_registry_.call_tool("inference/get/status", {});
  auto models = internal_registry_.call_tool("inference/get/models", {});

  // 2. 模型选择决策 (ADR-0034 IModelRouter 注入)
  auto selected = router_->route(RoutingContext{models, task_context});

  // 3. 按需调整推理参数 (L3a 动态配置)
  internal_registry_.call_tool("inference/configure",
    {{"n_threads", "8"}, {"prefer", "latency"}});

  // 4. 执行推理 (内部调用, 豁免审批)
  auto result = internal_registry_.call_tool("inference/generate",
    {{"session_id", sid}, {"prompt", prompt}, {"sampler", sampler_config}});

  // 5. 工具调用 (走外部 registry, 经 ToolCoordinator 审批)
  auto tool_result = external_registry_->call_tool("selected_tool", args);
  ...
);
```

**注入说明 (P0 fix @Oracle review)**:
- `router_` 为 ADR-0034 `IModelRouter`, 通过 `std::shared_ptr<IModelRouter>` 注入编排 Plugin 构造函数
- `internal_registry_` 为**内部引用**, 持有 ToolRegistry 但不经过 ToolCoordinator 包装 (见 §6)
- `external_registry_` 为**外部引用**, 经过 ToolCoordinator 走审批, 用于面向 DSL workflow 的工具调用

### 4. 事件驱动的动态调节 (P0 fix @Oracle review)

编排 Plugin 订阅推理引擎的 lifecycle events:

```cpp
// 用 weak_ptr<this> 防止 Plugin 卸载后的 use-after-free
std::weak_ptr<OrchestrationPlugin> weak_self = shared_from_this();

bus_->subscribe_topic("inference.lifecycle.context_overflow",
  [weak_self](const ToolResult& ev) {
    auto self = weak_self.lock();
    if (!self) return;  // Plugin 已卸载, 跳过
    // KV cache 满 → 减小 n_ctx 或切换模型 (内部调用, 不需要审批)
    self->internal_registry_.call_tool("inference/configure",
      {{"n_ctx", "4096"}});
  });

bus_->subscribe_topic("inference.error.OOM",
  [weak_self](const ToolResult& ev) {
    auto self = weak_self.lock();
    if (!self) return;
    self->internal_registry_.call_tool("inference/configure",
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

### 6. 内部调用 vs 外部调用 — ToolCoordinator 豁免机制 (P0 fix @Oracle review)

**问题**: ADR-0031 IExecutionPolicy + ToolCoordinator 对所有非 ReadOnly 工具在 Plan 模式下要求审批。如果编排 Plugin 内部调用 `inference/generate` 经 ToolCoordinator, 则每次 LLM 生成都要用户确认 → 性能灾难。如果不经, 则编排 Plugin 成绕过审批的通道 → 安全漏洞。

**解决方案**: 双 registry 引用 + ToolCoordinator 内部 API。

#### 6.1 三类调用语义

| 调用类型 | Registry | ToolCoordinator | Audit emit |
|---------|----------|:---:|:---:|
| **外部调用** (DSL workflow → orchestration/execute) | `external_registry_` (经 ToolCoordinator 装饰) | ✅ | ✅ |
| **内部调用** (编排 → inference/* tools) | `internal_registry_` (无 ToolCoordinator) | ❌ 豁免 | ✅ 仅 emit `tool.audit.internal.{tool}` |
| **数据查询** (编排 → inference/get/*) | `internal_registry_` | ❌ 豁免 | ❌ ReadOnly 无需审计 |

#### 6.2 internal_registry 构造

```cpp
OrchestrationPlugin::OrchestrationPlugin(ToolRegistry& base_registry,
                                          shared_ptr<IInteractionBus> bus)
  : bus_(bus),
    // internal_registry: 直接持有 ToolRegistry 引用, 无 ToolCoordinator 装饰
    internal_registry_(base_registry),
    // external_registry: 经过 ToolCoordinator 装饰, 用于面向 DSL 的 orchestration/* tools
    external_registry_(base_registry,
                       make_policy_for_mode(),
                       approval_callback_,
                       bus_)
{
  // 订阅推理事件 (见 §4)
}
```

#### 6.3 审计补偿

为防止内部调用成为"暗角", 编排 Plugin 内部调用 `inference/*` 时**主动 emit** 审计事件:

```cpp
// 编排 Plugin 内部 wrapper
auto orchestrated_call_tool = [this](const string& name, const auto& args) {
  // 1. emit 内部 audit (区别于 ToolCoordinator 的外部 audit)
  bus_->emit("orchestration.audit.internal." + name, ToolResult{
    .meta = {{"source", "internal"}, {"policy_mode", current_mode_}}
  });
  // 2. 实际调用 (不经 ToolCoordinator)
  return internal_registry_.call_tool(name, args);
};
```

**审计事件命名**:
- 外部调用 (经 ToolCoordinator): `tool.audit.invoked` / `tool.audit.completed` / `tool.audit.denied` (ADR-0031 §决策 7)
- 内部编排调用: `orchestration.audit.internal.{tool_name}`
- 内部 LLM 调用 (ILLMProvider→inference/generate): `orchestration.audit.llm.generate`

#### 6.4 安全保证

| 风险 | 缓解 |
|------|------|
| 编排 Plugin 滥用内部豁免做破坏性操作 | 内部 audit emit 是强制的 (TUI/Telemetry 可见); ToolCategory=StateModify/Execute 的工具仍受 approval_policy 元数据约束 (ApprovalPolicy=plan 在内部 registry 装饰 metadata, 仅元数据层约束不被强制审批) |
| 编排 Plugin 意外破坏其他 Plugin 的状态 | 编排 Plugin 只对自己注入的工具调用 internal_registry, 不会破坏第三方工具状态 |
| 数据污染 | 内部 audit 事件 record call site, 事后可追溯 |

**约束限制**: internal_registry 豁免**不**跳过 `tool_registry_.call_tool` 本身的 ToolCategory / Layer 检查 (ToolRegistry 自身的 read access control); 仅跳过 ToolCoordinator 的 Approval 步骤。Layer check (Cognitive / Thinking / Workflow) 仍生效。

#### 6.5 依赖注入约定

```cpp
// DSLEngine 构造时
ToolRegistry base_registry;  // 无 ToolCoordinator 装饰
auto orchestrator = OrchestrationPlugin::create(
  base_registry,  // 内部用
  base_registry,  // 外部用 (OrchestrationPlugin 内部装饰 ToolCoordinator)
  bus,
  router  // ADR-0034 IModelRouter
);
```

---

### 7. 测试策略 (P1 fix @Oracle review)

| # | 测试名 | 覆盖 |
|---|--------|------|
| 1 | `internal_vs_external_registry` | 内部调用不触发 ToolCoordinator 审批 |
| 2 | `internal_audit_emit` | 每次内部 invoke 都 emit orchestration.audit.internal.* |
| 3 | `illmprovider_chain_three_layers` | DSLEngine → 编排 ILLMProvider → 推理 ILLMProvider 链路 |
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