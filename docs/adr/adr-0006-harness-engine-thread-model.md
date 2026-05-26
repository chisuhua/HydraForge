# ADR-0006: HarnessEngine 后台线程模型

## 状态

**已替代** (2026-05-25)

> 本 ADR 已被 [ADR-0020: 多智能体线程模型与隔离策略](./adr-0020-thread-model-isolation.md) 替代。
>
> HarnessEngine 的"每 Agent 一线程"模型不再适用。详见 ADR-0020 的 CognitiveWorker + DomainWorkerPool 模型。

原批准日期: 2026-05-12

## 背景

HarnessEngine 是 Phase 1 的核心执行引擎，负责在后台线程运行 DSLEngine，同时通过 EventBus 与 FTXUI 主线程通信。Phase 2 需要支持多 Agent 并发执行。

**参考文档**：
- ADR-1: ILLMProvider 流式接口设计
- ADR-2: EventBus 有界队列架构
- ADR-3: DSLEngine 线程安全与多实例架构
- ADR-5: LLM 后端配置与工厂模式

**设计目标**：
- 后台线程与主线程解耦
- 支持优雅退出（Ctrl+C）
- Phase 2 多 Agent 自然扩展

---

## 决策

### 1. 线程模型：线程池（每 Agent 一线程）

```
┌─────────────────────────────────────────────────────────────┐
│  HarnessEngine                                              │
│                                                              │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ agents_mutex_ (保护 agents_ 映射表)                    │ │
│  └─────────────────────────────────────────────────────────┘ │
│                           │                                  │
│         ┌─────────────────┼─────────────────┐              │
│         ▼                 ▼                 ▼              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │   Agent 1   │  │   Agent 2   │  │   Agent N   │         │
│  │  (planner)  │  │ (executor)  │  │  (critic)  │         │
│  │             │  │             │  │             │         │
│  │ jthread +   │  │ jthread +   │  │ jthread +   │         │
│  │ stop_source │  │ stop_source │  │ stop_source │         │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │
│         │                 │                 │              │
│         └─────────────────┼─────────────────┘              │
│                           ▼                                 │
│                   EventBus (Per-Agent)                      │
└─────────────────────────────────────────────────────────────┘
```

**为什么不用单 Worker 线程？**

| 方案 | 优点 | 缺点 |
|------|------|------|
| 单 Worker | 简单，无线程竞争 | 无法并发多 Agent |
| **线程池（每 Agent 一线程）** | 并发清晰，隔离好，Phase 2 扩展自然 | 线程数 = Agent 数（可控） |
| 协程 | 轻量，上下文切换快 | 复杂度高，调试难 |

**选择线程池的理由**：
- 每个 Agent 独立线程，隔离清晰
- std::jthread 自动管理生命周期
- Phase 2 添加新 Agent 只需 `add_agent()`
- 线程数有限（Agent 数量），不会膨胀

### 2. Agent 生命周期：手动管理

```cpp
// ============================================================
// HarnessEngine 生命周期
// ============================================================

class HarnessEngine {
public:
    // 初始化（Phase 1: 单 Agent）
    void initialize(const std::string& config_path) {
        config_ = LLMProviderFactory::load_config(config_path);
        factory_ = std::make_unique<LLMProviderFactory>();
    }

    // 添加 Agent
    std::shared_ptr<Agent> add_agent(
        std::string id,
        std::string backend_name,
        std::shared_ptr<EventBus> event_bus
    ) {
        auto agent = std::make_shared<Agent>(
            id, *factory_, config_, backend_name, event_bus
        );

        std::lock_guard lock(agents_mutex_);
        if (agents_.contains(id)) {
            throw std::invalid_argument("Agent already exists: " + id);
        }
        agents_[id] = agent;
        return agent;
    }

    // 启动所有 Agent
    void start_all() {
        std::lock_guard lock(agents_mutex_);
        for (auto& [id, agent] : agents_) {
            agent->start();
        }
    }

    // 停止所有 Agent（优雅退出）
    void stop_all() {
        // 1. 发送停止请求
        stop_source_.request_stop();

        // 2. 通知 UI 显示"正在取消..."
        broadcast_cancellation_in_progress();

        // 3. 等待所有 Agent 完成（最多 10s）
        for (auto& [id, agent] : agents_) {
            agent->join(std::chrono::seconds(10));
        }
    }

    // 强制终止（超时后调用）
    void force_stop() {
        std::lock_guard lock(agents_mutex_);
        for (auto& [id, agent] : agents_) {
            agent->request_stop();  // 发送 stop_token
            // 不等待，直接标记
        }
        event_bus_->push(UIEvent{
            type = EventType::USER_INPUT,
            payload = {{"intent", "cancelled"}, {"message", "执行已强制取消"}}
        });
    }

    // 检查是否运行中
    bool is_running() const {
        std::lock_guard lock(agents_mutex_);
        for (const auto& [id, agent] : agents_) {
            if (agent->is_running()) return true;
        }
        return false;
    }

private:
    std::mutex agents_mutex_;
    std::map<std::string, std::shared_ptr<Agent>> agents_;
    std::stop_source stop_source_;  // 全局停止源
};
```

