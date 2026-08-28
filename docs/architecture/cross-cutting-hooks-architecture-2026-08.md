# Cross-Cutting Hooks Architecture（横切架构与扩展指南）

**生成日期**: 2026-08-28
**最后验证**: 2026-08-28（v1.1 — PDK 模式重构 + 4 Pattern + CrossCuttingOrchestrator + 可选 Meta-Agent + 横切功能 DSL，验证命令见 §十）
**作者**: Architecture Working Group
**状态**: 🔍 Proposed v1.1（横切架构文档化 + PDK 模式重构 + Agent 编排管理策略，与 ADR-0081/0082/0085 联动）

> **定位**: 本文是 HydraForge 横切扩展架构的工作文档，覆盖现有 6 层抽象扩展点的设计原理 + 4 种功能扩展范式 + 通过 Agent first-class 编排管理横切功能的策略。
>
> **关联**:
> - 横切契约来源：ADR-0043（命名约定）、ADR-0069（Tool Hook）、ADR-0081（Agent Hook）、ADR-0082（Agent first-class Registry）、ADR-0083（IEvaluator）、ADR-0084（Mutation Governance）、ADR-0068（事件发射契约）
> - ship 证据：T14（行为回归）+ T15（Trajectory IR）+ T17（SkillCompiler）+ IEvaluator V2 + T19 GEPA + T21 Prompt Evidence Gate + ADR-0068 v1.4（27+ 主题注册）
> - 关联 cap-map §八（B7 自进化基础应用 MVP）

---

## 一、设计哲学：Why 横切架构

### 1.1 横切关注点（Cross-Cutting Concerns）的本质

Agent 系统中有 4 类典型的横切关注点：

| 关注点 | 示例 | 缺失后果 |
|--------|------|----------|
| **可观测性** | metrics/tracing/logging | 性能瓶颈不可诊断 |
| **安全合规** | PII 脱敏/审计/审批/限流 | 合规违规 |
| **资源控制** | cost tracking/token budget/rate limit | 资源耗尽 |
| **自适应** | retry/cache/circuit breaker | 系统脆弱 |

传统方案是在每个 Agent 业务逻辑中**硬编码**这些关注点 → 代码冗余 + 关注点耦合 + 难以复用。

**横切架构的目标**：把关注点从业务逻辑中**剥离**，通过**正交的扩展点**注入，使业务代码专注"做什么"，横切代码专注"怎么观察/控制/保障"。

### 1.2 HydraForge 的横切架构设计原则

1. **正交分层**：每层扩展点粒度不同，互不重叠，按需组合
2. **Agent first-class**：Agent 是运行时 first-class 实体（ADR-0082），横切能力可被 Agent 自身编排管理
3. **默认 fail-safe**：hook 失败/异常不阻断主流程（ADR-0081 §不变量）
4. **HookErrorPolicy 统一**：FailClosed/FailOpen 贯穿所有 hook 类型（ADR-0069）
5. **Decorator + Registry + Bus 三模式共存**：根据粒度选择最合适的注入方式
6. **零业务代码侵入**：所有横切关注点通过扩展点注入，business code 无需感知

### 1.3 与 Semantica 等项目的对比

| 维度 | Semantica | HydraForge 横切架构 |
|------|-----------|---------------------|
| 横切机制 | 依赖元数据 + 工作流组合 | **6 层抽象 Hook 矩阵** |
| 拦截粒度 | LLM call level | **LLM/Tool/Agent step/Lifecycle 全覆盖** |
| 装饰器链 | 无显式 | `ILLMProviderDecorator`（GoF Decorator + final 转发）|
| Hook 策略 | 自定义 | `HookErrorPolicy` (FailClosed/FailOpen) 统一 |
| 失败处理 | 自定义 | **不变量**：hook 异常不阻断主流程 |
| 事件系统 | 自定义 | ADR-0068 Canonical Topic Registry（27+ 主题）|
| Agent 编排 | 单一类型 + 工作流 | `IAgentRegistry` + `IAgentComposition`（多类型组合）|

**HydraForge 优势**：6 层抽象覆盖完整调用链 + Agent first-class 支持自管理。

---

## 二、6 层抽象扩展点矩阵（核心架构）

按拦截粒度从最内层到最外层：

```
┌─────────────────────────────────────────────────────────────────────┐
│  L5  IInteractionBus (全局事件流) ─── 27+ 主题, 任意订阅            │
├─────────────────────────────────────────────────────────────────────┤
│  L4  IApprovalHandler (执行授权) ─── single tool/skill call        │
├─────────────────────────────────────────────────────────────────────┤
│  L3  IAgentRegistry + IAgentComposition ─── Agent 生命周期/组合    │
├─────────────────────────────────────────────────────────────────────┤
│  L2  IAgentHookRegistry (Agent step hook) ─── per-step pre/post    │
├─────────────────────────────────────────────────────────────────────┤
│  L1  IToolHookRegistry (tool pre/post hook) ─── per-tool 调用       │
├─────────────────────────────────────────────────────────────────────┤
│  L0  ILLMProviderDecorator ─── per-LLM-call (final + 转发)        │
└─────────────────────────────────────────────────────────────────────┘
         调用顺序: Agent step (L2) → tool call (L1) → LLM (L0)
```

每层都是**正交**的，可独立叠加。

### L0 — ILLMProvider Decorator（最内层）

**位置**: `include/agenticdsl/contract/i_llm_provider_decorator.h`
**状态**: ✅ Shipped (Phase 5 C16, 2026-07-09)
**粒度**: 单次 LLM 调用

**已 ship 实现**:
- `CostTrackingDecorator` — P0 budget hole 修复
- `ComplianceDecorator` — hash-only 合规记录
- `RateLimitDecorator` — token-bucket 限流
- `TracingDecorator` — `llm.request` / `llm.response` 事件

**核心 API**:
```cpp
class ILLMProviderDecorator : public ILLMProvider {
public:
    Result generate(const GenerationRequest& req) override final {
        // 1. pre_check hook (optional)
        // 2. inner_->generate(req) — final 转发
        // 3. post_check hook (optional)
    }
};
```

**典型用法**（链式组合）:
```cpp
auto provider = std::make_unique<CostTrackingDecorator>(
    std::make_unique<ComplianceDecorator>(
        std::make_unique<RateLimitDecorator>(
            std::make_unique<RealOpenAIProvider>(config))));
engine.set_llm_provider(std::move(provider));
```

**适用场景**:
- Token 计费（CostTracking）
- 合规审计（Compliance）
- 速率限制（RateLimit）
- 性能追踪（Tracing）
- 缓存（Cache）
- 重试（Retry）

---

### L1 — IToolHookRegistry（工具调用 pre/post）

**位置**: `include/agenticdsl/contract/itool_hook_registry.h`
**状态**: ✅ Shipped (ADR-0069, 2026-08-04)
**粒度**: 单次工具调用前后

**核心 API**:
```cpp
enum class HookErrorPolicy { FailClosed, FailOpen };

struct PreHookResult {
    enum Action { Continue, Deny, ModifyArgs } action = Continue;
    std::unordered_map<std::string, std::string> modified_args;
    std::string deny_reason;
};

class IToolHookRegistry {
public:
    virtual size_t register_pre_hook(const std::string& tool_glob,
                                     ToolPreHook hook,
                                     int priority,
                                     HookErrorPolicy policy) = 0;
    // 类似 register_post_hook
};
```

**已 ship 实现**:
- 5 钩子类型: `tools/pre-execute` / `tools/post-execute` / `env/pre-validate` 等
- `HookErrorPolicy` FailClosed/FailOpen 策略
- tool_glob 通配符（`fs/*` / `*` / `inference.*`）

**典型用法**:
```cpp
ToolHookRegistry registry;
registry.register_pre_hook("fs/*", [](const ToolMetadata& m, const ToolCallContext& ctx) {
    if (m.category == ToolCategory::Dangerous) {
        return PreHookResult{Deny, {}, "dangerous_blocked_by_policy"};
    }
    return PreHookResult{Continue};
}, /*priority=*/100, HookErrorPolicy::FailClosed);
```

