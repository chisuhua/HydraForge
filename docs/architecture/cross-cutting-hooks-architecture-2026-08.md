# Cross-Cutting Hooks Architecture（横切架构与扩展指南）

**生成日期**: 2026-08-28
**最后验证**: 2026-08-28（v1.2 — Oracle 评审 H1-H4 + M1-M9 修正应用：真实 API 校正 + 命名空间卫生 + 文档清理，验证命令见 §十）
**作者**: Architecture Working Group
**状态**: 🔍 Proposed v1.2（横切架构文档化 + PDK 模式重构 + Agent 编排管理策略，与 ADR-0081/0082/0085 联动；Oracle 评审 H1-H4 + M1-M9 已应用）

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
auto provider = std::make_unique<agenticdsl::CostTrackingDecorator>(
    std::make_unique<agenticdsl::ComplianceDecorator>(
        std::make_unique<agenticdsl::RateLimitDecorator>(
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
agenticdsl::ToolHookRegistry registry;
registry.register_pre_hook("fs/*", [](const agenticdsl::ToolMetadata& m,
                                     const agenticdsl::ToolCallContext& ctx) {
    if (m.category == agenticdsl::ToolCategory::Dangerous) {
        return agenticdsl::PreHookResult{agenticdsl::PreHookResult::Deny, {},
                                         "dangerous_blocked_by_policy"};
    }
    return agenticdsl::PreHookResult{agenticdsl::PreHookResult::Continue};
}, /*priority=*/100, agenticdsl::HookErrorPolicy::FailClosed);
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
agenticdsl::InMemoryAgentHookRegistry registry;

// 给所有 react-loop agent 添加 PII 过滤
registry.register_pre_hook("react-loop/*",
    [](const agenticdsl::IAgent& agent, const std::string& step_input) {
        agenticdsl::AgentPreHookResult r;
        if (agent.name() == "alice") {
            r.modified_context["scrubbed_input"] = pii_scrubber.scrub(step_input);
            r.action = agenticdsl::AgentPreHookResult::ModifyContext;
        }
        return r;
    }, /*priority=*/50, agenticdsl::HookErrorPolicy::FailClosed);
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

**核心 API (AgentRegistry)** — `include/agenticdsl/contract/iagent_registry.h` (ADR-0082):
```cpp
class IAgent {
public:
    virtual const std::string& name() const = 0;  // 类型标识 "react-loop-v1"
    virtual const std::string& id() const = 0;    // 实例 ID
};

struct AgentConfig {
    std::string instance_id;  // 实例 ID（若空，create() 自动生成）
};

class IAgentRegistry {
public:
    virtual bool register_agent(const std::string& string_id, AgentFactory factory) = 0;
    virtual std::unique_ptr<IAgent> create(const std::string& string_id,
                                            const AgentConfig& config) = 0;
    virtual bool unregister(const std::string& string_id) = 0;
    virtual std::vector<std::string> list_registered() const = 0;
    virtual bool is_registered(const std::string& string_id) const = 0;
    virtual size_t size() const = 0;
};
```

> **⚠️ 真实 API（Oracle H1 修正）**: 无 `resolve(id)` / `list()` 虚构方法。
> 查询用 `list_registered()` / `is_registered()` / `size()`；`create()` 返回
> `std::unique_ptr<IAgent>`（未注册 string_id → nullptr）。

**核心 API (AgentComposition)** — `include/agenticdsl/contract/iagent_composition.h` (ADR-0060):
```cpp
template <typename T>
struct AgentResult {
    bool ok = false;
    T value{};
    std::optional<ErrorCode> error_code;
    std::string message;
};

class IAgentComposition {
public:
    virtual AgentResult<std::string> call(
        const std::string& agent_id,
        const std::string& args,
        std::chrono::milliseconds timeout = std::chrono::seconds(30)) = 0;

    virtual std::future<AgentResult<std::string>> call_async(
        const std::string& agent_id,
        const std::string& args,
        std::function<void(AgentResult<std::string>)> callback = nullptr,
        std::chrono::milliseconds timeout = std::chrono::seconds(30)) = 0;

    virtual TaskHandle delegate(
        const std::string& agent_id,
        const std::string& task,
        const std::string& priority = "normal") = 0;  // 返回 TaskHandle，非 AgentResult

    virtual StreamHandle stream(
        const std::string& agent_id,
        const std::string& args);  // Phase 2 占位（抛 logic_error）
};
```

**4 种编排模式**:
1. **call** — 同步调用（等结果返回）
2. **call_async** — 异步调用（返回 future）
3. **delegate** — 委派（父 agent 调用子 agent）
4. **stream** — 流式调用（Phase 2 占位）

**典型用法**:
```cpp
// 注册 agent 类型 (make_in_memory_agent_registry() 返回 unique_ptr<IAgentRegistry>)
auto registry = agenticdsl::make_in_memory_agent_registry();
registry->register_agent("react-loop-v1", [](const agenticdsl::AgentConfig& cfg) {
    return std::make_unique<ReactLoopAgent>(cfg);
});

// 创建实例 (AgentConfig 仅含 instance_id, 空则自动生成)
auto agent = registry->create("react-loop-v1", {.instance_id = "alice-001"});
bool known = registry->is_registered("react-loop-v1");  // true
size_t n = registry->size();                            // 1

// 编排调用 (IAgentComposition; call 第三参为超时, 默认 30s)
auto comp = agenticdsl::make_agent_composition(registry);
auto result = comp->call(agent->id(), "summarize this document",
                         std::chrono::seconds(30));
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
class StrictApprovalHandler : public agenticdsl::IApprovalHandler {
    bool process_request(...) override {
        if (meta.allowed_layers.contains(agenticdsl::Layer::L3)) {
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
bus.subscribe("mutation.committed", [](const agenticdsl::BusEvent& e) {
    audit_log.record(e.payload.data["mutation_id"]);
});

// 全局 metrics
bus.subscribe("cognitive.task.*", [](const agenticdsl::BusEvent& e) {
    metrics.record_latency(e.payload.latency_ms);
});

// 跨子系统集成
bus.subscribe("gepa.commit.*", [](const agenticdsl::BusEvent& e) {
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
2. 重写 `generate()` / `generate_stream()` / `available_models()` 标记 `final`（基类已 final）
3. 在 `generate()` 中: pre_check → `inner_->generate()` → post_check
4. 链式构造注入

> **⚠️ 真实模式（Oracle B2 修正）**: 基类 `generate()` / `generate_stream()` / `available_models()`
> 已标记 `final`（`i_llm_provider_decorator.h`），**子类无法 override `generate()`**。
> 子类只能 override protected `decorate_*` 钩子（`pre_check_generate` / `decorate_generate` /
> `decorate_generate_stream` / `decorate_available_models`），默认 pass-through。
> **链深硬约束**: `ILLMProviderDecorator::wrap_chain()` 限制装饰器数 ≤ 3（含 inner 总层数 ≤ 4），
> 超出抛 `DecoratorChainTooDeep`（4 decorators + 1 inner = 5 层非法）。

**完整示例 — 实现 Retry Decorator（真实钩子模式）**:
```cpp
class RetryDecorator : public agenticdsl::ILLMProviderDecorator {
public:
    RetryDecorator(std::unique_ptr<agenticdsl::ILLMProvider> inner, int max_retries = 3)
        : agenticdsl::ILLMProviderDecorator(std::move(inner)), max_retries_(max_retries) {}

protected:
    // 基类 generate() 为 final, 子类只能 override decorate_* 钩子 (真实模式)
    // 注意: decorate_generate 接收 inner 结果做后处理; 重试需在钩子内再次调用 inner_
    agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError>
    decorate_generate(const agenticdsl::GenerationRequest& req,
                      agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError> inner_result) override {
        for (int i = 0; i < max_retries_; ++i) {
            if (inner_result.ok()) return inner_result;
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << i)));
            // 重试: 再次调用 inner_ (真实实现可携带 stop_token)
            inner_result = inner_->generate(req, std::stop_token{});
        }
        return inner_result;
    }

private:
    int max_retries_;
};

