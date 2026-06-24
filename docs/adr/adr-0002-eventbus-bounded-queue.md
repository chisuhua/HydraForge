# ADR-0002: EventBus 有界队列架构

## 状态

**❌ Not Implemented** (2026-06-13) — **V2 版** 设计文档已锁定，但 EventBus / DispatchMode 全系统从未实施；Phase 1 由 IInteractionBus + InMemoryBus（ADR-0019）承担事件通信职责。

> **2026-06-13 审计备注（OpenSpec change `docs-code-drift-audit-2026-06`）**：状态由 "✅ Approved" 调整为 "❌ Not Implemented"。
> - 事实依据：`grep -rn "EventBus\|DispatchMode" src/ include/` → 0 hits；`InMemoryBus` 是简化实现（mutex + queue），无 Taskflow/async_simple 依赖。
> - 决策：保留本 ADR 作为 HydraForge Phase 1 FTXUI/HarnessEngine 子系统的设计蓝图参考，**不修改主体内容**。
> - 未来触发条件：若 Phase 2 出现 >10K events/s 吞吐需求或 Per-Session 严格隔离需求，**重新评估** EventBus 实施；否则 InMemoryBus 路径继续（详见 ADR-0019 §1.1 重新评估触发条件段落）。
> - 重新评估路径：`InMemoryBus` 已抽象为 `IInteractionBus` 接口，未来 EventBus 实现可作为新的 `IInteractionBus` 实现平滑替换，不破坏 API 表面。

## 背景

HydraForge Phase 1 需要 FTXUI 主线程与后台 HarnessEngine 执行线程之间的线程安全通信。HarnessEngine 通过 EventBus 向 FTXUI 推送 LLM token 流、工具执行状态、错误信息等事件。

关键约束：
- LLM 流式输出可达 30+ tokens/秒，不能每个 token 都触发 UI 重绘
- TOOL_START/END 等关键事件不能丢失
- 多 Agent 架构下事件来源隔离（每个 Agent 独立 EventBus）
- Phase 2 需要支持 Agent 间通信

> **V2 变更**：本 ADR 从纯 UI 事件总线扩展为**全系统可观测性总线**，集成 ADR-0030 的 AsyncRuntime 双层异步架构，增加 DispatchMode 分发机制。

---

## 决策

### 1. 总体架构：与 ADR-0030 集成的分层 EventBus

```
┌───────────────────────────────────────────────────────────────┐
│  事件生产者（各层均可发布）                                       │
│  ├── 计算层（Taskflow）: LLMCallFinished, ToolExecuted          │
│  ├── 控制层（async_simple）: SessionStarted, ModeChanged        │
│  └── 用户交互: UserMessage, ApprovalResponse                    │
├───────────────────────────────────────────────────────────────┤
│  EventBus（传输层 — 轻量路由器）                                  │
│  ├── 全局 MPMC 有界队列（混合模式：全局队列 + Session 过滤）      │
│  ├── 事件路由：按类型分发到订阅者                                │
│  └── 背压策略：按优先级丢弃旧事件（Critical 永不丢弃）           │
├───────────────────────────────────────────────────────────────┤
│  事件消费者（DispatchMode 决定执行线程）                          │
│  ├── Inline（同步）→ 极轻量操作（TraceRecorder）                │
│  ├── TaskflowAsync → 计算密集型（CostCollector）                │
│  └── CoroSpawn → IO/长等待（TUI Updater, 用户审批）             │
└───────────────────────────────────────────────────────────────┘
```

**设计原则**：
- **EventBus 是轻量路由器**，不拥有线程，消费者决定在哪个 Executor 上执行
- **全局队列 + Session 过滤**：简化实现，同时保持 Per-Agent 隔离语义
- **DispatchMode**：与 ADR-0030 的双层架构对齐（Taskflow 计算层 / async_simple 控制层）
- Phase 2 可通过 Global EventBus 实现 Agent 间通信

---

### 2. 事件优先级与分类

