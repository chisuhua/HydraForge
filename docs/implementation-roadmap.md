# Implementation Roadmap

> 基于 ADR-0019 ~ ADR-0030 的实施追踪，记录所有确认的代码改动。
>
> **最后更新**: 2026-05-27 | **状态约定**: `[ ]` 待实施 / `[x]` 已完成 / `[~]` 进行中

---

## 依赖关系总览

```
Phase 1 (独立并行)
├── ADR-0019 P1 — IInteractionBus + InMemoryBus
├── ADR-0020 P1 — CognitiveWorker + DomainWorkerPool
├── ADR-0021 P1 — DECLARE_TOOL 宏
└── ADR-0022 P1 — PluginInfo + PluginLoader 核心

Phase 2 (依赖 Phase 1)
├── ADR-0019 P2 — DSLEngine bus 集成 + NodeExecutor token push
├── ADR-0020 P2 — StateStore + 锁优化
├── ADR-0021 P2 — DEFINE_AGENT 模板
├── ADR-0022 P2 — PDK 集成 (pdk_plugin_info 展开)
└── ADR-0023 P1 — ToolResult 结构体 + call_tool 改造

Phase 3 (集成)
├── ADR-0019 P3 — TUI Chat 应用
├── ADR-0020 P3 — ISandboxController 预留
├── ADR-0021 P3 — PluginLifecycle + .so 加载
├── ADR-0022 P3 — Plugin_v1 生命周期支持
└── ADR-0023 P2 — wrap_tool 兼容 + Event 改造

Phase 4 (ADR-0030 异步架构)
├── ADR-0030 P1 — 引入 Taskflow + async_simple 依赖
├── ADR-0030 P2 — AsyncRuntime 核心 + 桥接层
├── ADR-0030 P3 — TopoScheduler 并行化改造
└── ADR-0030 P4 — Context 线程安全 + 增量 DAG

Phase 5 (EventBus + CostCollector)
├── ADR-0002 P1 — EventBus 核心实现（V2 更新）
├── ADR-0002 P2 — EventBus 与 ADR-0030 集成
├── ADR-0032 P1 — CostCollector 核心实现
└── ADR-0032 P2 — CostCollector 与 BudgetController 集成

Phase 6 (ADR-0031 执行策略 + ADR-0004 V2 安全模型)
├── ADR-0031 P1 — IExecutionPolicy 接口 + 三种模式实现
├── ADR-0031 P2 — 审批流程集成（EventBus + async_simple 协程挂起）
├── ADR-0004 P1 — ToolCategory + ApprovalPolicy + LayerProfile 元数据
├── ADR-0004 P2 — ToolRegistry 元数据注册（保留旧 API 兼容）
└── ADR-0004 P3 — 预览生成器（diff / 命令预览）

Phase 7 (ADR-0033 Session 层级体系)
├── ADR-0033 P1 — Session 类型定义 + ExecutionSession → DagExecutionContext 重命名
├── ADR-0033 P2 — DSLEngine 会话感知接口 + TopoScheduler SubtaskSession 集成
├── ADR-0033 P3 — BudgetController USD 成本扩展（ADR-0032 集成）
└── ADR-0033 P4 — Session 持久化层（可选，延期）

Phase 8 (ADR-0034 IModelRouter)
├── ADR-0034 P1 — 同步路由 + 同步调用（当前）
├── ADR-0034 P2 — 异步/流式支持（ADR-0030 就绪后）
└── ADR-0034 P3 — 批量/舰队模式（ADR-0002 就绪后）

Phase 9 (ADR-0036 混合内核架构总纲)
└── 架构文档 + 接口定义（ICognitiveOrchestrator 等），推进实施

Phase 8 (完成)
├── ADR-0023 P3 — PDK RETURN_SUCCESS/RETURN_ERROR 展开
├── ADR-0023 P4 — 移除向后兼容代码
└── 集成测试 + 性能调优
```

---

## Phase 1: 独立并行

### ADR-0019 P1 — IInteractionBus + InMemoryBus

**目标**: 定义契约层核心接口，提供同进程内存实现。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 1.1 | `src/common/contract/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_contract` |
| 1.2 | `src/common/contract/events.h` | 新建 | [ ] | `EventType`, `Event`, `Token`, `Session` 结构体 |
| 1.3 | `src/common/contract/iinteraction_bus.h` | 新建 | [ ] | `IInteractionBus` 抽象接口 |
| 1.4 | `src/common/contract/inmemory_bus.h` | 新建 | [ ] | `InMemoryBus` 声明 |
| 1.5 | `src/common/contract/inmemory_bus.cpp` | 新建 | [ ] | `InMemoryBus` 实现 |
| 1.6 | `CMakeLists.txt` (根目录) | 修改 | [ ] | 添加 `add_subdirectory(common/contract)` |

**验证**: `InMemoryBus` 单元测试通过，`send_user_message` + `subscribe_tokens` 回调正确。

---

### ADR-0020 P1 — CognitiveWorker + DomainWorkerPool

**目标**: 实现工作线程框架，每个 CognitiveWorker 持有独立 DSLEngine。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 2.1 | `src/common/worker/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_worker` |
| 2.2 | `src/common/worker/task_queue.h` | 新建 | [ ] | `TaskQueue<T>` (std::mutex + std::queue, 非 LockFreeQueue) |
| 2.3 | `src/common/worker/cognitive_worker.h` | 新建 | [ ] | `CognitiveWorker` 声明 |
| 2.4 | `src/common/worker/cognitive_worker.cpp` | 新建 | [ ] | `CognitiveWorker` 实现 |
| 2.5 | `src/common/worker/domain_worker_pool.h` | 新建 | [ ] | `DomainWorkerPool` 声明 |
| 2.6 | `src/common/worker/domain_worker_pool.cpp` | 新建 | [ ] | `DomainWorkerPool` 实现 |
| 2.7 | `CMakeLists.txt` (根目录) | 修改 | [ ] | 添加 `add_subdirectory(common/worker)` |

