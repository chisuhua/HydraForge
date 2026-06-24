# ADR-0020: 多智能体线程模型与隔离策略

## 状态

**✅ Approved (2026-06-24, Sprint 3 ship + Phase 1 智能体层 100% re-confirm)** — V3.3 版。SimpleCognitiveOrchestrator（MVP 单轮 ReAct）+ CognitiveWorker（Sprint 2 已 ship, 9/9 ctest pass）+ **DomainWorkerPool**（Sprint 3 已 ship, 7/7 ctest pass）全部实施。ADR-0020 §2.2 P1 + P2 全部 ✅ Resolved。2026-06-24 Phase 1 智能体层 100% 收官(per OpenSpec change `tech-debt-and-phase1-closure` Sprint 5 ship) re-confirm Approved 状态。

> **Sprint 3 增量 (2026-06-19, OpenSpec change `2026-06-30-domain-worker-pool`)**：DomainWorkerPool 类落地（`include/agenticdsl/cognitive/domain_worker_pool.h` + `src/modules/cognitive/domain_worker_pool.cpp`）。N 个 std::jthread worker + 共享 FIFO 任务队列 (多消费者模式) + shared_mutex 保护 handler 注册表 + IInteractionBus 集成 (domain.task.* 事件)。CP.22 协议 6/6 项通过 (`.omo/plans/2026-06-30-cp22-audit.md`)。7 个新测试 + 30 基线 = 31/31 ctest pass, 1000x 并发用例 TSan 干净 (Dockerfile.tsan 待 CI 验证)。

> **Sprint 2 增量 (2026-06-18, OpenSpec change `2026-06-23-cognitive-worker`)**：CognitiveWorker 类落地（`include/agenticdsl/cognitive/cognitive_worker.h` + `src/modules/cognitive/cognitive_worker.cpp`）。构造签名遵循本 ADR §3.1 V3.2 修正（unique_ptr<DSLEngine> + shared_ptr<IInteractionBus>）。析构函数 out-of-line 隐式 stop()+join (TD-CW-02 修复)。错误码 bridge 覆盖 SimpleCognitiveOrchestrator 9 处 legacy string 路径 (TD-CW-03)。9 个新测试 + 29 基线 = 30/30 ctest pass。

> **C1 迁移注记 (2026-06-08, commit 3f28020)**：引擎 LLM 注入接口由 `LlamaAdapter*` 改为 `ILLMProvider*`（抽象流式接口，详见 ADR-0001）。原 `LlamaAdapter` 仍可用但需通过 `LlamaAdapterProvider` 包装。本 ADR 中 §2 的成员变量 `llm_provider_` 已同步更新。

## 替代关系

**本 ADR 替代 ADR-0006（HarnessEngine 后台线程模型）**。

ADR-0006 中定义的"每 Agent 一线程"模型不再适用。本 ADR 将智能体分为**认知智能体（编排者）** 和**领域智能体（执行者）**，对应 CognitiveWorker + DomainWorkerPool，而非 HarnessEngine 的平面 Agent 列表。

> **V3 变更**：本 ADR Phase 2/4 的协程计划已迁移至 ADR-0030（AsyncRuntime 双层异步架构）。
> 协程实现统一使用 async_simple 库，不再自研协程基础设施。

> **V3.2 修正 (2026-06-18, OpenSpec change `2026-06-23-cognitive-worker`)**：§3.1 CognitiveWorker 构造签名由 `CognitiveWorker(std::shared_ptr<IInteractionBus> bus)` 修正为 `CognitiveWorker(std::unique_ptr<DSLEngine> engine, std::shared_ptr<IInteractionBus> bus)`。
> - **修正理由**：与 `DSLEngine::from_markdown` / `from_file` 静态工厂（返回 `std::unique_ptr<DSLEngine>`）配套, 测试可直接注入 mock-configured engine, 无需经 markdown 解析。
> - **隔离语义不变**：Worker 仍独占 `engine_` 成员, 与主线程不共享（ADR-0003 per-instance 隔离保留）。
> - **影响范围**：仅 §3.1 代码示例（行 199）, §3.1 文字说明及 §6 锁顺序不变。Sprint 2 实施时 CognitiveWorker 构造签名遵循本修正。

## 背景

### 现有代码库状态

HydraForge 当前实现**完全是单线程同步模型**：

