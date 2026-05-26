# ADR-0019: IInteractionBus 接口与 TUI Chat MVP 架构

## 状态

**提议中** (2026-05-25) — **V2 版**，基于 Oracle 审查讨论更新

## 背景

### 问题

HydraForge 当前架构存在以下问题：

1. **同步阻塞执行**：`DSLEngine::run()` 是同步阻塞调用，无法实时推送 Token 流
2. **无用户交互接口**：执行时无用户输入通道，无法实现多轮对话
3. **无事件总线**：组件间通过直接调用耦合，无法支持分布式/跨进程通信
4. **紧耦合**：`engine.h` 直接 `#include "modules/scheduler/topo_scheduler.h"`，模块边界模糊

### 目标

为 HydraForge 构建一个**多领域智能体协作架构的骨架**，支持：

- **Thin Client**：TUI Chat 作为极简展示终端
- **Thick Runtime**：HydraForge 基座承载所有智能体能力
- **领域扩展**：通过 `domain::tool` 前缀支持 code/browser/fs 等领域

---

## 决策

### 1. 与现有 EventBus（ADR-0002）的关系

#### 1.1 分层架构：IInteractionBus 位于 EventBus 之上

```
┌──────────────────────────────────────────────────────┐
│  IInteractionBus (应用层协议)                          │
│  Session 管理 / Token 流 / 多轮对话                    │
│  session_id ↔ subscribe_tokens ↔ push_token          │
├──────────────────────────────────────────────────────┤
│  EventBus (传输层基础设施) — ADR-0002                  │
│  有界队列 / 优先级 / 节流合并 / Per-Agent 隔离          │
│  LLM_TOKEN / TOOL_START / ERROR 事件路由              │
└──────────────────────────────────────────────────────┘
```

**职责划分**：

| 层次 | 职责 | 不负责 |
|------|------|--------|
| **IInteractionBus** | Session 管理、Token 回调分发、多轮对话、领域发现 | 队列管理、优先级、节流、线程安全 |
| **EventBus** | 有界队列、事件优先级、30Hz 节流合并、Per-Agent 隔离 | Session 语义、应用层协议 |

**MVP 简化**：
- MVP 阶段 EventBus 未实现（ADR-0002 仅有设计文档）
- `InMemoryBus` 直接实现 IInteractionBus，用 `std::mutex` 替代 EventBus 队列
- 未来 EventBus 实现后，`InMemoryBus` 可重构为底层使用 EventBus

### 2. 契约层设计

#### 2.1 契约层位置

```
src/common/contract/
├── CMakeLists.txt
├── iinteraction_bus.h       # 核心接口 (abstract class)
├── events.h                 # Event/Token 结构
└── inmemory_bus.cpp/h       # MVP 实现
```

#### 2.2 Event 类型

EventType 继承 ADR-0002 的分类思路，但简化到应用层所需的最小集：

```cpp
// src/common/contract/events.h
namespace agenticdsl {

// 应用层事件类型
enum class EventType : uint8_t {
    UserMessage = 0,       // 用户输入
    AssistantToken,        // LLM 流式 Token
    AssistantComplete,     // LLM 输出完成
    ToolCall,              // 工具调用开始
    ToolResult,            // 工具调用结果
    Error,                 // 错误
    Status                 // 状态更新
};

// 传输层事件结构（未来对接 EventBus 时扩展）
struct Event {
    EventType type;
    std::string session_id;
    std::string content;           // 文本内容
    nlohmann::json metadata;       // 附加元数据
    int64_t timestamp_ms;

    Event(EventType t, std::string sid, std::string content = {})
        : type(t), session_id(std::move(sid)), content(std::move(content)),
          metadata(nlohmann::json::object()), timestamp_ms(0) {}
};

// 流式 Token
struct Token {
    std::string content;
    bool is_final;                // 最后一个 Token
    std::string speaker;          // "user" | "assistant"
};

// 会话
struct Session {
    std::string id;
    std::vector<nlohmann::json> messages;  // 对话历史
    nlohmann::json context;                // DSL Context
    bool is_complete = false;
};

} // namespace agenticdsl
```

#### 2.3 IInteractionBus 抽象接口

```cpp
// src/common/contract/iinteraction_bus.h
namespace agenticdsl {

using TokenCallback = std::function<void(const Token&)>;
using EventCallback = std::function<void(const Event&)>;

class IInteractionBus {
public:
    virtual ~IInteractionBus() = default;

    // 应用层接口
    virtual std::string send_user_message(const std::string& message) = 0;
    virtual void subscribe_tokens(const std::string& session_id,
                                  TokenCallback callback) = 0;
    virtual void subscribe_events(const std::string& session_id,
                                  EventCallback callback) = 0;
    virtual std::vector<std::string> list_sessions() const = 0;
    virtual Session get_session(const std::string& session_id) const = 0;
    virtual void close_session(const std::string& session_id) = 0;

    // 引擎层接口（供 DSLEngine / NodeExecutor 调用）
    virtual void push_token(const std::string& session_id,
                            const Token& token) = 0;
    virtual void push_event(const std::string& session_id,
                            const Event& event) = 0;
};

} // namespace agenticdsl
```