**约束**:
- CognitiveWorker 持有 `unique_ptr<DSLEngine>`，非 `shared_ptr` (ADR-0003 per-agent 隔离)
- TaskQueue 使用 `std::mutex` + `std::queue`，不使用 LockFreeQueue

**验证**: 两个 CognitiveWorker 可并行运行，各有独立 DSLEngine，互不干扰。

---

### ADR-0021 P1 — DECLARE_TOOL 宏

**目标**: 提供 `DECLARE_TOOL` 宏，将工具注册从 ~20 行样板降到 ~5 行领域逻辑。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 3.1 | `include/hydraforge/pdk.h` | 新建 | [ ] | 统一入口，包含所有子头文件 |
| 3.2 | `include/hydraforge/pdk/tool_macros.h` | 新建 | [ ] | `DECLARE_TOOL` 宏定义 |
| 3.3 | `include/hydraforge/pdk/safe_exec.h` | 新建 | [ ] | `SafeExec` 声明式沙箱 (MVP: 超时+异常) |
| 3.4 | `include/hydraforge/pdk/test_mocks.h` | 新建 | [ ] | `MockSandbox`, `FakeStateStore`, `StubLLM` |
| 3.5 | `include/hydraforge/pdk/agent_templates.h` | 新建 | [ ] | `DEFINE_AGENT`, `AgentLoopType` (骨架，Phase 2 实现) |
| 3.6 | `CMakeLists.txt` (PDK 仓库) | 新建 | [ ] | INTERFACE 库，无需编译 |

**约束**:
- PDK 是独立仓库 `hydraforge-pdk`，不在 `src/` 下
- PDK 只封装通用开发模式，不包含领域逻辑
- PDK 不依赖 Runtime 内部实现，只依赖契约接口

**验证**: PDK 示例编译通过，无 Runtime 依赖。

---

### ADR-0022 P1 — PluginInfo + PluginLoader 核心

**目标**: 定义插件元数据结构和加载器核心。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 4.1 | `src/common/plugin/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_plugin` |
| 4.2 | `src/common/plugin/plugin_info.h` | 新建 | [ ] | `PluginInfo` POD 结构体 |
| 4.3 | `src/common/plugin/plugin_loader.h` | 新建 | [ ] | `PluginLoader` 声明 |
| 4.4 | `src/common/plugin/plugin_loader.cpp` | 新建 | [ ] | `load_all()`, `load_so()`, 版本检查, 路径白名单 |
| 4.5 | `CMakeLists.txt` (根目录) | 修改 | [ ] | 添加 `add_subdirectory(common/plugin)` |

**约束**:
- `PluginInfo` 必须是 POD (纯 C 类型)，可被 `dlsym` 安全读取
- `abi_version` 作为唯一版本门控，`major.minor.patch` 仅用于显示
- `dlopen` 使用 `RTLD_NOW | RTLD_LOCAL`

**验证**: `load_so()` 加载 mock `.so`，`plugin_info` 可读，`register_tools` 被调用。

---

## Phase 2: 依赖 Phase 1

### ADR-0019 P2 — DSLEngine bus 集成 + NodeExecutor token push

**前提**: ADR-0019 P1 (IInteractionBus 可用)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 5.1 | `src/core/engine.h` | 修改 | [ ] | 添加 `std::atomic<std::shared_ptr<IInteractionBus>> bus_` |
| 5.2 | `src/core/engine.cpp` | 修改 | [ ] | `set_interaction_bus()`, `run_async()`, `get_session_context()` |
| 5.3 | `src/modules/executor/node_executor.h` | 修改 | [ ] | 添加 `set_interaction_bus()` |
| 5.4 | `src/modules/executor/node_executor.cpp` | 修改 | [ ] | `execute_dsl_node()` 中调用 `bus_->push_token()` |

**约束**:
- `bus_` 使用 `std::atomic<std::shared_ptr<IInteractionBus>>` (C++20)
- NodeExecutor 在 LLM token 到达时逐 token 推送，非等待完整响应

**验证**: `run_async()` 调用后 token 通过 `InMemoryBus` 实时到达订阅者。

---

### ADR-0020 P2 — StateStore + 锁优化

**前提**: ADR-0020 P1 (CognitiveWorker 可用)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 6.1 | `src/core/state_store.h` | 新建 | [ ] | `StateStore` 线程安全 KV 存储 |
| 6.2 | `src/core/state_store.cpp` | 新建 | [ ] | 实现 (std::shared_mutex) |
| 6.3 | `src/common/tools/registry.h` | 修改 | [ ] | `call_tool()` 用 `shared_lock`, 权限检查在锁外 |
| 6.4 | `CMakeLists.txt` (根目录) | 修改 | [ ] | 链接 `agenticdsl_core` |

**约束**:
- 全局锁顺序: StateStore → ToolRegistry → Worker Queue → InMemoryBus

**验证**: ThreadSanitizer 无 data race。

---

### ADR-0021 P2 — DEFINE_AGENT 模板

**前提**: ADR-0020 P1 (CognitiveWorker 可用)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 7.1 | `include/hydraforge/pdk/agent_templates.h` | 修改 | [ ] | `DEFINE_AGENT` 宏完整实现 |
| 7.2 | `include/hydraforge/pdk/state_access.h` | 新建 | [ ] | `StateReader`, `StateWriter`, `NamespaceGuard` |

