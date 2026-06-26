# ADR-0030：AsyncRuntime 双层异步架构（Taskflow + async_simple）
> 📋 **Phase 5 规划: 异步架构** (规划于 2026-05/06, 2026-06-09 整理归档) — 见 `implementation-roadmap.md`

> **⚠️ V1 SUPERSEDED by V2** (2026-06-26, OpenSpec change `2026-06-26-doc-alignment-adr-states`)
> **V1 替代原因**: Slice 00 已 ship (2026-06-07, S0.1-S0.6), Taskflow v4.0 + async_simple v1.4 依赖实际已引入; Sprint 2/3 CognitiveWorker + DomainWorkerPool 验证 std::jthread (C++20 RAII) 替代 async_simple 协程层更轻量。
> **详见**: [`docs/adr/adr-0030-async-runtime-v2.md`](../adr/adr-0030-async-runtime-v2.md) (V2: Taskflow + std::jthread + IInteractionBus)
> **保留原因**: 历史可追溯, V2 不复用 V1 编号。

## 状态

**❌ 未实施** (2026-05-27, 2026-06-09 标注废弃) — **V1 版 (SUPERSEDED)**,基于 Oracle 审查与 Taskflow v4.0 调研结论锁定

## 领域

基座 / 并发执行模型

## 关联

- ADR-0002（EventBus）
- ADR-0019（IInteractionBus）
- ADR-0020（线程模型）
- ADR-0025（并行子任务）

---

## 背景

### 当前代码库状态

HydraForge 当前执行模型为**纯同步单线程**：

| 组件 | 现状 |
|------|------|
| `DSLEngine::run()` | 同步阻塞，无并发 |
| `TopoScheduler::execute()` | 单线程 `while` 循环串行执行节点 |
| `Fork/Join` | 假并行——分支在 `while` 循环中逐个执行 |
| **线程原语** | **零使用**（仅 `std::thread::hardware_concurrency()` 读取 CPU 核心数） |
| `Context` | 共享可变状态，按引用传递并就地修改 |
| `build_dag()` | 运行时全局重建（`clear()` + 全量重建），非线程安全 |

### 需求

编程助手智能体需要以下并发能力：

| 需求 | 特征 | 适合模型 |
|------|------|---------|
| DAG 节点并行执行 | 短时计算、确定性拓扑 | **任务图引擎** |
| 舰队模式 16 路 LLM 调用 | IO 密集、全部完成后聚合 | **并行提交+等待** |
| LLM Token 流式推送 | 增量数据、长生命周期 | **协程 yield** |
| 用户审批等待 (/apply) | 外部事件、不确定时长 | **协程 suspend** |
| IPER 循环控制 | 有限状态机、条件转移 | **协程状态机** |
| 优先级响应（用户中断） | 可抢占、即时响应 | **协程调度器** |

单一并发模型无法覆盖所有场景。经评估 6 种方案后，选择 **双层架构**。

### 调研结论

#### Taskflow v4.0 协程支持状态

**Taskflow v4.0 不支持 C++20 协程语义（`co_await`、`co_yield`）**。

| 证据来源 | 关键内容 |
|---------|---------|
| **GitHub Issue #492** | 维护者明确回复："*Currently, we do not support co_await and coroutines, but it's under our research agenda*" |
| **GitHub Issue #763** (2026-02，仍 Open) | 维护者回复："*this is something we are very interested but still working on designing the interface*" |
| **v4.0 发布说明** | 仅提及 C++20 迁移用于性能提升，**无任何协程 API** |
| **官方文档** | `corun()` 是**协作式线程执行**（cooperative multitasking），**非 C++20 coroutine** |

**Taskflow v4 实际提供的异步能力**：

| API | 语义 | 与协程关系 |
|-----|------|-----------|
| `executor.async(lambda)` | 返回 `std::future<T>` | ❌ 无协程集成 |
| `executor.silent_async(lambda)` | 无返回值异步任务 | ❌ 无协程集成 |
| `runtime.corun()` | 协作执行（不阻塞线程） | ❌ 非 `co_await` |
| `taskgroup.async()` | 任务组内异步 | ❌ 无协程集成 |