**适用场景**:
- 参数脱敏（DB query 参数 PII 过滤）
- 危险工具拦截（`fs.rm` / `shell.exec` 二次审批）
- 工具调用审计
- 调用超时控制
- 调用重试
- Metrics 埋点

---

### L2 — IAgentHookRegistry（Agent step pre/post）⭐ 关键横切点

**位置**: `include/agenticdsl/contract/iagent_hook_registry.h`
**状态**: ✅ Shipped (ADR-0081, 2026-08-21)
**粒度**: Agent 每 step 推理前后（**Agent-scoped**，非 per-LLM-call）

**核心 API**:
```cpp
struct AgentPreHookResult {
    enum Action { Continue, Deny, ModifyContext } action = Continue;
    std::string deny_reason;
    std::unordered_map<std::string, std::string> modified_context;
};

using AgentPreHook = std::function<AgentPreHookResult(
    const IAgent& agent, const std::string& step_input)>;

class IAgentHookRegistry {
public:
    virtual void register_pre_hook(const std::string& agent_glob,
                                   AgentPreHook hook,
                                   int priority,
                                   HookErrorPolicy policy) = 0;
};
```

**关键设计**:
- **Agent-scoped**（per-agent 类型注册）—— 与 L0 ILLMProvider Decorator 的关键差异
- `agent_glob` 通配（`react-loop/*` / `*`）
- 复用 `IToolHookRegistry` 的 `HookErrorPolicy`（避免双轨）
- 不变量：**hook 失败/异常不阻断主流程**

**典型用法**:
```cpp
AgentHookRegistry registry;

// 给所有 react-loop agent 添加 PII 过滤
registry.register_pre_hook("react-loop/*",
    [](const IAgent& agent, const std::string& step_input) {
        AgentPreHookResult r;
        if (agent.name() == "alice") {
            r.modified_context["scrubbed_input"] = pii_scrubber.scrub(step_input);
            r.action = AgentPreHookResult::ModifyContext;
        }
        return r;
    }, /*priority=*/50, HookErrorPolicy::FailClosed);
```

**适用场景**:
- **PII 过滤**（alice 角色专属）
- **Policy injection**（系统提示注入）
- **蒸馏 capture**（ADR-0080 D10 CaptureMode）
- **Scrub**（ADR-0080 v1.2 D10 Decouple）
- **Per-agent metrics**（按 agent 类型分流）
- **Per-agent rate limit**（不同 agent 不同配额）
- **Per-agent retry policy**

---

### L3 — IAgentRegistry + IAgentComposition（Agent 生命周期 + 编排）

**位置**:
- `include/agenticdsl/contract/iagent_registry.h` (ADR-0082 ✅ Shipped 2026-08-21)
- `include/agenticdsl/contract/iagent_composition.h` (ADR-0060 ✅ Shipped)

**粒度**: Agent 实例注册 + 派生 + 组合

**核心 API (AgentRegistry)**:
```cpp
class IAgent {
public:
    virtual const std::string& name() const = 0;  // 类型标识 "react-loop-v1"
    virtual const std::string& id() const = 0;    // 实例 ID
};

class IAgentRegistry {
public:
    virtual bool register_agent(const std::string& name, AgentFactory factory) = 0;
    virtual std::unique_ptr<IAgent> create(const std::string& name,
                                            const AgentConfig& config) = 0;
    virtual std::optional<IAgent> resolve(const std::string& id) const = 0;
    virtual std::vector<std::string> list() const = 0;
};
```

**核心 API (AgentComposition)**:
```cpp
class IAgentComposition {
public:
    virtual AgentResult<std::string> call(
        const std::string& agent_id,
        const std::string& args,
        const CompositionContext& ctx) = 0;
    
    virtual AgentResult<std::string> delegate(
        const std::string& parent_agent_id,
        const std::string& subagent_name,
        const std::string& args,
        const CompositionContext& ctx) = 0;
    
    virtual std::future<AgentResult<std::string>> call_async(...);
    virtual std::optional<StreamHandle> stream(...);
};
```

**4 种编排模式**:
1. **call** — 同步调用（等结果返回）
2. **call_async** — 异步调用（返回 future）
3. **delegate** — 委派（父 agent 调用子 agent）
4. **stream** — 流式调用（Phase 2 占位）

**典型用法**:
```cpp
// 注册 agent 类型
AgentRegistry registry;
registry.register_agent("react-loop-v1", [](const AgentConfig& cfg) {
    return std::make_unique<ReactLoopAgent>(cfg);
});

// 创建实例
auto agent = registry.create("react-loop-v1", {.instance_id = "alice-001"});

// 编排调用
AgentComposition comp(&registry);
auto result = comp.call("alice-001", "summarize this document", {});
```

**适用场景**:
- Agent 实例生命周期管理
- Agent 类型扩展（React/PlanExecute/ForkJoin）
- Agent 派生（spawn_agent DSL 节点）
- Agent 组合（多 agent 协同）
- Agent 自描述（AgentConfig 元数据）

---

### L4 — IApprovalHandler（执行授权）

**位置**: `include/agenticdsl/policy/iapproval_handler.h`
**状态**: ✅ Shipped (ADR-0031 §决策 5)
**粒度**: 单次执行前的人类/机器审批

**核心 API**:
```cpp
class IApprovalHandler {
public:
    virtual bool process_request(const ToolMetadata& meta,
                                 const ToolCallContext& ctx,
                                 const ToolPreview& preview) = 0;
};
```

**已 ship 集成**: T19 GEPA Phase 2 commit（agent + L3 必走审批）

**典型用法**:
```cpp
class StrictApprovalHandler : public IApprovalHandler {
    bool process_request(...) override {
        if (meta.allowed_layers.contains(Layer::L3)) {
            return ask_human_via_tui(meta, ctx, preview);  // 人类审批
        }
        return true;  // 其他层放行
    }
};
```

**适用场景**:
- 人类审批 gate（L3 危险工具）
- 自动审批策略（基于规则的 allow/deny/ask）
- 审批历史持久化（ADR-0079 Session 4-scope）
- 审批 delegation（agent-to-agent 授权）

---

### L5 — IInteractionBus（全局事件流）

**位置**: `include/agenticdsl/contract/iinteraction_bus.h`
**状态**: ✅ Shipped (2026-06-24, ADR-0019)
**粒度**: 全局事件订阅

**核心 API**:
```cpp
class IInteractionBus {
public:
    virtual void emit(const BusEvent& event) = 0;
    virtual size_t subscribe(const std::string& topic,
                             std::function<void(const BusEvent&)> callback) = 0;
    virtual void unsubscribe(size_t token) = 0;
};
```

**已 ship 主题** (ADR-0068 v1.4 附录 A 27+ 主题):
- `mutation.*` (4 主题, ADR-0084)
- `gepa.*` (6 主题, T19)
- `evaluation.result` (1 主题, ADR-0083)
- `skill.compilation.*` (3 主题, T17)
- `llm.dsl.*` (2 主题, T21)
- `prompt.*` (1 主题, T21)
- `cognitive.task.*` (2 主题)
- `domain.task.*` (3 主题)
- `dsl.call.*` (2 主题)
- `agent.*` (3 主题, ADR-0057)
- 等等

**典型用法**:
```cpp
// 全局审计
bus.subscribe("mutation.committed", [](const BusEvent& e) {
    audit_log.record(e.payload.data["mutation_id"]);
});

// 全局 metrics
bus.subscribe("cognitive.task.*", [](const BusEvent& e) {
    metrics.record_latency(e.payload.latency_ms);
});

// 跨子系统集成
bus.subscribe("gepa.commit.*", [](const BusEvent& e) {
    if (e.topic == "gepa.commit.committed") {
        external_dashboard.notify(e);
    }
});
```