**约束**:
- `DEFINE_AGENT` 展开后内部创建 `CognitiveWorker`
- `ON_INTENT` 回调在 CognitiveWorker 的工作线程中执行

**验证**: `DEFINE_AGENT(coding, REACT_LOOP)` 编译通过，`ON_INTENT` 回调被调用。

---

### ADR-0022 P2 — PDK 集成

**前提**: ADR-0021 P1, ADR-0022 P1

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 8.1 | `include/hydraforge/pdk/tool_macros.h` | 修改 | [ ] | `DECLARE_TOOL` 展开时生成 `pdk_plugin_info` + `pdk_register_tools` |
| 8.2 | `src/common/plugin/plugin_loader.cpp` | 修改 | [ ] | 支持 `Plugin_v1` 生命周期结构体 |

**验证**: PDK 编译的 `.so` 被 `PluginLoader::load_so()` 正确加载和注册。

---

### ADR-0023 P1 — ToolResult 结构体 + call_tool 改造

**前提**: 无

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 9.1 | `src/core/types/tool_result.h` | 新建 | [ ] | `ToolResult` 结构体 + `to_json()`/`from_json()` |
| 9.2 | `src/common/tools/registry.h` | 修改 | [ ] | `call_tool()` 签名改为返回 `ToolResult` |
| 9.3 | `src/common/tools/registry.cpp` | 修改 | [ ] | 返回值改造 + 错误码化 |
| 9.4 | `src/modules/executor/node_executor.cpp` | 修改 | [ ] | 消费 `ToolResult`, `if(!result.ok)` 分支 |
| 9.5 | `src/core/types/node.h` | 修改 | [ ] | `execute_tool_call` 签名更新 |

**验证**: 所有现有测试通过，`ToolResult::from_json(to_json()) == 原始值`。

---

## Phase 3: 集成

### ADR-0019 P3 — TUI Chat 应用

**前提**: ADR-0019 P2 (bus 集成完成), ADR-0020 P1 (CognitiveWorker 可用)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 10.1 | `examples/agent_chat/CMakeLists.txt` | 新建 | [ ] | 链接 `agenticdsl_core` + `ftxui` |
| 10.2 | `examples/agent_chat/src/main.cpp` | 新建 | [ ] | 入口: 创建 engine + bus + TUI |
| 10.3 | `examples/agent_chat/src/chat_client.h` | 新建 | [ ] | `ChatClient` 封装 |
| 10.4 | `examples/agent_chat/src/chat_client.cpp` | 新建 | [ ] | `connect()`, `send_message()`, `on_token()` |
| 10.5 | `examples/agent_chat/src/tui.h` | 新建 | [ ] | `TUI` 声明 |
| 10.6 | `examples/agent_chat/src/tui.cpp` | 新建 | [ ] | FTXUI 渲染 (修正后 C++20 语法) |

**约束**:
- FTXUI `vbox({...})` 内不能直接放 `for` 循环，使用 `build_message_elements()`
- 键盘事件使用 `CatchEvent`, 不可用 `Event::CtrlC` 伪事件 (使用 `screen.ExitLoopClosure()`)

**验证**: `./agent_chat` 启动，发送消息后 Token 逐字显示，多轮对话上下文保持。

---

### ADR-0020 P3 — ISandboxController 预留

**前提**: ADR-0023 P1 (ToolResult 可用)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 11.1 | `src/common/sandbox/sandbox_controller.h` | 新建 | [ ] | `ISandboxController` 抽象接口 |
| 11.2 | `src/common/sandbox/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_sandbox` |

**约束**: Phase 3 仅预留接口，不实现 fork/seccomp/cgroups。

**验证**: 接口编译通过。

---

### ADR-0021 P3 — PluginLifecycle + .so 加载

**前提**: ADR-0022 P2 (PDK 集成完成)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 12.1 | `include/hydraforge/pdk/lifecycle.h` | 新建 | [ ] | `PluginLifecycle` 结构体 (on_load, register_tools, on_unload) |
| 12.2 | `include/hydraforge/pdk/logging.h` | 新建 | [ ] | `StructuredLog`, `SpanTracer` |

**验证**: 插件加载后 `on_load` 被调用，卸载时 `on_unload` 被调用。

---

### ADR-0022 P3 — Plugin_v1 生命周期支持

**前提**: ADR-0022 P2

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 13.1 | `src/common/plugin/plugin_loader.h` | 修改 | [ ] | `Plugin_v1` 生命周期钩子 |
| 13.2 | `src/common/plugin/plugin_loader.cpp` | 修改 | [ ] | 加载流程: `on_load` → `register_tools` |

**验证**: `on_load` 返回 false 时插件被拒绝加载。

---

### ADR-0023 P2 — wrap_tool 兼容 + Event 改造

**前提**: ADR-0023 P1, ADR-0019 P1

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 14.1 | `src/common/tools/registry.cpp` | 修改 | [ ] | `wrap_legacy_tool()` 包装默认 3 个工具 |
| 14.2 | `src/common/contract/events.h` | 修改 | [ ] | `Event.content` 改为 `nlohmann::json` |
| 14.3 | `src/common/contract/iinteraction_bus.h` | 修改 | [ ] | `on_tool_result` 回调类型更新 |

**验证**: 旧格式工具通过包装后返回统一信封。

---

## Phase 4: 完成

### ADR-0023 P3 — PDK 集成 RETURN_SUCCESS/RETURN_ERROR

**前提**: ADR-0023 P2

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 15.1 | `include/hydraforge/pdk/tool_macros.h` | 修改 | [ ] | `RETURN_SUCCESS`/`RETURN_ERROR` 展开为 `ToolResult` |

