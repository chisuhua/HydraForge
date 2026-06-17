# ADR-0019: IInteractionBus 接口与 TUI Chat MVP 架构

## 状态

**✅ Approved (2026-06-08, 通过 phase-c1-migration)** — V2.1 版，IInteractionBus + InMemoryBus MVP 已实施（commits 5f21ea3, f07a4b4）。后续 P2（DSLEngine bus 集成）移交 Phase 1。

> **2026-06-08 截至 (commit f07a4b4)**：`IInteractionBus` 与 `InMemoryBus` 的公共头文件迁移到 `include/agenticdsl/contract/`，实现文件 (`inmemory_bus.cpp`) 与 `CMakeLists.txt` 保留在 `src/common/contract/`。`events.h` 在 M5.2 简化跳过，Event/Token/Session 类型内联到 `IInteractionBus` 头文件。

## 状态变更日志

### 2026-06-17 — Sprint 1b 完成 + OpenSpec change `2026-06-15-residual-engine-h-decoupling` 启动 (R5 重分类)

**问题 §1.4 (跨模块耦合) 状态变更**：🟡 部分解决 (维持中, 待 OpenSpec change 完成 4 跨模块 include 移除后标记 ✅ 已解决).

**Sprint 1b (commit `248d209`, 2026-06-17) 吸收的工作**：
- 3 deep `modules/` 移除: `topo_scheduler.h` / `markdown_parser.h` / `budget_controller.h` (PIMPL-lite)
- 1 contract 头新增: `agenticdsl/contract/iinteraction_bus.h` (本 ADR P2 集成)

**OpenSpec change `2026-06-15-residual-engine-h-decoupling` 计划完成的工作** (P1 active, 待 T5 执行后实际 ship):
- 1 leaf `modules/trace/trace_exporter.h` 移除: TraceRecord 上移到 `include/agenticdsl/types/trace_record.h`
- 3 `common/` 移除: `mock_provider.h` / `registry.h` 通过 contract 抽象 (IProviderFactory facade over LLMProviderFactory / IToolRegistry 镜像 ADR-0023 §C.3)
- 保留 `common/llm/llm_types.h` (types 头文件例外)
- 2 contract 头新增: `agenticdsl/contract/iprovider_factory.h` / `itool_registry.h`

**当前 (2026-06-17) 状态**:
```bash
$ grep -c '#include "modules/\|#include "common/' src/core/engine.h
4   # Sprint 1b 完成后剩余 4 include: 1 leaf modules/ + 3 common/
```

**Plan 退出标准 `grep = 0`**: 未达成 (因 llm_types.h 保留作为 types 头文件例外, 实际接受 grep = 1)
**OpenSpec change 执行完成时**: T5 验证后将本 ADR §1.4 状态更新为 ✅ 已解决, `grep -c = 1` (仅 llm_types.h types 头文件例外)

### 2026-06-12 — Stage 4 Tasks 16-20：engine.h 跨模块耦合部分解耦

**问题 §1.4 (跨模块耦合) 状态变更**：⛔ 待解决 → 🟡 **部分解决**（3 deep `modules/` 已移除: topo_scheduler.h/markdown_parser.h/budget_controller.h; 剩余 1 leaf `modules/trace/trace_exporter.h` + 3 `common/` (llm_types.h/mock_provider.h/registry.h) 待独立 ADR 解耦）。
> **2026-06-13 审计更正（OpenSpec change `docs-code-drift-audit-2026-06`）**：原 "2/3 modules/ 直接 include 已解决" 描述不准确——3 deep modules/ 实际全部移除;1 leaf modules/trace/ + 3 common/ 残留待 future ADR。Plan 退出标准 `grep -c '#include "modules/\|common/' src/core/engine.h = 0` 当前实际 = 4。

**已完成的解耦动作**（Stage 4 / Tasks 16-20）：

| Task | 文件 | 变更 |
|------|------|------|
| 16 | `include/agenticdsl/contract/ischeduler.h`（新建） | 定义 `IScheduler` 抽象接口 |
| 16 | `include/agenticdsl/contract/iparser.h`（新建） | 定义 `IParser` 抽象接口 |
| 17 | `src/modules/scheduler/topo_scheduler.h` | `class TopoScheduler : public IScheduler` + `override` 关键字 |
| 17 | `src/modules/parser/markdown_parser.h` | `class MarkdownParser : public IParser` + `override` 关键字 |
| 18 | 根 `CMakeLists.txt` | 新增 `agenticdsl::headers` INTERFACE 库，链接 `include/` |
| 20 | `src/modules/executor/node_executor.h` | 持有 `std::unique_ptr<IParser>` 而非嵌入 `MarkdownParser` |