### 3. InMemoryBus 实现 (MVP)

```cpp
// src/common/contract/inmemory_bus.h
namespace agenticdsl {

class InMemoryBus : public IInteractionBus {
public:
    InMemoryBus() = default;

    // IInteractionBus 接口实现
    std::string send_user_message(const std::string& message) override;
    void subscribe_tokens(const std::string& session_id,
                          TokenCallback callback) override;
    void subscribe_events(const std::string& session_id,
                          EventCallback callback) override;
    std::vector<std::string> list_sessions() const override;
    Session get_session(const std::string& session_id) const override;
    void close_session(const std::string& session_id) override;

    // 引擎层调用
    void push_token(const std::string& session_id,
                    const Token& token) override;
    void push_event(const std::string& session_id,
                    const Event& event) override;

private:
    struct SessionData {
        Session session;
        std::vector<TokenCallback> token_callbacks;
        std::vector<EventCallback> event_callbacks;
    };

    SessionData& get_or_create_session(const std::string& session_id);
    std::string generate_session_id();

    // MVP 使用 std::mutex 保护会话表
    // Phase 2 可替换为 EventBus（ADR-0002）的有界队列
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SessionData> sessions_;
    std::atomic<int64_t> session_counter_{0};
};

} // namespace agenticdsl
```

### 4. DSLEngine 增强

#### 4.1 线程安全的 InteractionBus 引用

```cpp
// src/core/engine.h 新增
class DSLEngine {
public:
    // 注入 InteractionBus
    // 使用 std::atomic<shared_ptr> 确保跨线程安全
    // ADR-0020: bus_ 可能被主线程设置，被 Worker 线程的 NodeExecutor 读取
    void set_interaction_bus(std::shared_ptr<IInteractionBus> bus) {
        bus_.store(bus, std::memory_order_release);
    }

    std::shared_ptr<IInteractionBus> get_bus() const {
        return bus_.load(std::memory_order_acquire);
    }

    // 异步运行：由 Worker 线程调用
    // 每个 DSLEngine 实例只属于一个 Worker（ADR-0003 per-agent 隔离）
    void run_async(const std::string& session_id,
                   const std::string& user_message);

    // 获取会话上下文
    Context get_session_context(const std::string& session_id);

private:
    // 原子指针 — 跨线程安全
    std::atomic<std::shared_ptr<IInteractionBus>> bus_{nullptr};

    // Per-session 上下文
    std::unordered_map<std::string, Context> session_contexts_;
    std::mutex context_mutex_;  // 保护 session_contexts_
};
```

### 5. TUI Chat 应用

#### 5.1 定位

TUI Chat 是 HarnessEngine（ADR-0006）的**职能继承者**：

- ADR-0006 定义的理论线程模型 → 由 ADR-0020 替代
- ADR-0006 定义的 HarnessEngine（后台+FTXUI 通信）→ 由 TUI Chat 实现
- **ADR-0006 将标记为"被替代"**

#### 5.2 目录结构

```
examples/agent_chat/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # 入口
│   ├── chat_client.h/cpp     # HydraForgeClient 封装
│   ├── tui.h/cpp              # FTXUI 渲染
│   └── domain_registry.h/cpp  # 领域工具注册
└── README.md
```

#### 5.3 FTXUI TUI (修正后的合法 C++20)

```cpp
// examples/agent_chat/src/tui.h
namespace agenticdsl {

struct Message {
    enum class Role { User, Assistant, System };
    Role role;
    std::string content;
    bool is_streaming = false;
};

class TUI {
public:
    explicit TUI(ChatClient& client);
    void run();

private:
    std::vector<ftxui::Element> build_message_elements() const;
    void handle_input(const std::string& input);

    ChatClient& client_;
    std::string input_buffer_;
    bool running_ = true;
};

} // namespace agenticdsl
```