// 使用 (链深: Retry + CostTracking + inner = 3 层 ≤ 4, 合法)
auto provider = std::make_unique<RetryDecorator>(
    std::make_unique<CostTrackingDecorator>(
        std::make_unique<RealOpenAIProvider>(config)), /*max_retries=*/3);
engine.set_llm_provider(std::move(provider));
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
agenticdsl::ToolHookRegistry registry;

// 给所有工具调用添加 100 req/s 限流
RateLimiter limiter(100 /* req/s */);
registry.register_pre_hook("*",
    [&limiter](const agenticdsl::ToolMetadata& m, const agenticdsl::ToolCallContext& ctx) {
        if (!limiter.try_acquire()) {
            return agenticdsl::PreHookResult{agenticdsl::PreHookResult::Deny, {}, "rate_limit_exceeded"};
        }
        return agenticdsl::PreHookResult{agenticdsl::PreHookResult::Continue};
    }, /*priority=*/1000, agenticdsl::HookErrorPolicy::FailClosed);

// 给 fs.* 工具添加路径白名单
PathWhitelist whitelist("/safe/", "/tmp/");
registry.register_pre_hook("fs.*",
    [&whitelist](const agenticdsl::ToolMetadata& m, const agenticdsl::ToolCallContext& ctx) {
        auto path = ctx.args.at("path");
        if (!whitelist.contains(path)) {
            return agenticdsl::PreHookResult{agenticdsl::PreHookResult::Deny, {}, "path_not_whitelisted"};
        }
        return agenticdsl::PreHookResult{agenticdsl::PreHookResult::Continue};
    }, /*priority=*/500, agenticdsl::HookErrorPolicy::FailClosed);
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
1. 使用 `agenticdsl::IAgentRegistry` 注册 Agent 类型（factory function）
2. 使用 `agenticdsl::IAgentRegistry::create()` 实例化 Agent
3. 使用 `agenticdsl::IAgentComposition::call()` / `call_async()` / `delegate()` 调用 Agent