**适用场景**:
- 全局审计/合规记录
- 跨子系统集成（外部 dashboard / SIEM / billing）
- 实时 metrics 收集
- 自适应 feedback loop（事件 → Agent hook）
- 调试 trace（开发期）

---

## 三、4 种功能扩展范式

按"扩展关注点的注入位置"分类。**不同范式可叠加使用**。

### 范式 1: Decorator 模式（透明包装）

**适用层**: L0（ILLMProviderDecorator）

**何时使用**:
- 想对每个 LLM 调用做完全相同的处理
- 不需要区分 agent / tool 类型
- 实现简单、零配置

**实现步骤**:
1. 继承 `ILLMProviderDecorator`
2. 重写 `generate()` / `generate_stream()` / `available_models()` 标记 `final`
3. 在 `generate()` 中: pre_check → `inner_->generate()` → post_check
4. 链式构造注入

**完整示例 — 实现 Retry Decorator**:
```cpp
class RetryDecorator : public ILLMProviderDecorator {
public:
    RetryDecorator(std::unique_ptr<ILLMProvider> inner, int max_retries = 3)
        : ILLMProviderDecorator(std::move(inner)), max_retries_(max_retries) {}
    
    Result generate(const GenerationRequest& req) override final {
        Result last_result;
        for (int i = 0; i < max_retries_; ++i) {
            last_result = inner_->generate(req);
            if (last_result.ok()) return last_result;
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << i)));
        }
        return last_result;
    }
    
private:
    int max_retries_;
};

// 使用
auto provider = std::make_unique<RetryDecorator>(
    std::make_unique<CostTrackingDecorator>(
        std::make_unique<RealOpenAIProvider>(config)), /*max_retries=*/5);
```

**优点**: 透明、易测试、易组合
**缺点**: 仅作用于 LLM 层，无法区分 agent / tool 类型

---

### 范式 2: Registry Hook 模式（基于 glob 通配）

**适用层**: L1（ToolHookRegistry）+ L2（AgentHookRegistry）

**何时使用**:
- 想针对特定类型/模式的资源（agent type / tool name）注入关注点
- 需要优先级排序（多个 hook 协同）
- 需要 `HookErrorPolicy`（FailClosed/FailOpen）

**实现步骤**:
1. 选择 Registry 类型（ToolHookRegistry / AgentHookRegistry）
2. 编写 hook 函数
3. 调用 `register_pre_hook()` / `register_post_hook()` 传入 glob + hook + priority + policy
4. Registry 在调用点自动触发匹配的 hook

**完整示例 — 实现 Tool Rate Limiter**:
```cpp
ToolHookRegistry registry;

// 给所有工具调用添加 100 req/s 限流
RateLimiter limiter(100 /* req/s */);
registry.register_pre_hook("*",
    [&limiter](const ToolMetadata& m, const ToolCallContext& ctx) {
        if (!limiter.try_acquire()) {
            return PreHookResult{Deny, {}, "rate_limit_exceeded"};
        }
        return PreHookResult{Continue};
    }, /*priority=*/1000, HookErrorPolicy::FailClosed);

// 给 fs.* 工具添加路径白名单
PathWhitelist whitelist("/safe/", "/tmp/");
registry.register_pre_hook("fs.*",
    [&whitelist](const ToolMetadata& m, const ToolCallContext& ctx) {
        auto path = ctx.args.at("path");
        if (!whitelist.contains(path)) {
            return PreHookResult{Deny, {}, "path_not_whitelisted"};
        }
        return PreHookResult{Continue};
    }, /*priority=*/500, HookErrorPolicy::FailClosed);
```

**hook 调用顺序**:
1. 按 priority 降序排列（priority 高的先执行）
2. 同 priority 按注册顺序
3. 任一 hook 返回 `Deny` 立即短路（除非 policy=FailOpen 且后续 hook 可覆盖）
4. ModifyArgs 累积合并（多个 hook 可链式修改参数）

**优点**: 灵活、glob 通配、优先级控制、fail-safe 默认
**缺点**: 需要理解 priority 语义

---

### 范式 3: Agent Composition 模式（运行时编排）

**适用层**: L3（IAgentRegistry + IAgentComposition）

**何时使用**:
- 需要运行时动态创建/组合 Agent
- 需要多 Agent 协同（父 agent 调用子 agent）
- 需要 Agent 生命周期管理

**实现步骤**:
1. 使用 `IAgentRegistry` 注册 Agent 类型（factory function）
2. 使用 `IAgentRegistry::create()` 实例化 Agent
3. 使用 `IAgentComposition::call()` / `call_async()` / `delegate()` 调用 Agent

**完整示例 — 实现 Subagent Spawning**:
```cpp
// 1. 注册 agent 类型
AgentRegistry registry;
registry.register_agent("react-loop-v1", [](const AgentConfig& cfg) {
    return std::make_unique<ReactLoopAgent>(cfg);
});
registry.register_agent("plan-execute-v1", [](const AgentConfig& cfg) {
    return std::make_unique<PlanExecuteAgent>(cfg);
});

// 2. 创建主 agent + 子 agent
auto main_agent = registry.create("react-loop-v1", {.instance_id = "main"});
auto sub_agent = registry.create("plan-execute-v1", {.instance_id = "sub"});

// 3. 主 agent 委派任务给子 agent
AgentComposition comp(&registry);
auto result = comp.delegate(main_agent->id(), sub_agent->name(),
                            "research quantum computing", {});

// 4. 异步流式调用
auto future = comp.call_async("main", "long-running task", {});
auto stream = comp.stream("main", "real-time analysis", {});
```

**4 种编排模式**:
| 模式 | 同步性 | 用途 |
|------|--------|------|
| `call` | 同步 | 短任务，等结果 |
| `call_async` | 异步 | 长任务，并发 |
| `delegate` | 同步委派 | 父子 agent 协同 |
| `stream` | 流式（V2 占位）| 实时响应 |

**优点**: Agent first-class、运行时组合清晰、支持多 agent 协同
**缺点**: 复杂场景需要 agent 间通信协议

---

### 范式 4: Event Bus 订阅模式（全局观察）

**适用层**: L5（IInteractionBus）

**何时使用**:
- 关注点是**观察**（metrics/audit/logging）而非**修改**
- 跨子系统集成（外部系统消费事件）
- 自适应反馈 loop（事件 → 触发其他 action）

**实现步骤**:
1. 获取 IInteractionBus 引用
2. 调用 `subscribe(topic_pattern, callback)` 订阅事件
3. 在 callback 中处理 BusEvent

**完整示例 — 实现全局 Mutation Auditor**:
```cpp
class MutationAuditor {
public:
    MutationAuditor(IInteractionBus* bus) {
        bus->subscribe("mutation.*", [this](const BusEvent& e) {
            handle_mutation_event(e);
        });
    }
    
private:
    void handle_mutation_event(const BusEvent& e) {
        if (e.topic == "mutation.committed") {
            auto mutation_id = e.payload.data["mutation_id"];
            auto agent_id = e.payload.data["agent_id"];
            audit_db.insert(mutation_id, agent_id, "committed",
                            std::chrono::system_clock::now());
        } else if (e.topic == "mutation.denied") {
            auto denial_reason = e.payload.data["denial_reason"];
            metrics.increment("mutation.denied", denial_reason);
        }
    }
    
    AuditDatabase audit_db;
    MetricsCollector metrics;
};
```

**优点**: 非侵入、跨子系统、解耦
**缺点**: 仅观察不修改（需配合其他范式）

---

## 四、PDK 模式管理横切功能（v1.1 重构）