```cpp
// ============================================================
// 事件类型定义
// ============================================================

enum class EventType {
    // LLM 相关
    LLM_TOKEN,         // 单个 token 输出（高频）
    LLM_END,           // LLM 生成结束
    LLM_ERROR,         // LLM 调用错误

    // 工具相关
    TOOL_START,        // 工具开始执行
    TOOL_END,          // 工具执行结束
    TOOL_ERROR,        // 工具执行错误

    // 系统相关
    SYSTEM_LOG,        // 系统日志（调试用）
    TRACE_EVENT,       // 追踪事件
    DAG_COMPLETE,      // DAG 执行完成
    USER_INPUT,        // 用户输入（取消/确认）
    ERROR,             // 系统级错误
};

enum class EventPriority {
    Critical = 0,   // 永不丢弃：USER_INPUT, DAG_COMPLETE, ERROR
    High = 1,      // 队列满时降级丢弃：TOOL_START, TOOL_END
    Normal = 2,    // 队列满时批量丢弃：LLM_END, TOOL_ERROR
    Low = 3,       // 优先丢弃：LLM_TOKEN, SYSTEM_LOG, TRACE_EVENT
};

// 事件结构
struct UIEvent {
    EventType type;
    EventPriority priority;
    std::string source_node;       // 事件来源节点路径
    std::string source_agent;      // 来源 Agent ID（Phase 2）
    nlohmann::json payload;       // 事件数据
    std::chrono::steady_clock::time_point timestamp;

    // 优先级判定
    static EventPriority priority_of(EventType type) {
        switch (type) {
            case EventType::USER_INPUT:
            case EventType::DAG_COMPLETE:
            case EventType::ERROR:
                return EventPriority::Critical;
            case EventType::TOOL_START:
            case EventType::TOOL_END:
                return EventPriority::High;
            case EventType::LLM_END:
            case EventType::TOOL_ERROR:
            case EventType::LLM_ERROR:
                return EventPriority::Normal;
            default:
                return EventPriority::Low;
        }
    }
};
```

---

### 3. 有界队列与背压策略（V2 更新）

**V2 变更**：
- 从 Per-Agent 多队列改为**全局单队列 + Session 过滤**
- 背压策略从"丢弃新事件"改为"**按优先级丢弃旧事件**"
- 增加 DispatchMode 分发机制（与 ADR-0030 集成）

