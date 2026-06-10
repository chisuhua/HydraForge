# ADR-0032: CostCollector 成本收集与预算控制
> ⛔ **已废弃 (2026-06-09)** — 代码侧 0 命中,仅作设计历史保留。详见 OpenSpec change `tech-debt-and-doc-cleanup`
## 状态

**❌ 未实施** (2026-05-27, 2026-06-09 标注废弃)

代码侧无 `CostCollector` 类,`BudgetController` 也无 cost 字段。详见 OpenSpec change `tech-debt-and-doc-cleanup`。

## 领域

基座 / 可观测性 / 成本管理

## 关联

- ADR-0002（EventBus）— 事件传输层
- ADR-0030（AsyncRuntime）— 计算层分发
- ADR-0029（成本监控）— 从编程助手层降为基座层

---

## 背景

### 当前代码库状态

| 组件 | 现状 | 评估 |
|------|------|------|
| **BudgetController** | ✅ 已实现——计数限制（节点数、LLM 调用次数、执行时间） | 仅计数，无成本计算 |
| **ExecutionBudget** | ✅ 原子计数器（`std::atomic<int>`） | 线程安全，但无 USD 维度 |
| **成本追踪** | ❌ **零实现**——无价格、无 Token 计数、无模型定价 | 需要新增 |
| **EventBus** | ❌ **零实现**——ADR-0002 仅有设计文档 | 依赖 ADR-0002 V2 实现 |

### 问题

1. **无成本可见性**：用户不知道每次 LLM 调用花了多少钱
2. **无缓存优化追踪**：DeepSeek 前缀缓存节省了多少成本？
3. **BudgetController 仅计数**：`max_llm_calls=10` 不反映实际成本（GPT-4 vs GPT-3.5 价格差 10 倍）
4. **与 ADR-0030 的关系**：成本收集应在 Taskflow 计算池中执行（计算密集型）

---

## 决策

### 1. 职责边界：CostCollector vs BudgetController

| 维度 | BudgetController（现有） | CostCollector（新增） |
|------|------------------------|---------------------|
| **职责** | **计数限制**（节点数、调用次数、执行时间） | **成本追踪**（USD、Token 数、缓存节省） |
| **触发动作** | 超限 → **终止执行**（硬限制） | 超预算 → **告警 + 模式切换建议**（软限制） |
| **数据类型** | `int`（计数） | `double`（USD） |
| **线程安全** | `std::atomic<int>` | `std::shared_mutex` + 累积计算 |
| **事件来源** | 直接调用（`try_consume_llm_call()`） | 订阅 EventBus `LLMCallFinished` |

**关系**：两者互补
- BudgetController 保护系统不被滥用（硬限制）
- CostCollector 帮助用户控制成本（软限制 + 可视化）

### 2. 架构定位

```
┌───────────────────────────────────────────────────────────────┐
│  事件生产者                                                    │
│  └── LLM 调用完成 → EventBus.emit(LLMCallFinished)            │
├───────────────────────────────────────────────────────────────┤
│  EventBus（ADR-0002）                                          │
│  └── 路由 LLMCallFinished 到订阅者                             │
├───────────────────────────────────────────────────────────────┤
│  CostCollector（消费者，TaskflowAsync 模式）                    │
│  ├── 订阅 LLMCallFinished 事件                                 │
│  ├── 计算成本（定价表 × Token 数 × 缓存折扣）                  │
│  ├── 更新 Session 累积成本                                     │
│  └── 发布 CostUpdated 事件（供 TUI 订阅）                      │
├───────────────────────────────────────────────────────────────┤
│  TUI / 日志 / 告警系统                                         │
│  └── 订阅 CostUpdated 事件，实时显示成本                       │
└───────────────────────────────────────────────────────────────┘
```

**关键决策**：CostCollector 是 **EventBus 消费者**，不是独立线程。通过 `DispatchMode::TaskflowAsync` 在 Taskflow 计算池中执行。