**验证**: PDK 编译的工具返回格式与 Runtime 期望一致。

### ADR-0023 P4 — 移除向后兼容代码

**前提**: ADR-0023 P3

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 16.1 | `src/common/tools/registry.cpp` | 修改 | [ ] | 移除 `wrap_legacy_tool` |
| 16.2 | `src/modules/executor/node_executor.cpp` | 修改 | [ ] | 移除旧格式分支判断 |

**验证**: 无向后兼容代码残留。

---

## Phase 4: ADR-0030 异步架构

### ADR-0030 P1 — 引入 Taskflow + async_simple 依赖

**目标**: 引入双层异步架构所需的第三方库，无破坏性变更。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 17.1 | `external/taskflow/` | 引入 | [ ] | Taskflow v4.0 (header-only) |
| 17.2 | `external/async_simple/` | 引入 | [ ] | async_simple v1.4 (编译 ~10 个 .cpp) |
| 17.3 | `CMakeLists.txt` (根目录) | 修改 | [ ] | `add_subdirectory(external/taskflow)` |
| 17.4 | `CMakeLists.txt` (根目录) | 修改 | [ ] | `add_subdirectory(external/async_simple)` |
| 17.5 | `CMakeLists.txt` (根目录) | 修改 | [ ] | `target_link_libraries(agenticdsl_core PRIVATE async_simple)` |

**约束**:
- Taskflow v4.0 要求 C++20 编译器（GCC ≥ 11, Clang ≥ 12）
- async_simple 编译选项：`-DASYNC_SIMPLE_ENABLE_TESTS=OFF`

**验证**: `cmake ..` 配置成功，`make -j` 编译通过。

---

### ADR-0030 P2 — AsyncRuntime 核心 + 桥接层

**目标**: 实现 AsyncRuntime 统一入口和桥接层。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 18.1 | `src/common/async/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_async` |
| 18.2 | `src/common/async/async_runtime.h` | 新建 | [ ] | `AsyncRuntime` 声明 |
| 18.3 | `src/common/async/async_runtime.cpp` | 新建 | [ ] | `AsyncRuntime` 实现 |
| 18.4 | `src/common/async/bridge.h` | 新建 | [ ] | 桥接工具：`await_taskflow()`, `await_future()` |
| 18.5 | `src/common/async/bridge.cpp` | 新建 | [ ] | 桥接实现（使用 `FutureAwaiter`，禁止忙等待） |
| 18.6 | `tests/test_async_runtime.cpp` | 新建 | [ ] | 桥接层单元测试 |
| 18.7 | `CMakeLists.txt` (根目录) | 修改 | [ ] | 添加 `add_subdirectory(common/async)` |

**约束**:
- `await_future()` 必须使用 `async_simple::coro::FutureAwaiter`，禁止 `while + Yield` 轮询
- `AsyncRuntime` 持有 `tf::Executor` 和 `async_simple::Executor`，生命周期由 `DSLEngine` 管理
- 桥接层代码集中在 3 个函数内，便于测试

**验证**:
- `test_async_runtime.cpp` 通过：协程可 `co_await` Taskflow 图完成
- `test_async_runtime.cpp` 通过：`await_future` 无忙等待（CPU 占用 < 1%）

---

### ADR-0030 P3 — TopoScheduler 并行化改造

**目标**: 将 TopoScheduler 从单线程串行改为 Taskflow 并行执行。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 19.1 | `src/modules/scheduler/topo_scheduler.h` | 修改 | [ ] | 添加 `execute_parallel()` 和 `execute_async()` |
| 19.2 | `src/modules/scheduler/topo_scheduler.cpp` | 修改 | [ ] | 实现 Taskflow 并行路径 |
| 19.3 | `src/modules/scheduler/topo_scheduler.cpp` | 修改 | [ ] | `execute_sync()` 实现为 `sync_await(execute_async(...))` |
| 19.4 | `src/modules/scheduler/topo_scheduler.cpp` | 修改 | [ ] | Fork/Join 改为真并行（Taskflow Subflow） |
| 19.5 | `tests/test_scheduler_parallel.cpp` | 新建 | [ ] | 并行调度单元测试 |

**约束**:
- `execute_sync()` 必须实现为 `sync_await(execute_async(...))`，禁止维护两套独立路径
- 并行节点执行前必须创建 Context 分支副本（`Context::fork()`）
- Join 时通过 `ContextMergePolicy` 合并结果

**验证**:
- 现有同步测试继续通过（`execute_sync` 兼容性）
- 并行测试：Fork/Join 分支真正并行执行（ThreadSanitizer 验证）

---

### ADR-0030 P4 — Context 线程安全 + 增量 DAG

**目标**: 解决并行执行的两个关键阻塞问题。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 20.1 | `src/core/types/context.h` | 修改 | [ ] | 添加 `fork()` / `merge()` 方法 |
| 20.2 | `src/core/types/context.cpp` | 新建 | [ ] | `Context::fork()` 实现（深拷贝或 COW） |
| 20.3 | `src/core/types/context.cpp` | 新建 | [ ] | `Context::merge()` 实现（按 `ContextMergePolicy`） |
| 20.4 | `src/modules/scheduler/topo_scheduler.cpp` | 修改 | [ ] | 增量 DAG 更新：`append_node()` 替代 `build_dag()` 全量重建 |
| 20.5 | `src/modules/scheduler/topo_scheduler.h` | 修改 | [ ] | `node_map_` 使用 `std::shared_mutex` 保护 |
| 20.6 | `tests/test_context_thread_safe.cpp` | 新建 | [ ] | Context 分支/合并测试 |
| 20.7 | `tests/test_incremental_dag.cpp` | 新建 | [ ] | 增量 DAG 更新测试 |