| 组件 | 现状 |
|------|------|
| `DSLEngine::run()` | 同步阻塞，无并发 |
| `TopoScheduler` | 顺序执行 fork/join（模拟并发） |
| `ToolRegistry` | 扁平 namespace，无线程保护 |
| `EventBus` | 仅有设计文档（ADR-0002），无实现 |
| **线程原语** | **零使用**（无 `std::mutex`/`std::atomic`/`std::jthread`） |

### 问题

1. **无法并发**：单线程同步执行，多核 CPU 利用率低
2. **无法流式推送**：阻塞等待 LLM 响应，无法边生成边显示
3. **无隔离**：崩溃即全崩，无沙箱保护
4. **无用户交互**：执行中无法接收用户输入

### 参考文档

| ADR | 内容 | 与本 ADR 关系 |
|-----|------|--------------|
| ADR-0002 | EventBus 有界队列 | 事件通信机制（IInteractionBus 底层） |
| ADR-0003 | DSLEngine 线程安全与多实例 | **核心依赖**：每 Worker 独立 DSLEngine |
| ADR-0004 | ToolRegistry 安全模型 | 权限校验与锁模型交互 |
| ADR-0006 | HarnessEngine 后台线程模型 | **已替代**：被 CognitiveWorker+DomainPool 取代 |
| ADR-0019 | IInteractionBus 接口 | CognitiveWorker 通过此接口通信 |

---

## 决策

### 1. 分层线程模型

#### 1.1 组件分类

| 组件类型 | 线程模型 | 隔离级别 | MVP 支持 |
|---------|---------|---------|---------|
| **基座核心** (DSL/State/Registry) | **主线程** | 无隔离 | ✅ MVP |
| **认知智能体** (编排者) | **独立工作线程** | 线程级隔离 | ✅ MVP |
| **领域智能体** (执行者) | **独立工作线程** | 线程级隔离 | ✅ MVP |
| **LLM Gateway** | **回调** → 协程 | 协程级隔离 | 🔜 Phase 2 |
| **危险工具** (文件/浏览器/系统) | **子进程沙箱** | **进程级隔离** | 🔜 Phase 2 |
| **安全工具** (只读查询) | **工作线程** | 线程级隔离 | ✅ MVP |

#### 1.2 MVP 线程架构

```
HydraForge 进程 (MVP)
├── 主线程 (Main Thread)
│   ├── L0 DSL Engine — 编译/调度/执行 (<5ms, 纯计算)
│   ├── L1 State Store — 内存状态访问
│   ├── L2 Tool Registry — Schema 校验
│   └── Event Loop — 事件分发
│
├── 认知智能体工作线程 (Cognitive Worker) [MVP]
│   ├── 意图理解
│   ├── 任务分解
│   ├── DSL 生成
│   └── 结果聚合
│
├── 领域智能体工作线程池 (Domain Workers) [MVP]
│   ├── human:: — 人类交互 (confirm/clarify)
│   ├── code:: — 编程助手 (future)
│   └── 其他领域 (future)
│
└── 后台线程 (Background Threads)
    └── Memory Compression — TokenJuice 压缩
```

**注意**：MVP 阶段暂不实现：
- 协程（使用 `std::function` 回调替代）
- 子进程沙箱（使用 ADR-0004 权限校验替代）

---

### 2. 主线程：基座核心

#### 2.1 职责边界

```cpp
// 主线程运行组件 — 纯计算，零 IO 等待
class MainThreadComponents {
public:
    // ✅ 主线程可以运行
    void run_dsl_engine() {
        // Kahn 拓扑排序: O(V+E), 无 IO
        // 节点执行: <5ms, 无阻塞
    }

    void run_state_store() {
        // 原子操作读写，无锁
    }

    void run_tool_registry() {
        // Schema 校验, O(1) 哈希查找
    }

    // ❌ 主线程禁止执行
    // - 网络请求 (LLM API)
    // - 文件 IO (磁盘读写)
    // - 子进程等待 (fork/exec)
    // - 长时间计算 (代码分析)
};
```

#### 2.2 主线程安全约束