> **v1.0 → v1.1 重构说明（重要）**：原文档 §四 提出 4 种"Agent 模式"（Side-effect / Policy / Orchestrator / Adapter）。本版本 v1.1 **修正**为：**4 范式独立 PDK Pattern + 1 个 CrossCuttingOrchestrator 编排器 + 可选 Meta-Agent 自管理**。
>
> **理由**：
> 1. **PDK 一致性**：现有 `LoopDispatcher` 模式是 3 个独立 Loop class（React/PlanExecute/ForkJoin）+ 1 个 dispatcher 模板分发，而非"Loop Agent god class"。横切功能管理应采用完全相同的 PDK 模式。
> 2. **SRP 原则**：原"Orchestrator Agent"违反单一职责——调度逻辑应在无状态的 Orchestrator class 而非 Agent。
> 3. **可扩展性**：新增第 5 种范式只需新增 1 个 PDK Pattern class + 注册到 Orchestrator，无需修改任何既有代码。
> 4. **DSL 实例化**：横切功能配置 DSL（`examples/cross_cutting/dsl/*.cc.md`）类比 Agent DSL（`*.agent.md`）。

### 4.1 核心设计：4 范式独立 PDK Pattern + Orchestrator

**与 PDK Loop Agent 的对等映射**:

| PDK Loop Agent（已 ship） | 横切功能管理（v1.1 推荐） |
|---------------------------|---------------------------|
| `class ReactLoop` 实现 React 循环 | `class DecoratorPattern` 实现 Decorator 范式 |
| `class PlanExecuteLoop` 实现 3 阶段 | `class HookPattern` 实现 Hook 范式 |
| `class ForkJoinLoop` 实现并发分支 | `class CompositionPattern` 实现 Composition 范式 |
| （未来）`class StreamLoop` 实现流式 | `class BusPattern` 实现 Event Bus 范式 |
| `LoopDispatcher<LoopType>` 模板分发 | `class CrossCuttingOrchestrator` 动态分发 |
| `loop_type: react_loop` DSL 字段 | `pattern_type: decorator-v1` 配置字段 |
| `examples/pdk_chat_demo/dsl/*.agent.md` 实例化 | `examples/cross_cutting/dsl/*.cc.md` 实例化 |
| `include/agenticdsl/pdk/agent_loops/*` | `include/agenticdsl/pdk/cross_cutting/*`（v1.1 实施） |

### 4.2 统一抽象：`ICrossCuttingPattern`

**位置（v1.1 待实施）**: `include/agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h`

```cpp
namespace hydraforge::pdk {

// 统一抽象: 4 范式共同接口 (类比 LoopResult)
class ICrossCuttingPattern {
public:
    virtual ~ICrossCuttingPattern() = default;
    
    // Pattern 唯一标识 (类比 AgentLoopType::React/PlanExecute/ForkJoin)
    virtual const std::string& name() const = 0;
    
    // 应用 pattern 配置到目标基础设施 (类比 Loop::run)
    //   decorator-v1 → 修改 ILLMProvider 链
    //   hook-v1      → 注册 hook 到 ToolHookRegistry/AgentHookRegistry
    //   composition-v1 → 创建 agent 并注入到目标 registry
    //   bus-v1       → 订阅 IInteractionBus 主题
    virtual void apply(const nlohmann::json& pattern_config,
                      CrossCuttingContext& ctx) = 0;
};

// Pattern 标识常量 (类比 AgentLoopType 枚举)
namespace cross_cutting_pattern {
    constexpr const char* Decorator = "decorator-v1";
    constexpr const char* Hook = "hook-v1";
    constexpr const char* Composition = "composition-v1";
    constexpr const char* Bus = "bus-v1";
}

// 共享上下文 (4 范式都需要的基础设施引用)
struct CrossCuttingContext {
    IAgentRegistry* agent_registry;
    IAgentHookRegistry* agent_hook_registry;
    IToolHookRegistry* tool_hook_registry;
    IInteractionBus* bus;
    ILLMProvider** llm_provider_slot;  // 槽位供 decorator 替换
};

}  // namespace hydraforge::pdk
```

### 4.3 4 范式独立 PDK 实现（v1.1 实施路径）

#### Pattern 1: DecoratorPattern

**位置**: `include/agenticdsl/pdk/cross_cutting/decorator_pattern.h`

```cpp
namespace hydraforge::pdk {

class DecoratorPattern : public ICrossCuttingPattern {
public:
    const std::string& name() const override {
        static const std::string n = "decorator-v1";
        return n;
    }
    
    void apply(const nlohmann::json& config, CrossCuttingContext& ctx) override {
        // 配置示例: {"decorators": ["CostTracking", "Compliance", "Retry"]}
        auto decorators = config["decorators"].get<std::vector<std::string>>();
        for (const auto& name : decorators) {
            auto decorator = DecoratorFactory::create(name, std::move(*ctx.llm_provider_slot));
            *ctx.llm_provider_slot = std::move(decorator);
        }
    }
};

}  // namespace hydraforge::pdk
```

#### Pattern 2: HookPattern

**位置**: `include/agenticdsl/pdk/cross_cutting/hook_pattern.h`

```cpp
namespace hydraforge::pdk {

class HookPattern : public ICrossCuttingPattern {
public:
    const std::string& name() const override {
        static const std::string n = "hook-v1";
        return n;
    }
    
    void apply(const nlohmann::json& config, CrossCuttingContext& ctx) override {
        // 配置示例: {"hooks": [{"glob": "L3_*", "type": "pre", "policy": "FailClosed", "handler": "human-approval"}]}
        for (const auto& hook_cfg : config["hooks"]) {
            std::string target_registry = hook_cfg.value("target", "tool");  // "tool" / "agent"
            auto hook = HookFactory::create(hook_cfg["handler"], hook_cfg);
            
            if (target_registry == "tool") {
                ctx.tool_hook_registry->register_pre_hook(
                    hook_cfg["glob"], hook, hook_cfg["priority"], 
                    parse_policy(hook_cfg["policy"]));
            } else if (target_registry == "agent") {
                ctx.agent_hook_registry->register_pre_hook(
                    hook_cfg["glob"], hook, hook_cfg["priority"],
                    parse_policy(hook_cfg["policy"]));
            }
        }
    }
};

}  // namespace hydraforge::pdk
```

#### Pattern 3: CompositionPattern

**位置**: `include/agenticdsl/pdk/cross_cutting/composition_pattern.h`

```cpp
namespace hydraforge::pdk {

class CompositionPattern : public ICrossCuttingPattern {
public:
    const std::string& name() const override {
        static const std::string n = "composition-v1";
        return n;
    }
    
    void apply(const nlohmann::json& config, CrossCuttingContext& ctx) override {
        // 配置示例: {"agents": [{"name": "privacy-policy-v1", "scope": "react-loop/*"}]}
        for (const auto& agent_cfg : config["agents"]) {
            auto agent = ctx.agent_registry->create(agent_cfg["name"], {});
            auto hook = [agent](const IAgent& a, const std::string& input) {
                return static_cast<PrivacyPolicyAgent*>(agent.get())
                    ->enforce_policy(a, input);
            };
            ctx.agent_hook_registry->register_pre_hook(
                agent_cfg["scope"], hook, /*priority=*/100, HookErrorPolicy::FailClosed);
        }
    }
};

}  // namespace hydraforge::pdk
```

#### Pattern 4: BusPattern

**位置**: `include/agenticdsl/pdk/cross_cutting/bus_pattern.h`

```cpp
namespace hydraforge::pdk {

class BusPattern : public ICrossCuttingPattern {
public:
    const std::string& name() const override {
        static const std::string n = "bus-v1";
        return n;
    }
    
    void apply(const nlohmann::json& config, CrossCuttingContext& ctx) override {
        // 配置示例: {"subscriptions": ["mutation.*"], "handler": "siem-adapter-v1"}
        for (const auto& topic : config["subscriptions"]) {
            ctx.bus->subscribe(topic, [ctx, config](const BusEvent& e) {
                forward_to_handler(config["handler"], e, ctx);
            });
        }
    }
};

}  // namespace hydraforge::pdk
```

### 4.4 Orchestrator: CrossCuttingOrchestrator（类比 LoopDispatcher）