**约束**:
- `Context::fork()` 必须创建独立副本，禁止共享可变状态
- `node_map_` 读多写少，使用 `std::shared_mutex`（读锁并行，写锁独占）
- 增量更新仅添加新节点和边，不重建整个图

**验证**:
- ThreadSanitizer 无 data race
- 动态图添加后，现有节点指针保持有效

---

## Phase 5: EventBus + CostCollector

### ADR-0002 P1 — EventBus 核心实现（V2 更新）

**目标**: 实现全局 MPMC 有界队列 EventBus，支持 DispatchMode 分发。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 21.1 | `src/common/event/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_event` |
| 21.2 | `src/common/event/event_types.h` | 新建 | [ ] | 事件类型目录（命名空间组织） |
| 21.3 | `src/common/event/bounded_queue.h` | 新建 | [ ] | MPMC 有界队列模板 |
| 21.4 | `src/common/event/event_bus.h` | 新建 | [ ] | `IEventBus` + `InMemoryEventBus` 声明 |
| 21.5 | `src/common/event/event_bus.cpp` | 新建 | [ ] | `InMemoryEventBus` 实现 |
| 21.6 | `tests/test_event_bus.cpp` | 新建 | [ ] | 单元测试：多线程 push/pop |
| 21.7 | `CMakeLists.txt` (根目录) | 修改 | [ ] | 添加 `add_subdirectory(common/event)` |

**约束**:
- 全局队列容量 4096，按优先级丢弃旧事件
- Critical 事件永不丢弃（阻塞等待空位）
- 独立分发线程，从队列取事件分发到订阅者

**验证**:
- ThreadSanitizer 无 data race
- 1000 events/秒下无丢包（Critical）

---

### ADR-0002 P2 — EventBus 与 ADR-0030 集成

**目标**: EventBus 支持三种 DispatchMode，与 AsyncRuntime 集成。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 22.1 | `src/common/event/event_bus.cpp` | 修改 | [ ] | 实现 `dispatch_to_subscriber()` 三种模式 |
| 22.2 | `src/core/engine.cpp` | 修改 | [ ] | LLM/Tool 调用完成后发布事件 |
| 22.3 | `src/modules/executor/node_executor.cpp` | 修改 | [ ] | 发布 `NodeExecuted` 事件 |
| 22.4 | `src/modules/trace/trace_exporter.cpp` | 修改 | [ ] | 改为 EventBus 消费者（Inline 模式） |
| 22.5 | `tests/test_dispatch_mode.cpp` | 新建 | [ ] | 测试三种 DispatchMode |

**约束**:
- `Inline` 模式：handler 执行时间 < 100μs
- `TaskflowAsync` 模式：提交到 Taskflow 计算池
- `CoroSpawn` 模式：spawn 到 async_simple 协程调度器

**验证**:
- `TaskflowAsync` 消费者在 Taskflow 线程中执行
- `CoroSpawn` 消费者在协程调度器中执行

---

### ADR-0032 P1 — CostCollector 核心实现

**目标**: 实现成本收集器，订阅 LLMCallFinished 事件，计算成本。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 23.1 | `src/common/cost/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_cost` |
| 23.2 | `src/common/cost/pricing.h` | 新建 | [ ] | `ModelPricing` 结构体 |
| 23.3 | `src/common/cost/pricing.cpp` | 新建 | [ ] | `PricingTable` 实现 |
| 23.4 | `src/common/cost/cost_collector.h` | 新建 | [ ] | `CostCollector` + `SessionCost` 声明 |
| 23.5 | `src/common/cost/cost_collector.cpp` | 新建 | [ ] | `CostCollector` 实现 |
| 23.6 | `src/common/cost/llm_pricing.json` | 新建 | [ ] | 默认定价配置 |
| 23.7 | `tests/test_cost_collector.cpp` | 新建 | [ ] | 单元测试 |
| 23.8 | `CMakeLists.txt` (根目录) | 修改 | [ ] | 添加 `add_subdirectory(common/cost)` |

**约束**:
- 未知模型：记录警告，跳过成本计算（不中断执行）
- 缓存折扣：`cache_hit=true` 时成本 = 原价 × `cached_input_discount`
- 线程安全：`std::shared_mutex` 保护 `session_costs_`

**验证**:
- 已知 Token 数 × 定价 = 预期成本
- ThreadSanitizer 无 data race

---

### ADR-0032 P2 — CostCollector 与 BudgetController 集成

**目标**: 在 DSLEngine 中集成 CostCollector，与 BudgetController 互补。

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 24.1 | `src/core/engine.h` | 修改 | [ ] | 添加 `CostCollector` 成员 |
| 24.2 | `src/core/engine.cpp` | 修改 | [ ] | 创建 CostCollector，加载定价配置 |
| 24.3 | `src/common/llm/llama_adapter.cpp` | 修改 | [ ] | LLM 调用完成后发布 `LLMCallFinished` |
| 24.4 | `src/core/engine.cpp` | 修改 | [ ] | 超预算时发布 `BudgetExceeded` 事件 |
| 24.5 | `tests/test_budget_integration.cpp` | 新建 | [ ] | 集成测试 |

**约束**:
- BudgetController 硬限制（超限终止执行）
- CostCollector 软限制（超预算告警 + 模式切换建议）
- 两者独立运行，不互相阻塞

**验证**:
- BudgetController 超限 → 执行终止
- CostCollector 超预算 → `BudgetExceeded` 事件发布

---