**完整示例 — 实现 Subagent Spawning**:
```cpp
// 1. 注册 agent 类型 (make_in_memory_agent_registry() 返回 unique_ptr<IAgentRegistry>)
auto registry = agenticdsl::make_in_memory_agent_registry();
registry->register_agent("react-loop-v1", [](const agenticdsl::AgentConfig& cfg) {
    return std::make_unique<ReactLoopAgent>(cfg);
});
registry->register_agent("plan-execute-v1", [](const agenticdsl::AgentConfig& cfg) {
    return std::make_unique<PlanExecuteAgent>(cfg);
});

// 2. 创建主 agent + 子 agent (真实 AgentConfig 仅含 instance_id)
auto main_agent = registry->create("react-loop-v1", {.instance_id = "main"});
auto sub_agent = registry->create("plan-execute-v1", {.instance_id = "sub"});

// 3. 主 agent 委派任务给子 agent (make_agent_composition 返回 unique_ptr<IAgentComposition>)
auto comp = agenticdsl::make_agent_composition(registry);
comp->delegate(sub_agent->id(), "research quantum computing");

// 4. 异步调用 (call_async 返回 std::future<AgentResult<std::string>>)
auto future = comp->call_async(sub_agent->id(), "long-running task");

// 5. 流式调用 (Phase 2 占位, 抛 logic_error)
// auto stream = comp->stream(sub_agent->id(), "real-time analysis");
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
    MutationAuditor(agenticdsl::IInteractionBus* bus) {
        bus->subscribe("mutation.*", [this](const agenticdsl::BusEvent& e) {
            handle_mutation_event(e);
        });
    }
    
private:
    void handle_mutation_event(const agenticdsl::BusEvent& e) {
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

## 四、PDK 模式管理横切功能（v1.2 重构）

> **v1.0 → v1.2 重构说明（重要）**：原文档 §四 提出 4 种"Agent 模式"（Side-effect / Policy / Orchestrator / Adapter）。本版本 v1.2 **修正**为：**4 范式独立 PDK Pattern + 1 个 CrossCuttingOrchestrator 编排器 + 可选 Meta-Agent 自管理**。
>
> **理由**：
> 1. **PDK 一致性**：现有 `LoopDispatcher` 模式是 3 个独立 Loop class（React/PlanExecute/ForkJoin）+ 1 个 dispatcher 模板分发，而非"Loop Agent god class"。横切功能管理应采用完全相同的 PDK 模式。
> 2. **SRP 原则**：原"Orchestrator Agent"违反单一职责——调度逻辑应在无状态的 Orchestrator class 而非 Agent。
> 3. **可扩展性**：新增第 5 种范式只需新增 1 个 PDK Pattern class + 注册到 Orchestrator，无需修改任何既有代码。
> 4. **DSL 实例化**：横切功能配置 DSL（`examples/cross_cutting/dsl/*.cc.md`）类比 Agent DSL（`*.agent.md`）。
>
> **v1.1 → v1.2 增量（Oracle 评审 H1-H4 + M1-M9）**：真实 API 校正（H1/M3）、命名空间卫生（H2）、字段统一 `type:`（H3）、文档清理与案例重写（H4）、Orchestrator FailOpen 错误处理（M1）、`CrossCuttingContext` 增补 `approval_handler` L4 通道（M2）、Orchestrator 可选 pattern 注入（M7）、DSL schema 复用 ADR-0073（M6）。B1-B3 已在 v1.1 应用（`set_llm_provider` 回调 + `decorate_generate` 钩子 + 零-diff 清单）。

### 4.1 核心设计：4 范式独立 PDK Pattern + Orchestrator

**与 PDK Loop Agent 的对等映射**:

| PDK Loop Agent（已 ship） | 横切功能管理（v1.2 推荐） |
|---------------------------|---------------------------|
| `class ReactLoop` 实现 React 循环 | `class DecoratorPattern` 实现 Decorator 范式 |
| `class PlanExecuteLoop` 实现 3 阶段 | `class HookPattern` 实现 Hook 范式 |
| `class ForkJoinLoop` 实现并发分支 | `class CompositionPattern` 实现 Composition 范式 |
| （未来）`class StreamLoop` 实现流式 | `class BusPattern` 实现 Event Bus 范式 |
| `LoopDispatcher<LoopType>` 模板分发 | `class CrossCuttingOrchestrator` 动态分发 |
| `loop_type: react_loop` DSL 字段 | `type: decorator-v1` 配置字段 |
| `examples/pdk_chat_demo/dsl/*.agent.md` 实例化 | `examples/cross_cutting/dsl/*.cc.md` 实例化 |
| `include/agenticdsl/pdk/agent_loops/*` | `include/agenticdsl/pdk/cross_cutting/*`（v1.2 实施） |

### 4.2 统一抽象：`ICrossCuttingPattern`

**位置（v1.2 待实施）**: `include/agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h`

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
    agenticdsl::IAgentRegistry* agent_registry;
    agenticdsl::IAgentHookRegistry* agent_hook_registry;
    agenticdsl::IToolHookRegistry* tool_hook_registry;
    agenticdsl::IApprovalHandler* approval_handler;   // L4 通道 (HookPattern 审批集成, Oracle M2)
    agenticdsl::IInteractionBus* bus;
    // L0 通道: set_llm_provider 回调 (替代 v1.0 虚构的 ILLMProvider** llm_provider_slot 槽位, Oracle B1)
    // Orchestrator 不触碰既有 engine.h —— 回调由用户在构造 Orchestrator 时
    // 绑定到 DSLEngine::set_llm_provider (真实 API, src/core/engine.h:130)
    std::function<void(std::unique_ptr<agenticdsl::ILLMProvider>)> set_llm_provider;
};

}  // namespace hydraforge::pdk
```

### 4.3 4 范式独立 PDK 实现（v1.2 实施路径）

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
        // 配置示例: {"decorators": ["CostTracking", "Compliance", "PII-Scrub"]}
        // FailOpen (Oracle M1): 未绑定 set_llm_provider 回调时跳过, 不阻断主流程
        if (!ctx.set_llm_provider) return;
        auto decorators = config["decorators"].get<std::vector<std::string>>();
        if (decorators.size() > 3) {
            // 链深硬约束 (Oracle B2): 4 decorators + 1 inner = 5 层 > 4 → 抛 DecoratorChainTooDeep
            throw agenticdsl::ILLMProviderDecorator::DecoratorChainTooDeep(
                static_cast<int>(decorators.size()) + 1);
        }
        // 从内到外构造装饰器链 (最外层 = CostTracking 保证计费), 一次性注入
        // 链构造委托 DecoratorFactory (V1 实施, 类比 ILLMProviderDecorator::wrap_chain 静态工厂)
        auto chain = DecoratorFactory::create_chain(decorators);
        // L0 通道 (Oracle B1): 通过回调注入, Orchestrator 不触碰 engine.h
        ctx.set_llm_provider(std::move(chain));  // → DSLEngine::set_llm_provider (用户构造时绑定)
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
        // 配置示例: {"hooks": [{"target": "tool", "glob": "fs/*", "type": "pre",
        //           "priority": 1000, "policy": "FailClosed", "handler": "path-whitelist-v1"},
        //          {"target": "approval", "glob": "L3_*", "type": "pre",
        //           "priority": 1000, "policy": "FailClosed"}]}
        for (const auto& hook_cfg : config["hooks"]) {
            std::string target_registry = hook_cfg.value("target", "tool");  // "tool" / "agent" / "approval"
            std::string handler_name = hook_cfg.value("handler", "");
            agenticdsl::HookErrorPolicy policy = parse_policy(hook_cfg["policy"]);
            
            if (target_registry == "tool") {
                ctx.tool_hook_registry->register_pre_hook(
                    hook_cfg["glob"],
                    HookFactory::create_tool_hook(handler_name, hook_cfg, ctx),
                    hook_cfg["priority"], policy);
            } else if (target_registry == "agent") {
                ctx.agent_hook_registry->register_pre_hook(
                    hook_cfg["glob"],
                    HookFactory::create_agent_hook(handler_name, hook_cfg, ctx),
                    hook_cfg["priority"], policy);
            } else if (target_registry == "approval") {
                // Oracle M2: L4 审批通道 —— target="approval" 的 hook 直接调用
                // ctx.approval_handler->process_request(...) (真实 API, ADR-0031 ✅ ship)
                ctx.tool_hook_registry->register_pre_hook(
                    hook_cfg["glob"],
                    [&ctx](const agenticdsl::ToolMetadata& m,
                           const agenticdsl::ToolCallContext& c,
                           const std::unordered_map<std::string, std::string>& args) {
                        if (!ctx.approval_handler) {
                            // FailOpen: 未注入审批器时放行, 不阻断主流程 (Oracle M1)
                            return agenticdsl::PreHookResult{agenticdsl::PreHookResult::Continue};
                        }
                        agenticdsl::ToolPreview preview;  // 真实结构: command_line / risk_summary / metadata_json
                        preview.command_line = m.name;
                        preview.risk_summary = "dangerous operation";
                        return ctx.approval_handler->process_request(m, c, preview)
                                   ? agenticdsl::PreHookResult{agenticdsl::PreHookResult::Continue}
                                   : agenticdsl::PreHookResult{agenticdsl::PreHookResult::Deny,
                                                               {}, "human_denied"};
                    },
                    hook_cfg["priority"], policy);
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
        // 配置示例: {"agents": [{"name": "privacy-policy-v1",
        //           "config": {"instance_id": "privacy-main"},   // 完整 AgentConfig (Oracle M3)
        //           "scope": "react-loop/*"}]}
        for (const auto& agent_cfg : config["agents"]) {
            // 真实 AgentConfig 仅含 instance_id (Oracle M3): 从 agent_cfg["config"] 解析,
            // instance_id 可选, 空时 create() 自动生成 UUID-like 实例 ID
            agenticdsl::AgentConfig cfg;
            if (agent_cfg.contains("config")) {
                cfg.instance_id = agent_cfg["config"].value("instance_id", "");
            }
            // 真实 API (Oracle H1): 仅 register_agent + create, 无 resolve()/list()
            // 若类型未注册, 注册宿主 factory (重复注册返回 false, 不静默覆盖)
            if (!ctx.agent_registry->is_registered(agent_cfg["name"])) {
                ctx.agent_registry->register_agent(
                    agent_cfg["name"], [](const agenticdsl::AgentConfig& c) {
                        return std::make_unique<CrossCuttingAgent>(c);  // 宿主横切功能 Agent
                    });
            }
            // 未注册的 string_id → create() 返回 nullptr (真实 API, Oracle H1)
            auto agent = ctx.agent_registry->create(agent_cfg["name"], cfg);
            if (!agent) continue;  // FailOpen: 创建失败时跳过, 不阻断主流程

            // 注入为 agent pre-hook (AgentPreHook 签名: (const IAgent&, const std::string&))
            // Oracle H4: 删除 v1.0 static_cast 手动 hook; 通过 IAgent 最小接口 (name/id) 引用
            agenticdsl::AgentPreHook hook =
                [agent_id = agent->id()](const agenticdsl::IAgent& a,
                                         const std::string& input) {
                    agenticdsl::AgentPreHookResult r;
                    r.modified_context["guarded_agent"] = agent_id;
                    r.action = agenticdsl::AgentPreHookResult::ModifyContext;
                    return r;
                };
            ctx.agent_hook_registry->register_pre_hook(
                agent_cfg["scope"], hook, /*priority=*/100,
                agenticdsl::HookErrorPolicy::FailClosed);
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
            // subscribe 返回 size_t token, 可用于 unsubscribe
            ctx.bus->subscribe(topic, [ctx, config](const agenticdsl::BusEvent& e) {
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
    // Oracle B1: Orchestrator 不触碰既有 engine.h —— set_llm_provider 回调由用户在构造时
    // 绑定到 DSLEngine::set_llm_provider (真实 API, src/core/engine.h:130)。
    // Oracle M7: patterns 可选注入, 默认注册 4 个内置 pattern (向后兼容)。
    CrossCuttingOrchestrator(
        agenticdsl::IAgentRegistry& agent_reg,
        agenticdsl::IAgentHookRegistry& agent_hook_reg,
        agenticdsl::IToolHookRegistry& tool_hook_reg,
        agenticdsl::IInteractionBus& bus,
        agenticdsl::IApprovalHandler* approval_handler = nullptr,              // L4 通道 (Oracle M2)
        std::function<void(std::unique_ptr<agenticdsl::ILLMProvider>)>
            set_llm_provider = nullptr,                                        // L0 通道 (Oracle B1)
        std::vector<std::unique_ptr<ICrossCuttingPattern>> patterns = {})      // Oracle M7
        : set_llm_provider_(std::move(set_llm_provider)) {
        ctx_.agent_registry = &agent_reg;
        ctx_.agent_hook_registry = &agent_hook_reg;
        ctx_.tool_hook_registry = &tool_hook_reg;
        ctx_.bus = &bus;
        ctx_.approval_handler = approval_handler;
        if (patterns.empty()) {
            // 默认注册 4 个内置 pattern (向后兼容)
            register_pattern(std::make_unique<DecoratorPattern>());
            register_pattern(std::make_unique<HookPattern>());
            register_pattern(std::make_unique<CompositionPattern>());
            register_pattern(std::make_unique<BusPattern>());
        } else {
            for (auto& p : patterns) register_pattern(std::move(p));
        }
    }
    
    // 主入口: 类比 LoopDispatcher::dispatch(loop_type)
    void dispatch(const nlohmann::json& cross_cutting_config) {
        // 将 L0 provider 注入回调注入 ctx (不触碰 engine.h, 由用户构造时绑定)
        ctx_.set_llm_provider = set_llm_provider_;
        
        for (const auto& pattern_cfg : cross_cutting_config["patterns"]) {
            std::string type = pattern_cfg["type"];
            auto it = patterns_.find(type);
            if (it == patterns_.end()) {
                // Oracle M1: 未知 pattern 默认 FailOpen (记 warning + 跳过), 不阻断主流程;
                // throw 仅限 schema 非法 (cross_cutting_config 结构错误, 如 YAML 解析失败)。
                // 不变量 4 (强化): Orchestrator dispatch 失败不影响主流程。
                std::cerr << "[cross-cutting] WARN: unknown pattern: " << type << std::endl;
                continue;
            }
            try {
                it->second->apply(pattern_cfg["config"], ctx_);  // 各范式独立 apply
            } catch (const std::exception& e) {
                // Oracle M1: pattern apply 抛异常也按 FailOpen 处理 —— 记 warning + 跳过,
                // 避免单个 pattern 故障阻断后续 pattern / 主流程
                std::cerr << "[cross-cutting] WARN: pattern " << type
                          << " apply failed (fail-open): " << e.what() << std::endl;
            }
        }
    }
    
    // 注册自定义 pattern (扩展点)
    void register_pattern(std::unique_ptr<ICrossCuttingPattern> pattern) {
        patterns_[pattern->name()] = std::move(pattern);
    }
    
private:
    CrossCuttingContext ctx_;
    std::unordered_map<std::string, std::unique_ptr<ICrossCuttingPattern>> patterns_;
    std::function<void(std::unique_ptr<agenticdsl::ILLMProvider>)> set_llm_provider_;  // L0 通道 (Oracle B1)
};

}  // namespace hydraforge::pdk
```

**关键设计点**：
- **无状态**：Orchestrator 只持有基础设施引用 + pattern 集合，**不存储业务状态**
- **运行时分发**：基于 JSON 配置动态选择 pattern（vs LoopDispatcher 的编译期模板特化）
- **扩展点**：`register_pattern()` 方法允许注册自定义 pattern（V2 扩展）

### 4.5 横切功能 DSL（类比 Agent DSL）

**位置**: `examples/cross_cutting/dsl/*.cc.md`（v1.2 实施）

`examples/cross_cutting/dsl/high_security_mode.cc.md`（横切功能 DSL - 高安全模式，与 `*.agent.md` 同构，每个 `### AgenticDSL` 段一个独立 YAML 代码块）:

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
      decorators: ["CostTracking", "Compliance", "PII-Scrub"]  # 链深 ≤4 含 inner (Oracle B2 移除 Retry)
      
  # Pattern 2: Tool + Agent hooks
  - type: hook-v1
    config:
      hooks:
        - target: approval          # Oracle M2: L4 审批通道 (approval_handler->process_request)
          glob: "L3_*"
          type: pre
          priority: 1000
          policy: FailClosed
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
          config:
            instance_id: "privacy-main"   # 完整 AgentConfig (Oracle M3): instance_id 可选, 空则 create() 自动生成
          scope: "react-loop/*"
        - name: metrics-collector-v1
          config:
            instance_id: "metrics-main"
          scope: "*"
          
  # Pattern 4: Event Bus 订阅
  - type: bus-v1
    config:
      subscriptions: ["mutation.committed", "gepa.commit.committed"]
      handler: external-siem-adapter-v1
```

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

// 初始化 Orchestrator (Oracle M2: approval_handler L4 通道; Oracle B1: set_llm_provider L0 通道)
CrossCuttingOrchestrator orch(agent_registry, agent_hook_registry,
                                tool_hook_registry, bus,
                                /*approval_handler=*/&approval,
                                [&engine](auto p) { engine.set_llm_provider(std::move(p)); });

