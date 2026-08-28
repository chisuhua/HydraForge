# Cross-Cutting Hooks Architecture（横切架构与扩展指南）

**生成日期**: 2026-08-28
**最后验证**: 2026-08-28（v1.0 — 6 层抽象 + 4 范式 + Agent 编排，验证命令见 §十）
**作者**: Architecture Working Group
**状态**: 🔍 Proposed（横切架构文档化 + Agent 编排管理策略，与 ADR-0081/0082 联动）

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

## 四、Agent first-class 编排管理横切功能

### 4.1 设计原则：Agent 作为横切功能的"管理者"

**核心思想**: 横切功能（如 PII 过滤、metrics 收集、retry 策略）本身可以被建模为 **Agent**，由 IAgentRegistry 管理生命周期，由 IAgentComposition 编排调用。

**为什么这样做**:
1. **可发现性**：横切功能作为 agent 注册，可通过 registry.list() 发现所有可用功能
2. **可编排性**：横切功能可被主 agent 调用、组合、覆盖
3. **可观测性**：横切功能作为 agent，有独立 metrics / traces
4. **可演化性**：横切功能可独立 ship，无需改动业务代码
5. **可测试性**：横切功能作为 agent，可独立测试

### 4.2 横切功能 Agent 模式

#### 模式 A: Side-effect Agent（副作用代理）

**定义**: 副作用 Agent 不返回业务结果，只执行横切关注点（如 metrics 记录、审计）。

```cpp
class MetricsCollectorAgent : public IAgent {
public:
    MetricsCollectorAgent(const AgentConfig& cfg) : cfg_(cfg) {}
    
    std::string name() const override { return "metrics-collector-v1"; }
    std::string id() const override { return cfg_.instance_id; }
    
    // 接收 event payload，发送 metrics
    void process_event(const BusEvent& event) {
        auto metric_name = "agent.event." + event.topic;
        metrics_.increment(metric_name, event.payload);
    }
    
private:
    AgentConfig cfg_;
    MetricsCollector metrics_;
};

// 注册
registry.register_agent("metrics-collector-v1", [](const AgentConfig& cfg) {
    return std::make_unique<MetricsCollectorAgent>(cfg);
});
```

**使用场景**: metrics 收集、审计记录、log 持久化

---

#### 模式 B: Policy Agent（策略代理）

**定义**: Policy Agent 实现 `IAgentHookRegistry` 接口，对其他 agent 的行为施加策略约束。

```cpp
class PrivacyPolicyAgent : public IAgent {
public:
    std::string name() const override { return "privacy-policy-v1"; }
    std::string id() const override { return cfg_.instance_id; }
    
    // 作为 AgentHookRegistry 的 hook 注入
    AgentPreHookResult enforce_privacy_policy(const IAgent& agent,
                                              const std::string& step_input) {
        if (agent.name() == "react-loop-v1") {
            auto scrubbed = pii_scrubber_.scrub(step_input);
            if (scrubbed.contains_pii()) {
                return {Deny, "pii_detected_in_step_input", {}};
            }
            return {ModifyContext, "", {{"scrubbed_input", scrubbed.text()}}};
        }
        return {Continue, "", {}};
    }
    
private:
    PIIScrubber pii_scrubber_;
};

// 注册 + 注入
registry.register_agent("privacy-policy-v1", [](const AgentConfig& cfg) {
    return std::make_unique<PrivacyPolicyAgent>(cfg);
});

auto policy_agent = registry.create("privacy-policy-v1", {});

// 把 policy agent 作为 hook 注入到目标 agent hook registry
agent_hook_registry.register_pre_hook(
    "react-loop/*",
    [policy_agent](const IAgent& agent, const std::string& input) {
        return static_cast<PrivacyPolicyAgent*>(policy_agent.get())
            ->enforce_privacy_policy(agent, input);
    }, /*priority=*/100, HookErrorPolicy::FailClosed);
```

**使用场景**: PII 过滤、policy injection、合规检查

---

#### 模式 C: Orchestrator Agent（编排代理）