```cpp
// DSLEngine — 主线程运行，成员通过不可变设计 + 原子访问保护
class DSLEngine {
    // 图谱加载后不可变
    std::shared_ptr<const std::vector<ParsedGraph>> graphs_;

    // 工具注册表 — shared_mutex 保护
    // ADR-0003: 读多写少场景
    std::shared_mutex tool_mutex_;

    // InteractionBus — 跨线程安全（主线程设，Worker 线程用）
    std::atomic<std::shared_ptr<IInteractionBus>> bus_{nullptr};

    // LLM 抽象 — 只在 Worker 线程调用
    // C1 后 (commit 3f28020) 引擎通过 ILLMProvider* 注入 LLM，原 LlamaAdapter 仍可用但需通过 LlamaAdapterProvider 包装。
    ILLMProvider* llm_provider_;
};
```

#### 2.2.1 IProviderFactory 工厂注入 (2026-06-17, OpenSpec change `2026-06-15-residual-engine-h-decoupling`)

**补充关系**: `IProviderFactory` (ADR-0005 V1.1 facade) 与本 ADR §2.2 `ILLMProvider*` 注入模式**互补**:
- `std::unique_ptr<IProviderFactory> provider_factory_` — Per-Engine 持有**工厂**
- `ILLMProvider* llm_provider_` — Per-Engine 持有**实例**(来自 `provider_factory_->create(config)`)
- 工厂内部 `LLMProviderFactory` (ADR-0005 §3) 已是 thread-safe, 多线程并发 `create()` 安全
- Worker 隔离仍由本 ADR §4.1 mutex+queue 控制, **不与 factory 冲突**

**生命周期**:
- DSLEngine 构造时: `provider_factory_ = std::make_unique<LLMProviderFactory>()` (含 MockCreator 默认注册)
- `from_markdown()` 时: `llm_provider_ = provider_factory_->create(LLMConfig{...}).release()`
- Per-Worker `std::unique_ptr<DSLEngine>` 持有独立 factory + provider 指针

**Related**: ADR-0005 V1.1 (IProviderFactory facade), ADR-0019 §1.4 (engine.h 解耦)

#### 2.2.2 CognitiveWorker 集成 (2026-06-18, OpenSpec change `2026-06-23-cognitive-worker`)

> **Sprint 2 实施状态**: ✅ CognitiveWorker 类已 ship（`include/agenticdsl/cognitive/cognitive_worker.h` + `src/modules/cognitive/cognitive_worker.cpp`）。
> §2.2.1 (IProviderFactory 工厂注入) 与本节 CognitiveWorker **互补**: 工厂创建 provider, CognitiveWorker 持独立 DSLEngine 实例。

**CognitiveWorker 实施要点**:
- 构造: `(unique_ptr<DSLEngine>, shared_ptr<IInteractionBus>)` — 遵循本 ADR §3.1 V3.2 修正
- 构造时强制 `engine_->set_interaction_bus(bus_)` (F7 ordering 契约) — engine 后续 dsl.call.* / tool.* 事件通过 Worker bus 转发
- 状态机: `enum class State { idle, running, stopped }` (`std::atomic<State>`) — 公开方法前置条件 entry 处 assert
- 析构: out-of-line 定义, `state_ == running` 时隐式 `stop()` + join thread (TD-CW-02 修复 `std::terminate` 风险)
- 内部实现: 委托 `SimpleCognitiveOrchestrator` (P1.T2 已 ship, 接受 `IToolRegistry*`)
- 错误传播: bridge `error_code_from_string()` 覆盖 SimpleCognitiveOrchestrator 9 处 legacy `meta["error_code"]` 字符串 → `ErrorCode` enum (TD-CW-03)
- 事件: `cognitive.task.started` / `cognitive.task.completed` 通过 `IInteractionBus` 转发

**测试覆盖** (9 case, 33 assertions, 0 回归):
1. 基本启动/停止
2. 任务提交 + 同步结果
3. 优雅停止
4. 错误传播 (含 ErrorCode enum + trace_id 关联)
5. 多线程并发 submit_task (10 线程 × 100 次, TSan 验证)
6. 状态机前置条件
7. LLM error → ErrorCode enum bridge
8. F7 set_interaction_bus 顺序契约
9. 析构函数安全 (TD-CW-02)

**Sprint 3 锁定契约** (CognitiveWorker 不可破坏的对外 API):
- `submit_task(task_id, prompt)` 签名 + 语义
- 事件 topic `cognitive.task.started` / `cognitive.task.completed`
- `ToolResult.trace_id` 携带 task_id 关联

---

### 3. 工作线程：智能体执行

#### 3.1 认知智能体工作线程