// 应用横切配置 (一次 dispatch 应用所有 patterns)
orch.dispatch(config.to_json());
```

> **Oracle M6 — DSL schema 校验**: V1 实施阶段定义 `cross_cutting_schema.json`，
> 复用 ADR-0073 nlohmann JSON Schema 校验器（`tool_schema_validator.h`，JSON Schema 2020-12
> 最小子集：type/properties/required/items/enum）对 `/cross_cutting` 段的 `patterns[].type` /
> `config` 字段做结构校验。否则 YAML `type` 字段拼写错误运行时才暴露
> （FailOpen warning），难以在加载期发现。

### 4.6 自管理 Meta-Agent（可选高级特性）

**位置**: `include/agenticdsl/pdk/cross_cutting/cross_cutting_meta_agent.h`（v1.2 实施）

> **设计争议**：v1.0 提出 `CrossCuttingMetaAgent` 作为自管理入口。v1.2 修正为**可选高级特性**，因为：
> 1. **SRP 违反**：单点决策所有范式 = god class
> 2. **违反 Loop 设计哲学**：PDK Loop 没有"MetaLoop"集中决策
> 3. **使用场景窄**：仅"自适应系统"场景需要，多数应用用 Config/Compile-time 即可
>
> **替代方案**：Orchestrator 是必需的核心组件；Meta-Agent 是可选包装层。

```cpp
namespace hydraforge::pdk {

// 可选: Meta-Agent 自管理（基于 Orchestrator 之上）
class CrossCuttingMetaAgent : public agenticdsl::IAgent {
public:
    CrossCuttingMetaAgent(const agenticdsl::AgentConfig& cfg,
                          CrossCuttingOrchestrator& orch,
                          ConfigRegistry& config_registry)
        : cfg_(cfg), orch_(orch), config_registry_(config_registry) {}
    