### 3. 定价模型

```cpp
// ===== src/common/cost/pricing.h =====

struct ModelPricing {
    std::string model_name;
    double input_price_per_1k;      // $/1K input tokens
    double output_price_per_1k;     // $/1K output tokens
    double cached_input_discount;   // 缓存折扣（如 0.1 = 10% 价格）
    
    // 计算单次调用成本
    double calculate_cost(size_t input_tokens, 
                          size_t output_tokens,
                          bool cache_hit) const {
        double input_cost = (input_tokens / 1000.0) * input_price_per_1k;
        double output_cost = (output_tokens / 1000.0) * output_price_per_1k;
        
        if (cache_hit) {
            input_cost *= cached_input_discount;
        }
        
        return input_cost + output_cost;
    }
};

// 定价表（从 llm_pricing.json 加载）
class PricingTable {
public:
    void load_from_json(const nlohmann::json& config);
    std::optional<ModelPricing> get_pricing(const std::string& model) const;
    
private:
    std::unordered_map<std::string, ModelPricing> pricing_map_;
};
```

**定价配置文件**（`llm_pricing.json`）：

```json
{
    "pricing": [
        {
            "model": "deepseek-v4-pro",
            "input_price_per_1k": 0.014,
            "output_price_per_1k": 0.028,
            "cached_input_discount": 0.1
        },
        {
            "model": "deepseek-v4-flash",
            "input_price_per_1k": 0.002,
            "output_price_per_1k": 0.006,
            "cached_input_discount": 0.1
        }
    ],
    "budget": {
        "default_session_budget_usd": 1.0,
        "warning_threshold": 0.8
    }
}
```

### 4. CostCollector 核心设计

```cpp
// ===== src/common/cost/cost_collector.h =====

struct SessionCost {
    double total_cost_usd = 0.0;
    double cached_savings_usd = 0.0;
    size_t total_calls = 0;
    size_t cached_calls = 0;
    size_t total_input_tokens = 0;
    size_t total_output_tokens = 0;
    std::unordered_map<std::string, double> cost_by_model;
    
    nlohmann::json to_json() const {
        return {
            {"total_cost_usd", total_cost_usd},
            {"cached_savings_usd", cached_savings_usd},
            {"total_calls", total_calls},
            {"cached_calls", cached_calls},
            {"total_input_tokens", total_input_tokens},
            {"total_output_tokens", total_output_tokens},
            {"cost_by_model", cost_by_model}
        };
    }
};

class CostCollector {
public:
    CostCollector(IEventBus& bus, PricingTable pricing);
    ~CostCollector();
    
    // 查询接口（线程安全）
    SessionCost get_session_cost(const std::string& session_id) const;
    double get_total_cost() const;
    
    // 预算检查（与 BudgetController 互补）
    bool is_over_budget(const std::string& session_id, 
                        double budget_usd) const;
    double get_budget_usage_ratio(const std::string& session_id,
                                   double budget_usd) const;
    
    // 获取所有 Session 成本（用于全局统计）
    std::unordered_map<std::string, SessionCost> get_all_costs() const;

private:
    void on_llm_call_finished(const Event& event);
    void publish_cost_updated(const std::string& session_id,
                               double delta_cost,
                               double savings);
    
    IEventBus& bus_;
    SubscriptionId subscription_id_;
    PricingTable pricing_table_;
    
    mutable std::shared_mutex costs_mutex_;
    std::unordered_map<std::string, SessionCost> session_costs_;
};
```

**实现**（TaskflowAsync 模式）：