**关键设计约束**：
- **每个 CognitiveWorker 拥有独立 DSLEngine 实例**（ADR-0003 per-agent 隔离）
- **不共享 DSLEngine**：Worker 进程内创建自己的 DSLEngine
- 通过 IInteractionBus 与主线程通信
- 任务队列使用 `std::mutex` + `std::queue`（MVP 不需要 LockFreeQueue）

```cpp
// src/common/worker/cognitive_worker.h
#ifndef AGENTICDSL_WORKER_COGNITIVE_H
#define AGENTICDSL_WORKER_COGNITIVE_H

#include <jthread>
#include <stop_token>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

namespace agenticdsl {

struct CognitiveTask {
    std::string session_id;
    std::string user_message;
    std::function<void(const Token&)> on_token;
    std::function<void(const Event&)> on_event;
};

class CognitiveWorker {
public:
    // 每个 Worker 拥有独立的 engine 和 bus
    // [2026-06-18 修正] 构造签名从 (bus) 改为 (engine, bus) 以适配
    // DSLEngine::from_markdown / from_file 工厂模式（返回 unique_ptr<DSLEngine>）。
    // 优点：测试可直接注入 mock-configured engine, 无需经 markdown 解析。
    // per-agent 隔离语义不变：Worker 独占 engine_, 与主线程不共享。
    CognitiveWorker(std::unique_ptr<DSLEngine> engine,
                    std::shared_ptr<IInteractionBus> bus);

    ~CognitiveWorker();

    void start();
    void stop();

    // 提交任务（线程安全）
    void submit_task(CognitiveTask task);

private:
    void run(std::stop_token st);

    // ── 每个 Worker 独立的引擎实例 ──
    std::unique_ptr<DSLEngine> engine_;  // 独立实例（ADR-0003）
    std::shared_ptr<IInteractionBus> bus_;

    // ── 任务队列（MVP: mutex+queue） ──
    // 选择理由：简单、可靠、够用。无锁队列（LockFreeQueue）延迟到 Phase 2
    // 当性能瓶颈凸显时再引入，避免无锁编程的复杂性。
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<CognitiveTask> task_queue_;

    std::jthread thread_;
};

} // namespace agenticdsl

#endif
```

**工作循环**：

```cpp
// src/common/worker/cognitive_worker.cpp
void CognitiveWorker::run(std::stop_token st) {
    while (!st.stop_requested()) {
        CognitiveTask task;

        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait(lock, [&] {
                return !task_queue_.empty() || st.stop_requested();
            });
            if (st.stop_requested()) break;
            task = std::move(task_queue_.front());
            task_queue_.pop();
        }

        try {
            // EPoll Token 回调
            bus_->subscribe_tokens(task.session_id, task.on_token);
            bus_->subscribe_events(task.session_id, task.on_event);

            // 使用本 Worker 的 engine_ 执行（非共享）
            engine_->run_async(task.session_id, task.user_message);

        } catch (const std::exception& e) {
            bus_->push_event(task.session_id, Event{
                EventType::Error, task.session_id, e.what()
            });
        }
    }
}
```

#### 3.2 领域智能体工作线程池

**✅ 已实施 (Sprint 3 ship, 2026-06-19)**：实际位置 `include/agenticdsl/cognitive/domain_worker_pool.h` + `src/modules/cognitive/domain_worker_pool.cpp`（非 §3.2 早期草图的 `src/common/worker/`，Phase 1 重构后统一在 `src/modules/cognitive/`）。与本节设计一致的关键点：
- `std::jthread` 协作式取消 (C++20, std::stop_token)
- `std::shared_mutex` 保护 handler 注册表
- `std::queue<DomainTask>` + `std::mutex` + `std::condition_variable` 共享任务队列 (多消费者模式, **非** dispatcher 线程)
- `domain.task.started` / `domain.task.completed` / `domain.task.failed` 事件 topic 遵循 `<module>.<verb>` 约定
- CP.22 协议 6/6 项通过 (`.omo/plans/2026-06-30-cp22-audit.md`)

实施差异 (vs §3.2 早期草图):
1. **位置**: `include/agenticdsl/cognitive/` (非 `src/common/worker/`) — 与 CognitiveWorker 统一在 cognitive 模块
2. **双构造重载**: `(num_threads)` 与 `(num_threads, shared_ptr<IInteractionBus>)` — Sprint 3 扩展, F7 顺序契约对齐 CognitiveWorker
3. **unregister_domain_handler**: 增加, 支持运行时取消注册
4. **析构函数 out-of-line**: PIMPL-lite 模式, 同 CognitiveWorker TD-CW-02
5. **状态机**: 显式 `enum class State { idle, running, stopped }`, 同 CognitiveWorker
6. **OpenSpec change**: `2026-06-30-domain-worker-pool` (Sprint 3 真实实施)

