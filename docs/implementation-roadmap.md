# Implementation Roadmap

> 全项目实施追踪：综合 ADR-0001~0036、Implementation Slices、AgenticDSL 自举路径。
>
> **最后更新**: 2026-06-03 | **状态约定**: `[ ]` 待实施 / `[x]` 已完成 / `[~]` 进行中 / `[!]` 阻塞

---

## 项目全景

```
当前基线 (已实现)
└── ADR-0001~0018 — 8 核心模块 + Common 组件 ~4,532 行
    │
    ▼
Pre-Phase — 核心接口定义 (仅头文件)
│   ├── ICognitiveOrchestrator 接口
│   ├── IExecutionPolicy 接口
│   └── Session 类型前置声明
│
    ▼
Slice 00 — 基础设施验证
│   └── Taskflow + async_simple external/ 引入 + CMake 配置
│
    ▼
Phase 0 — MVP: 云端LLM集成 + 三层调用链验证
│   ├── 0.1 云端 LLM 适配器 (OpenAI/Claude)
│   ├── 0.2 三层调用链 (orchestrator → LLM → tool)
│   └── 0.3 最小契约层 (IInteractionBus + ToolResult)
│
    ▼
Phase 1 — 智能体层
│   ├── ADR-0019 IInteractionBus + TUI Chat
│   ├── ADR-0020 CognitiveWorker + 线程隔离
│   ├── ADR-0021 PDK + DECLARE_TOOL
│   ├── ADR-0022 PluginLoader
│   └── ADR-0023 ToolResult 标准化
│
    ▼
Phase 2 — 异步架构
│   └── ADR-0030 Taskflow + async_simple + 并行调度
│
    ▼
Phase 3 — 执行策略与安全
│   ├── ADR-0031 IExecutionPolicy (P1-P4 完整分解)
│   ├── ADR-0031 ToolCoordinator (call_tool_with_policy 中间件)
│   ├── ADR-0004 V2 元数据 + 审批
│   ├── ADR-0032 CostCollector
│   └── ADR-0033 Session 层级体系
│
    ▼
Phase 4 — 模型路由与混合内核
│   ├── ADR-0034 IModelRouter
│   └── ADR-0036 混合内核架构
│
    ▼
Phase 4.5 — MVP 清理与技术债务消除
│   ├── 替换 SimpleCognitiveOrchestrator 为正式实现
│   ├── 移除 MockLLMProvider 和硬编码路由
│   └── examples/ 目录职责梳理
│
    ▼
Phase 5 — 自举与服务化 (远期)
    ├── 阶段 1: DSL 可编程参数 + Session 隔离
    ├── 阶段 2: 质量评估闭环 + 服务分层
    └── 阶段 3: 完全自举 + 推理 API 服务化
```

---

## 代码状态真实断面

| 区域 | 状态 | 规模 |
|------|------|------|
| **8 个核心模块** (parser/scheduler/executor/context/budget/trace/library/system) | ✅ 已实现 | ~4,532 行 |
| **Common 组件** (llm/tools/utils) | ✅ 已实现 | 含 HttpAdapter + LlamaAdapter |
| **ADR-0019~0036 新增组件** (contract/worker/plugin/async/event/cost/sandbox/pdk/cognitive) | ❌ 零代码 | 13 个目录全部不存在 |
| **外部依赖** (Taskflow / async_simple) | ❌ 未引入 | 依赖 llama.cpp 已移除 |
| **测试** | ✅ 全部通过 | 12/12 全部通过 (2026-06-03 验证) |

### 紧急问题 (需立即修复)

| # | 问题 | 状态 | 说明 |
|---|------|------|------|
| 1 | `tests/test_basic.cpp` 合并冲突 | ✅ 已修复 | 选择 HEAD 版本 |
| 2 | `tests/CMakeLists.txt` llama.cpp 引用 | ✅ 已修复 | 已移除已删除的依赖路径 |
| 3 | 其他测试二进制构建超时 | ✅ 已解决 | 2026-06-03 验证 12/12 全部构建+运行通过 |
| 4 | 无 `.clang-format` / `.clang-tidy` | [ ] 待添加 | 建议基础配置（非阻塞） |

---

## Pre-Phase — 核心接口定义 (仅头文件)

> **来源**: Eric 审查建议 — ICognitiveOrchestrator、IExecutionPolicy、Session 类型应在任何实施前定义
> **目标**: 确保所有 Phase 有统一编译目标，Slice 01 可直接继承
> **工期**: 0.5 天
> **原则**: 仅 `.h` 头文件，无 `.cpp` 实现，无行为

本阶段定义的接口将在后续 Phase 中逐步实现：

### 接口清单

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| P0.1 | `include/hydraforge/cognitive/icognitive_orchestrator.h` | 新建 | [ ] | `ICognitiveOrchestrator` 抽象接口 (process 方法) |
| P0.2 | `include/hydraforge/policy/iexecution_policy.h` | 新建 | [ ] | `IExecutionPolicy` 抽象接口 (requires_approval 方法) |
| P0.3 | `include/hydraforge/types/session_fwd.h` | 新建 | [ ] | `UserSession`, `TaskSession`, `SubtaskSession` 前置声明 |
| P0.4 | `CMakeLists.txt` (根) | 修改 | [ ] | 添加 `include/hydraforge/` 到头文件搜索路径 |