> **关键澄清**：Taskflow 的 `corun()` 是协作式多任务（cooperative multitasking），与 C++20 stackless coroutine 的 `co_await`/`co_yield` 是两套不同的并发模型，**不可互换**。

#### async_simple 评估结论

| 维度 | async_simple | 评估 |
|------|:-----------:|------|
| 版本 | v1.4 (2025-07) | ✅ 活跃维护 |
| Stars | ~2K | ⚠️ 社区较小但稳定 |
| C++20 协程 | `Lazy<T>`、`Generator<T>`、`co_await`/`co_yield` | ✅ 完整支持 |
| 编译要求 | ~10 个 .cpp（非 header-only） | ✅ 可接受 |
| 性能 | 优于 folly::coro（官方基准测试） | ✅ 性能优秀 |
| 生态 | 与 ASIO 集成示例，yalantinglibs 生态 | ✅ 有实际应用验证 |

**async_simple 核心能力**：
- `Lazy<T>`：惰性求值协程，适合 IPER 循环
- `Generator<T>`：`co_yield` 流式推送，适合 Token 流
- `collectAll()`/`collectAny()`：并行等待，适合舰队模式
- `syncAwait()`：同步桥接，适合现有代码集成
- `RescheduleLazy`：可绑定 Executor，适合线程调度

---

## 决策

### 采用 Taskflow（计算层）+ async_simple（控制层）双层异步架构

由于 Taskflow v4.0 **不支持 C++20 协程**，必须引入独立的协程库。async_simple 是当前最佳选择。

```
┌──────────────────────────────────────────────────────────────┐
│  L2: 控制层 — async_simple                                     │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ • IPER 循环编排（Lazy<T> 协程状态机）                      │ │
│  │ • LLM Token 流式推送（AsyncGenerator / co_yield）          │ │
│  │ • 用户审批等待（co_await event，不占线程）                 │ │
│  │ • Session 生命周期管理                                     │ │
│  │ • 优先级调度（协程调度器 round-robin + priority queue）    │ │
│  └──────────────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────────────┤
│  L1: 计算层 — Taskflow                                         │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ • DAG 节点并行执行（Taskflow graph）                       │ │
│  │ • Fork/Join 子图（Subflow）                                │ │
│  │ • 舰队模式批量调用（executor.async() + wait_for_all()）   │ │
│  │ • 纯计算并行（parallel_for：模板渲染、AST 解析）          │ │
│  │ • EventBus 消费者并行分发                                  │ │
│  └──────────────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────────────┤
│  L0: 基础设施                                                  │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ • tf::Executor（work-stealing 线程池，可配置大小）         │ │
│  │ • async_simple::Executor（协程调度器）                     │ │
│  │ • 桥接层：协程 ↔ Taskflow Future 互转                     │ │
│  └──────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

### 职责划分原则

**判断一个操作归属哪层的决策树**：

```
需要中途挂起/恢复？──── Yes ──→ 控制层（async_simple）
     │
     No
     │
需要 DAG 依赖调度？──── Yes ──→ 计算层（Taskflow）
     │
     No
     │
是否 IO 阻塞 > 100ms？── Yes ──→ 控制层（async_simple 的 IO 协程）
     │
     No
     │
纯 CPU 计算 ────────────────→ 计算层（Taskflow executor.async()）
```

| 操作 | 归属层 | 理由 |
|------|--------|------|
| DSL 解析（MarkdownParser） | Taskflow | 纯 CPU，无需 yield |
| 模板渲染（Inja） | Taskflow | 纯 CPU |
| DAG 拓扑排序 + 节点分发 | Taskflow | 原生 DAG 图执行 |
| Fork/Join 并行分支 | Taskflow Subflow | 原生支持 |
| 舰队模式 N 路 LLM | **混合** | Taskflow 提交 N 个任务；async_simple 管理流式回调 |
| LLM 流式 token | async_simple | 需要 co_yield 增量推送 |
| 用户审批等待 | async_simple | co_await 外部事件，不占线程 |
| IPER 循环 | async_simple | 状态机 + 条件挂起 |
| EventBus 事件分发 | Taskflow | 消费者并行执行，无需 yield |
| 成本收集聚合 | Taskflow | 纯计算聚合 |
| Session 模式切换 | async_simple | 需要中断当前执行流 |

---

## 核心接口设计

### 1. AsyncRuntime（统一入口）

```cpp
#include <taskflow/taskflow.hpp>
#include <async_simple/coro/Lazy.h>
#include <async_simple/coro/SyncAwait.h>
#include <async_simple/executors/SimpleExecutor.h>