**定义**: Orchestrator Agent 通过 `IAgentComposition` 编排其他 agent 的调用顺序。

```cpp
class ReflexionOrchestrator : public IAgent {
public:
    std::string name() const override { return "reflexion-orchestrator-v1"; }
    std::string id() const override { return cfg_.instance_id; }
    
    // 编排: 失败 → 反思 → 修订 → 重试（基于 T19 GEPA Phase 2 commit）
    AgentResult<std::string> orchestrate(const std::string& task) {
        // 1. 调用主 agent
        auto result = comp_->call(main_agent_id_, task, {});
        if (result.ok) return result;
        
        // 2. 触发 GEPA 反思循环（另一个 agent）
        auto reflection_result = comp_->call(gepa_agent_id_, 
                                              "reflect on failure: " + result.message,
                                              {});
        
        // 3. 应用修订后的 prompt 重试
        return comp_->call(main_agent_id_, reflection_result.value, {});
    }
    
private:
    IAgentComposition* comp_;
    std::string main_agent_id_;
    std::string gepa_agent_id_;
};

// 注册
registry.register_agent("reflexion-orchestrator-v1", [](const AgentConfig& cfg) {
    return std::make_unique<ReflexionOrchestrator>(cfg);
});
```

**使用场景**: 多 agent 协同、反思循环、工作流编排

---

#### 模式 D: Adapter Agent（适配代理）

**定义**: Adapter Agent 将不同来源的事件/数据适配到统一接口。

```cpp
class ExternalSystemAdapterAgent : public IAgent {
public:
    std::string name() const override { return "external-siem-adapter-v1"; }
    std::string id() const override { return cfg_.instance_id; }
    
    // 订阅内部事件，转换为外部 SIEM 格式
    void setup_subscription(IInteractionBus* bus) {
        bus->subscribe("mutation.*", [this](const BusEvent& e) {
            forward_to_siem(e);
        });
        bus->subscribe("gepa.*", [this](const BusEvent& e) {
            forward_to_siem(e);
        });
    }
    
private:
    void forward_to_siem(const BusEvent& e) {
        // 转换为 SIEM 格式 (CEF / LEEF / JSON)
        auto siem_event = convert_to_cef(e);
        siem_client_.send(siem_event);
    }
    
    SIEMClient siem_client_;
};
```

**使用场景**: 外部系统集成（SIEM / billing / dashboard）、事件格式转换

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

## 五、实战案例：给所有 Agent 添加 5 种横切功能

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

## 十一、总结

HydraForge 横切架构的核心优势：
1. **6 层抽象** 覆盖完整调用链（LLM → Tool → Agent step → Lifecycle → Global）
2. **正交分层** 互不耦合，按需组合
3. **Agent first-class** 横切功能可被 Agent 自身管理（Side-effect / Policy / Orchestrator / Adapter）
4. **fail-safe 默认** hook 失败不阻断主流程
5. **HookErrorPolicy 统一** FailClosed/FailOpen 贯穿所有 hook
6. **事件驱动** 27+ 主题统一注册 + 全局订阅
7. **V1 已 ship** 多个横切能力（Decorator + Hook Registry + Approval Handler + EventBus）
8. **可扩展** 新增横切能力只需新增 hook 或 decorator，不修改业务代码

**对比 Semantica**: HydraForge 在 hook 粒度完整性、Agent first-class、事件驱动、HookErrorPolicy 标准化方面有显著优势。

**下一步建议**:
1. 创建横切功能 Agent 模板（基类）作为示例
2. 实施"全局 Metrics + Audit + Privacy"三件套示范 Agent
3. 文档化每种范式的具体实施步骤（已有 §三 + §四）

**问题**：你希望我接下来：
1. 创建示例横切功能 Agent 代码模板（Side-effect + Policy + Orchestrator + Adapter）？
2. 实施一个示范横切功能（如全局 PII 脱敏 Agent）作为完整示例？
3. 继续 Layer 3 收官（T20 AFlow MCTS）？
4. 或者其他方向？