### ADR-0033 P1 — Session 类型定义 + ExecutionSession 重命名

**目标**: 创建三层会话类型，将 ExecutionSession 重命名为 DagExecutionContext，无行为变更。

**前提**: 无

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 25.1 | `src/core/types/session.h` | 新建 | [ ] | `UserSession`、`TaskSession`、`SubtaskSession` 定义 |
| 25.2 | `src/core/types/session.cpp` | 新建 | [ ] | Session 方法实现 |
| 25.3 | `src/modules/scheduler/execution_session.h` | 修改 | [ ] | 类重命名为 `DagExecutionContext`，添加 `using ExecutionSession = DagExecutionContext;` 别名 |
| 25.4 | `src/modules/scheduler/execution_session.cpp` | 修改 | [ ] | 同步更新类名 |
| 25.5 | `src/modules/scheduler/topo_scheduler.h` | 修改 | [ ] | 新增 `BranchExecutionResult` 结构体 |
| 25.6 | `CMakeLists.txt` (根目录) | 修改 | [ ] | 添加 `session.cpp` 编译单元 |

**约束**:
- `UserSession.messages` 仅追加写（ADR-0023），无 mutator 暴露
- `TaskSession` 持有 `Context`（`nlohmann::json`），不替换底层类型
- `DagExecutionContext` 别名保持旧代码兼容

**验证**:
- 编译通过，无行为变更
- `using ExecutionSession = DagExecutionContext;` 编译通过

---

### ADR-0033 P2 — DSLEngine 会话感知接口 + TopoScheduler 集成

**目标**: DSLEngine 新增带会话的重载，TopoScheduler 分支执行返回 SubtaskSession。

**前提**: ADR-0033 P1, ADR-0031 P1 (IExecutionPolicy), ADR-0030 P2 (AsyncRuntime)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 26.1 | `src/core/engine.h` | 修改 | [ ] | 新增 `run(UserSession&, const std::string&)` 重载 |
| 26.2 | `src/core/engine.cpp` | 修改 | [ ] | 实现会话感知版本：创建/复用 TaskSession、失败模式检查 |
| 26.3 | `src/modules/scheduler/topo_scheduler.cpp` | 修改 | [ ] | `execute_single_branch()` 返回 `BranchExecutionResult`，自动归档 SubtaskSession |
| 26.4 | `tests/test_session.cpp` | 新建 | [ ] | Session 层级单元测试 |

**约束**:
- TaskSession 失败计数 < 3 时复用同一会话（IPER retry）
- 失败计数 ≥ 3 时自动创建新 TaskSession（通知 TUI，不阻塞）
- 所有 SubtaskSession 结果（成功+失败）归档到 TaskSession

**验证**:
- `run(user_sess, msg)` 正确创建/复用 TaskSession
- 模拟 3 次失败，确认第 4 次创建新会话
- fork/join 执行后 `task_sess.subtask_sessions()` 包含所有分支结果

---

### ADR-0033 P3 — BudgetController USD 成本扩展

**目标**: 扩展 BudgetController 支持 USD 成本追踪（ADR-0032 集成）。

**前提**: ADR-0033 P2, ADR-0032 P2 (CostCollector)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 27.1 | `src/modules/budget/budget_controller.h` | 修改 | [ ] | 新增 `set_cost_limit()`、`try_consume_cost()`、`current_cost()` 接口 |
| 27.2 | `src/modules/budget/budget_controller.cpp` | 修改 | [ ] | 实现 USD 成本累计与检查 |
| 27.3 | `tests/test_budget_usd.cpp` | 新建 | [ ] | USD 成本单元测试 |

**约束**:
- 使用 `double` 存储成本，精度阈值 $0.001
- BudgetController 硬限制（超限终止执行）
- CostCollector 软限制（超预算告警），两者独立

**验证**:
- `try_consume_cost()` 正确累计 `current_cost_`
- 成本超限触发 `BudgetExceeded` 事件

---

### ADR-0033 P4 — Session 持久化层（可选，延期）

**目标**: Session 持久化到磁盘，支持进程重启恢复。

**前提**: ADR-0033 P3

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 28.1 | `src/core/persistence/session_persistence.h` | 新建 | [ ] | 抽象持久化接口 `ISessionPersistence` |
| 28.2 | `src/core/persistence/json_session_store.h` | 新建 | [ ] | JSON 文件存储实现 |
| 28.3 | `src/core/persistence/json_session_store.cpp` | 新建 | [ ] | 序列化/反序列化实现 |
| 28.4 | `src/core/types/session.cpp` | 修改 | [ ] | 新增 `save()` / `load()` 方法 |
| 28.5 | `tests/test_session_persistence.cpp` | 新建 | [ ] | 持久化单元测试 |

**约束**:
- MVP 阶段可延期，默认内存存储
- UserSession 限制 `task_sessions` 最大历史数量（默认 100）

**验证**:
- 序列化/反序列化后会话状态一致
- `ctest -R "persistence" --output-on-failure`

---

## Phase 8: IModelRouter (ADR-0034)

### ADR-0034 P1 — 同步路由 + 同步调用

**目标**: 实现最小化模型路由，非破坏性扩展现有 ILLMProvider。

**前提**: 无

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 29.1 | `src/common/llm/llm_types.h` | 修改 | [ ] | 添加 `ModelCapability` 结构体 |
| 29.2 | `src/common/llm/llm_types.h` | 修改 | [ ] | `ILLMProvider` 新增 `available_models()` + `is_model_available()` 默认实现 |
| 29.3 | `src/common/llm/model_router.h` | 新建 | [ ] | `IModelRouter` 接口 + `RoutingContext` |
| 29.4 | `src/common/llm/model_registry.h` | 新建 | [ ] | `ModelRegistry` 类 |
| 29.5 | `src/common/llm/default_model_router.h` | 新建 | [ ] | `DefaultModelRouter` 实现 |
| 29.6 | `src/modules/executor/node_executor.cpp` | 修改 | [ ] | `execute_llm_call()` 集成路由层 |
| 29.7 | `tests/test_model_router.cpp` | 新建 | [ ] | 路由单元测试 |