### 3. Agent 线程实现

```cpp
// ============================================================
// Agent 线程
// ============================================================

class Agent {
public:
    Agent(
        std::string id,
        LLMProviderFactory& factory,
        const LLMConfig& config,
        const std::string& backend_name,
        std::shared_ptr<EventBus> event_bus
    )
        : id_(std::move(id))
        , event_bus_(std::move(event_bus))
        , llm_(factory.create(backend_name, config))
        , engine_(std::move(llm_))  // DSLEngine 持有 LLM provider
    {
        // 创建独立的 stop_source
        stop_source_ = std::stop_source();
    }

    ~Agent() {
        // 确保线程已结束
        if (worker_.joinable()) {
            worker_.request_stop();
            worker_.join();
        }
    }

    // 加载 DSL
    void load_dsl(const std::string& path) {
        std::lock_guard lock(mutex_);
        engine_.load_graphs(path);
        state_ = State::Loaded;
    }

    // 启动执行
    void start() {
        std::lock_guard lock(mutex_);
        if (state_ != State::Loaded) {
            throw std::runtime_error("Agent must be loaded before start");
        }
        state_ = State::Running;
        worker_ = std::jthread([this](std::stop_token token) {
            run_loop(token);
        });
    }

    // 请求停止（协作式）
    void request_stop() {
        stop_source_.request_stop();
    }

    // 等待线程结束
    void join(std::chrono::seconds timeout) {
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    bool is_running() const {
        return worker_.joinable() && state_ == State::Running;
    }

private:
    enum class State { Empty, Loaded, Running, Stopped };

    std::string id_;
    std::shared_ptr<EventBus> event_bus_;
    std::unique_ptr<ILLMProvider> llm_;
    DSLEngine engine_;

    std::jthread worker_;            // 后台执行线程
    std::stop_source stop_source_;  // 独立停止源
    mutable std::mutex mutex_;
    State state_ = State::Empty;

    // 执行循环
    void run_loop(std::stop_token token) {
        try {
            while (!token.stop_requested()) {
                auto result = engine_.step(token);

                if (result.is_complete()) {
                    event_bus_->push(UIEvent{
                        type = EventType::DAG_COMPLETE,
                        payload = result.final_context()
                    });
                    break;
                }

                if (result.is_error()) {
                    handle_error(result.error());
                }
            }
        } catch (const std::exception& e) {
            handle_error(ErrorInfo{"INTERNAL", e.what()});
        }

        state_ = State::Stopped;
    }

    void handle_error(const ErrorInfo& err) {
        event_bus_->push(UIEvent{
            type = EventType::ERROR,
            payload = {
                {"code", err.code},
                {"message", err.message},
                {"agent", id_}
            }
        });
    }
};
```

### 4. 优雅退出：分离式取消