class AsyncRuntime {
public:
    struct Config {
        size_t taskflow_threads = std::thread::hardware_concurrency();
        size_t io_threads = 16;             // 用于 HTTP 阻塞调用
        size_t coro_threads = 4;            // 协程调度器线程
    };
    
    explicit AsyncRuntime(Config config);
    
    // --- 计算层接口（Taskflow）---
    tf::Executor& executor();               // 获取 Taskflow executor
    tf::Future<void> run(tf::Taskflow& graph);  // 执行 DAG 图
    
    // 便捷：并行提交 N 个任务，等待全部完成
    template<typename F>
    std::vector<std::invoke_result_t<F>> 
    parallel_map(std::vector<F>&& tasks);
    
    // --- 控制层接口（async_simple）---
    async_simple::Executor* coro_executor(); // 协程调度器
    
    // 启动一个顶层协程（Session 主循环等）
    template<typename T>
    void spawn(async_simple::coro::Lazy<T>&& coro);
    
    // 同步桥接：在非协程上下文中等待协程完成
    template<typename T>
    T sync_await(async_simple::coro::Lazy<T>&& coro);
    
    // --- 桥接接口 ---
    
    // 在协程中等待 Taskflow 图完成（不阻塞协程线程）
    async_simple::coro::Lazy<void> 
    await_taskflow(tf::Taskflow& graph);
    
    // 在协程中等待 Taskflow async 任务完成
    template<typename T>
    async_simple::coro::Lazy<T> 
    await_future(std::future<T>&& fut);
    
    // --- 生命周期 ---
    void shutdown();                        // 优雅停止所有线程
    bool is_running() const;
    
private:
    tf::Executor tf_executor_;              // Taskflow 线程池
    tf::Executor io_executor_;              // IO 专用线程池
    std::unique_ptr<async_simple::executors::SimpleExecutor> coro_executor_;
};
```

### 2. 桥接层：协程等待 Taskflow 执行

```cpp
// 核心桥接：让协程 co_await 一个 Taskflow 图的完成
// 实现原理：Taskflow 执行完成后唤醒挂起的协程
async_simple::coro::Lazy<void> 
AsyncRuntime::await_taskflow(tf::Taskflow& graph) {
    // 创建一个 Promise/Future 对
    async_simple::Promise<void> promise;
    auto lazy_future = promise.getFuture();
    
    // 在 Taskflow 图末尾添加通知节点
    graph.emplace([p = std::move(promise)]() mutable {
        p.setValue();  // 唤醒等待的协程
    });
    
    // 提交图到 executor
    tf_executor_.run(graph);
    
    // 协程挂起，直到图执行完毕
    co_await std::move(lazy_future);
}

// 桥接：协程等待 std::future
// ✅ 修正：使用 async_simple 的 FutureAwaiter（非忙等待）
template<typename T>
async_simple::coro::Lazy<T>
AsyncRuntime::await_future(std::future<T>&& fut) {
    // 使用 async_simple 提供的 FutureAwaiter，真异步等待
    co_return co_await async_simple::coro::FutureAwaiter(std::move(fut));
}
```

> **重要修正**：早期设计中使用 `while + co_await Yield` 轮询是**忙等待**（每秒轮询 1000 次），已修正为使用 `FutureAwaiter` 实现真异步等待。

### 3. IPER 循环（控制层示例）

```cpp
using Lazy = async_simple::coro::Lazy;