```cpp
// ============================================================
// V2: 全局 MPMC 有界队列（替代 Per-Agent 多队列）
// ============================================================

template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity) : capacity_(capacity) {}
    
    // 非阻塞入队：按优先级丢弃旧事件腾出空间
    bool try_push(T item, EventPriority priority) {
        std::lock_guard lock(mutex_);
        
        if (queue_.size() >= capacity_) {
            // 按优先级丢弃：丢弃最低优先级的旧事件
            if (!drop_oldest_low_priority(priority)) {
                return false;  // 无法腾出空间（全是 Critical）
            }
        }
        
        queue_.push_back({std::move(item), priority});
        cv_.notify_one();
        return true;
    }
    
    // 阻塞入队（Critical 事件使用）
    void push(T item, EventPriority priority) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] {
            return queue_.size() < capacity_ || 
                   drop_oldest_low_priority(priority);
        });
        queue_.push_back({std::move(item), priority});
    }
    
    // 出队（按优先级：Critical > High > Normal > Low）
    std::optional<T> pop(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [&] { return !queue_.empty(); })) {
            return std::nullopt;
        }
        
        // 找到最高优先级的元素
        auto it = std::max_element(queue_.begin(), queue_.end(),
            [](const auto& a, const auto& b) { return a.priority < b.priority; });
        
        T result = std::move(it->item);
        queue_.erase(it);
        return result;
    }

private:
    bool drop_oldest_low_priority(EventPriority incoming_priority) {
        // 找到最低优先级且低于 incoming 的最旧元素
        auto it = std::find_if(queue_.begin(), queue_.end(),
            [&](const auto& e) { return e.priority < incoming_priority; });
        
        if (it != queue_.end()) {
            queue_.erase(it);
            return true;
        }
        return false;
    }
    
    size_t capacity_;
    std::deque<std::pair<T, EventPriority>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

// ============================================================
// V2: DispatchMode（与 ADR-0030 集成）
// ============================================================

enum class DispatchMode {
    Inline,         // 发布线程内联执行（极轻量操作：TraceRecorder）
    TaskflowAsync,  // 提交到 Taskflow 计算池（CostCollector）
    CoroSpawn       // 作为协程 spawn 到 async_simple（TUI Updater）
};

struct SubscribeOptions {
    DispatchMode mode = DispatchMode::Inline;
    std::string filter_session;  // 可选：只接收特定 session 的事件
    int priority = 0;            // 消费者优先级
};

// ============================================================
// V2: EventBus 接口（抽象层）
// ============================================================

class IEventBus {
public:
    virtual ~IEventBus() = default;
    
    // 发布事件（线程安全，任何线程可调用）
    virtual void emit(Event event) = 0;
    virtual void emit(std::string type, std::string session_id, 
                      nlohmann::json payload = {}) = 0;
    
    // 订阅事件（返回 SubscriptionId 用于取消订阅）
    virtual SubscriptionId subscribe(
        std::string event_type,
        std::function<void(const Event&)> handler,
        SubscribeOptions options = {}) = 0;
    
    virtual void unsubscribe(SubscriptionId id) = 0;
    virtual EventBusMetrics get_metrics() const = 0;
};

// ============================================================
// V2: InMemoryEventBus 实现
// ============================================================

class InMemoryEventBus : public IEventBus {
public:
    struct Config {
        size_t queue_capacity = 4096;
        bool drop_on_full = false;  // false: 阻塞; true: 丢弃
        size_t batch_size = 32;     // 批量分发大小
    };
    
    InMemoryEventBus(Config config, AsyncRuntime& runtime);
    
    void emit(Event event) override;
    
    SubscriptionId subscribe(
        std::string event_type,
        std::function<void(const Event&)> handler,
        SubscribeOptions options = {}) override;
    
    void unsubscribe(SubscriptionId id) override;
    
private:
    void dispatch_to_subscriber(const Event& event, Subscriber& sub);
    void dispatch_loop(std::stop_token stop);
    
    Config config_;
    BoundedQueue<Event> queue_;
    AsyncRuntime& runtime_;
    std::jthread dispatch_thread_;
    
    mutable std::shared_mutex subscribers_mutex_;
    std::unordered_map<std::string, std::vector<Subscriber>> subscribers_;
    std::atomic<uint64_t> next_subscription_id_{0};
    EventBusMetrics metrics_;
};

// 分发逻辑：根据 DispatchMode 选择 Executor
void InMemoryEventBus::dispatch_to_subscriber(
    const Event& event, Subscriber& sub) 
{
    switch (sub.options.mode) {
        case DispatchMode::Inline:
            sub.handler(event);  // 同步执行
            break;
            
        case DispatchMode::TaskflowAsync:
            runtime_.executor().silent_async(
                [handler = sub.handler, event]() { handler(event); });
            break;
            
        case DispatchMode::CoroSpawn:
            runtime_.spawn(
                [handler = sub.handler, event]() 
                -> async_simple::coro::Lazy<void> {
                    handler(event);
                    co_return;
                }());
            break;
    }
}
        std::lock_guard lock(mutex_);
        enqueue(std::move(event));
    }

    bool try_push(UIEvent event) {
        std::lock_guard lock(mutex_);
        return enqueue(std::move(event));
    }

    // -------------------- 消费者接口（UI 线程）--------------------

    bool try_pop(UIEvent& out) {
        std::lock_guard lock(mutex_);
        return dequeue(out);
    }

    std::vector<UIEvent> drain_all() {
        std::lock_guard lock(mutex_);
        std::vector<UIEvent> result;
        UIEvent ev;
        while (dequeue(ev)) {
            result.push_back(std::move(ev));
        }
        return result;
    }

    // -------------------- 订阅机制 --------------------

    using Handler = std::function<void(const UIEvent&)>;
    void subscribe(EventType type, Handler handler) {
        std::lock_guard lock(mutex_);
        handlers_[type].push_back(std::move(handler));
    }

    // -------------------- 统计与调试 --------------------
    size_t size() const { return queues_.critical.size() + queues_.normal.size() + queues_.low.size(); }
    bool empty() const { return size() == 0; }

private:
    Config config_;
    mutable std::mutex mutex_;
    PriorityQueues queues_;

    // 订阅处理器
    std::unordered_map<EventType, std::vector<Handler>> handlers_;

    // 入队逻辑（已持有锁）
    bool enqueue(UIEvent event) {
        auto priority = UIEvent::priority_of(event.type);

        switch (priority) {
        case EventPriority::Critical:
            queues_.critical.push(std::move(event));
            break;

        case EventPriority::High:
            if (queues_.normal.size() >= config_.normal_capacity) {
                queues_.normal.pop_front();  // 丢弃最旧的 High 事件
            }
            queues_.normal.push_back(std::move(event));
            break;

        case EventPriority::Normal:
            if (queues_.normal.size() >= config_.normal_capacity) {
                // 批量丢弃：移除一半
                for (size_t i = 0; i < config_.normal_capacity / 2; ++i) {
                    queues_.normal.pop_front();
                }
            }
            queues_.normal.push_back(std::move(event));
            break;

        case EventPriority::Low:
            if (event.type == EventType::LLM_TOKEN) {
                // LLM_TOKEN 特殊处理：保留到 token buffer
                enqueue_token(std::move(event));
            } else {
                if (queues_.low.size() >= config_.low_capacity) {
                    queues_.low.pop_front();
                }
                queues_.low.push_back(std::move(event));
            }
            break;
        }

        return true;  // 始终成功（背压通过丢弃实现）
    }

    void enqueue_token(UIEvent event) {
        auto& buf = queues_.recent_tokens;
        auto token = event.payload["token"].get<std::string>();

        if (buf.size() >= config_.token_buffer_size) {
            buf.pop_front();
        }
        buf.push_back(token);

        // 同时更新 payload 中的 token
        event.payload["token"] = token;
        if (queues_.low.size() >= config_.low_capacity) {
            queues_.low.pop_front();
        }
        queues_.low.push_back(std::move(event));
    }

    // 出队逻辑（已持有锁，Priority: Critical > High > Normal > Low）
    bool dequeue(UIEvent& out) {
        if (!queues_.critical.empty()) {
            out = std::move(queues_.critical.front());
            queues_.critical.pop();
            return true;
        }
        if (!queues_.normal.empty()) {
            out = std::move(queues_.normal.front());
            queues_.normal.pop_front();
            return true;
        }
        if (!queues_.low.empty()) {
            out = std::move(queues_.low.front());
            queues_.low.pop_front();
            return true;
        }
        return false;
    }
};
```