```cpp
CostCollector::CostCollector(IEventBus& bus, PricingTable pricing)
    : bus_(bus), pricing_table_(std::move(pricing)) 
{
    // 订阅 LLMCallFinished 事件（在 Taskflow 计算池中执行）
    subscription_id_ = bus_.subscribe(
        EventTypes::LLM::CallFinished,
        [this](const Event& e) { on_llm_call_finished(e); },
        {
            .mode = SubscribeOptions::DispatchMode::TaskflowAsync,
            .priority = 1  // 高优先级消费者
        }
    );
}

void CostCollector::on_llm_call_finished(const Event& event) {
    auto& payload = event.payload;
    
    std::string model = payload["model"];
    size_t input_tokens = payload["prompt_tokens"];
    size_t output_tokens = payload["completion_tokens"];
    bool cache_hit = payload.value("cache_hit", false);
    
    // 查找定价
    auto pricing_opt = pricing_table_.get_pricing(model);
    if (!pricing_opt) {
        // 未知模型：记录警告，跳过成本计算
        std::cerr << "[CostCollector] Unknown model: " << model << std::endl;
        return;
    }
    
    auto& pricing = *pricing_opt;
    
    // 计算成本
    double total_cost = pricing.calculate_cost(input_tokens, output_tokens, false);
    double actual_cost = pricing.calculate_cost(input_tokens, output_tokens, cache_hit);
    double savings = total_cost - actual_cost;
    
    // 更新累积
    {
        std::unique_lock lock(costs_mutex_);
        auto& session = session_costs_[event.session_id];
        session.total_cost_usd += actual_cost;
        session.cached_savings_usd += savings;
        session.total_calls++;
        if (cache_hit) session.cached_calls++;
        session.total_input_tokens += input_tokens;
        session.total_output_tokens += output_tokens;
        session.cost_by_model[model] += actual_cost;
    }
    
    // 发布 CostUpdated 事件（供 TUI 订阅）
    publish_cost_updated(event.session_id, actual_cost, savings);
}

void CostCollector::publish_cost_updated(const std::string& session_id,
                                          double delta_cost,
                                          double savings) 
{
    auto session = get_session_cost(session_id);
    
    bus_.emit(EventTypes::Cost::Updated, session_id, {
        {"delta_cost_usd", delta_cost},
        {"total_cost_usd", session.total_cost_usd},
        {"cached_savings_usd", savings},
        {"total_savings_usd", session.cached_savings_usd},
        {"budget_usage_ratio", get_budget_usage_ratio(session_id, 1.0)}
    });
}
```

### 5. 与 BudgetController 的集成

```cpp
// ===== src/core/engine.cpp（集成示例）=====

ExecutionResult DSLEngine::run(const Context& context) {
    // ... 现有逻辑 ...
    
    // 检查 BudgetController（硬限制）
    if (budget_controller_.exceeded()) {
        return {false, "Budget exceeded: " + budget_controller_.get_budget()->to_string(), 
                context, std::nullopt};
    }
    
    // 检查 CostCollector（软限制 + 告警）
    if (cost_collector_ && 
        cost_collector_->is_over_budget(session_id, session_budget_usd_)) {
        // 发布告警事件（不终止执行）
        event_bus_->emit(EventTypes::Cost::BudgetExceeded, session_id, {
            {"total_cost_usd", cost_collector_->get_session_cost(session_id).total_cost_usd},
            {"budget_usd", session_budget_usd_},
            {"suggestion", "Switch to cheaper model or enable cache"}
        });
    }
    
    // ... 继续执行 ...
}
```

### 6. 事件 Payload 规范

> **C1 迁移注记 (2026-06-08, commit 33096f0)**：C1 之前 LLM 错误/完成事件由适配器通过 EventBus 推送式发布（`LLMCallFinished`）。C1 后改为 `IGenerationStream::error()` 拉取式传递（详见 ADR-0001），不再走 EventBus 推送路径。

**LLMCallFinished**（C1 后错误通过 `IGenerationStream::error()` 拉取式传递，commit 33096f0，而非 EventBus 推送式发布）：

```json
{
    "model": "deepseek-v4-pro",
    "prompt_tokens": 1500,
    "completion_tokens": 320,
    "total_tokens": 1820,
    "duration_ms": 2340,
    "cache_hit": true,
    "cache_discount": 0.9,
    "session_id": "sess_abc123",
    "trace_id": "trace_xyz"
}
```