    std::string name() const override {
        static const std::string n = "cross-cutting-meta-v1";
        return n;
    }
    std::string id() const override { return cfg_.instance_id; }
    
    // Meta-Agent 自主决定启用哪些横切功能
    agenticdsl::AgentResult<std::string> self_configure(const std::string& goal) {
        // 1. 查询预定义 goal 配置 (类比 pdk_chat_demo preset)
        auto config = config_registry_.get_goal_config(goal);
        if (!config) {
            return {"", {}, agenticdsl::ErrorCode::NotFound, "Unknown goal: " + goal};
        }
        
        // 2. 调用 Orchestrator 应用配置
        orch_.dispatch(config->to_json());
        
        return {"configured_for_" + goal, {}, std::nullopt, ""};
    }
    
    // 注册为 agent（可选）
    void register_to(agenticdsl::IAgentRegistry& registry) {
        registry.register_agent(name(), [this](const agenticdsl::AgentConfig& cfg) {
            return std::make_unique<CrossCuttingMetaAgent>(cfg, orch_, config_registry_);
        });
    }
    
private:
    agenticdsl::AgentConfig cfg_;
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
- 单环境部署 → 用 Config 驱动（见 §4.7 策略 2）
- 固定配置 → 用 Compile-time 注入（见 §4.7 策略 1）
- 动态启用/禁用单个功能 → 用 Hot-Reload Manager（见 §4.7 策略 3）

### 4.7 4 种管理策略（v1.2 保留，实施路径对齐 §4.4 Orchestrator 构造）

> **v1.2 说明（Oracle 评审）**：本节的"策略"是**使用方式**（如何触发 Orchestrator），
> 不是 v1.0 已废弃的"4 种 Agent 模式"（Side-effect/Policy/Orchestrator/Adapter）。
> 全部策略均通过 `CrossCuttingOrchestrator` 统一入口（H4：案例一律 `orch.dispatch()`）。

#### 策略 1: Compile-time 注入（直接调用 Orchestrator）

```cpp
int main() {
    auto registry = agenticdsl::make_in_memory_agent_registry();
    auto agent_hooks = agenticdsl::make_in_memory_agent_hook_registry();
    agenticdsl::ToolHookRegistry tool_hooks;
    agenticdsl::InMemoryBus bus;
    
    // 初始化 Orchestrator (Oracle M7: patterns 可选注入, 默认注册 4 个内置 pattern)
    CrossCuttingOrchestrator orch(*registry, *agent_hooks, tool_hooks, bus);
    
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

## 5.0 与 PDK Loop Agent 的对等映射（v1.2 关键洞察）

| PDK Loop Agent（已 ship） | 横切功能管理（v1.2 推荐） |
|---------------------------|---------------------------|
| 3 个独立 Loop class | **4 个独立 Pattern class** |
| `LoopDispatcher<LoopType>` 编译期模板分发 | `CrossCuttingOrchestrator` 运行时 JSON 分发 |
| `AgentLoopType` 枚举 | `cross_cutting_pattern::Decorator/Hook/Composition/Bus` 常量 |
| `LoopResult` 统一返回类型 | `void apply(...)` 统一应用接口 + `ICrossCuttingPattern` 抽象 |
| `loop_type: react_loop` DSL 字段 | `type: decorator-v1` 配置字段（Oracle H3 统一） |
| `examples/pdk_chat_demo/dsl/*.agent.md` | `examples/cross_cutting/dsl/*.cc.md` |
| `DEFINE_AGENT` 宏（agent_macros.h） | （V2 待定，可考虑 `DECLARE_CROSS_CUTTING` 宏） |
| **无 MetaLoop** | **无强制 MetaCrossCutting**（可选 MetaAgent） |

**关键洞察**：PDK Loop 设计从未做"集中决策的 MetaLoop"，而是用 dispatcher 模板 + 独立 Loop class 组合。横切功能管理应遵循完全相同的设计哲学——**Orchestrator 是必需核心**，**MetaAgent 是可选包装**。

### 5.1 与 Semantica 重新对比（v1.2 更新）

| 维度 | Semantica | HydraForge 横切架构 v1.2 |
|------|-----------|-------------------------|
| 横切机制 | 依赖元数据 + 工作流组合 | **6 层抽象 + 4 PDK Pattern + Orchestrator** |
| 拦截粒度 | LLM call level | LLM/Tool/Agent step/Lifecycle 全覆盖 |
| 范式抽象 | 扁平工作流 | **4 范式独立 PDK Pattern + 抽象接口** |
| 扩展性 | 单一集中决策 | **register_pattern() 扩展点 + DSL 声明** |
| Agent 编排 | 单一类型 + 工作流 | `agenticdsl::IAgentRegistry` + `agenticdsl::IAgentComposition` |
| 装饰器链 | 无显式 | `agenticdsl::ILLMProviderDecorator`（GoF Decorator + `decorate_*` 钩子）|
| Hook 策略 | 自定义 | `agenticdsl::HookErrorPolicy` (FailClosed/FailOpen) 统一 |
| 失败处理 | 自定义 | **不变量**：hook 异常不阻断主流程 |
| 事件系统 | 自定义 | ADR-0068 Canonical Topic Registry（27+ 主题）|
| DSL 实例化 | 自定义 | `examples/cross_cutting/dsl/*.cc.md` 类比 Agent DSL |
| schema 校验 | 自定义 | 复用 ADR-0073 nlohmann JSON Schema 校验器（Oracle M6）|

---

## 五、实战案例：给所有 Agent 添加 5 种横切功能（v1.2 更新）

> **v1.2 更新（Oracle H4）**：5 个案例统一通过 `CrossCuttingOrchestrator::dispatch()` 应用
> （删除 v1.0 手动 `registry.create` + `bus.subscribe` + `static_cast` 手动 hook 代码）。
> 每个案例 = 1 份 `*.cc.md` DSL 配置 + 1 次 `orch.dispatch(config.to_json())`，与 §4.5 DSL 实例化一致。
> **未展示的用例依赖**：所有案例假设 `agent_registry` / `agent_hook_registry` / `tool_hooks` / `bus`
> 已在 `main()` 构建，与 §4.7 策略 1 一致。

### 案例 1: 全局 Metrics 收集

**需求**: 收集所有 agent 的 step_count / tool_call_count / token_usage

**实现**（范式 4: Event Bus + BusPattern）:
```yaml
# examples/cross_cutting/dsl/metrics_mode.cc.md
### AgenticDSL `/cross_cutting`
patterns:
  - type: bus-v1
    config:
      subscriptions: ["cognitive.task.*", "tool.execution.*"]
      handler: global-metrics-v1
```
```cpp
// 案例 1 (Oracle H4): 统一走 orch.dispatch —— 依赖在 main() 构建 (§4.7 策略 1)
CrossCuttingConfig config = CrossCuttingConfig::load("metrics_mode.cc.md");
CrossCuttingOrchestrator orch(*registry, *agent_hooks, tool_hooks, bus);
orch.dispatch(config.to_json());   // BusPattern 订阅 cognitive.task.* / tool.execution.*
```

---

### 案例 2: 危险操作二次审批

**需求**: 所有 L3 危险工具调用前需要人类审批

**实现**（范式 2: Registry Hook + L1 拦截点 + L4 审批通道; Oracle M2: `target: approval` 类型）:
```yaml
# examples/cross_cutting/dsl/approval_mode.cc.md
### AgenticDSL `/cross_cutting`
patterns:
  - type: hook-v1
    config:
      hooks:
        - target: approval       # L4 审批通道: 调用 approval_handler->process_request
          glob: "L3_*"
          type: pre
          priority: 1000
          policy: FailClosed
```
```cpp
// L4 审批通道 (Oracle M2): approval_handler 在构造时注入 Orchestrator
CrossCuttingOrchestrator orch(*registry, *agent_hooks, tool_hooks, bus,
                              /*approval_handler=*/&approval);
orch.dispatch(config.to_json());   // HookPattern target:approval 注册 L3_* pre-hook → 审批
```

---

### 案例 3: PII 数据脱敏

**需求**: 给所有 agent 输入自动脱敏 PII

**实现**（范式 2 + L2 拦截点 + HookPattern agent 分支）:
```yaml
# examples/cross_cutting/dsl/pii_mode.cc.md
### AgenticDSL `/cross_cutting`
patterns:
  - type: hook-v1
    config:
      hooks:
        - target: agent
          glob: "*"
          type: pre
          priority: 500
          policy: FailClosed
          handler: privacy-policy-v1
```
```cpp
CrossCuttingOrchestrator orch(*registry, *agent_hooks, tool_hooks, bus);
orch.dispatch(config.to_json());   // HookPattern 注册 agent pre-hook → 自动 scrub PII
```

---

### 案例 4: 自动 Retry + 计费

**需求**: 给所有 LLM 调用添加重试 + token 计费

**实现**（范式 1: Decorator + L0 拦截点; 链深 = 2 decorators + 1 inner = 3 层 ≤ 4; Oracle H4: 统一 `orch.dispatch`）:
```yaml
# examples/cross_cutting/dsl/llm_resilience_mode.cc.md
### AgenticDSL `/cross_cutting`
patterns:
  - type: decorator-v1
    config:
      decorators: ["CostTracking", "Retry"]   # Oracle B2: 链深 ≤4 含 inner (Retry 为自定义 decorator)
```
```cpp
// L0 通道 (Oracle B1): set_llm_provider 回调绑到 DSLEngine::set_llm_provider, Orchestrator 不触碰 engine.h
CrossCuttingOrchestrator orch(*registry, *agent_hooks, tool_hooks, bus,
                              nullptr,
                              [&engine](auto p) { engine.set_llm_provider(std::move(p)); });
orch.dispatch(config.to_json());   // DecoratorPattern 注入 CostTracking + Retry 链
```

---

### 案例 5: 跨子系统审计集成

**需求**: 所有 mutation 事件转发到外部 SIEM

**实现**（范式 4: Event Bus + BusPattern）:
```yaml
# examples/cross_cutting/dsl/siem_mode.cc.md
### AgenticDSL `/cross_cutting`
patterns:
  - type: bus-v1
    config:
      subscriptions: ["mutation.*"]
      handler: external-siem-adapter-v1
```
```cpp
CrossCuttingOrchestrator orch(*registry, *agent_hooks, tool_hooks, bus);
orch.dispatch(config.to_json());   // BusPattern 订阅 mutation.* → 外部 SIEM
```

---

## 六、决策矩阵：何时使用哪种范式

> **v1.2 更新（Oracle H4）**：删除 v1.0 残留的"模式 A/B/C/D"引用（已被 §四 4 范式 + Pattern 取代）。
> "策略 N"为 §4.7 使用方式编号。

| 关注点类型 | 推荐范式 | 推荐层级 | 推荐 Pattern / 使用方式 |
|------------|----------|----------|-----------------|
| **Metrics 收集** | 范式 4 (Event Bus) | L5 | BusPattern |
| **审计日志** | 范式 4 (Event Bus) | L5 | BusPattern |
| **PII 脱敏** | 范式 2 (Hook Registry) | L2 (per-agent) 或 L1 (per-tool) | HookPattern (agent/tool 分支) |
| **危险操作审批** | 范式 2 (Hook Registry) | L1 + L4 | HookPattern (`target: approval`) |
| **Token 计费** | 范式 1 (Decorator) | L0 | DecoratorPattern |
| **Retry** | 范式 1 (Decorator) | L0 | DecoratorPattern |
| **Rate Limit** | 范式 1 或 2 | L0 或 L1 | DecoratorPattern 或 HookPattern |
| **外部系统集成** | 范式 4 (Event Bus) | L5 | BusPattern |
| **自适应反思循环** | 范式 3 (Composition) | L3 | CompositionPattern |
| **Policy 动态切换** | 范式 2 + 策略 3 | L2 | HookPattern + Hot-Reload (§4.7 策略 3) |
| **跨 Agent 工作流** | 范式 3 (Composition) | L3 | CompositionPattern |
| **多环境差异化** | 范式 2 + 策略 2 | L2 | HookPattern + Config 驱动 (§4.7 策略 2) |

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
- 验证: Agent first-class 行为（registry.create / list_registered / is_registered / unregister）

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

## 十一、总结（v1.2 更新）

### v1.0 → v1.1 核心修正（历史）
- **删除** v1.0 §四 "4 Agent 模式"（Side-effect / Policy / Orchestrator / Adapter）
- **新增** v1.1 §四 "4 PDK Pattern + Orchestrator + 可选 Meta-Agent"
- **关键洞察**：横切功能管理应采用与 PDK Loop Agent **完全相同**的设计模式（独立 class + dispatcher 编排）

### v1.2 核心修正（Oracle 评审 H1-H4 + M1-M9）
- **H1**: §三 范式 3 + §四 4.3 CompositionPattern 代码校正为真实 IAgentRegistry / IAgentComposition API（register_agent + create；无 resolve()/list()/std::optional<IAgent>）
- **H2**: 代码样本统一 `agenticdsl::` 命名空间限定（IAgentRegistry / IAgentHookRegistry / IToolHookRegistry / IInteractionBus / ILLMProvider / BusEvent / HookErrorPolicy / IAgent / AgentResult / AgentConfig）
- **H3**: DSL 字段统一 `type:`（grep 验证 0 个旧字段名残留）
- **H4**: 删除 v1.0 残留重复"策略 1-4"段（§4.7 保留为使用方式 + §5.0/5.1 标题规范化）；§五 5 案例统一 `orch.dispatch(config.to_json())`，删除 `static_cast<PrivacyPolicyAgent*>` 手动 hook
- **M1**: Orchestrator 未知 pattern 默认 FailOpen（记 warning + 跳过），throw 仅限 schema 非法
- **M2**: CrossCuttingContext 增加 `approval_handler`（L4 通道）+ HookPattern `target: approval` 类型
- **M3**: CompositionPattern 接受完整 `agent_cfg.config`（AgentConfig 仅含 instance_id）
- **M6**: DSL schema 复用 ADR-0073 nlohmann JSON Schema 校验器（`cross_cutting_schema.json`）
- **M7**: Orchestrator 构造可选 `patterns` 注入（默认注册 4 内置 pattern）
- **M8/M9**: ADR-0069 状态 🟡 Partial + README ADR-0081/0082 状态 ✅ Approved

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

- **ADR-0081** Pre-Step Hook Contract（IAgentHookRegistry, Agent-scoped）— ✅ Approved (2026-08-21)
- **ADR-0082** Agent First-Class Registry（IAgentRegistry）— ✅ Approved (2026-08-21)
- **ADR-0085** Cross-Cutting Pattern PDK（🔍 Proposed → 评审转 Approved）— v1.2 §四 4 Pattern + Orchestrator 设计依据
- **ADR-0069** ToolCoordinator Hook（IToolHookRegistry, HookErrorPolicy）— 🟡 Partial（已 ship 契约）
- **ADR-0068** Event Emission Contract（27+ 主题）— ✅ Approved（BusPattern 依赖）
- **ADR-0021** PDK Design（PDK Plugin 范式）— 横切功能管理作为 PDK 子模式

### 下一步建议

**短期**:
1. ADR-0085 评审转 ✅ Approved（Oracle H1-H4 + M1-M9 修正已完成）
2. 创建 `pdk-cross-cutting-patterns` OpenSpec change 实施 4 Pattern + Orchestrator + DSL

**中期**:
3. 实施示例横切功能（全局 PII 脱敏 + Metrics + Audit 三件套）
4. 集成到 pdk_chat_demo（类比 examples/pdk_chat_demo/dsl/）

**长期**:
5. 横切功能 marketplace（社区贡献新 Pattern）
6. Meta-Agent 自管理（自适应系统场景）