```cpp
// ============================================================
// 分离式取消流程
// ============================================================

/*
 * 取消流程：
 * 1. 用户 Ctrl+C 或 stop_all()
 * 2. stop_source_.request_stop()
 * 3. EventBus 推送 USER_INPUT (cancellation_in_progress)
 * 4. TUI 显示"正在取消..."
 * 5. 每个 Agent 的 worker 检查 stop_token，请求停止
 * 6. 等待最多 10s
 * 7. 如果超时，force_stop() 强制终止
 * 8. EventBus 推送 USER_INPUT (cancelled)
 */

// ============================================================
// 信号处理
// ============================================================

#include <csignal>

class SignalHandler {
public:
    static void install(HarnessEngine* engine) {
        instance_.engine_ = engine;
        std::signal(SIGINT, handler);
        std::signal(SIGTERM, handler);
    }

private:
    static SignalHandler instance_;
    HarnessEngine* engine_ = nullptr;

    static void handler(int signal) {
        if (!instance_.engine_) return;

        static std::atomic<bool> force_timer_started{false};

        // 第一次 Ctrl+C：优雅退出
        if (!force_timer_started.exchange(true)) {
            instance_.engine_->stop_all();

            // 启动强制终止计时器（10s 后）
            std::thread([=]() {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                if (instance_.engine_->is_running()) {
                    instance_.engine_->force_stop();
                }
                force_timer_started = false;
            }).detach();
        } else {
            // 第二次 Ctrl+C：强制终止
            instance_.engine_->force_stop();
        }
    }
};

// ============================================================
// TUI 中的取消处理
// ============================================================

void HarnessTUI::handle_user_input(const UIEvent& ev) {
    auto intent = ev.payload.value("intent", "");

    if (intent == "cancellation_in_progress") {
        status_pane_.set_message(ev.payload.value("message", "正在取消..."));
        // 显示取消动画
    }
    else if (intent == "cancelled") {
        status_pane_.set_message("执行已取消");
        // 显示最终状态
    }
}
```

### 5. 错误传播：EventBus 统一通道

```cpp
// ============================================================
// 错误类型定义（与 ADR-1 LLMError 统一）
// ============================================================

struct ErrorInfo {
    std::string code;      // "LLM_ERROR", "SECURITY", "INTERNAL", "NETWORK"
    std::string message;
    bool retryable = false;
};

// ============================================================
// 错误处理流程
// ============================================================

void Agent::run_loop(std::stop_token token) {
    try {
        while (!token.stop_requested()) {
            auto result = engine_.step(token);

            if (result.is_complete()) {
                event_bus_->push(UIEvent{
                    type = EventType::DAG_COMPLETE,
                    payload = result.final_context()
                });
                break;
            }

            if (result.is_error()) {
                // 所有错误通过 EventBus 传递
                event_bus_->push(UIEvent{
                    type = EventType::ERROR,
                    priority = EventPriority::High,
                    payload = {
                        {"code", result.error().code},
                        {"message", result.error().message},
                        {"retryable", result.error().retryable},
                        {"agent", id_}
                    }
                });

                // 如果错误可重试，继续执行
                // 如果错误不可重试，停止
                if (!result.error().retryable) {
                    break;
                }
            }
        }
    } catch (const LLMError& e) {
        event_bus_->push(UIEvent{
            type = EventType::LLM_ERROR,
            payload = {
                {"code", to_string(e.code)},
                {"message", e.message},
                {"retryable", e.retryable()},
                {"agent", id_}
            }
        });
    } catch (const SecurityError& e) {
        event_bus_->push(UIEvent{
            type = EventType::ERROR,
            payload = {
                {"code", "SECURITY"},
                {"message", e.message},
                {"tool", e.tool_name},
                {"agent", id_}
            }
        });
    } catch (const std::exception& e) {
        event_bus_->push(UIEvent{
            type = EventType::ERROR,
            payload = {
                {"code", "INTERNAL"},
                {"message", e.what()},
                {"agent", id_}
            }
        });
    }
}

// ============================================================
// TUI 错误展示
// ============================================================

void HarnessTUI::on_error(const UIEvent& ev) {
    auto code = ev.payload["code"].get<std::string>();
    auto message = ev.payload["message"].get<std::string>();
    auto agent = ev.payload.value("agent", "");

    std::string full_message = agent.empty()
        ? message
        : "[" + agent + "] " + message;

    if (code == "SECURITY") {
        status_pane_.set_error("安全错误: " + full_message);
        // 显示安全警告
    } else if (code == "LLM_ERROR") {
        bool retryable = ev.payload.value("retryable", false);
        if (retryable) {
            status_pane_.set_warning("LLM 错误 (自动重试): " + full_message);
        } else {
            status_pane_.set_error("LLM 错误: " + full_message);
        }
    } else {
        status_pane_.set_error("执行错误: " + full_message);
    }
}
```

---

## Phase 2 多 Agent 扩展

### 架构预留

```cpp
// ============================================================
// Phase 2: Agent 依赖调度
// ============================================================

struct AgentConfig {
    std::string id;
    std::string backend;           // 使用的 LLM 后端
    std::vector<std::string> depends_on;  // 依赖的 Agent
    int priority = 0;             // 调度优先级
};

// Phase 2: HarnessEngine 扩展
class HarnessEngine {
    // 调度器（Phase 2 实现）
    std::unique_ptr<AgentScheduler> scheduler_;

    // 依赖解析
    std::vector<std::string> get_execution_order() {
        // 拓扑排序，返回执行顺序
    }

    // 跨 Agent 通信（通过 Global EventBus）
    std::shared_ptr<EventBus> global_bus_;
};
```