---

### 4. V2 新增：事件类型目录（与 ADR-0030 对齐）

```cpp
// ============================================================
// 事件类型目录（编译期 constexpr 字符串）
// ============================================================

namespace EventTypes {
    // LLM 相关
    namespace LLM {
        constexpr auto CallStarted    = "llm.call.started";
        constexpr auto CallFinished   = "llm.call.finished";
        constexpr auto TokenGenerated = "llm.token.generated";
    }
    
    // 工具相关
    namespace Tool {
        constexpr auto CallStarted   = "tool.call.started";
        constexpr auto CallFinished  = "tool.call.finished";
        constexpr auto ApprovalReq   = "tool.approval.requested";
        constexpr auto ApprovalResp  = "tool.approval.response";
    }
    
    // 成本相关（ADR-0032 CostCollector）
    namespace Cost {
        constexpr auto Updated        = "cost.updated";
        constexpr auto BudgetExceeded = "cost.budget.exceeded";
    }
    
    // Session 相关
    namespace Session {
        constexpr auto Started    = "session.started";
        constexpr auto Ended      = "session.ended";
        constexpr auto ModeChanged = "session.mode.changed";
    }
    
    // DAG 执行
    namespace Execution {
        constexpr auto NodeExecuted    = "execution.node.executed";
        constexpr auto GraphCompleted  = "execution.graph.completed";
        constexpr auto ForkStarted     = "execution.fork.started";
        constexpr auto JoinCompleted   = "execution.join.completed";
    }
    
    // 用户交互
    namespace User {
        constexpr auto Message    = "user.message";
        constexpr auto Interrupt  = "user.interrupt";
    }
}

// 事件 Payload 规范（JSON Schema）
// LLMCallFinished
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

// CostUpdated（由 CostCollector 发布）
{
    "session_id": "sess_abc123",
    "delta_cost_usd": 0.0023,
    "total_cost_usd": 0.0451,
    "cached_savings_usd": 0.0180,
    "model": "deepseek-v4-pro"
}
```

---

### 4. 30Hz 节流与事件合并