**`src/core/engine.h` 当前 include 状态**（截至 2026-06-12）：

| include | 状态 | 说明 |
|---------|------|------|
| `common/llm/llm_types.h` | 🟡 保留 | ILLMProvider* / ILLMTool / LLMParams 接口定义 |
| `common/llm/mock_provider.h` | 🟡 保留 | 默认 LLM provider 实现 |
| `common/tools/registry.h` | 🟡 保留 | `ToolRegistry` 成员（Task 20 计划处理） |
| `modules/budget/budget_controller.h` | 🟡 保留 | `BudgetController` 成员（需 **PIMPL** 才能完全解耦） |
| `modules/trace/trace_exporter.h` | 🟡 保留 | `TraceRecord` POD 定义（Stage 4+ 待迁移） |
| ~~`modules/scheduler/topo_scheduler.h`~~ | ✅ 移除 | 改用 `agenticdsl/contract/ischeduler.h` |
| ~~`modules/parser/markdown_parser.h`~~ | ✅ 移除 | 改用 `agenticdsl/contract/iparser.h` |
| `agenticdsl/contract/ischeduler.h` | ✅ 新增 | 抽象接口（Task 16） |
| `agenticdsl/contract/iparser.h` | ✅ 新增 | 抽象接口（Task 16） |

**剩余耦合清单（需未来 Stage 处理）**：
- `budget_controller.h`：BudgetController 是值类型成员，需要 PIMPL (`unique_ptr<Impl>`) 才能让 engine.h 不暴露内部实现
- `trace_exporter.h`：TraceRecord 是 POD 类型，可改为 `forward declare` + `agenticdsl::types::TraceRecord` 独立头
- `llm_types.h` / `mock_provider.h` / `registry.h`：需评估是否下沉到 `core/types/` 或 `contract/`

**已知构建环境问题（与本次重构无关）**：
- `external/async_simple/uthread/internal/thread.cc` 单文件编译耗时 > 30s，导致 `cmake --build` 全量超时
- **这不是 Stage 4 重构引入的问题**，是预先存在的环境/工具链问题
- 本次验证采用 `cpp -E` 预处理（轻量、跳过链接与代码生成）作为替代方案
- 全量 `cmake --build` + `ctest` 验证需在外部 CI 或修复 async_simple 后执行

**6 个 examples 审计结果**（2026-06-12, `cpp -E` 预处理）：

| 目录 | 有 .cpp | 预处理结果 | 备注 |
|------|---------|-----------|------|
| `examples/agent_basic/` | ✅ | ✅ PASS | 使用 `core/engine.h`（Task 4 已统一路径） |
| `examples/slice_01_tool_call/` | ✅ | ✅ PASS | 使用 `core/engine.h` + 新增 cognitive header |
| `examples/agent_loop/` | ✅ | ✅ PASS（预处理） | 引用已删除的 `agenticdsl::PromptBuilder`（commit ac9e684），**完整编译会失败** |
| `examples/agent_simple/` | ✅ | ❌ FAIL | 引用 `common/utils.h`（不存在，文件已重组到 `common/utils/*.h`）+ 已删除的 `LlamaAdapter`（commit 2804eac） |
| `examples/skill_porting/` | ❌ 无 .cpp | N/A | 仅含 DSL Markdown 文件与目录骨架 |
| `examples/superpowers/` | ❌ 无 .cpp | N/A | 仅含 `.agent.md` DSL 文件 |

> **注**：`agent_simple` 与 `agent_loop` 在 Stage 1 Task 4 已标记为 DEPRECATED，迁移至 `MockLLMProvider` / `ILLMProvider` 模式的工作属于未来 OpenSpec change 范围（参见 `.omo/plans/project-organization.md` Stage 3 / Task 14）。本次 Task 21 不修复，仅记录状态。

## 背景

### 问题

HydraForge 当前架构存在以下问题：