**位置**: `include/agenticdsl/pdk/cross_cutting/cross_cutting_orchestrator.h`

```cpp
namespace hydraforge::pdk {

// Orchestrator 是无状态的分发器 (类比 LoopDispatcher 编译期分发)
// 关键差异: CrossCuttingOrchestrator 是运行时分发 (基于 JSON 配置),
//           LoopDispatcher 是编译期分发 (基于模板特化)
class CrossCuttingOrchestrator {
public:
    CrossCuttingOrchestrator(IAgentRegistry& agent_reg,
                              IAgentHookRegistry& agent_hook_reg,
                              IToolHookRegistry& tool_hook_reg,
                              IInteractionBus& bus)
        : ctx_{&agent_reg, &agent_hook_reg, &tool_hook_reg, &bus, nullptr} {
        register_pattern(std::make_unique<DecoratorPattern>());
        register_pattern(std::make_unique<HookPattern>());
        register_pattern(std::make_unique<CompositionPattern>());
        register_pattern(std::make_unique<BusPattern>());
    }
    
    // 主入口: 类比 LoopDispatcher::dispatch(loop_type)
    void dispatch(const nlohmann::json& cross_cutting_config) {
        ctx_.llm_provider_slot = current_llm_provider_slot_;
        
        for (const auto& pattern_cfg : cross_cutting_config["patterns"]) {
            std::string type = pattern_cfg["type"];
            auto it = patterns_.find(type);
            if (it == patterns_.end()) {
                throw std::runtime_error("Unknown cross_cutting pattern: " + type);
            }
            it->second->apply(pattern_cfg["config"], ctx_);  // 各范式独立 apply
        }
    }
    
    // 注册自定义 pattern (扩展点)
    void register_pattern(std::unique_ptr<ICrossCuttingPattern> pattern) {
        patterns_[pattern->name()] = std::move(pattern);
    }
    
private:
    CrossCuttingContext ctx_;
    std::unordered_map<std::string, std::unique_ptr<ICrossCuttingPattern>> patterns_;
    ILLMProvider** current_llm_provider_slot_ = nullptr;  // 由 DSLEngine 提供
};

}  // namespace hydraforge::pdk
```

**关键设计点**：
- **无状态**：Orchestrator 只持有基础设施引用 + pattern 集合，**不存储业务状态**
- **运行时分发**：基于 JSON 配置动态选择 pattern（vs LoopDispatcher 的编译期模板特化）
- **扩展点**：`register_pattern()` 方法允许注册自定义 pattern（V2 扩展）

### 4.5 横切功能 DSL（类比 Agent DSL）

**位置**: `examples/cross_cutting/dsl/*.cc.md`（v1.1 实施）

```yaml
# examples/cross_cutting/dsl/high_security_mode.cc.md
# 横切功能 DSL - 高安全模式

### AgenticDSL `/__meta__`
```yaml
version: "1.0"
mode: high_security
description: "Enable strict privacy + audit + approval"
```

### AgenticDSL `/cross_cutting`
```yaml
patterns:
  # Pattern 1: LLM 装饰器链
  - type: decorator-v1
    config:
      decorators: ["CostTracking", "Compliance", "PII-Scrub", "Retry"]
      
  # Pattern 2: Tool + Agent hooks
  - type: hook-v1
    config:
      hooks:
        - target: tool
          glob: "L3_*"
          type: pre
          priority: 1000
          policy: FailClosed
          handler: human-approval-v1
        - target: agent
          glob: "react-loop/*"
          type: pre
          priority: 500
          policy: FailClosed
          handler: privacy-policy-v1
          
  # Pattern 3: Composition (Agent first-class)
  - type: composition-v1
    config:
      agents:
        - name: privacy-policy-v1
          scope: "react-loop/*"
          config: {pii_patterns: ["email", "phone", "ssn"]}
        - name: metrics-collector-v1
          scope: "*"
          config: {metrics_backend: "prometheus"}
          
  # Pattern 4: Event Bus 订阅
  - type: bus-v1
    config:
      subscriptions: ["mutation.committed", "gepa.commit.committed"]
      handler: external-siem-adapter-v1

### AgenticDSL `/meta_agent` (可选)
```yaml
# 可选: 启用 Meta-Agent 自管理模式
meta_agent:
  enabled: true
  name: "cross-cutting-meta-v1"
  self_configure_goals:
    - "high_security_mode"
    - "cost_optimization_mode"
    - "development_mode"
```

### 使用方式:
```cpp
// 加载 DSL
CrossCuttingConfig config = CrossCuttingConfig::load("high_security_mode.cc.md");

// 初始化 Orchestrator
CrossCuttingOrchestrator orch(agent_registry, agent_hook_registry,
                                tool_hook_registry, bus);

// 应用横切配置 (一次 dispatch 应用所有 patterns)
orch.dispatch(config.to_json());
```

### 4.6 自管理 Meta-Agent（可选高级特性）

**位置**: `include/agenticdsl/pdk/cross_cutting/cross_cutting_meta_agent.h`（v1.1 实施）

> **设计争议**：v1.0 提出 `CrossCuttingMetaAgent` 作为自管理入口。v1.1 修正为**可选高级特性**，因为：
> 1. **SRP 违反**：单点决策所有范式 = god class
> 2. **违反 Loop 设计哲学**：PDK Loop 没有"MetaLoop"集中决策
> 3. **使用场景窄**：仅"自适应系统"场景需要，多数应用用 Config/Compile-time 即可
>
> **替代方案**：Orchestrator 是必需的核心组件；Meta-Agent 是可选包装层。

```cpp
namespace hydraforge::pdk {

// 可选: Meta-Agent 自管理（基于 Orchestrator 之上）
class CrossCuttingMetaAgent : public IAgent {
public:
    CrossCuttingMetaAgent(const AgentConfig& cfg,
                          CrossCuttingOrchestrator& orch,
                          ConfigRegistry& config_registry)
        : cfg_(cfg), orch_(orch), config_registry_(config_registry) {}
    
    std::string name() const override {
        static const std::string n = "cross-cutting-meta-v1";
        return n;
    }
    std::string id() const override { return cfg_.instance_id; }
    
    // Meta-Agent 自主决定启用哪些横切功能
    AgentResult<std::string> self_configure(const std::string& goal) {
        // 1. 查询预定义 goal 配置 (类比 pdk_chat_demo preset)
        auto config = config_registry_.get_goal_config(goal);
        if (!config) {
            return {"", {}, ErrorCode::NotFound, "Unknown goal: " + goal};
        }
        
        // 2. 调用 Orchestrator 应用配置
        orch_.dispatch(config->to_json());
        
        return {"configured_for_" + goal, {}, std::nullopt, ""};
    }
    
    // 注册为 agent（可选）
    void register_to(IAgentRegistry& registry) {
        registry.register_agent(name(), [this](const AgentConfig& cfg) {
            return std::make_unique<CrossCuttingMetaAgent>(cfg, orch_, config_registry_);
        });
    }
    
private:
    AgentConfig cfg_;
    CrossCuttingOrchestrator& orch_;
    ConfigRegistry& config_registry_;
};

}  // namespace hydraforge::pdk
```

**Meta-Agent 工作流**:
```
用户: meta_agent.self_configure("high_security_mode")
  ↓
MetaAgent → ConfigRegistry.get("high_security_mode") → CrossCuttingConfig
  ↓
MetaAgent → CrossCuttingOrchestrator.dispatch(config)
  ↓
Orchestrator → 遍历 patterns → 4 Pattern.apply() 独立执行
  ↓
结果: decorator-v1 + hook-v1 + composition-v1 + bus-v1 全部生效
```

**适用场景**（仅以下情况需要 Meta-Agent）:
- **自适应系统**: 根据运行时目标（high_security / cost_optimization / dev_mode）自动切换横切策略
- **多租户配置**: 每个租户有独立的横切配置，Meta-Agent 按租户 ID 加载