```cpp
// ============================================================
// UI 线程主循环（TUI 层）
// ============================================================

class TUIThread {
    std::shared_ptr<EventBus> event_bus_;
    ftxui::ScreenInteractive screen_;

public:
    void run() {
        auto last_redraw = std::chrono::steady_clock::now();

        while (running_) {
            // 1. 拉取所有可用事件（非阻塞）
            auto events = event_bus_->drain_all();

            // 2. 事件合并：多个 LLM_TOKEN 合并为最后一个
            auto consolidated = consolidate_events(std::move(events));

            // 3. 更新 UI 状态
            for (auto& ev : consolidated) {
                dispatch_to_panes(ev);
            }

            // 4. 节流重绘：每 33ms (30Hz) 最多一次
            auto now = std::chrono::steady_clock::now();
            if (now - last_redraw >= 33ms && !consolidated.empty()) {
                screen_.PostEvent(ftxui::Event::Custom);
                last_redraw = now;
            }

            std::this_thread::sleep_for(16ms);  // 60Hz 检查频率
        }
    }

private:
    // 事件合并：保留最后一个 LLM_TOKEN（模拟打字机效果）
    std::vector<UIEvent> consolidate_events(std::vector<UIEvent> events) {
        std::vector<UIEvent> result;
        std::optional<std::string> last_token;
        std::optional<UIEvent> last_token_event;

        for (auto& ev : events) {
            if (ev.type == EventType::LLM_TOKEN) {
                // 保留最后一个 token 及其事件
                last_token = ev.payload["token"].get<std::string>();
                last_token_event = std::move(ev);
            } else {
                result.push_back(std::move(ev));
            }
        }

        // 添加最后一个 token（不添加中间的，避免打字机效果太慢）
        if (last_token_event) {
            last_token_event->payload["token"] = *last_token;
            result.push_back(std::move(*last_token_event));
        }

        return result;
    }

    void dispatch_to_panes(const UIEvent& ev) {
        // 分发到对应面板...
    }
};
```

---

### 5. 多 Agent 架构

```cpp
// ============================================================
// Agent 定义（Phase 1 & 2）
// ============================================================

class Agent {
public:
    Agent(std::string id, std::shared_ptr<EventBus> bus)
        : id_(std::move(id)), event_bus_(std::move(bus)) {}

    const std::string& id() const { return id_; }
    EventBus* event_bus() const { return event_bus_.get(); }

private:
    std::string id_;
    std::shared_ptr<EventBus> event_bus_;
    DSLEngine engine_;
    std::jthread executor_;
};

// ============================================================
// TUI 多订阅
// ============================================================

class HarnessTUI {
    std::vector<std::shared_ptr<EventBus>> agent_buses_;

public:
    void add_agent(std::shared_ptr<Agent> agent) {
        agent_buses_.push_back(agent->event_bus());

        // 订阅所有事件
        agent->event_bus()->subscribe(EventType::LLM_TOKEN, [&](const UIEvent& ev) {
            on_llm_token(ev);
        });
        agent->event_bus()->subscribe(EventType::TOOL_END, [&](const UIEvent& ev) {
            on_tool_end(ev);
        });
        // ...
    }

private:
    void on_llm_token(const UIEvent& ev) {
        // 更新 thought pane...
    }
    void on_tool_end(const UIEvent& ev) {
        // 更新 tool pane...
    }
};
```

---

## 权衡

### 为什么不是单一全局 EventBus？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **Per-Agent EventBus** | 隔离清晰，某个 Agent flood 不影响其他 | TUI 需要多路订阅 |
| **全局 EventBus + source_agent** | TUI 单一订阅 | 事件洪泛可能阻塞所有 Agent，需要在入队时带 agent_id |

**选择 Per-Agent 的理由**：
- 多 Agent 隔离是一等需求（ADR-3）
- 每个 Agent 独立配置背压策略
- Phase 2 Agent 间通信走 Global EventBus，不影响本地事件处理

### 为什么 LLM_TOKEN 可以丢弃？

- 用户感知的是"快速打字"效果，每秒 30 个 token vs 每秒 10 个 token 用户感受不到差异
- 保留最近 10 个 token 用于"正在输入..."提示
- Critical 事件（TOOL_END）永不被 LLM_TOKEN 阻塞

### 为什么 30Hz 而非 60Hz？

- 人眼对 30fps 以上变化不敏感
- 减少 FTXUI 的 PostEvent 开销
- token 产生速度（30+/秒）远超视觉感知，合并后效果一样

---

## 配置参数

```cpp
// llm_config.json 扩展
{
    "eventbus": {
        "normal_capacity": 200,
        "low_capacity": 50,
        "token_buffer_size": 10,
        "drain_interval_ms": 33
    }
}
```