### 关键接口

```cpp
// icognitive_orchestrator.h
class ICognitiveOrchestrator {
public:
    virtual ~ICognitiveOrchestrator() = default;
    virtual void process(const std::string& session_id,
                         std::function<void(ExecutionResult)> on_complete) = 0;
};

// iexecution_policy.h
class IExecutionPolicy {
public:
    virtual ~IExecutionPolicy() = default;
    virtual bool requires_approval(const ToolMetadata& meta,
                                   const ToolCallContext& ctx) const = 0;
};
```

**验证**: 3 个头文件编译通过 | 后续 Phase 的类可直接继承

---

## Slice 00 — 基础设施验证

> **来源**: Eric 审查建议 — Taskflow/async_simple 引入和 CMake 验证
> **目标**: 引入异步依赖并验证编译通过，为 Phase 2 铺路
> **前提**: 无 (可独立并行)
> **工期**: 1-2 天

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| S0.1 | `external/taskflow/` | 引入 | [ ] | Taskflow v4.0 (header-only, git submodule) |
| S0.2 | `external/async_simple/` | 引入 | [ ] | async_simple v1.4 |
| S0.3 | `CMakeLists.txt` (根) | 修改 | [ ] | `add_subdirectory(external/taskflow)` |
| S0.4 | `CMakeLists.txt` (根) | 修改 | [ ] | `add_subdirectory(external/async_simple)` |
| S0.5 | `external/CMakeLists.txt` | 修改 | [ ] | `-DASYNC_SIMPLE_ENABLE_TESTS=OFF` |
| S0.6 | `tests/test_taskflow_bridge.cpp` | 新建 | [ ] | 最小桥接测试: Taskflow 图可被 co_await |

**验证**: `cmake .. && make -j$(nproc)` 编译通过 | 桥接测试执行成功

---

## Phase 0 — MVP: Foundation for Agentic Layer

> **目标**: 以最小代码改动，实现"云端 LLM 可调用 + 三层架构可验证"的端到端通路。
>
> **设计原则** (来自 `docs/implementation-slices.md`)：
> 1. 每切片验证一个架构假设
> 2. 最小代码量，不做工程完备
> 3. 可独立验证，有明确"通/不通"标准
>
> **工期估算**: 1-2 周 (3 个并行 Track)

```
Track 0.1 ─ 云端 LLM 集成 (独立)
Track 0.2 ─ 三层调用链验证 (依赖 Track 0.1)
Track 0.3 ─ 最小契约层 (与 0.1/0.2 并行)
```

---

### Track 0.1: 云端 LLM 集成

**来源**: `docs/agenticdsl/implementation/phase-0-implementation.md` + `docs/agenticdsl/implementation/self-bootstrapping-path.md`
**目标**: 实现 OpenAI/Claude 兼容的云端 LLM 适配器，建立路由器
**工期**: 3-4 天

#### Step 0.1.1: 接口统一 (0.5 天)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| M1.1 | `src/common/llm/llm_config.h` | 新建 | [ ] | 统一配置结构 (合并 LLMConfig + LLMParams) |
| M1.2 | `src/common/llm/llm_adapter.h` | 修改 | [ ] | 标记 `ILLMAdapter` 为 `[[deprecated]]` |
| M1.3 | `src/common/llm/llm_types.h` | 修改 | [ ] | 导入统一配置，移除重复定义 |

#### Step 0.1.2: CloudLLMAdapter (1-2 天)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| M2.1 | `src/common/llm/cloud_llm_adapter.h` | 新建 | [ ] | CloudLLMAdapter 接口 |
| M2.2 | `src/common/llm/openai_adapter.h` | 新建 | [ ] | OpenAI 适配器声明 |
| M2.3 | `src/common/llm/openai_adapter.cpp` | 新建 | [ ] | OpenAI API 调用实现 |
| M2.4 | `src/common/llm/anthropic_adapter.h` | 新建 | [ ] | Anthropic 适配器声明 |
| M2.5 | `src/common/llm/anthropic_adapter.cpp` | 新建 | [ ] | Claude API 调用实现 |
| M2.6 | `src/common/llm/http_client.h` | 新建 | [ ] | HTTP 客户端封装 |
| M2.7 | `src/common/llm/http_client.cpp` | 新建 | [ ] | HTTP 实现 (复用 cpp-httplib) |

#### Step 0.1.3: LLMRouter (1 天)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| M3.1 | `src/common/llm/llm_router.h` | 新建 | [ ] | `LLMRouter` 路由决策 |
| M3.2 | `src/common/llm/llm_router.cpp` | 新建 | [ ] | 云端/本地自动路由 |
| M3.3 | `src/core/engine.cpp` | 修改 | [ ] | 注册云端推理工具 |

**验证**: `cloud_adapter->generate("你好")` 返回内容 | 路由选择正确后端

---

### Track 0.2: 三层调用链验证 (Slice 01)