**不适用场景**（直接用 Orchestrator 即可）:
- 单环境部署 → 用 Config 驱动（策略 2）
- 固定配置 → 用 Compile-time 注入（策略 1）
- 动态启用/禁用单个功能 → 用 Hot-Reload Manager（策略 3）

### 4.7 4 种管理策略（保留 v1.0 但更新实施）

#### 策略 1: Compile-time 注入（直接调用 Orchestrator）

```cpp
int main() {
    AgentRegistry registry;
    AgentHookRegistry agent_hooks;
    ToolHookRegistry tool_hooks;
    InteractionBus bus;
    
    // 初始化 Orchestrator
    CrossCuttingOrchestrator orch(registry, agent_hooks, tool_hooks, bus);
    
    // 直接 dispatch DSL 配置
    orch.dispatch(load_yaml("high_security_mode.cc.md"));
    
    return run_app(registry, bus);
}
```

**适用场景**: 单环境、配置固定

#### 策略 2: Config 驱动（从 YAML/JSON 加载）

```cpp
// 启动时从配置加载
CrossCuttingConfig config = CrossCuttingConfig::load("cross_cutting.yaml");
CrossCuttingOrchestrator orch(...);
orch.dispatch(config.to_json());
```

#### 策略 3: Hot-Reload（运行时动态增删）

```cpp
class HotReloadManager {
    void enable_pattern(const std::string& pattern_name,
                       const nlohmann::json& pattern_config,
                       CrossCuttingOrchestrator& orch) {
        orch.dispatch({{"patterns", {{
            {"type", pattern_name},
            {"config", pattern_config}
        }}}});
    }
    
    void disable_pattern(const std::string& pattern_name) {
        // TODO: 反向取消 (V2 需要 pattern 撤回支持)
    }
};
```

#### 策略 4: Meta-Agent 自管理（可选高级特性）

见 §4.6。

---

## 5.0 与 PDK Loop Agent 的对等映射（v1.1 关键洞察）

| PDK Loop Agent（已 ship） | 横切功能管理（v1.1 推荐） |
|---------------------------|---------------------------|
| 3 个独立 Loop class | **4 个独立 Pattern class** |
| `LoopDispatcher<LoopType>` 编译期模板分发 | `CrossCuttingOrchestrator` 运行时 JSON 分发 |
| `AgentLoopType` 枚举 | `cross_cutting_pattern::Decorator/Hook/Composition/Bus` 常量 |
| `LoopResult` 统一返回类型 | `void apply(...)` 统一应用接口 + `ICrossCuttingPattern` 抽象 |
| `loop_type: react_loop` DSL 字段 | `pattern_type: decorator-v1` 配置字段 |
| `examples/pdk_chat_demo/dsl/*.agent.md` | `examples/cross_cutting/dsl/*.cc.md` |
| `DEFINE_AGENT` 宏（agent_macros.h） | （V2 待定，可考虑 `DECLARE_CROSS_CUTTING` 宏） |
| **无 MetaLoop** | **无强制 MetaCrossCutting**（可选 MetaAgent） |

**关键洞察**：PDK Loop 设计从未做"集中决策的 MetaLoop"，而是用 dispatcher 模板 + 独立 Loop class 组合。横切功能管理应遵循完全相同的设计哲学——**Orchestrator 是必需核心**，**MetaAgent 是可选包装**。

### 5.1 与 Semantica 重新对比（v1.1 更新）

| 维度 | Semantica | HydraForge 横切架构 v1.1 |
|------|-----------|-------------------------|
| 横切机制 | 依赖元数据 + 工作流组合 | **6 层抽象 + 4 PDK Pattern + Orchestrator** |
| 拦截粒度 | LLM call level | LLM/Tool/Agent step/Lifecycle 全覆盖 |
| 范式抽象 | 扁平工作流 | **4 范式独立 PDK Pattern + 抽象接口** |
| 扩展性 | 单一集中决策 | **register_pattern() 扩展点 + DSL 声明** |
| Agent 编排 | 单一类型 + 工作流 | `IAgentRegistry` + `IAgentComposition` |
| 装饰器链 | 无显式 | `ILLMProviderDecorator`（GoF Decorator + final 转发）|
| Hook 策略 | 自定义 | `HookErrorPolicy` (FailClosed/FailOpen) 统一 |
| 失败处理 | 自定义 | **不变量**：hook 异常不阻断主流程 |
| 事件系统 | 自定义 | ADR-0068 Canonical Topic Registry（27+ 主题）|
| DSL 实例化 | 自定义 | `examples/cross_cutting/dsl/*.cc.md` 类比 Agent DSL |

---

### 4.3 Agent 编排管理横切功能的 4 种策略

#### 策略 1: 注册时注入（Compile-Time）

**做法**: 横切功能 Agent 在 main() 启动时注册，并自动注入到目标 agent。

```cpp
int main() {
    AgentRegistry registry;
    InteractionBus bus;
    
    // 注册业务 agents
    registry.register_agent("react-loop-v1", ...);
    
    // 注册横切功能 agents
    registry.register_agent("privacy-policy-v1", ...);
    registry.register_agent("metrics-collector-v1", ...);
    registry.register_agent("external-siem-adapter-v1", ...);
    
    // 自动注入横切 hooks
    auto privacy = registry.create("privacy-policy-v1", {});
    AgentHookRegistry hooks;
    hooks.register_pre_hook("react-loop/*", 
        [privacy](const IAgent& a, const std::string& i) {
            return static_cast<PrivacyPolicyAgent*>(privacy.get())
                ->enforce_privacy_policy(a, i);
        }, 100, HookErrorPolicy::FailClosed);
    
    // 自动注入 metrics subscriber
    auto metrics = registry.create("metrics-collector-v1", {});
    bus.subscribe("agent.*", [metrics](const BusEvent& e) {
        static_cast<MetricsCollectorAgent*>(metrics.get())->process_event(e);
    });
    
    return run_app(registry, hooks, bus);
}
```

**适用场景**: 应用启动时固定配置

---

#### 策略 2: 配置驱动注入（Runtime Config）

**做法**: 横切功能 Agent 从 YAML/JSON 配置动态加载。

```yaml
# cross_cutting.yaml
agents:
  privacy-policy-v1:
    enabled: true
    priority: 100
    scope: "react-loop/*"
    config:
      pii_patterns: ["email", "phone", "ssn"]
      fail_closed: true
  
  metrics-collector-v1:
    enabled: true
    subscriptions: ["agent.*", "mutation.*", "gepa.*"]
  
  external-siem-adapter-v1:
    enabled: false  # 可禁用
    siem_endpoint: "https://siem.example.com"
```

```cpp
// 启动时从配置加载
CrossCuttingConfig config = load_yaml("cross_cutting.yaml");
for (auto& [name, agent_cfg] : config.agents) {
    if (agent_cfg.enabled) {
        auto agent = registry.create(name, {});
        inject_into_hooks_and_bus(agent, agent_cfg);
    }
}
```

**适用场景**: 多环境部署（dev/staging/prod 不同横切策略）

---

#### 策略 3: 运行时动态注入（Hot-Reload）

**做法**: 横切功能 Agent 可在运行时动态添加/移除，无需重启。

```cpp
class HotReloadManager {
public:
    void enable_cross_cutting(const std::string& agent_name) {
        auto agent = registry_.create(agent_name, {});
        auto hook_token = hooks_.register_pre_hook("*", 
            [agent](const IAgent& a, const std::string& i) {
                return agent->enforce_policy(a, i);
            });
        active_agents_[agent_name] = {agent, hook_token};
    }
    
    void disable_cross_cutting(const std::string& agent_name) {
        auto& entry = active_agents_[agent_name];
        hooks_.unregister_pre_hook(entry.hook_token);
        active_agents_.erase(agent_name);
    }
    
private:
    AgentRegistry& registry_;
    AgentHookRegistry& hooks_;
    std::unordered_map<std::string, ActiveAgent> active_agents_;
};

// 运行时动态启用
hot_reload.enable_cross_cutting("privacy-policy-v1");
hot_reload.enable_cross_cutting("metrics-collector-v1");
// 运行时禁用
hot_reload.disable_cross_cutting("external-siem-adapter-v1");
```