Lazy<SessionResult> iper_loop(
    AsyncRuntime& runtime,
    SessionContext& ctx,
    IExecutionPolicy* policy) 
{
    while (!ctx.is_ended()) {
        // ===== Infer 阶段 =====
        auto intent = co_await infer_user_intent(ctx);
        
        // ===== Plan 阶段 =====
        auto plan = co_await generate_plan(intent, ctx);
        
        // Plan 模式：展示计划，等待用户确认
        if (policy->should_show_plan()) {
            ctx.event_bus().emit("PlanGenerated", plan);
            auto approval = co_await wait_user_event("PlanApproval");
            if (!approval.approved) continue;  // 回到 Infer
        }
        
        // ===== Execute 阶段 =====
        // 编译 Plan 为 Taskflow DAG
        tf::Taskflow dag = compile_plan_to_dag(plan, ctx);
        
        // 协程等待 DAG 执行完毕（不阻塞协程线程）
        co_await runtime.await_taskflow(dag);
        
        auto exec_result = collect_dag_results(dag);
        
        // ===== Reflect 阶段 =====
        auto reflection = co_await reflect_on_result(exec_result, ctx);
        ctx.append(reflection);
        
        if (reflection.success) break;
        // 否则继续循环
    }
    co_return ctx.finalize();
}
```

### 4. 舰队模式（混合示例）

```cpp
// 舰队模式：16 路并行 LLM 调用 + 流式 token 推送
Lazy<std::vector<LLMResponse>> fleet_execute(
    AsyncRuntime& runtime,
    std::vector<FleetTask> tasks,
    IInteractionBus& bus) 
{
    // 方案 A：全阻塞等待（简单，无流式）
    auto results = runtime.parallel_map(
        tasks | transform([](auto& t) {
            return [&t]() { return http_post(t.prompt); };
        }) | to_vector()
    );
    co_return results;
    
    // 方案 B：流式 + 并行（复杂，有流式体验）
    std::vector<Lazy<LLMResponse>> coros;
    for (auto& task : tasks) {
        coros.push_back(stream_single_llm(task, bus));
    }
    auto results = co_await async_simple::coro::collectAll(
        std::move(coros));
    co_return results;
}

// 单个 LLM 流式调用（控制层）
Lazy<LLMResponse> stream_single_llm(
    FleetTask& task, IInteractionBus& bus) 
{
    LLMResponse response;
    auto stream = co_await open_http_stream(task.prompt);
    
    while (auto chunk = co_await stream.next()) {
        response.append(*chunk);
        bus.push_token(task.session_id, *chunk);  // 实时推送
    }
    co_return response;
}
```

### 5. 用户审批（控制层示例）

```cpp
// 工具执行前的审批等待
Lazy<ToolResult> execute_with_approval(
    ToolCall& call, 
    AsyncRuntime& runtime,
    IInteractionBus& bus,
    IExecutionPolicy* policy) 
{
    // 检查是否需要审批
    if (policy->requires_tool_approval(call.tool_meta())) {
        // 生成 diff 预览
        auto preview = generate_preview(call);
        
        // 发送审批请求（非阻塞）
        bus.emit("RequestApproval", {
            .tool_name = call.name,
            .preview = preview,
            .session_id = call.session_id
        });
        
        // 协程挂起，等待用户事件（不占线程！）
        auto event = co_await wait_user_event(
            "ApprovalResponse", 
            call.session_id,
            std::chrono::minutes(5)  // 超时
        );
        
        if (!event.has_value()) {
            co_return ToolResult::timeout("User approval timeout");
        }
        if (!event->approved) {
            co_return ToolResult::rejected("User rejected");
        }
    }
    
    // 执行工具（可能在 Taskflow 线程池中）
    auto result = co_await runtime.await_future(
        runtime.executor().async([&call]() {
            return call.execute();
        })
    );
    co_return result;
}
```

---

## 与现有代码的集成策略

### Phase 0：引入依赖（无破坏性变更）

```cmake
# CMakeLists.txt
# Taskflow: header-only，直接添加 include path
add_subdirectory(external/taskflow)  # 或 target_include_directories

# async_simple: 需要编译少量 .cpp
add_subdirectory(external/async_simple)
target_link_libraries(agenticdsl_core PRIVATE async_simple)
```

### Phase 1：AsyncRuntime 作为可选组件

```cpp
// engine.h — 现有接口不变
class DSLEngine {
    // 新增：可选的异步运行时
    std::unique_ptr<AsyncRuntime> async_runtime_;
    