**来源**: `docs/implementation-slices.md` Slice 01
**目标**: 验证 ADR-0036 混合内核架构核心假设：基座层→认知层→领域层能通
**前提**: Track 0.1 (需要 LLM 能力)
**工期**: 5-7 天（含接口定义、MockLLMProvider、错误路径验证和可运行示例）

#### 验证场景

```
用户输入 "读取文件 main.cpp 的内容"
→ orchestrator.process(session_id, callback)
  → LLM 解析意图 → 识别出 "code::read_file"
  → ToolRegistry.call("code::read_file", {path: "main.cpp"})
  → ToolResult{ok:true, data: 文件内容}
→ callback(result)
→ 终端输出文件内容
```

#### 涉及文件

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| M4.1 | `src/common/llm/llm_types.h` | 修改 | [ ] | 添加 `ModelCapability` + `available_models()` |
| M4.2 | `src/common/llm/model_registry.h` | 新建 | [ ] | `ModelRegistry` 多模型注册 |
| M4.3 | `src/common/llm/model_router.h` | 新建 | [ ] | `IModelRouter` + `RoutingContext` |
| M4.4 | `src/common/llm/default_model_router.h` | 新建 | [ ] | `DefaultModelRouter` (首个可用模型) |
| M4.5 | `src/common/llm/default_model_router.cpp` | 新建 | [ ] | 路由逻辑 |
| M4.6 | `src/cognitive/simple_orchestrator.h` | 新建 | [ ] | `SimpleCognitiveOrchestrator` MVP |
| M4.7 | `src/cognitive/simple_orchestrator.cpp` | 新建 | [ ] | 硬编码 ReAct: LLM→解析→回调 |
| M4.8 | `src/common/llm/mock_llm_provider.h` | 新建 | [ ] | `MockLLMProvider` (离线开发/CI 用) |
| M4.9 | `examples/slice_01_tool_call/main.cpp` | 新建 | [ ] | 入口示例 (支持 `--mock` / `--live` 模式) |
| M4.10 | `examples/slice_01_tool_call/CMakeLists.txt` | 新建 | [ ] | 构建配置 |

**离线开发支持**:
```cpp
// MockLLMProvider：无网络也能验证链路逻辑
class MockLLMProvider : public ILLMProvider {
    Result<GenerationResult, LLMError> generate(...) override {
        // 返回预定义 JSON，模拟 LLM 解析结果
        return GenerationResult{
            .content = R"({"tool": "code::read_file", "args": {"path": "main.cpp"}})"
        };
    }
};
```

每个示例支持两种运行模式：
- `--mock`：使用 MockLLMProvider，验证链路逻辑（秒级完成，用于 CI/离线开发）
- `--live`：使用真实 API，验证端到端行为

#### 关键接口

```cpp
// ICognitiveOrchestrator（认知层）
class ICognitiveOrchestrator {
    virtual void process(const std::string& session_id,
                         std::function<void(ExecutionResult)> on_complete) = 0;
};

// SimpleCognitiveOrchestrator：单轮 ReAct
// process() 内部：
//   1. 调 LLM → 解析出 tool_call
//   2. registry.call(tool_call)
//   3. callback(result)
```

**验证标准**:
- ✅ `examples/slice_01_tool_call --mock` 输出完整调用链 (MockLLM→Tool→结果)，< 1 秒
- ✅ `examples/slice_01_tool_call --live` 输出完整调用链 (真实 LLM→Tool→结果)
- ✅ `tests/test_slice_01.cpp` 单元测试通过 (模拟 orchestrator 各组件)
- ✅ 错误路径验证：LLM 超时 → orchestrator 返回 `LLMError.Timeout`
- ✅ 错误路径验证：Tool 不存在 → ToolRegistry 返回 `ToolResult{ok:false}`
- ✅ 三层调用链延迟 < 500ms (mock 模式测量链路开销)

---

### Track 0.3: 最小契约层

**来源**: ADR-0019 P1 (简化), ADR-0023 P1 (简化)
**目标**: 定义契约层核心接口，为智能体层提供基础
**工期**: 2-3 天 (与 Track 0.1/0.2 并行)

#### Step 0.3.1: IInteractionBus + InMemoryBus (精简版)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| M5.1 | `src/common/contract/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_contract` |
| M5.2 | `src/common/contract/events.h` | 新建 | [ ] | `EventType`, `Event`, `Token`, `Session` 基础结构 |
| M5.3 | `src/common/contract/iinteraction_bus.h` | 新建 | [ ] | `IInteractionBus` 抽象接口 |
| M5.4 | `src/common/contract/inmemory_bus.h` | 新建 | [ ] | `InMemoryBus` 声明 |
| M5.5 | `src/common/contract/inmemory_bus.cpp` | 新建 | [ ] | `InMemoryBus` 实现 (mutex + queue) |

#### Step 0.3.2: ToolResult 标准化 (精简版)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| M6.1 | `src/core/types/tool_result.h` | 新建 | [ ] | `ToolResult` 结构体 + `to_json()`/`from_json()` |
| M6.2 | `src/common/tools/registry.h` | 修改 | [ ] | `call_tool()` 签名改为返回 `ToolResult` |
| M6.3 | `src/common/tools/registry.cpp` | 修改 | [ ] | 返回值改造 + 错误码化 |