**适用场景**: 故障恢复、安全事件响应、A/B 测试

---

#### 策略 4: Agent 自管理（Agent-as-Manager）

**核心思想**: 让一个 **Meta-Agent** 管理所有横切功能 Agent 的生命周期。

```cpp
class CrossCuttingMetaAgent : public IAgent {
public:
    std::string name() const override { return "cross-cutting-meta-v1"; }
    std::string id() const override { return "meta"; }
    
    // Meta-Agent 自主决定启用哪些横切功能
    AgentResult<std::string> self_configure(const std::string& goal) {
        // 1. 分析 goal（如 "high_security_mode" / "cost_optimization"）
        // 2. 选择合适的横切功能 agent 组合
        // 3. 动态启用/禁用
        if (goal == "high_security_mode") {
            hot_reload_.enable_cross_cutting("privacy-policy-v1");
            hot_reload_.enable_cross_cutting("audit-logger-v1");
            hot_reload_.disable_cross_cutting("cost-optimizer-v1");
        }
        return {"configured_for_" + goal, {}, std::nullopt, ""};
    }
    
private:
    HotReloadManager& hot_reload_;
};
```

**适用场景**: 自适应系统（根据目标自动配置横切能力）

---

## 五、实战案例：给所有 Agent 添加 5 种横切功能（v1.1 更新）

> **v1.1 更新**：5 个案例现在通过 `CrossCuttingOrchestrator::dispatch()` 应用（而非 v1.0 手动注册 agent hook/bus）。每个案例展示对应的 DSL 配置 + 1 段 Orchestrator dispatch 代码。

### 案例 1: 全局 Metrics 收集

**需求**: 收集所有 agent 的 step_count / tool_call_count / token_usage

**实现**（范式 4: Event Bus + 模式 A: Side-effect Agent）:
```cpp
class GlobalMetricsAgent : public IAgent {
public:
    std::string name() const override { return "global-metrics-v1"; }
    
    void process_event(const BusEvent& e) {
        if (e.topic == "cognitive.task.completed") {
            metrics_.increment("agent.task.completed",
                               e.payload.data["agent_type"]);
        } else if (e.topic == "tool.execution.end") {
            metrics_.increment("agent.tool.calls",
                               e.payload.data["tool_name"]);
        }
    }
};

// 注入
auto metrics_agent = registry.create("global-metrics-v1", {});
bus.subscribe("cognitive.task.*", [metrics_agent](const BusEvent& e) {
    static_cast<GlobalMetricsAgent*>(metrics_agent.get())->process_event(e);
});
```

---

### 案例 2: 危险操作二次审批

**需求**: 所有 L3 危险工具调用前需要人类审批

**实现**（范式 2: Registry Hook + L1 拦截点）:
```cpp
registry.register_pre_hook("L3_*",
    [](const ToolMetadata& m, const ToolCallContext& ctx) {
        if (m.category == ToolCategory::Dangerous) {
            // 触发人类审批
            return approval_handler_->process_request(m, ctx, 
                                                       ToolPreview{m, ctx.args})
                   ? PreHookResult{Continue}
                   : PreHookResult{Deny, {}, "human_denied"};
        }
        return PreHookResult{Continue};
    }, /*priority=*/1000, HookErrorPolicy::FailClosed);
```

---

### 案例 3: PII 数据脱敏

**需求**: 给所有 agent 输入自动脱敏 PII

**实现**（范式 2 + 模式 B: Policy Agent + L2 拦截点）:
```cpp
auto privacy_agent = registry.create("privacy-policy-v1", {});
AgentHookRegistry hooks;
hooks.register_pre_hook("*",
    [privacy_agent](const IAgent& agent, const std::string& input) {
        return static_cast<PrivacyPolicyAgent*>(privacy_agent.get())
            ->enforce_policy(agent, input);  // 自动 scrub PII
    }, /*priority=*/500, HookErrorPolicy::FailClosed);
```

---

### 案例 4: 自动 Retry + Circuit Breaker

**需求**: 给所有 LLM 调用添加 3 次 retry + 失败时熔断

**实现**（范式 1: Decorator + L0 拦截点）:
```cpp
auto retry_decorator = std::make_unique<RetryDecorator>(
    std::make_unique<CircuitBreakerDecorator>(
        std::make_unique<RealOpenAIProvider>(config)),
    /*max_retries=*/3, /*circuit_threshold=*/5);
engine.set_llm_provider(std::move(retry_decorator));
```

---

### 案例 5: 跨子系统审计集成

**需求**: 所有 mutation 事件转发到外部 SIEM

**实现**（范式 4: Event Bus + 模式 D: Adapter Agent）:
```cpp
auto siem_agent = registry.create("external-siem-adapter-v1", {});
bus.subscribe("mutation.*", [siem_agent](const BusEvent& e) {
    static_cast<ExternalSystemAdapterAgent*>(siem_agent.get())
        ->forward_to_siem(e);
});
```

---

## 六、决策矩阵：何时使用哪种范式

| 关注点类型 | 推荐范式 | 推荐层级 | 推荐 Agent 模式 |
|------------|----------|----------|-----------------|
| **Metrics 收集** | 范式 4 (Event Bus) | L5 | 模式 A (Side-effect Agent) |
| **审计日志** | 范式 4 (Event Bus) | L5 | 模式 A 或 D (Adapter) |
| **PII 脱敏** | 范式 2 (Hook Registry) | L2 (per-agent) 或 L1 (per-tool) | 模式 B (Policy Agent) |
| **危险操作审批** | 范式 2 (Hook Registry) | L1 | 直接 hook（无需 Agent 包装） |
| **Token 计费** | 范式 1 (Decorator) | L0 | 直接 decorator（无需 Agent 包装） |
| **Retry** | 范式 1 (Decorator) | L0 | 直接 decorator |
| **Rate Limit** | 范式 1 或 2 | L0 或 L1 | 视粒度而定 |
| **外部系统集成** | 范式 4 (Event Bus) | L5 | 模式 D (Adapter Agent) |
| **自适应反思循环** | 范式 3 (Composition) | L3 | 模式 C (Orchestrator Agent) |
| **Policy 动态切换** | 范式 2 + 策略 3 | L2 | 模式 B + Hot-Reload |
| **跨 Agent 工作流** | 范式 3 (Composition) | L3 | 模式 C (Orchestrator Agent) |
| **多环境差异化** | 范式 2 + 策略 2 | L2 | 配置驱动注入 |

---

## 七、测试与验证

### 7.1 每种范式的测试要求

**Decorator 范式测试**:
- 单元测试: 单个 decorator 的 pre/post hook 行为
- 集成测试: 链式 decorator 的组合行为
- 验证: hook 失败不影响主流程（try-catch 包裹）

**Hook Registry 范式测试**:
- 单元测试: glob 匹配、priority 排序、policy 行为
- 集成测试: 多 hook 协同（Deny 短路、ModifyArgs 累积）
- 验证: HookErrorPolicy FailClosed/FailOpen 行为

**Agent Composition 范式测试**:
- 单元测试: 单 agent call/delegate/stream
- 集成测试: 多 agent 协同工作流
- 验证: Agent first-class 行为（registry.create / resolve / unregister）

**Event Bus 范式测试**:
- 单元测试: subscribe / unsubscribe / emit
- 集成测试: 跨子系统事件传播
- 验证: 主题订阅 glob 匹配（如 `mutation.*`）

### 7.2 横切功能 Agent 测试