**约束**:
- 纯同步接口，零 async_simple 依赖
- `ILLMProvider` 扩展为默认实现，不破坏现有代码
- 路由延迟 < 1ms

**验证**:
- 现有 `LlamaAdapter` / `HttpLLMAdapter` 无需修改即可编译
- 配置 task_type 映射后，相同任务类型路由到相同模型
- 预算不足时自动切换低成本模型

---

### ADR-0034 P2 — 异步/流式支持（ADR-0030 就绪后）

**目标**: 添加异步路由和流式调用支持。

**前提**: ADR-0034 P1, ADR-0030 P2 (AsyncRuntime)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 30.1 | `src/common/llm/model_router.h` | 修改 | [ ] | 新增 `route_async()` 方法 |
| 30.2 | `src/common/llm/llm_call_coordinator.h` | 新建 | [ ] | `LLMCallCoordinator` 并发管理 |
| 30.3 | `src/common/llm/streaming_model_router.h` | 新建 | [ ] | 流式路由支持 |

**约束**:
- 仅在 ADR-0030 实现后启动
- 保持 Phase 1 同步接口兼容

**验证**:
- `route_async()` 可在协程中调用
- 流式响应正确路由到对应模型

---

### ADR-0034 P3 — 批量/舰队模式（ADR-0002 就绪后）

**目标**: 支持批量路由和舰队模式并行调用。

**前提**: ADR-0034 P2, ADR-0002 P2 (EventBus)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 31.1 | `src/common/llm/model_router.h` | 修改 | [ ] | 新增 `route_batch()` 方法 |
| 31.2 | `src/common/llm/llm_call_coordinator.cpp` | 修改 | [ ] | 集成 EventBus 发布 LLMCallFinished |
| 31.3 | `tests/test_fleet_routing.cpp` | 新建 | [ ] | 舰队模式路由测试 |

**约束**:
- 仅在 ADR-0002 实现后启动
- Fleet Mode 子任务强制路由到低成本模型

**验证**:
- 批量路由返回正确模型列表
- EventBus 正确发布 LLMCallFinished 事件

---

## DomainAgentDescriptor（延迟，待 ADR-0022 + ADR-0034 P1 就绪）

**状态**: 延迟。不创建 ADR-0035，改为实现 `IDomainAgent` C++ 抽象类（静态链接 MVP）。

### 前置条件
| # | 依赖 | 原因 |
|---|------|------|
| 1 | ADR-0022（插件加载） | 需要 `PluginLoader` 基础设施就绪后，才能设计动态加载的 Descriptor |
| 2 | ADR-0034 P1（ModelRegistry） | `IDomainAgent::create_router()` 需要 ModelRouterRegistry |

### 设计方向（已锁定，不创建 ADR）
- 使用 **C++ 抽象类 `IDomainAgent`**，非 C-API struct
- 静态链接（MVP），延迟到 ADR-0022 后再支持动态加载
- 生命周期钩子为同步虚函数（`on_session_start`, `on_session_end`, `on_mode_change`）
- `create_router()` 返回 `unique_ptr<IModelRouter>`，调用方管理生命周期
- 领域前缀冲突：拒绝重复注册

---

## 附录 A: 跨 ADR 依赖表

| 任务 | 依赖 | 被依赖 |
|------|------|--------|
| ADR-0019 P1 | — | ADR-0019 P2, ADR-0020 P1 |
| ADR-0020 P1 | ADR-0019 P1 (IInteractionBus) | ADR-0020 P2, ADR-0021 P2 |
| ADR-0021 P1 | — | ADR-0021 P2, ADR-0022 P2 |
| ADR-0022 P1 | — | ADR-0022 P2, ADR-0022 P3 |
| ADR-0023 P1 | — | ADR-0023 P2, ADR-0020 P3 |
| ADR-0030 P1 | — | ADR-0030 P2 |
| ADR-0030 P2 | ADR-0030 P1 | ADR-0030 P3 |
| ADR-0030 P3 | ADR-0030 P2, ADR-0030 P4 | — |
| ADR-0030 P4 | ADR-0030 P1 | ADR-0030 P3 |
| ADR-0019 P2 | ADR-0019 P1 | ADR-0019 P3 |
| ADR-0020 P2 | ADR-0020 P1 | — |
| ADR-0021 P2 | ADR-0020 P1, ADR-0021 P1 | — |
| ADR-0022 P2 | ADR-0021 P1, ADR-0022 P1 | ADR-0022 P3 |
| ADR-0023 P2 | ADR-0023 P1, ADR-0019 P1 | ADR-0023 P3 |
| ADR-0019 P3 | ADR-0019 P2, ADR-0020 P1 | — |
| ADR-0020 P3 | ADR-0023 P1 | — |
| ADR-0021 P3 | ADR-0022 P2 | — |
| ADR-0022 P3 | ADR-0022 P2 | — |
| ADR-0023 P3 | ADR-0023 P2, ADR-0021 P1 | ADR-0023 P4 |
| ADR-0023 P4 | ADR-0023 P3 | — |
| ADR-0002 P1 | — | ADR-0002 P2 |
| ADR-0002 P2 | ADR-0002 P1, ADR-0030 P2 | — |
| ADR-0032 P1 | ADR-0002 P1 | ADR-0032 P2 |
| ADR-0032 P2 | ADR-0032 P1, ADR-0030 P2 | — |
| ADR-0031 P1 | — | ADR-0031 P2 |
| ADR-0031 P2 | ADR-0031 P1, ADR-0002 P1 | — |
| ADR-0004 P1 | — | ADR-0004 P2 |
| ADR-0004 P2 | ADR-0004 P1 | ADR-0004 P3 |
| ADR-0004 P3 | ADR-0004 P2 | — |
| ADR-0033 P1 | — | ADR-0033 P2 |
| ADR-0033 P2 | ADR-0033 P1, ADR-0031 P1, ADR-0030 P2 | ADR-0033 P3 |
| ADR-0033 P3 | ADR-0033 P2, ADR-0032 P2 | ADR-0033 P4 |
| ADR-0033 P4 | ADR-0033 P3 | — |
| ADR-0034 P1 | — | ADR-0034 P2 |
| ADR-0034 P2 | ADR-0034 P1, ADR-0030 P2 | ADR-0034 P3 |
| ADR-0034 P3 | ADR-0034 P2, ADR-0002 P2 | — |
| ADR-0036 | ADR-0033, ADR-0031, ADR-0034, ADR-0030, ADR-0019 | — |