**约束**:
- MVP 阶段 `bus_` 使用 `std::mutex` (非 `std::atomic<std::shared_ptr>`)
- `ToolResult` 信封格式: `{"ok": bool, "data": ..., "meta": {...}}`
- 错误码格式: `ERR_<DOMAIN>.<SUB>`

**验证**: `InMemoryBus` 单元测试通过 | `ToolResult::from_json(to_json()) == 原始值`

---

## Phase 1 — 智能体层

> 以下为 MVP 之后的完整智能体层，代码量较大 (>2000 行)。
>
> **前提**: Phase 0 Track 0.3 (契约层可用)

### ADR-0019 P2 — DSLEngine bus 集成 + NodeExecutor token push

**前提**: ADR-0019 P1 (IInteractionBus 可用)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 1.1 | `src/core/engine.h` | 修改 | [ ] | 添加 `std::atomic<std::shared_ptr<IInteractionBus>> bus_` |
| 1.2 | `src/core/engine.cpp` | 修改 | [ ] | `set_interaction_bus()`, `run_async()`, `get_session_context()` |
| 1.3 | `src/modules/executor/node_executor.h` | 修改 | [ ] | 添加 `set_interaction_bus()` |
| 1.4 | `src/modules/executor/node_executor.cpp` | 修改 | [ ] | `execute_dsl_node()` 中逐 token 推送 |

---

### ADR-0020 P1 — CognitiveWorker + DomainWorkerPool

**前提**: ADR-0019 P1

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 2.1 | `src/common/worker/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_worker` |
| 2.2 | `src/common/worker/task_queue.h` | 新建 | [ ] | `TaskQueue<T>` (std::mutex + std::queue) |
| 2.3 | `src/common/worker/cognitive_worker.h` | 新建 | [ ] | `CognitiveWorker` 声明 |
| 2.4 | `src/common/worker/cognitive_worker.cpp` | 新建 | [ ] | 持有 `unique_ptr<DSLEngine>` |
| 2.5 | `src/common/worker/domain_worker_pool.h` | 新建 | [ ] | `DomainWorkerPool` 声明 |
| 2.6 | `src/common/worker/domain_worker_pool.cpp` | 新建 | [ ] | 实现 |

---

### ADR-0021 P1 — DECLARE_TOOL 宏 (PDK)

**前提**: 无 (独立仓库)

> **IDomainAgent 与 PDK 的关系**: ADR-0021 的 `DECLARE_TOOL` 宏是 `IDomainAgent`（ADR-0036 定义）的**简化入口**。
> - `IDomainAgent` = C++ 抽象类，提供完整生命周期钩子（`on_session_start`、`create_router` 等）
> - `DECLARE_TOOL` = 宏封装，快速注册工具而无须实现完整 Agent 接口
> - MVP 阶段使用 `DECLARE_TOOL`；Phase 4 后可通过实现 `IDomainAgent` 获得完整生命周期控制
> - 两者可共存：一个 Agent 可以同时有 `DECLARE_TOOL` 注册的工具和 `IDomainAgent` 管理的工具

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 3.1 | `include/hydraforge/pdk.h` | 新建 | [ ] | 统一入口 |
| 3.2 | `include/hydraforge/pdk/tool_macros.h` | 新建 | [ ] | `DECLARE_TOOL` 宏 (~20行→~5行) |
| 3.3 | `include/hydraforge/pdk/safe_exec.h` | 新建 | [ ] | `SafeExec` 声明式沙箱 (超时+异常) |
| 3.4 | `include/hydraforge/pdk/test_mocks.h` | 新建 | [ ] | `MockSandbox`, `FakeStateStore`, `StubLLM` |
| 3.5 | `include/hydraforge/pdk/agent_templates.h` | 新建 | [ ] | `DEFINE_AGENT` 骨架 |

---

### ADR-0022 P1 — PluginInfo + PluginLoader 核心

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 4.1 | `src/common/plugin/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_plugin` |
| 4.2 | `src/common/plugin/plugin_info.h` | 新建 | [ ] | `PluginInfo` POD (C 兼容) |
| 4.3 | `src/common/plugin/plugin_loader.h` | 新建 | [ ] | `PluginLoader` 声明 |
| 4.4 | `src/common/plugin/plugin_loader.cpp` | 新建 | [ ] | `load_all()`, `load_so()`, 版本检查 |

---

### ADR-0023 P2~P4 — ToolResult 完善

**前提**: Phase 0 M6.x 基础 ToolResult

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 5.1 | `src/common/tools/registry.cpp` | 修改 | [ ] | `wrap_legacy_tool()` 兼容包装 |
| 5.2 | `src/common/contract/events.h` | 修改 | [ ] | `Event.content` 改为 `nlohmann::json` |
| 5.3 | `include/hydraforge/pdk/tool_macros.h` | 修改 | [ ] | `RETURN_SUCCESS`/`RETURN_ERROR` 展开 |

---

## Phase 2 — 异步架构 (ADR-0030)

> **前提**: Phase 1 智能体层完成
> **状态**: ✅ 已批准，待实施