每个横切功能 Agent 必须包含：
1. **单元测试**: Agent 自身行为（process_event / enforce_policy / orchestrate）
2. **注册测试**: `registry.register_agent` 成功
3. **注入测试**: Agent 成功注入到目标 hook/bus
4. **端到端测试**: 横切功能生效（如 privacy policy 真的拦截了 PII 输入）

---

## 八、明确 out of scope（V2 deferred）

- **Agent 远程通信**（RPC/gRPC 跨进程 Agent 调用）—— V2 + ADR-0077 gRPC data plane
- **Agent 自动发现**（多 agent registry 协同发现）—— V2
- **Agent 联邦**（跨实例 agent 共享）—— V2
- **横切功能版本管理**（同一横切功能多版本并存）—— V2
- **动态 Policy 推理**（Agent 自学习横切策略）—— V2 + T22 Fine-tune

---

## 九、与 ADR/已 ship 实现的关联

### 9.1 直接 ship 的横切能力

| 能力 | Ship 状态 | 来源 |
|------|-----------|------|
| ILLMProviderDecorator 链 | ✅ Shipped (2026-07-09) | Phase 5 C16 |
| IToolHookRegistry + 5 钩子 | ✅ Shipped (2026-08-04) | ADR-0069 |
| IAgentHookRegistry | ✅ Shipped (2026-08-21) | ADR-0081 |
| IAgentRegistry + IAgent | ✅ Shipped (2026-08-21) | ADR-0082 |
| IAgentComposition (4 模式) | ✅ Shipped | ADR-0060 |
| IApprovalHandler | ✅ Shipped (2026-07-31) | ADR-0031 |
| IInteractionBus + 27+ 主题 | ✅ Shipped (2026-06-24) | ADR-0019/0068 v1.4 |
| HookErrorPolicy (FailClosed/FailOpen) | ✅ Shipped | ADR-0069 |

### 9.2 通过横切机制 ship 的能力

| 能力 | 横切机制 | 状态 |
|------|----------|------|
| T14 行为回归 | L1 ToolHook + L2 AgentHook | ✅ Shipped |
| T17 SkillCompiler event emission | L5 EventBus | ✅ Shipped |
| T19 GEPA Phase 2 commit | L3 Composition + L4 Approval + L5 EventBus | ✅ Shipped |
| T21 Prompt Evidence Gate | L5 EventBus (2 llm.dsl.*) | ✅ Shipped |
| IEvaluator V2 | L2 AgentHook (set_evaluator) | ✅ Shipped |
| MutationGovernor | L4 Approval + L5 EventBus | ✅ Shipped |

### 9.3 关联 ADR 编号

- ADR-0043: PDK 工具命名约定（`module.verb` + `agent_glob`/`tool_glob`）
- ADR-0060: Agent Composition 模式（call/call_async/delegate/stream）
- ADR-0068: 事件发射契约（Canonical Topic Registry 27+ 主题）
- ADR-0069: ToolCoordinator Hook（IToolHookRegistry + HookErrorPolicy）
- ADR-0081: Pre-Step Hook Contract（IAgentHookRegistry Agent-scoped）
- ADR-0082: Agent First-Class Registry（IAgentRegistry + IAgent 最小骨架）

---

## 十、验证命令

```bash
# 6 层抽象扩展点文件存在性
ls include/agenticdsl/contract/i_llm_provider_decorator.h
ls include/agenticdsl/contract/itool_hook_registry.h
ls include/agenticdsl/contract/iagent_hook_registry.h
ls include/agenticdsl/contract/iagent_registry.h
ls include/agenticdsl/contract/iagent_composition.h
ls include/agenticdsl/policy/iapproval_handler.h
ls include/agenticdsl/contract/iinteraction_bus.h

# ADR 状态
grep -E "ADR-(0043|0060|0068|0069|0081|0082)" docs/README.md | head -10

# 已 ship 横切能力 grep
grep -r "ILLMProviderDecorator\|IToolHookRegistry\|IAgentHookRegistry\|IAgentRegistry\|IAgentComposition\|IApprovalHandler" \
  include/agenticdsl/ --include="*.h" -l

# ctest 横切测试
ctest -R "test_(hook|agent|composition|approval|interaction)" --output-on-failure
```

---

## 十一、总结（v1.1 更新）

### v1.1 核心修正
- **删除** v1.0 §四 "4 Agent 模式"（Side-effect / Policy / Orchestrator / Adapter）
- **新增** v1.1 §四 "4 PDK Pattern + Orchestrator + 可选 Meta-Agent"
- **关键洞察**：横切功能管理应采用与 PDK Loop Agent **完全相同**的设计模式（独立 class + dispatcher 编排）

### 横切架构核心优势
1. **6 层抽象** 覆盖完整调用链（LLM → Tool → Agent step → Lifecycle → Global）
2. **正交分层** 互不耦合，按需组合
3. **4 PDK Pattern**（Decorator / Hook / Composition / Bus）独立可扩展
4. **Orchestrator 无状态分发**（类比 LoopDispatcher）
5. **Agent first-class** 横切功能可建模为 Agent（Composition Pattern）
6. **可选 Meta-Agent 自管理**（高级特性，非必需）
7. **fail-safe 默认** hook 失败不阻断主流程（ADR-0081 §不变量）
8. **HookErrorPolicy 统一** FailClosed/FailOpen 贯穿所有 hook
9. **DSL 实例化** `examples/cross_cutting/dsl/*.cc.md` 类比 `*.agent.md`
10. **事件驱动** 27+ 主题统一注册 + 全局订阅

### 对比 Semantica
HydraForge 在 **横切抽象粒度完整性、PDK 模式一致性、Agent first-class、HookErrorPolicy 标准化、事件驱动、DSL 实例化** 方面有显著优势。

### 与既有架构的对等性
| 维度 | PDK Loop Agent | 横切功能管理 |
|------|----------------|-------------|
| 独立 class | ReactLoop / PlanExecuteLoop / ForkJoinLoop | DecoratorPattern / HookPattern / CompositionPattern / BusPattern |
| Dispatcher | LoopDispatcher<LoopType> 模板 | CrossCuttingOrchestrator 动态 |
| DSL 实例化 | `*.agent.md` | `*.cc.md` |
| Meta 抽象 | （无）| （可选 Meta-Agent） |

### 关联 ADR

- **ADR-0081** Pre-Step Hook Contract（IAgentHookRegistry, Agent-scoped）— v1.1 §四 引用
- **ADR-0082** Agent First-Class Registry（IAgentRegistry）— CompositionPattern 依赖
- **ADR-0085** Cross-Cutting Pattern PDK（待创建）— v1.1 §四 4 Pattern + Orchestrator 设计依据
- **ADR-0069** ToolCoordinator Hook（IToolHookRegistry, HookErrorPolicy）— HookPattern 依赖
- **ADR-0068** Event Emission Contract（27+ 主题）— BusPattern 依赖
- **ADR-0021** PDK Design（PDK Plugin 范式）— 横切功能管理作为 PDK 子模式

### 下一步建议

**短期**:
1. 创建 ADR-0085（Cross-Cutting Pattern PDK 设计依据）
2. 创建 `pdk-cross-cutting-patterns` OpenSpec change 实施 4 Pattern + Orchestrator + DSL

**中期**:
3. 实施示例横切功能（全局 PII 脱敏 + Metrics + Audit 三件套）
4. 集成到 pdk_chat_demo（类比 examples/pdk_chat_demo/dsl/）

**长期**:
5. 横切功能 marketplace（社区贡献新 Pattern）
6. Meta-Agent 自管理（自适应系统场景）

### 问题

**你想接下来**:
1. 立即创建 ADR-0085（基于本文档 §4.1-4.5 设计依据）？
2. 创建 OpenSpec change `pdk-cross-cutting-patterns`（4 Pattern + Orchestrator + DSL + DSL examples + 测试）？
3. 实施示例横切功能（全局 PII 脱敏作为首个 Pattern 实现）？
4. 继续 Layer 3 收官（T20 AFlow MCTS）？
5. 或者其他方向？