**CostUpdated**（由 CostCollector 发布）：

```json
{
    "session_id": "sess_abc123",
    "delta_cost_usd": 0.0023,
    "total_cost_usd": 0.0451,
    "cached_savings_usd": 0.0180,
    "total_savings_usd": 0.1560,
    "budget_usage_ratio": 0.0451,
    "model": "deepseek-v4-pro"
}
```

**BudgetExceeded**（由 CostCollector 发布）：

```json
{
    "session_id": "sess_abc123",
    "total_cost_usd": 1.23,
    "budget_usd": 1.0,
    "overage_ratio": 0.23,
    "suggestion": "Switch to deepseek-v4-flash for 7x cost reduction"
}
```

---

## 实施计划

### Phase 1：核心实现

| # | 文件 | 操作 | 说明 |
|---|------|------|------|
| 1 | `src/common/cost/CMakeLists.txt` | 新建 | 静态库 `agenticdsl_cost` |
| 2 | `src/common/cost/pricing.h/cpp` | 新建 | `ModelPricing` + `PricingTable` |
| 3 | `src/common/cost/cost_collector.h/cpp` | 新建 | `CostCollector` + `SessionCost` |
| 4 | `src/common/cost/llm_pricing.json` | 新建 | 默认定价配置 |
| 5 | `tests/test_cost_collector.cpp` | 新建 | 单元测试 |

### Phase 2：集成

| # | 文件 | 操作 | 说明 |
|---|------|------|------|
| 6 | `src/common/llm/llama_adapter.cpp` | 修改 | LLM 调用完成后发布 `LLMCallFinished` 事件 |
| 7 | `src/core/engine.cpp` | 修改 | 创建 CostCollector，集成预算检查 |
| 8 | `src/core/engine.h` | 修改 | 添加 `CostCollector` 成员 |
| 9 | `CMakeLists.txt`（根目录） | 修改 | 添加 `add_subdirectory(common/cost)` |

### Phase 3：TUI 集成（依赖 ADR-0019）

| # | 文件 | 操作 | 说明 |
|---|------|------|------|
| 10 | `examples/agent_chat/src/tui.cpp` | 修改 | 订阅 `CostUpdated` 事件，显示实时成本 |

---

## 验证标准

| 标准 | 验证方法 |
|------|---------|
| 成本计算准确 | 测试：已知 Token 数 × 定价 = 预期成本 |
| 缓存折扣正确 | 测试：`cache_hit=true` 时成本 = 原价 × 折扣 |
| 线程安全 | ThreadSanitizer：多 Session 并发更新无 data race |
| 事件发布 | 验证：`LLMCallFinished` → `CostUpdated` 事件链正确 |
| 预算检查 | 验证：超预算时发布 `BudgetExceeded` 事件 |

---

## 风险与缓解

| 风险 | 严重度 | 缓解 |
|------|--------|------|
| 未知模型定价 | 中 | 记录警告，跳过成本计算（不中断执行） |
| 定价配置过期 | 低 | 支持运行时重新加载 `llm_pricing.json` |
| 累积精度丢失 | 低 | 使用 `double`，定期重置（每 Session） |
| 与 BudgetController 职责混淆 | 中 | 文档明确：BudgetController 硬限制，CostCollector 软限制 |

---

## 参考

- [ADR-0002: EventBus 有界队列架构](./adr-0002-eventbus-bounded-queue.md) — 事件传输层
- [ADR-0030: AsyncRuntime 双层异步架构](./adr-0030-async-runtime-dual-layer.md) — TaskflowAsync 分发模式
- [ADR-0029: 成本监控](./adr-0029-cost-monitoring.md) — 从编程助手层迁移至基座层

---

*文档版本: v1.0*
*最后更新: 2026-05-27*