### ADR-0030 P1 — 引入依赖

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 6.1 | `external/taskflow/` | 引入 | [ ] | Taskflow v4.0 (header-only) |
| 6.2 | `external/async_simple/` | 引入 | [ ] | async_simple v1.4 |
| 6.3 | `CMakeLists.txt` (根) | 修改 | [ ] | `add_subdirectory(external/taskflow)` + `add_subdirectory(external/async_simple)` |

**约束**: `await_future()` 必须使用 `FutureAwaiter`，**禁止** `while + Yield` 轮询

### ADR-0030 P2 — AsyncRuntime 核心 + 桥接层

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 7.1 | `src/common/async/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_async` |
| 7.2 | `src/common/async/async_runtime.h` | 新建 | [ ] | `AsyncRuntime` 声明 |
| 7.3 | `src/common/async/async_runtime.cpp` | 新建 | [ ] | 实现 (持 Executor) |
| 7.4 | `src/common/async/bridge.h` | 新建 | [ ] | `await_taskflow()`, `await_future()` |
| 7.5 | `src/common/async/bridge.cpp` | 新建 | [ ] | 桥接实现 |

### ADR-0030 P3 — TopoScheduler 并行化

**前提**: ADR-0030 P2, P4

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 8.1 | `src/modules/scheduler/topo_scheduler.h` | 修改 | [ ] | `execute_parallel()` / `execute_async()` |
| 8.2 | `src/modules/scheduler/topo_scheduler.cpp` | 修改 | [ ] | Taskflow 并行路径 + Fork/Join Subflow |

### ADR-0030 P4 — Context 线程安全 + 增量 DAG

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 9.1 | `src/core/types/context.h` | 修改 | [ ] | 添加 `fork()` / `merge()` |
| 9.2 | `src/core/types/context.cpp` | 新建 | [ ] | `fork()` 深拷贝 / `merge()` 策略 |
| 9.3 | `src/modules/scheduler/topo_scheduler.cpp` | 修改 | [ ] | `append_node()` 增量 DAG |

---

## Phase 3 — 执行策略与安全

> **前提**: Phase 1 (ToolResult), Phase 2 (AsyncRuntime for 协程挂起)

### ADR-0031 — IExecutionPolicy (P1-P4 完整分解)

#### P1: 接口定义 + AgentModePolicy 实现

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 10.1 | `src/common/policy/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_policy` |
| 10.2 | `src/common/policy/execution_policy.h` | 新建 | [ ] | `IExecutionPolicy` 抽象接口 |
| 10.3 | `src/common/policy/execution_policy.cpp` | 新建 | [ ] | `AgentModePolicy` 实现 |

**AgentModePolicy 行为**: 写入操作需审批 (`category != ReadOnly`)

#### P2: PlanModePolicy + YoloModePolicy 实现

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 10.4 | `src/common/policy/execution_policy.cpp` | 修改 | [ ] | 添加 `PlanModePolicy` 实现 |
| 10.5 | `src/common/policy/execution_policy.cpp` | 修改 | [ ] | 添加 `YoloModePolicy` 实现 |

**PlanModePolicy 约束**:
- Plan 模式：写入操作需审批 (`category != ReadOnly`)
- YOLO 模式：安全底线 (`approval.force_approval_always` 不跳过)
- 切换到 YOLO 需要用户确认安全警告

#### P3: ToolCoordinator — `call_tool_with_policy()` 中间件

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 10.6 | `src/common/tools/tool_coordinator.h` | 新建 | [ ] | `ToolCoordinator` 类 (`call_tool_with_policy`) |
| 10.7 | `src/common/tools/tool_coordinator.cpp` | 新建 | [ ] | 审批链: 政策检查→审批→执行→结果 |

**职责归属**: ToolCoordinator 是基座层中间件，位于 ToolRegistry 上层：

```
ToolRegistry.call(name, params)  ← 原始 API（跳过策略）
       ↑
ToolCoordinator.call_tool_with_policy(name, params, policy)
       ↑
NodeExecutor / CognitiveWorker   ← 调用方
```

#### P4: 模式切换事件 + 审批超时

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 10.8 | `src/common/policy/execution_policy.h` | 修改 | [ ] | 添加 `ModeSwitchEvent`、`ApprovalResult` 类型 |
| 10.9 | `src/common/tools/tool_coordinator.cpp` | 修改 | [ ] | 审批超时处理 (5min → `ApprovalTimeout`) |
| 10.10 | `tests/test_execution_policy.cpp` | 新建 | [ ] | 三模式策略单元测试 |

**验证**: 每种模式审批决策正确 | 超时自动拒绝 | 模式切换可追踪

---

### ADR-0031 一致性映射

Eric 审查指出的问题——`call_tool_with_policy()` 之前在 Roadmap 中无对应位置——已通过 P3 修复。与 `implementation-slices.md` Slice 03 的对应关系：