```cpp
// include/agenticdsl/cognitive/domain_worker_pool.h (Sprint 3 实际位置)
namespace agenticdsl {

struct DomainTask {
    std::string domain;          // "code", "browser", "fs"
    std::string tool_name;       // "code::edit_file"
    nlohmann::json arguments;
    std::string output_key;      // 结果写入的 key
};

---

### 4. 线程间通信

#### 4.1 任务队列（MVP: std::mutex + std::queue）

**MVP 使用 `std::mutex` + `std::queue`，不引入 LockFreeQueue**。

| 方案 | 优点 | 缺点 | MVP 选择 |
|------|------|------|---------|
| `std::mutex` + `std::queue` | 简单、100% 正确、易调试 | 锁竞争（可忽略，队列非热点） | **✅ 选择** |
| LockFreeQueue | 无锁、高性能 | 内存泄漏风险、ABA 问题、实现复杂  | ❌ 延迟到 Phase 2 |

```cpp
// MVP 任务队列 — 够用
template<typename T>
class TaskQueue {
public:
    void push(T task) {
        std::lock_guard lock(mutex_);
        queue_.push(std::move(task));
    }

    std::optional<T> pop() {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        auto task = std::move(queue_.front());
        queue_.pop();
        return task;
    }

    bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
};
```

**Phase 2 引入 LockFreeQueue 前的条件**：
1. Profile 证明 `std::mutex` 是性能瓶颈
2. 单元测试覆盖 SPSC 场景
3. 通过 ThreadSanitizer 验证无 data race

#### 4.2 State Store 线程安全

```cpp
// src/core/state_store.h
namespace agenticdsl {

class StateStore {
public:
    // 读操作 — 共享锁
    nlohmann::json read(const std::string& key) const {
        std::shared_lock lock(mutex_);
        auto it = data_.find(key);
        return it != data_.end() ? it->second : nlohmann::json{};
    }

    // 写操作 — 独占锁
    void write(const std::string& key, const nlohmann::json& value) {
        std::unique_lock lock(mutex_);
        data_[key] = value;
    }

    // 命名空间批量读取
    std::unordered_map<std::string, nlohmann::json> read_namespace(
        const std::string& ns_prefix
    ) const {
        std::shared_lock lock(mutex_);
        std::unordered_map<std::string, nlohmann::json> result;
        for (const auto& [key, value] : data_) {
            if (key.starts_with(ns_prefix)) {
                result[key] = value;
            }
        }
        return result;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, nlohmann::json> data_;
};

} // namespace agenticdsl
```

---

### 5. ToolRegistry 锁模型（修正）

**修正前的问题**：
- `call_tool()` 使用 `unique_lock`（写锁），阻塞所有读操作
- 权限检查（ADR-0004）在锁内执行，可能导致数秒阻塞

**修正后的锁模型**：

```cpp
class ToolRegistry {
    std::shared_mutex mutex_;

    nlohmann::json call_tool(const std::string& name, const Args& args) {
        // Step 1: 读锁 — 只查找工具函数，不修改注册表
        std::function<...> func;
        {
            std::shared_lock lock(mutex_);
            auto it = tools_.find(name);
            if (it == tools_.end()) {
                return {{"error", "Tool not found: " + name}};
            }
            func = it->second;
        }
        // ↑ 锁已被释放

        // Step 2: 权限检查（锁外执行，可能等待用户确认）
        // ADR-0004: Ask/Allow/Deny
        auto permission = check_permission(name, args);
        if (permission == ToolPermission::Deny) {
            return {{"error", "Permission denied for tool: " + name}};
        }
        if (permission == ToolPermission::Ask) {
            // 发送 USER_INPUT 事件，异步等待确认
            auto confirmed = wait_for_user_confirmation(name, args);
            if (!confirmed) {
                return {{"error", "User rejected tool call: " + name}};
            }
        }

        // Step 3: 执行工具（锁外执行，工具可长时间运行）
        return func(args);
    }