### Phase 2 调度策略

| 策略 | 实现 | 适用场景 |
|------|------|---------|
| 平等调度 | 所有 Agent 竞争 CPU | Agent 间无依赖 |
| 优先级调度 | priority 字段排序 | planner > executor > critic |
| 依赖调度 | DAG 拓扑排序 | planner → executor → critic |

---

## 权衡

### 为什么不用单 Worker 线程？

| 方案 | 优点 | 缺点 |
|------|------|------|
| 单 Worker | 实现简单 | 无法并发多 Agent |
| **线程池** | 隔离清晰，扩展自然 | 线程数 = Agent 数 |
| 协程 | 轻量 | 复杂度高，调试难 |

**选择线程池**：多 Agent 是一等需求，线程隔离是最清晰的设计。

### 为什么手动管理生命周期？

CLI 工具的典型模式：
```bash
./harness_cli start   # 启动
./harness_cli status  # 查看状态
./harness_cli stop    # 停止
```

自动管理（任务提交时自动启动）更适合服务器场景，不适合 CLI。

### 为什么 EventBus 统一错误？

- 所有事件（LLM_TOKEN、TOOL_START、ERROR）同一通道
- TUI 只需订阅一个 EventBus
- 错误处理与普通事件处理一致

---

## 实现要求

### Phase 1 必须完成

| # | 任务 | 验证方式 |
|---|------|---------|
| 1 | HarnessEngine 基本结构 | 单元测试：add_agent, start_all, stop_all |
| 2 | Agent 线程管理 | 测试：Agent 正确启动/停止 |
| 3 | stop_token 传播 | 测试：Ctrl+C 触发 stop_all() |
| 4 | 错误 EventBus 传递 | 集成测试：LLM 错误显示在 TUI |
| 5 | 分离式取消 | 测试：10s 超时强制终止 |

### 测试用例

```cpp
TEST_CASE("HarnessEngine starts and stops cleanly") {
    HarnessEngine engine;
    engine.initialize("test_config.yaml");

    auto agent = engine.add_agent("test", "openai", event_bus);
    agent->load_dsl("test.agent.md");

    engine.start_all();
    REQUIRE(agent->is_running());

    engine.stop_all();
    REQUIRE(!agent->is_running());
}

TEST_CASE("Agent reports errors via EventBus") {
    // 模拟 LLM 错误
    mock_llm_->set_next_error(LLMError{
        LLMError::Code::RateLimited,
        "rate limited",
        std::chrono::seconds(1)
    });

    engine.start_all();

    // 验证错误事件
    auto ev = event_bus_->wait_for_event(EventType::ERROR, 5s);
    REQUIRE(ev.has_value());
    CHECK(ev->payload["code"] == "RateLimited");
}
```

---

## 影响范围

| 组件 | 变更 |
|------|------|
| `src/harness/harness_engine.h/cpp` | 新增 HarnessEngine 类 |
| `src/harness/agent.h/cpp` | 新增 Agent 类 |
| `src/harness/event_bus.h/cpp` | 可能需要扩展（Agent 订阅） |
| `src/harness/tui/harness_tui.h/cpp` | 信号处理集成 |
| `examples/harness_cli/main.cpp` | 使用 HarnessEngine |

---

## 替代方案

### 替代 1：单 Worker 线程（被否决）

**否决理由**：无法支持 Phase 2 多 Agent 并发。

### 替代 2：C++20 协程（被否决）

**否决理由**：调试复杂，Phase 1 过早优化。

### 替代 3：Future/Promise 错误传播（被否决）

**否决理由**：结构化但不适合流式错误，与 EventBus 统一通道冲突。

---

## 结论

采用线程池（每 Agent 一线程）+ 手动生命周期管理：

- **线程模型**：每个 Agent 独立 `std::jthread`
- **生命周期**：显式 add/start/stop
- **优雅退出**：分离式取消（stop_token + 10s 超时 + 强制终止）
- **错误传播**：EventBus 统一通道
- **多 Agent**：Phase 2 在此基础上扩展调度器

此设计支持：
- **Phase 1**：单 Agent 流式 TUI
- **Phase 2**：多 Agent 并发 + 依赖调度

---

*文档版本: v1.0*
*最后更新: 2026-05-12*