| Slice 03 文件 | Roadmap 位置 | 说明 |
|--------------|-------------|------|
| `tool_metadata.h` | Phase 3 ADR-0004 V2 | ToolCategory 定义 |
| `execution_policy.h` | Phase 3 ADR-0031 P1 | IExecutionPolicy 接口 |
| `tool_coordinator.h/cpp` | Phase 3 ADR-0031 P3 | call_tool_with_policy 中间件 |
| `slice_03_approval` | Phase 3 (端到端验证) | examples/ 示例 |

---

### ADR-0004 V2 — 安全模型升级

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 11.1 | `src/common/tools/tool_metadata.h` | 新建 | [ ] | `ToolCategory`, `ApprovalPolicy`, `LayerProfile` |
| 11.2 | `src/common/tools/registry.h` | 修改 | [ ] | `register_tool(ToolMetadata, func)` 新 API |
| 11.3 | `src/common/tools/registry.cpp` | 修改 | [ ] | 保留旧 API 兼容 |

### ADR-0032 — CostCollector

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 12.1 | `src/common/cost/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_cost` |
| 12.2 | `src/common/cost/pricing.h` | 新建 | [ ] | `ModelPricing` |
| 12.3 | `src/common/cost/cost_collector.h` | 新建 | [ ] | `CostCollector` + `SessionCost` |
| 12.4 | `src/common/cost/cost_collector.cpp` | 新建 | [ ] | 订阅 EventBus 计算成本 |

### ADR-0033 — Session 层级体系

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 13.1 | `src/core/types/session.h` | 新建 | [ ] | `UserSession`, `TaskSession`, `SubtaskSession` |
| 13.2 | `src/modules/scheduler/execution_session.h` | 修改 | [ ] | 重命名 `DagExecutionContext`, 别名兼容 |
| 13.3 | `src/modules/budget/budget_controller.h` | 修改 | [ ] | `set_cost_limit()`, `try_consume_cost()` |

---

## Phase 4 — 模型路由与混合内核

### ADR-0034 — IModelRouter

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 14.1 | `src/common/llm/model_registry.h` | 新建 | [ ] | (若 Phase 0 未创建) |
| 14.2 | `src/common/llm/model_router.h` | 完善 | [ ] | 异步路由 `route_async()` |
| 14.3 | `src/common/llm/llm_call_coordinator.h` | 新建 | [ ] | 并发管理 |

### ADR-0025 — 舰队模式并行执行

> **来源**: `docs/implementation-slices.md` Slice 04
> **目标**: 支持 16 路并行 LLM 调用，全部完成后聚合结果
> **前提**: ADR-0034 P2 (异步路由), ADR-0030 P3 (Taskflow 并行)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 14.4 | `src/common/llm/fleet_orchestrator.h` | 新建 | [ ] | `FleetOrchestrator` 并行调度 |
| 14.5 | `src/common/llm/fleet_orchestrator.cpp` | 新建 | [ ] | 分片→并行→聚合流程 |
| 14.6 | `examples/slice_04_fleet/main.cpp` | 新建 | [ ] | 舰队模式端到端示例 |
| 14.7 | `tests/test_fleet_orchestrator.cpp` | 新建 | [ ] | 并行调用单元测试 |

### ADR-0036 — 混合内核架构总纲

**前提**: ADR-0033, ADR-0031, ADR-0034, ADR-0030, ADR-0019

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 15.1 | `src/core/cognitive_orchestrator.h` | 完善 | [ ] | `ICognitiveOrchestrator` 完整接口 |

> 本阶段主要完善 Phase 0 已搭建的三层调用链，添加完整生命周期管理。

---

## Phase 4.5 — MVP 清理与技术债务消除

> **来源**: Eric 审查建议 — MVP 临时代码应标注清理计划
> **目标**: 在正式功能开发前清理 MVP 阶段的 hack 代码
> **工期**: 1-2 天

### 清理清单

| # | 任务 | 涉及文件 | 说明 |
|---|------|---------|------|
| 16.1 | 替换 `SimpleCognitiveOrchestrator` | `src/cognitive/simple_orchestrator.*` | 替换为完整的 `CognitiveOrchestrator` 实现 |
| 16.2 | 移除 `MockLLMProvider` | `src/common/llm/mock_llm_provider.h` | (如正式实现已覆盖) |
| 16.3 | 替换硬编码路由策略 | `src/common/llm/default_model_router.cpp` | 替换为配置驱动的路由 |
| 16.4 | 移除 `TODO(mvp):` 标记 | 全局搜索 | 逐个确认并替换 |
| 16.5 | 归档 `examples/agent_loop/` | `examples/archive/` | 标记为 legacy，正式环路接管后归档 |
| 16.6 | 目录职责分离 | `examples/` vs `tests/` | examples/ 给人看，tests/ 给 CI |

### examples/ 与 tests/ 职责约定

```
examples/                        # 人工可运行的演示（开发者用 ./build.sh && ./example 验证）
├── agent_basic/                 # 已有：基础 DSL 执行
├── agent_simple/                # 已有：简化 DSL 执行
├── agent_loop/                  # MVP 遗留：多轮 Agent 循环（→ 16.5 归档）
├── slice_01_tool_call/          # 新增：三层调用链验证
├── slice_02_routing/            # 新增：多模型路由验证
├── slice_03_approval/           # 新增：审批流程验证
└── slice_04_fleet/              # 新增：舰队并行验证

tests/                           # 自动化测试（CI 运行，无需人工介入）
├── test_tool_registry.cpp       # 已有
├── test_model_router.cpp        # 新增（Slice 02 的自动化版本）
├── test_execution_policy.cpp    # 新增（Slice 03 的自动化版本）
└── test_parallel_executor.cpp   # 新增（Slice 04 的自动化版本）
```

