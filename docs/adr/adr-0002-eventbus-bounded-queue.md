# ADR-0002: EventBus 有界队列架构

## 状态

**已批准** (2026-05-12)

## 背景

HydraForge Phase 1 需要 FTXUI 主线程与后台 HarnessEngine 执行线程之间的线程安全通信。HarnessEngine 通过 EventBus 向 FTXUI 推送 LLM token 流、工具执行状态、错误信息等事件。

关键约束：
- LLM 流式输出可达 30+ tokens/秒，不能每个 token 都触发 UI 重绘
- TOOL_START/END 等关键事件不能丢失
- 多 Agent 架构下事件来源隔离（每个 Agent 独立 EventBus）
- Phase 2 需要支持 Agent 间通信

---

## 决策

### 1. 总体架构：分层 Per-Agent EventBus

```
┌─────────────────────────────────────────────────────────────┐
│  Global EventBus (Phase 2 扩展)                              │
│  - 系统级事件广播 (DAG_COMPLETE, ERROR)                       │
│  - Agent 间通信预留                                          │
└───────────────────────┬─────────────────────────────────────┘
                        │ subscribe()
┌───────────────────────┴─────────────────────────────────────┐
│  Per-Agent EventBus                                          │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ PriorityQueue (有界)                                    ││
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐   ││
│  │  │ Critical Q  │  │ Normal Q    │  │ Low Q       │   ││
│  │  │ (无界/链式) │  │ (200 上限)  │  │ (50 上限)   │   ││
│  │  └─────────────┘  └─────────────┘  └─────────────┘   ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

**设计原则**：
- 每个 Agent 拥有独立的 EventBus，隔离事件源
- 某个 Agent 的事件 flood 不会影响其他 Agent
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

### 3. 有界队列与背压策略

```cpp
// ============================================================
// 优先级分队列（有界）
// ============================================================

struct PriorityQueues {
    // Critical：无限容量，永不丢弃
    std::queue<UIEvent> critical;

    // Normal：容量 200，超限丢弃最旧的
    static constexpr size_t NORMAL_CAPACITY = 200;
    std::deque<UIEvent> normal;

    // Low：容量 50，超限批量丢弃最旧的（保留最新 10 个 token）
    static constexpr size_t LOW_CAPACITY = 50;
    static constexpr size_t TOKEN_BUFFER_SIZE = 10;
    std::deque<UIEvent> low;

    // LLM_TOKEN 特殊缓冲（独立于 Low 队列）
    std::deque<std::string> recent_tokens;  // 保留最近 N 个 token
};

// ============================================================
// EventBus 核心实现
// ============================================================

class EventBus : public std::enable_shared_from_this<EventBus> {
public:
    struct Config {
        size_t normal_capacity = 200;
        size_t low_capacity = 50;
        size_t token_buffer_size = 10;
    };

    explicit EventBus(Config config = {}) : config_(config) {}

    // -------------------- 生产者接口（后台线程）--------------------

    void push(UIEvent event) {
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

## 影响范围

| 组件 | 变更 |
|------|------|
| `src/harness/event_bus.h/cpp` | 新增核心 EventBus 类 |
| `src/harness/harness_engine.h/cpp` | 创建并管理 Agent EventBus |
| `src/harness/tui/harness_tui.h/cpp` | 多路订阅 + 节流循环 |

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

采用分层 Per-Agent EventBus 架构：

- **4 级优先级**：Critical > High > Normal > Low
- **混合背压**：Critical 永不丢弃，Low 优先丢弃，LLM_TOKEN 合并
- **30Hz 节流**：合并多个 token，每帧最多一次重绘
- **Per-Agent 隔离**：每个 Agent 独立 EventBus，TUI 多路订阅
- **Phase 2 预留**：Global EventBus 用于 Agent 间通信

此设计支持：
- **Phase 1**：单/多 Agent 流式 TUI，无 UI 卡顿
- **Phase 2**：Agent 间通信扩展
- **长期**：配置化参数，运行时可调

---

*文档版本: v1.0*
*最后更新: 2026-05-12*