## 附录 B: 关键约束检查清单

实施前必须确认以下约束被遵守：

| 约束 | 来源 | 检查点 |
|------|------|--------|
| DSLEngine 每 CognitiveWorker 独立实例 | ADR-0003, ADR-0020 | CognitiveWorker 持有 `unique_ptr<DSLEngine>` |
| `bus_` 使用 `std::atomic<shared_ptr>` | ADR-0019 | DSLEngine::bus_ 声明 |
| FTXUI vbox 内无原始 for 循环 | ADR-0019 (审查修正) | `build_message_elements()` 预构建 vector |
| LockFreeQueue 不用于 MVP | ADR-0020 (审查修正) | TaskQueue 使用 `std::mutex` + `std::queue` |
| ToolRegistry 使用 shared_lock | ADR-0020 (审查修正) | `call_tool` 读锁 + 锁外权限检查 |
| `abi_version` 作为版本门控 | ADR-0022 | PluginInfo.abi_version 对比 CURRENT_ABI_VERSION |
| `RTLD_LOCAL` 符号隔离 | ADR-0022 | dlopen flags |
| 信封格式 `{"ok","data","meta"}` | ADR-0023 | ToolResult.to_json() 输出 |
| 错误码 `ERR_<DOMAIN>.<SUB>` | ADR-0023 | error_code 字符串格式 |
| `await_future` 禁止忙等待 | ADR-0030 | 使用 `FutureAwaiter`，禁止 `while + Yield` 轮询 |
| Context 分支隔离 | ADR-0030 | `fork()` 创建独立副本，禁止共享可变状态 |
| 增量 DAG 更新 | ADR-0030 | `append_node()` 替代 `build_dag()` 全量重建 |
| `node_map_` 线程安全 | ADR-0030 | 使用 `std::shared_mutex` 保护 |
| EventBus 全局队列 | ADR-0002 V2 | 全局 MPMC 队列，非 Per-Agent 多队列 |
| 背压丢弃旧事件 | ADR-0002 V2 | 按优先级丢弃旧事件，Critical 永不丢弃 |
| CostCollector 订阅模式 | ADR-0032 | TaskflowAsync 模式，在计算池执行 |
| 成本与计数分离 | ADR-0032 | BudgetController 硬限制，CostCollector 软限制 |
| Plan 模式审批写入操作 | ADR-0031 | `requires_approval()` 检查 `category != ReadOnly` |
| Agent 模式遵循工具策略 | ADR-0031 | `requires_approval()` 检查 `approval.requires_approval_in_agent` |
| YOLO 模式安全底线 | ADR-0031 | `requires_approval()` 检查 `approval.force_approval_always` |
| Layer Profile 层级检查 | ADR-0031 | `check_layer_permission()` 限制 L3 只能调用 ReadOnly |
| 审批超时处理 | ADR-0031 | 5 分钟超时返回 `ApprovalTimeout` |
| 权限检查锁外执行 | ADR-0031 | 锁内仅哈希表查找，锁外做权限检查 |
| YOLO 切换额外确认 | ADR-0031 | 切换到 YOLO 需要用户确认安全警告 |
| 旧 API 兼容保留 | ADR-0004 V2 | `register_tool(name, func)` 作为 deprecated 包装 |
| 工具元数据完整注册 | ADR-0004 V2 | 新 API `register_tool(ToolMetadata, func)` |
| UserSession.messages 追加写 | ADR-0033 | `const std::vector<ToolResult>& messages() const` 无 mutator |
| TaskSession 失败计数 | ADR-0033 | 3 次失败后自动创建新 TaskSession |
| DagExecutionContext 别名兼容 | ADR-0033 | `using ExecutionSession = DagExecutionContext;` 编译通过 |
| SubtaskSession 结果归档 | ADR-0033 | fork/join 后所有分支结果归档到 TaskSession |
| BudgetController USD 精度 | ADR-0033 | 使用 `double`，精度阈值 $0.001 |
| ILLMProvider 非破坏性扩展 | ADR-0034 | `available_models()` / `is_model_available()` 为默认实现 |
| 路由延迟 | ADR-0034 | `route()` 调用 < 1ms |
| 同步优先 | ADR-0034 | Phase 1 纯同步，零 async_simple 依赖 |
| 模型注册不冲突 | ADR-0034 | `register_model()` 返回 `false` 并记录警告 |