```cpp
// examples/agent_chat/src/tui.cpp
namespace agenticdsl {

std::vector<ftxui::Element> TUI::build_message_elements() const {
    using namespace ftxui;
    std::vector<Element> elements;

    for (const auto& msg : client_.get_messages()) {
        auto prefix = msg.role == Message::Role::User ? "> " : "";
        auto color = msg.role == Message::Role::User
                         ? Color::Blue
                         : Color::Green;

        // is_streaming: 追加光标指示器
        auto content = msg.is_streaming ? msg.content + "▊" : msg.content;
        elements.push_back(text(prefix + content) | color);
    }

    return elements;
}

void TUI::run() {
    using namespace ftxui;
    auto screen = ScreenInteractive::TerminalOutput();

    auto renderer = Renderer([this] {
        // 先构建消息元素，再组合到 vbox
        auto msg_elements = build_message_elements();

        return vbox({
            // 标题
            text("=== HydraForge Agent Chat ===") | bold | center,

            // 消息列表 + 分隔线
            vbox(std::move(msg_elements)) | flex | border,

            // 输入区
            hbox({
                text("> ") | Color::Yellow,
                input(&input_buffer_, "Type message...") | flex,
            }) | border,

            // 状态栏
            text("Press Enter to send | Ctrl+C to quit"),
        });
    });

    renderer |= CatchEvent([this](Event event) {
        if (event == Event::Character('\n') && !input_buffer_.empty()) {
            handle_input(input_buffer_);
            input_buffer_.clear();
            return true;
        }
        if (event == Event::CtrlC) {
            running_ = false;
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(renderer);
}

} // namespace agenticdsl
```

---

## 替代方案

### 与 EventBus 合并（被否决）

直接将 IInteractionBus 功能并入 ADR-0002 的 EventBus。

**否决理由**：
- EventBus 是低层传输（有界队列、优先级、节流）
- IInteractionBus 是高层协议（Session、Token 流）
- 职责不同，合并不合理

### 不使用接口抽象（被否决）

DSLEngine 直接包含 InMemoryBus。

**否决理由**：
- 无法替换为 WebSocket/HTTP 传输（Phase 2 需求）
- 无法独立测试
- 不利于扩展多 Agent 架构

---

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| **与 EventBus 关系** | IInteractionBus 在上，EventBus 在下 | 职责分离 |
| **事件类型** | 简化版（应用层），未来对接 ADR-0002 | MVP 最小化 |
| **跨线程安全** | `std::atomic<shared_ptr>` | C++20 原生支持 |
| **FTXUI 代码** | 修正为合法 C++20 | 编译器可验证 |
| **HarnessEngine** | 被 TUI Chat 替代 | ADR-0006 归档 |

---

## 实施计划

| Phase | 任务 | 产出 |
|-------|------|------|
| **Phase 1** | 创建 `src/common/contract/` 模块<br>实现 `IInteractionBus` + `InMemoryBus` | 契约层完成 |
| **Phase 2** | DSLEngine 添加 `set_interaction_bus()` + `run_async()`<br>NodeExecutor Token 流推送 | 基座支持异步 |
| **Phase 3** | 创建 `examples/agent_chat/`<br>实现 ChatClient + FTXUI TUI（修正后代码） | MVP 应用完成 |
| **Phase 4** | 测试端到端 Token 流式 + 多轮对话<br>验证线程安全 | 可运行聊天 |

---

## 验证标准

| 标准 | 验证方法 |
|------|---------|
| 编译通过 | `cmake .. && make -j$(nproc)` 无错误 |
| TUI 可运行 | `./agent_chat` 启动，显示聊天界面 |
| Token 流式显示 | 发送消息，Token 逐字出现 |
| 多轮对话 | 连续发送多条消息，上下文保持 |
| 线程安全 | 主线程 `set_interaction_bus` + Worker 线程 `push_token` 并发无 data race |

---

## 参考

- [ADR-0002: EventBus 有界队列](./adr-0002-eventbus-bounded-queue.md)
- [ADR-0003: DSLEngine 线程安全](./adr-0003-dslengine-thread-safety.md)
- [ADR-0006: HarnessEngine 后台线程模型（已替代）](./adr-0006-harness-engine-thread-model.md)
- [ADR-0020: 多智能体线程模型与隔离策略](./adr-0020-thread-model-isolation.md)

---

## 附录 A: 文件变更清单

| 操作 | 文件路径 |
|------|---------|
| **新建** | `src/common/contract/CMakeLists.txt` |
| **新建** | `src/common/contract/events.h` |
| **新建** | `src/common/contract/iinteraction_bus.h` |
| **新建** | `src/common/contract/inmemory_bus.h` |
| **新建** | `src/common/contract/inmemory_bus.cpp` |
| **修改** | `src/core/engine.h` (+ atomic_bus, run_async) |
| **修改** | `src/core/engine.cpp` (+ bus 集成) |
| **修改** | `src/modules/executor/node_executor.cpp` (+ token push) |
| **新建** | `examples/agent_chat/CMakeLists.txt` |
| **新建** | `examples/agent_chat/src/main.cpp` |
| **新建** | `examples/agent_chat/src/chat_client.h/cpp` |
| **新建** | `examples/agent_chat/src/tui.h/cpp` |
