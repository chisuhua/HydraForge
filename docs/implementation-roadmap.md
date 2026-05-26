# Implementation Roadmap

> 基于 ADR-0019 ~ ADR-0023 的实施追踪，记录所有确认的代码改动。
>
> **最后更新**: 2026-05-25 | **状态约定**: `[ ]` 待实施 / `[x]` 已完成 / `[~]` 进行中

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

Phase 4 (完成)
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

## 附录 A: 跨 ADR 依赖表

| 任务 | 依赖 | 被依赖 |
|------|------|--------|
| ADR-0019 P1 | — | ADR-0019 P2, ADR-0020 P1 |
| ADR-0020 P1 | ADR-0019 P1 (IInteractionBus) | ADR-0020 P2, ADR-0021 P2 |
| ADR-0021 P1 | — | ADR-0021 P2, ADR-0022 P2 |
| ADR-0022 P1 | — | ADR-0022 P2, ADR-0022 P3 |
| ADR-0023 P1 | — | ADR-0023 P2, ADR-0020 P3 |
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