### MVP 代码标记约定

所有 MVP 阶段的临时代码使用统一标记：

```cpp
// TODO(mvp): 替换为正式的 IPER 循环实现
// 当前仅为验证三层调用链而硬编码
class SimpleCognitiveOrchestrator : public ICognitiveOrchestrator {
    ...
};
```

**验证**: grep "TODO(mvp)" 计数归零 | 所有 MVP 标记被清理

---

## Phase 5 — AgenticDSL 自举与服务化 (远期)

**来源**: `docs/agenticdsl/implementation/self-bootstrapping-path.md`
**前提**: Phase 1~4 完成 (完整智能体基础设施)

### 阶段 1: DSL 可编程参数 + Session 隔离

| 步骤 | 目标 | 涉及文件 | 工期 |
|------|------|---------|------|
| Step 0 | Lazy ModuleState (json scope nesting) | `execution_session.h/cpp`, `node_executor.cpp` | 1-2 天 |
| Step 1 | Session Registry + Session Vars | `session_registry.h/cpp`, `engine.h` | 2-3 天 |
| Step 2 | YIELD / STREAM 节点 | `node.h`, `node_executor.h/cpp`, `topo_scheduler.cpp` | 2-3 天 |
| — | 推理标准库 7 子图 (并行) | `lib/inference/` 完善 | 并行 |

### 阶段 2: 子任务隔离

| 步骤 | 目标 | 工期 |
|------|------|------|
| Step 3 | Fork 语义扩展 + Checkpoint | 2-3 天 |
| Step 4 | Sub-Graph 隔离 | 3-4 天 |

### 阶段 3: 完全自举

| 步骤 | 目标 | 工期 |
|------|------|------|
| Step 5 | 静态分析 + Graph IR | 3-4 天 |
| Step 6 | 调度优化 + 服务化 | 4-5 天 |

---

## 实施优先级总表

```
等级      范围                                工期       收益
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚨 紧急    修复构建+测试                          <1天      解除阻塞
┃
🥇 P0.5    Pre-Phase 接口定义                    0.5天     统一编译目标
┃
🥇 P0.6    Slice 00 基础设施                     1-2天     异步依赖就绪
┃
🥇 P0      Phase 0 MVP                          1-2周     验证核心架构
┃          ├─ 0.1 云端LLM集成                   3-4天     可调用云端模型
┃          ├─ 0.2 三层调用链验证                 5-7天†    端到端通路 + 错误路径
┃          └─ 0.3 最小契约层                     2-3天     接口基础
┃
🥈 P1      Phase 1 智能体层                      3-4周     多Agent+插件
┃
🥉 P2      Phase 2 异步架构                      2-3周     并行执行
┃
🥉 P3      Phase 3 执行策略+安全                 2-3周     审批+预算
┃
🥉 P4      Phase 4 模型路由+内核                 2-3周     舰队模式
┃
🥉 P4.5    Phase 4.5 MVP清理                     1-2天     消除技术债务
┃
📅 远期    Phase 5 自举服务化                      —        API服务
```

† 工期较原估算 (3-5天) 上调：含接口定义、MockLLMProvider、错误路径验证和可运行示例。

---

## 架构层关系说明

### EventBus 与 IInteractionBus 的分层关系

> **来源**: Eric 审查 — ADR-0002 议题 2 决策
> **状态**: 已锁定，本路线图基于此关系设计

```
IInteractionBus (Phase 0 Track 0.3)
    │  高层抽象：面向 NodeExecutor / CognitiveWorker
    │  提供：send_user_message(), subscribe_tokens(), on_tool_result()
    │  实现：Phase 0 用 std::mutex + queue（MVP）
    │
    ▼ 下层转发
EventBus (Phase 2 ADR-0002 V2)
    │  底层传输：面向系统组件
    │  提供：MPMC 有界队列、优先级、DispatchMode
    │  实现：全局 MPMC 队列 + 独立分发线程
    │
    ▼ 物理层
线程/协程 (Phase 2 ADR-0030)
```

**关键设计点**:
- IInteractionBus 基于 EventBus 构建，而非替代它
- Phase 0 MVP 用 `std::mutex` 实现，暂不依赖 EventBus
- Phase 2 时 IInteractionBus 后端切换为 EventBus（接口不变，实现变化）
- 迁移方式：`InMemoryBus(Mutex)` → `InMemoryBus(EventBus)`，对上层透明

### Phase 通用完成标准

每个 Phase 完成后必须满足以下标准，而不仅是"代码存在"：