1. **同步阻塞执行**：`DSLEngine::run()` 是同步阻塞调用，无法实时推送 Token 流
2. **无用户交互接口**：执行时无用户输入通道，无法实现多轮对话
3. **无事件总线**：组件间通过直接调用耦合，无法支持分布式/跨进程通信
4. **紧耦合 (🟡 部分解决 2026-06-17, Sprint 1b 吸收 3/4, OpenSpec change `2026-06-15-residual-engine-h-decoupling` 处理剩余 4 include)**：`engine.h` 跨模块 include 待全部移除（保留 `common/llm/llm_types.h` types 头文件例外, 退出标准 `grep -c = 1`）。Sprint 1b (commit `248d209`) 移除 3 deep `modules/`, OpenSpec change `2026-06-15-residual-engine-h-decoupling` (P1 active, 5 周估时) 处理剩余 1 leaf `modules/trace/` + 3 `common/`。详见下方"状态变更日志"。

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

**重新评估 EventBus 实施的触发条件**（2026-06-13 补充）：

InMemoryBus 当前对 Phase 1 足够（单/多 Agent TUI Chat、< 1K events/s 量级）。当且仅当以下任一触发条件满足时，**才**重新评估 EventBus 实施：

| 触发条件 | 量化阈值 | 验证方式 |
|---|---|---|
| 吞吐瓶颈 | `InMemoryBus::emit()` 实测 P99 延迟 > 5ms | benchmark/ 压测 10K events/s |
| Per-Session 隔离 | 出现"某 session 高频事件阻塞其他 session"反馈 | 多 session 并发压测 |
| 优先级背压 | Critical 事件因 Low 事件堆积而延迟 > 100ms | 混合优先级压测 |
| 多 Agent 协作 | Phase 3 Agent 间通信需求落地 | ADR-0030 重新激活评估 |

**不触发则不实施**：上述条件未满足时，InMemoryBus 路径继续演进，EventBus 维持 "📦 设计历史" 状态。
**触发时替换路径**：`InMemoryBus` 已抽象为 `IInteractionBus` 接口；新 EventBus 实现作为 `IInteractionBus` 另一个实现类（如 `EventBusBackedInteractionBus`），通过 `set_interaction_bus()` 注入，无需修改 DSLEngine 业务逻辑。

### 2. 契约层设计

#### 2.1 契约层位置

```
src/common/contract/
├── CMakeLists.txt
└── inmemory_bus.cpp # MVP 实现

include/agenticdsl/contract/
├── iinteraction_bus.h # 核心接口 (abstract class)
└── inmemory_bus.h # InMemoryBus 声明
```

#### 2.2 Event 类型

EventType 继承 ADR-0002 的分类思路，但简化到应用层所需的最小集：

```cpp
// events.h 在 M5.2 阶段简化跳过；Event/Token/Session 类型直接内联到 IInteractionBus 头文件中
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
// include/agenticdsl/contract/iinteraction_bus.h
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
// include/agenticdsl/contract/inmemory_bus.h
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
    // MVP: 基于回调的异步（Phase 1）
    // Phase 2: 将迁移至 ADR-0030 的协程模型（async_simple::Lazy<T>）
    void run_async(const std::string& session_id,
                   const std::string& user_message);
    
    // 协程版本（ADR-0030 Phase 2 启用）
    // async_simple::coro::Lazy<ExecutionResult> 
    // run_async_coro(const std::string& session_id,
    //                const std::string& user_message);

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
| **Phase 2** | DSLEngine 添加 `set_interaction_bus()` + `run_async()`<br>NodeExecutor Token 流推送<br>~~回调式异步~~ → 对齐 ADR-0030 协程模型 | 基座支持异步 |
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
- [ADR-0030: AsyncRuntime 双层异步架构](../archive/adr/adr-0030-async-runtime-dual-layer.md) — Phase 2 协程实现依赖

---

## 附录 A: 文件变更清单

| 操作 | 文件路径 |
|------|---------|
| **新建** | `src/common/contract/CMakeLists.txt` |
| **跳过** | `src/common/contract/events.h` — M5.2 简化，事件类型内联到 `IInteractionBus` 头文件 |
| **新建** | `include/agenticdsl/contract/iinteraction_bus.h` |
| **新建** | `include/agenticdsl/contract/inmemory_bus.h` |
| **新建** | `src/common/contract/inmemory_bus.cpp` |
| **修改** | `src/core/engine.h` (+ atomic_bus, run_async) |
| **修改** | `src/core/engine.cpp` (+ bus 集成) |
| **修改** | `src/modules/executor/node_executor.cpp` (+ token push) |
| **新建** | `examples/agent_chat/CMakeLists.txt` |
| **新建** | `examples/agent_chat/src/main.cpp` |
| **新建** | `examples/agent_chat/src/chat_client.h/cpp` |
| **新建** | `examples/agent_chat/src/tui.h/cpp` |