    // 现有同步接口保持兼容
    ExecutionResult run(const Context& ctx);  // 同步，不变
    
    // 新增：异步执行（返回协程）
    async_simple::coro::Lazy<ExecutionResult> 
    run_async(const Context& ctx);
};
```

### Phase 2：TopoScheduler 双模式

```cpp
class TopoScheduler {
    // 现有同步路径保留（tests 继续工作）
    ExecutionResult execute_sync(Context ctx);
    
    // 新增：Taskflow 并行路径
    ExecutionResult execute_parallel(Context ctx, tf::Executor& executor);
    
    // 新增：异步路径（协程 + Taskflow）
    Lazy<ExecutionResult> execute_async(Context ctx, AsyncRuntime& runtime);
};
```

> **建议**：`execute_sync` 实现为 `sync_await(execute_async(...))`，避免维护两套独立路径。

---

## 线程模型与资源配置

```
┌─────────────────────────────────────────────────────────┐
│  进程内线程布局                                            │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  主线程 (Main Thread)                                    │
│  ├── 事件循环入口                                        │
│  └── sync_await 桥接点                                   │
│                                                          │
│  协程调度器线程 ×2-4 (async_simple Executor)              │
│  ├── IPER 循环协程                                       │
│  ├── 用户审批等待协程                                    │
│  ├── Session 管理协程                                    │
│  └── Token 流式推送协程                                  │
│                                                          │
│  Taskflow 计算池 ×N (tf::Executor, N = CPU cores)       │
│  ├── DAG 节点并行执行                                    │
│  ├── 模板渲染                                            │
│  └── EventBus 消费者分发                                 │
│                                                          │
│  IO 线程池 ×16 (tf::Executor, 专用)                     │
│  ├── HTTP/LLM API 调用                                   │
│  └── 文件系统操作                                        │
│                                                          │
└─────────────────────────────────────────────────────────┘