---

## 实现要求

### Phase 1 必须完成

| # | 任务 | 验证方式 |
|---|------|---------|
| 1 | EventBus 核心实现 | 单元测试：多线程 push/pop 无 data race |
| 2 | 优先级队列 | 测试：Critical 永不丢弃，Low 优先丢弃 |
| 3 | 30Hz 节流循环 | 验证：1000 tokens/秒下 UI 不卡顿 |
| 4 | Per-Agent EventBus | 两个 Agent 并发，TUI 正确显示 |

### TSAN 验证

```bash
# 并发测试
TSAN_OPTIONS=halt_on_error=1 ./build/harness_cli test.agent.md
# 检查：多线程 push/pop 无 race
```

---

## 影响范围（V2 更新）

| 组件 | 变更 |
|------|------|
| `src/common/event/event_bus.h/cpp` | 新增 IEventBus + InMemoryEventBus |
| `src/common/event/event_types.h` | 事件类型目录（命名空间组织） |
| `src/common/event/bounded_queue.h` | MPMC 有界队列模板 |
| `src/core/engine.h/cpp` | 集成 EventBus 发布（LLM/Tool 事件） |
| `src/modules/executor/node_executor.cpp` | 发布 NodeExecuted 事件 |
| `src/modules/trace/trace_exporter.cpp` | 改为 EventBus 消费者（Inline 模式） |
| `examples/agent_chat/` | TUI 订阅事件（CoroSpawn 模式） |

---

## V2 变更记录

| 版本 | 日期 | 变更 |
|------|------|------|
| V1 | 2026-05-12 | 初始版本：Per-Agent 多队列 EventBus，面向 UI |
| V2 | 2026-05-27 | 基于 ADR-0030 扩展为全系统可观测性总线：全局队列 + DispatchMode |

### V2 主要变更

1. **架构扩展**：从纯 UI 事件总线扩展为全系统可观测性总线
2. **队列模型**：Per-Agent 多队列 → 全局 MPMC 有界队列 + Session 过滤
3. **背压策略**：丢弃新事件 → 按优先级丢弃旧事件
4. **DispatchMode**：新增 Inline / TaskflowAsync / CoroSpawn 三种分发模式
5. **事件类型**：增加 Cost.* 命名空间（ADR-0032 CostCollector）
6. **集成**：明确与 ADR-0030 AsyncRuntime 的集成方式

---

## 替代方案

### 替代 1：无界队列 + 回调（被否决）

```cpp
void generate_stream(TokenCallback cb);  // 每次 token 调用 cb
```

**否决理由**：高吞吐量时内存无限增长，无法背压，无法取消。

### 替代 2：Lock-free SPSC 队列（被否决）

**否决理由**：实现复杂，C++20 无标准 lock-free queue 实现。`std::mutex` + `std::deque` 在 30Hz 场景下足够高效。

### 替代 3：Actor 模型（被否决）

每个事件是一个 Actor 消息。

**否决理由**：过度工程，Phase 1 不需要。

---

## 扩展能力（不破坏当前设计）

| 扩展需求 | 实现方式 |
|---------|---------|
| Agent 间通信 | Global EventBus（Phase 2） |
| 事件持久化 | 订阅 TRACE_EVENT 到日志系统 |
| 性能监控 | 内置队列大小统计 + 日志输出 |
| 自适应节流 | 根据队列积压动态调整 drain_interval |

---

## 结论

采用与 ADR-0030 集成的分层 EventBus 架构：

- **传输层**：全局 MPMC 有界队列 + 独立分发线程
- **分发层**：DispatchMode（Inline / TaskflowAsync / CoroSpawn）与 ADR-0030 双层架构对齐
- **背压策略**：按优先级丢弃旧事件（Critical 永不丢弃）
- **Session 隔离**：全局队列 + Per-Session 订阅过滤
- **事件类型**：编译期 constexpr 字符串，命名空间组织

此设计支持：
- **Phase 1**：单/多 Agent 流式 TUI，无 UI 卡顿
- **Phase 2**：CostCollector 成本追踪（ADR-0032）
- **Phase 3**：Agent 间通信扩展
- **长期**：配置化参数，运行时可调

---

*文档版本: v2.0*
*最后更新: 2026-05-27*