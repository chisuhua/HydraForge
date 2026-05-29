# Implementation Roadmap

> 全项目实施追踪：综合 ADR-0001~0036、Implementation Slices、AgenticDSL 自举路径。
>
> **最后更新**: 2026-05-29 | **状态约定**: `[ ]` 待实施 / `[x]` 已完成 / `[~]` 进行中 / `[!]` 阻塞

---

## 项目全景

```
当前基线 (已实现)
└── ADR-0001~0018 — 8 核心模块 + Common 组件 ~4,532 行
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
│   ├── ADR-0031 IExecutionPolicy (Plan/Agent/YOLO)
│   ├── ADR-0004 V2 元数据 + 审批
│   ├── ADR-0032 CostCollector
│   └── ADR-0033 Session 层级体系
│
    ▼
Phase 4 — 模型路由与混合内核
│   ├── ADR-0034 IModelRouter
│   └── ADR-0036 混合内核架构 (取代 multi-domain-agent-architecture.md)
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
| **测试** | ⚠️ 部分可用 | test_basic 可运行，其余因构建超时未验证 |

### 紧急问题 (需立即修复)

| # | 问题 | 状态 | 说明 |
|---|------|------|------|
| 1 | `tests/test_basic.cpp` 合并冲突 | ✅ 已修复 | 选择 HEAD 版本 |
| 2 | `tests/CMakeLists.txt` llama.cpp 引用 | ✅ 已修复 | 已移除已删除的依赖路径 |
| 3 | 其他测试二进制构建超时 | ⚠️ 待排查 | 可能 yaml-cpp 或链接耗时过长 |
| 4 | 无 `.clang-format` / `.clang-tidy` | [ ] 待添加 | 建议基础配置 |

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
**工期**: 3-5 天

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
| M4.8 | `examples/slice_01_tool_call/main.cpp` | 新建 | [ ] | 入口示例 |
| M4.9 | `examples/slice_01_tool_call/CMakeLists.txt` | 新建 | [ ] | 构建配置 |

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

**验证**: `./slice_01_tool_call` 输出完整调用链 (LLM→Tool→结果)

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

### ADR-0031 — IExecutionPolicy (3 模式)

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 10.1 | `src/common/policy/CMakeLists.txt` | 新建 | [ ] | 静态库 `agenticdsl_policy` |
| 10.2 | `src/common/policy/execution_policy.h` | 新建 | [ ] | `IExecutionPolicy` 接口 |
| 10.3 | `src/common/policy/execution_policy.cpp` | 新建 | [ ] | Plan/Agent/YOLO 三种模式 |
| 10.4 | `src/common/tools/tool_coordinator.h` | 新建 | [ ] | `call_tool_with_policy()` |
| 10.5 | `src/common/tools/tool_coordinator.cpp` | 新建 | [ ] | 审批 + 5min 超时 |

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
| 14.3 | `src/common/llm/llm_call_coordinator.h` | 新建 | [ ] | 并发管理 + 舰队模式 |

### ADR-0036 — 混合内核架构总纲

**前提**: ADR-0033, ADR-0031, ADR-0034, ADR-0030, ADR-0019

| # | 文件 | 操作 | 状态 | 说明 |
|---|------|------|------|------|
| 15.1 | `src/core/cognitive_orchestrator.h` | 完善 | [ ] | `ICognitiveOrchestrator` 完整接口 |

> 本阶段主要完善 Phase 0 已搭建的三层调用链，添加完整生命周期管理。

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
等级      范围                    工期      收益
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🚨 紧急    修复构建+测试              <1天    解除阻塞
┃
🥇 P0      Phase 0 MVP              1-2周    验证核心架构
┃          ├─ 0.1 云端LLM集成       3-4天    可调用云端模型
┃          ├─ 0.2 三层调用链验证     3-5天    端到端通路
┃          └─ 0.3 最小契约层         2-3天    接口基础
┃
🥈 P1      Phase 1 智能体层          3-4周    多Agent+插件
┃
🥉 P2      Phase 2 异步架构          2-3周    并行执行
┃
🥉 P3      Phase 3 执行策略+安全     2-3周    审批+预算
┃
🥉 P4      Phase 4 模型路由+内核     2-3周    舰队模式
┃
📅 远期    Phase 5 自举服务化          —       API服务
```

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
| 核心库构建 (agenticdsl_core) | ✅ 100% | `make agenticdsl_core` 通过 |
| test_basic 编译 | ✅ 通过 | 3 passed / 2 failed (预存在) |
| 全量测试 (ctest) | ⚠️ 仅 test_basic 可运行 | 其他待构建 |
| 合并冲突 | ✅ 已修复 | test_basic.cpp |
| llama.cpp 引用 | ✅ 已移除 | CMakeLists.txt |
| 代码格式化 | ❌ 无配置 | 无 `.clang-format` / `.clang-tidy` |
| 文档与代码一致 | ⚠️ 基本一致 | ADR-0019~0036 已记录但未实现 |