    // 工具注册 — 使用写锁
    void register_tool(std::string name, ToolFunc func) {
        std::unique_lock lock(mutex_);
        tools_[std::move(name)] = std::move(func);
    }

private:
    std::unordered_map<std::string, ToolFunc> tools_;
};
```

**锁模型规则**：

| 操作 | 锁类型 | 理由 |
|------|--------|------|
| `register_tool()` | `unique_lock`（写锁） | 修改注册表 |
| `unregister_tool()` | `unique_lock`（写锁） | 修改注册表 |
| `call_tool()` | `shared_lock`（读锁）+ 锁外执行 | 只查找，不修改 |
| `get_schema()` | `shared_lock`（读锁） | 只读查询 |
| `list_tools()` | `shared_lock`（读锁） | 只读查询 |

---

### 6. 死锁避免策略

多锁场景下的锁顺序：

```cpp
// 全局锁顺序（所有线程必须遵守）
// 1. StateStore::mutex_       (最高优先级)
// 2. ToolRegistry::mutex_     
// 3. CognitiveWorker::queue_mutex_
// 4. InMemoryBus::mutex_       (最低优先级)

// 正确示例：
void process_tool_result(const std::string& key, const ToolResult& result) {
    // 先 StateStore，再 Bus
    state_store_.write(key, result.to_json());   // 1
    bus_.push_event(session_id, event);          // 4
}

// 禁止：反序获取锁
// void wrong() {
//     bus_.lock();  // 4
//     state_store_.lock();  // 1 — 反向！可能导致死锁
// }
```

---

### 7. 进程沙箱（Phase 2 预留）

#### 7.1 沙箱策略

| 风险 | 场景 | 隔离方案 | MVP 替代 |
|------|------|---------|---------|
| **崩溃** | 浏览器执行恶意 JS | 子进程崩溃隔离 | 权限校验 (ADR-0004) |
| **泄露** | 文件工具读取敏感路径 | chroot jail + 白名单 | PathPolicy |
| **攻击** | 系统工具执行注入 | seccomp-bpf | 命令白名单 |
| **资源耗尽** | 打开超大文件 | cgroups 限制 | 超时控制 |
| **网络外联** | 恶意网站访问 | 网络命名空间 | 禁止网络工具 |

#### 7.2 沙箱接口预留

```cpp
// src/common/sandbox/sandbox_controller.h (Phase 2)
namespace agenticdsl {

struct SandboxResult {
    bool success;
    nlohmann::json result;
    std::string error;
    int exit_code;
};

struct SandboxConfig {
    size_t memory_limit_mb = 256;
    size_t cpu_time_limit_ms = 30000;
    size_t max_processes = 10;
    std::vector<std::string> allowed_paths;
    std::vector<std::string> denied_paths;
    bool network_allowed = false;
};

class ISandboxController {
public:
    virtual ~ISandboxController() = default;
    virtual SandboxResult execute(
        const std::string& tool_name,
        const nlohmann::json& args,
        const SandboxConfig& config
    ) = 0;
    virtual bool is_available() const = 0;
};

} // namespace agenticdsl
```

---

### 8. 协程模型（已迁移至 ADR-0030）

> **V3 变更**：本节原定义的自定义协程实现（`LLMTokenStream` 类）已废弃。
> 协程实现统一使用 ADR-0030 定义的 AsyncRuntime + async_simple 方案。

LLM 流式调用使用 async_simple 协程（参见 [ADR-0030](../archive/adr/adr-0030-async-runtime-dual-layer.md) 第 3 节）：

```cpp
// src/common/llm/stream_llm.h (Phase 2)
#include <async_simple/coro/Generator.h>

namespace agenticdsl {

// 使用 async_simple::Generator 替代自研 LLMTokenStream
async_simple::coro::Generator<Token> stream_llm_response(
    const std::string& prompt, 
    AsyncRuntime& runtime);

} // namespace agenticdsl
```

**迁移理由**：
- async_simple v1.4 提供完整的 C++20 协程支持（`Lazy<T>`、`Generator<T>`）
- 性能优于 folly::coro（官方基准测试验证）
- 避免自研协程基础设施的复杂性和维护成本
- 与 Taskflow 计算层通过桥接层无缝集成

---

## 替代方案

### 方案 A: 单线程事件循环

- **优点**：简单，无线程竞争
- **缺点**：无法利用多核，LLM IO 阻塞整个系统
- **结论**：不满足需求

### 方案 B: 全协程模型

- **优点**：轻量，上下文切换快
- **缺点**：C++20 协程复杂，集成成本高
- **结论**：Phase 2 考虑

### 方案 C: 线程池 + 任务队列

- **优点**：灵活，控制并发度
- **缺点**：任务分配复杂
- **结论**：MVP 使用每 Worker 独立线程（固定 N）

---

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| **DSLEngine 共享** | 每 Worker 独立实例 | 符合 ADR-0003 |
| **任务队列** | `std::mutex` + `std::queue` | 简单可靠，Phase 2 再优化 |
| **ToolRegistry 锁** | `shared_lock` 读锁 + 锁外执行 | 避免阻塞权限校验 |
| **锁顺序** | 全局排序 | 避免死锁 |
| **协程** | MVP 不使用 | Phase 2 引入 |
| **进程沙箱** | MVP 不实现 | Phase 2 引入 |

---

## 实施计划

| Phase | 任务 | 产出 |
|-------|------|------|
| **Phase 1** | 实现 `CognitiveWorker`（独立 DSLEngine）<br>实现 `DomainWorkerPool`<br>实现 `TaskQueue<T>`（mutex+queue）<br>`StateStore` 线程安全版 | 工作线程框架 |
| **Phase 2** | Profile 瓶颈<br>按需引入 LockFreeQueue<br>LLM 回调集成 | 通信优化 |
| **Phase 3** | `ISandboxController` 接口实现<br>cgroups/seccomp 集成<br>沙箱配置加载 | 进程隔离 |
| **Phase 4** | ~~C++20 协程 LLM 流式~~ → 迁移至 ADR-0030<br>集成 AsyncRuntime 协程调度<br>性能测试与调优 | 协程化 |

---

## 验证标准

| 标准 | 验证方法 |
|------|---------|
| 多线程安全 | `ThreadSanitizer` 运行测试无 data race |
| 独立 DSLEngine | 每个 CognitiveWorker 有自己的 `unique_ptr<DSLEngine>` |
| 锁顺序 | 代码审查确保全局锁排序 |
| 无锁泄漏 | 无 LockFreeQueue 相关泄漏 |
| 优雅退出 | Ctrl+C 后所有线程正确 join |
| 并发执行 | 提交多个任务，验证并行执行 |

---

## 参考

- [ADR-0002: EventBus 有界队列](./adr-0002-eventbus-bounded-queue.md)
- [ADR-0003: DSLEngine 线程安全](./adr-0003-dslengine-thread-safety.md)
- [ADR-0004: ToolRegistry 安全模型](./adr-0004-toolregistry-security.md)
- [ADR-0006: HarnessEngine 后台线程模型（已替代）](./adr-0006-harness-engine-thread-model.md)
- [ADR-0019: IInteractionBus 接口与 TUI Chat MVP](./adr-0019-iinteraction-bus-mvp.md)

---

## 附录 A: 与 ADR-0006 的差异

| 方面 | ADR-0006 (已替代) | 本 ADR |
|------|------------------|--------|
| **线程模型** | 每 Agent 一线程（平面） | CognitiveWorker + DomainWorkerPool（分层） |
| **DSLEngine** | Agent 持有 | CognitiveWorker 拥有独立实例 |
| **生命周期** | HarnessEngine 统一管理 | 每个 Worker 独立 start/stop |
| **通信** | EventBus | IInteractionBus + StateStore |
| **沙箱** | 未涉及 | Phase 2 预留接口 |
| **协程** | 未涉及 | ~~Phase 2 预留接口~~ → 迁移至 ADR-0030 |

## 附录 B: 文件变更清单

| 操作 | 文件路径 |
|------|---------|
| **新建** | `src/common/worker/CMakeLists.txt` |
| **新建** | `src/common/worker/cognitive_worker.h` |
| **新建** | `src/common/worker/cognitive_worker.cpp` |
| **新建** | `src/common/worker/domain_worker_pool.h` |
| **新建** | `src/common/worker/domain_worker_pool.cpp` |
| **新建** | `src/core/state_store.h` |
| **新建** | `src/core/state_store.cpp` |
| **新建** | `src/common/sandbox/sandbox_controller.h` (Phase 2) |
| **新建** | `src/common/async/async_runtime.h` (ADR-0030) |
| **修改** | `src/core/engine.h` (+ atomic_bus, 独立实例安全) |
| **修改** | `src/common/tools/registry.h` (+ shared_lock 模型) |