| 标准 | 说明 |
|------|------|
| ✅ 编译通过 | `make -j$(nproc)` 无错误 |
| ✅ 单元测试全绿 | `ctest --output-on-failure` 100% pass |
| ✅ 可运行示例 | `examples/slice_N_xxx/` 可执行并输出正确 |
| ✅ LSP 诊断清洁 | 无 error / warning |
| ✅ 错误路径覆盖 | 至少覆盖 2 种异常场景（超时、无效输入等） |
| ✅ 无 MVP 残留 | 无 `TODO(mvp)` 标记残留 |

---

## 附录 A: 跨文档 MVP 概念索引

本表显示各文档中"MVP"/"Phase 0"定义的对应关系：

| 文档 | 概念 | 对应本路线图位置 |
|------|------|-----------------|
| `docs/implementation-slices.md` | Slice 01 三层调用链 | Phase 0 Track 0.2 |
| `docs/agenticdsl/implementation/phase-0-implementation.md` | 云端 LLM 集成 (4 Steps) | Phase 0 Track 0.1 |
| `docs/agenticdsl/implementation/self-bootstrapping-path.md` | 阶段 0: 云端集成 + 质量保障 | Phase 0 Track 0.1 |
| `docs/agenticdsl/implementation-roadmap/01-roadmap.md` (IP-001) | 阶段 1: 核心自举能力 | Phase 5 阶段 1 |
| `docs/adr/adr-0019-iinteraction-bus-mvp.md` | IInteractionBus + TUI Chat | Phase 0 Track 0.3 + Phase 1 |
| `docs/archive/Roadmap.md` | v3.2 对话记忆 MVP | 远期/参考 |
| `docs/compiler/plan-phase1-foundation.md` | Compiler 阶段 1 | 独立编译器项目 |

---

## 附录 B: 跨 ADR 依赖表

| 任务 | 依赖 | 被依赖 |
|------|------|--------|
| Phase 0 Track 0.1 | — | Track 0.2 |
| Phase 0 Track 0.2 | Track 0.1 | — |
| Phase 0 Track 0.3 | — | Phase 1 |
| ADR-0019 P1 (Track 0.3) | — | ADR-0019 P2, ADR-0020 P1 |
| ADR-0020 P1 | ADR-0019 P1 | ADR-0020 P2, ADR-0021 P2 |
| ADR-0021 P1 | — | ADR-0021 P2, ADR-0022 P2 |
| ADR-0022 P1 | — | ADR-0022 P2, ADR-0022 P3 |
| ADR-0023 P1 (Track 0.3) | — | ADR-0023 P2, ADR-0020 P3 |
| ADR-0030 P1 | — | ADR-0030 P2 |
| ADR-0030 P2 | ADR-0030 P1 | ADR-0030 P3 |
| ADR-0030 P3 | ADR-0030 P2, ADR-0030 P4 | — |
| ADR-0030 P4 | ADR-0030 P1 | ADR-0030 P3 |
| ADR-0031 P1 | — | ADR-0031 P2 |
| ADR-0032 P1 | ADR-0002 P1 | ADR-0032 P2 |
| ADR-0033 P1 | — | ADR-0033 P2 |
| ADR-0034 P1 | — | ADR-0034 P2 |
| ADR-0036 | ADR-0033, ADR-0031, ADR-0034, ADR-0030, ADR-0019 | — |

---

## 附录 C: 项目健康检查清单

| 检查项 | 状态 | 说明 |
|--------|------|------|
| LSP 诊断 (src/core/) | ✅ 清洁 | 无错误 |
| 核心库构建 (agenticdsl_core) | ✅ 100% | `make agenticdsl_core` 通过 (2026-06-03) |
| test_basic 编译 | ✅ 通过 | 5/5 (2026-06-03) |
| 全量测试 (ctest) | ✅ 12/12 通过 | `ctest` 全部 100% pass (2026-06-03) |
| 合并冲突 | ✅ 已修复 | test_basic.cpp |
| llama.cpp 引用 | ✅ 已移除 | CMakeLists.txt |
| 代码格式化 | ❌ 无配置 | 无 `.clang-format` / `.clang-tidy` |
| 文档与代码一致 | ⚠️ 基本一致 | ADR-0019~0036 已记录但未实现 |

---

## 附录 D: 相关文档同步清单

本路线图变更时，以下文档可能需要同步更新：

| 文档 | 同步条件 | 上次同步 |
|------|---------|:--------:|
| `docs/implementation-slices.md` | Phase 变更、Slice 增删 | 2026-05-30 |
| `docs/prephase-slice00-phase0.md` | Pre-Phase / Slice 00 / Phase 0 任务修改 | 2026-05-30 |
| `docs/adr/relationships.md` | ADR 依赖变更、新 ADR 创建 | 2026-05-28 |
| `docs/agenticdsl/implementation-roadmap/01-roadmap.md` | Phase 5 任务变更 | 2026-05-22 |
| `docs/agenticdsl/implementation/self-bootstrapping-path.md` | Phase 5 阶段调整 | 2026-05-22 |
| `docs/agenticdsl/implementation/phase-0-implementation.md` | Track 0.1 任务变更 | 2026-05-23 |
| `docs/archive/Roadmap.md` | 仅归档，非常规同步 | — |

> 同步原则：每完成一个 Phase，检查上表中"同步条件"匹配的文档是否需要更新。