总线程数 ≈ 2 + 4 + N + 16 = 26 (8核机器)
```

---

## 技术风险与缓解

| 风险 | 严重度 | 缓解措施 |
|------|--------|---------|
| **两套模型认知负担** | 中 | 明确决策树（上文）；封装 AsyncRuntime 统一入口 |
| **桥接层 bug** | 中 | 桥接逻辑集中在 3 个函数中；充分单元测试 |
| **async_simple 非 header-only** | 低 | 编译量小（核心 < 10 个 .cpp）；CMake add_subdirectory 管理 |
| **协程调试困难** | 中 | 开发阶段使用 inline executor（同步执行）模拟；Release 才启用异步 |
| **Taskflow 无优先级** | 低 | 优先级在控制层（协程调度器）处理，不依赖 Taskflow |
| **线程数爆炸** | 低 | 严格限制：协程 4 + 计算 N + IO 16；可配置 |
| **死锁：协程等 Taskflow，Taskflow 线程满** | 中 | IO 和计算使用独立 Executor；协程等待时释放线程 |
| **共享可变 Context** | 🔴 高 | Context 添加 `fork()`/`merge()` 不可变分支支持 |
| **动态图重建冲突** | 🔴 高 | 增量 DAG 更新替代全量重建 |

---

## 与纯方案的对比（决策记录）

| 维度 | 纯 Taskflow | 纯 async_simple | **Taskflow + async_simple** |
|------|:----------:|:--------------:|:-------------------------:|
| DAG 并行 | ⭐⭐⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐⭐⭐ |
| 流式 Token | ⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| 用户审批 | ⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| IPER 循环 | ⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| 实现复杂度 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ |
| 依赖重量 | 轻 | 轻 | **中（可接受）** |
| 编程助手覆盖度 | 60% | 70% | **95%** |

> **注**：纯 Taskflow 方案因不支持协程，无法覆盖流式推送、用户审批等核心需求，故不采纳。

---

## 后续行动

- [x] 确认 Taskflow v4.0 不支持 C++20 协程（Issue #492, #763）
- [x] 确认 async_simple 为当前最佳协程库选择
- [x] 修正桥接层设计（移除忙等待，使用 FutureAwaiter）
- [ ] 更新 ADR-0020（线程模型）以对齐双 Executor 设计
- [ ] 创建 `src/common/async/` 目录，放置 AsyncRuntime 和桥接代码
- [ ] 引入 external/taskflow 和 external/async_simple
- [ ] 编写桥接层单元测试（`tests/test_async_runtime.cpp`）
- [ ] Context 添加 `fork()`/`merge()` 不可变分支支持
- [ ] TopoScheduler 实现增量 DAG 更新

---

## 对议题 1 决策点的最终回答

| # | 问题 | 最终决策 |
|---|------|---------|
| 1 | 并发模型选择 | **混合：Taskflow（DAG/计算）+ async_simple（控制/IO）** |
| 2 | IO 线程池大小 | 独立 tf::Executor(16)，可配置 |
| 3 | 队列满时策略 | Taskflow 内置 work-stealing；协程自然背压 |
| 4 | Future 是否支持 .then() | 不需要——async_simple 的 co_await 替代 .then() |
| 5 | 是否引入第三方库 | **是：Taskflow（header-only）+ async_simple（轻量编译）** |
| 6 | 与 DSLEngine 的关系 | AsyncRuntime 外部注入 DSLEngine（保持可测试性） |
| 7 | stop_token 传播 | 按 Session 分组——每个 IPER 协程有独立 CancellationToken |
| 8 | Taskflow 协程支持 | ❌ **不支持**（v4.0 确认），必须引入 async_simple |
| 9 | 桥接层实现 | 使用 `FutureAwaiter`（真异步），禁止忙等待 |

---

## 变更记录

| 版本 | 日期 | 变更内容 |
|------|------|---------|
| V1 | 2026-05-27 | 初始版本，基于 Taskflow v4.0 调研结论锁定双层架构 |
| V1.1 | 2026-05-27 | 修正桥接层 `await_future` 实现（移除忙等待，使用 FutureAwaiter） |
| V1.2 | 2026-05-27 | 增加 Taskflow 协程不支持的确切证据（Issue #492, #763） |

---

## 与 ADR-0036 的集成补充

### 1. ICognitiveOrchestrator.process() callback ↔ async_simple 桥接

ADR-0036 定义的 `process()` 接口为 callback 风格：

```cpp
void process(const std::string& session_id,
             std::function<void(ExecutionResult)> on_complete);
```

内部实现通过 AsyncRuntime 桥接到协程：

```cpp
void CognitiveOrchestrator::process(
    const std::string& session_id,
    std::function<void(ExecutionResult)> on_complete)
{
    // 在认知层工作线程中启动协程
    runtime_.spawn([=]() -> Lazy<void> {
        auto result = co_await run_iper_loop(session_id);
        // 协程结束时在协程上下文中回调
        on_complete(result);
    }());
}
```

**原则**：外部接口统一为 callback（基座层无需理解协程），内部实现使用 async_simple（认知层受益于协程的简洁性）。

### 2. 三种线程的关系

| 线程 | 职责 | 创建者 | 与 process() 的关系 |
|:---|:---|:---|:---|
| **主线程** | 基座初始化、UserSession 管理、IInteractionBus 事件循环 | 应用入口 | `process()` 在独立线程调用，不阻塞主线程事件循环 |
| **认知层工作线程** | 执行 `process()`，驱动 IPER 循环 | CognitiveOrchestrator 启动时创建（或动态） | `on_complete` 在此线程回调，非主线程 |
| **EventBus 分发线程** | 从 MPMC 队列取事件并分发给订阅者 | EventBus 启动时创建 | 事件 handler 在 EventBus 线程执行，认知层工作线程通过 `co_await` 等待事件 |

**约束**：
- `on_complete` 不可在主线程调用（避免阻塞事件循环）
- EventBus handler 应快速执行（< 100μs），复杂逻辑 post 到认知层工作线程或 Taskflow 计算池
- 跨线程的数据传递使用 `std::atomic<>` 或 EventBus 消息
