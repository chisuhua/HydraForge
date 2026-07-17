# ADR-0060: Agent 组合协议与声明式编排

## 状态

✅ Approved (2026-07-16, 架构评审确认)
✅ Updated (2026-07-16, 扩展为 6 种协作模式 — 同步 RPC、异步 RPC、pub/sub、子 Agent、并行、流式)

## 领域

Agent-as-Plugin 架构 / Agent 协作

## 关联

- [ADR-0019 — IInteractionBus](../adr-0019-iinteraction-bus-mvp.md) — 事件总线
- [ADR-0033 — Session Hierarchy](../adr-0033-session-hierarchy.md) — 会话分层
- [ADR-0053 — AgentDescriptor](./adr-0053-agent-descriptor-interface.md) — Agent 注册
- [ADR-0054 — Capability Discovery](./adr-0054-capability-discovery.md) — Agent 发现
- [ADR-0058 — Tool Schema Validation](./adr-0058-tool-schema-validation.md) — 契约校验

## 背景

### 问题

v1 的 ADR-0060 只定义了四种**调用模型**（工具调用链、DSL 子图、事件驱动、Wasm 混合），是**触发方式**而不是**协作模式**。当 Agent 之间需要：

- 并行协作（两个 Agent 同时工作）
- 子 Agent 委派（一个 Agent 创建并监控子 Agent）
- 异步回调（发送请求后不阻塞等待回调）
- 流式通道（持续的 LLM token 流）

调用方需要手工拼接 `call_tool` + `subscribe` + `execute_parallel`。缺乏统一的协作模式抽象。

此外，用户明确要求**进程内和进程间的协作方式必须统一**——Agent 开发者只需学一次 API，无论目标 Agent 是本地还是远程。

### 目标

定义 6 种**协议无关的协作模式**，进程内和进程间共享同一套 API，由 `IToolRegistry` 透明路由。

## 决策

### 决策 1 — 六种 Agent 协作模式

```
┌─────────────────────────────────────────────────────────────┐
│  Agent A 调用方                                              │
│                                                              │
│   ① call(request) → response         同步 RPC              │
│   ② call_async(request, callback)     异步 RPC              │
│   ③ emit(event_topic, payload)        pub/sub              │
│   ④ delegate(subagent_spec, monitor)  子 Agent              │
│   ⑤ parallel(tasks, options)          fork/join            │
│   ⑥ open_stream(handler)              流式通道              │
│                                                              │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
            ┌──────────────┼──────────────────┐
            ▼              ▼                  ▼
       进程内          进程间 (MCP)      跨框架 (A2A)
       ToolRegistry    RemoteAgentAdapter Agent Card
       IInteractionBus MCP Client        A2A Gateway
       DomainWorkerPool MCP Server
```

**v1 实现**：①②③④⑤（5 种）。
**Phase 2**：⑥ 流式通道。

#### 协作模式详细定义

##### ① call(request) → response（同步 RPC）

```cpp
auto result = call_tool("loop/run", {prompt, tools});
// 阻塞直到目标 Agent 完成，返回最终结果
```

| 实现层 | 进程内 | 进程间 |
|--------|--------|--------|
| 机制 | `IToolRegistry::call_tool()` | `RemoteAgentAdapter::call_remote()` → MCP `tools/call` |
| 阻塞 | 是 | 是 |
| 适合 | 简单请求-响应，叶子工具调用 | 同步远程调用 |

##### ② call_async(request, callback)（异步 RPC）

```cpp
// 进程内（v1）
bus->emit("chat.request", {prompt, tools, request_id});
bus->subscribe("chat.response." + request_id, [this](auto& e) {
    handle_response(e["data"]);
});

// 进程间（Phase 2）
remote.call_async("loop/run", args, [](auto& result) {
    handle_response(result);
});
```

| 实现层 | 进程内 | 进程间 |
|--------|--------|--------|
| 机制 | `IInteractionBus::emit` + `subscribe` 模式 | MCP `tools/call` + notifications 模式 |
| 关联 | request_id 关联请求与响应 | request_id 关联请求与响应 |

**关联规则**：
- 请求载荷必须包含 `request_id`（UUID）
- 响应事件名 = `chat.response.<request_id>`（约定模式）
- 调用方订阅自己的 request_id，回调完成后立即取消订阅

##### ③ emit(event_topic, payload)（pub/sub）

```cpp
bus->emit("user.input", {text: "..."});

// 其他 Agent 订阅
bus->subscribe("user.input", [](auto& e) {
    auto result = call_tool("loop/run", {prompt: e["text"]});
    bus->emit("loop.response", result);
});
```

| 实现层 | 进程内 | 进程间 |
|--------|--------|--------|
| 机制 | `IInteractionBus` (Phase 2 Redis/PG bus) | MCP `notifications` |
| 多对多 | ✅ | ✅ |
| 异步 | ✅ | ✅ |

##### ④ delegate(subagent_spec, monitor)（子 Agent）

```cpp
// Agent A 创建子 Agent 并监控其生命周期
auto sub_agent_id = orchestrator.delegate(
    SubAgentSpec{
        .agent_id = "code.review",
        .task = {code: "...", language: "cpp"},
        .session_id = parent_session_id,
        .max_lifetime_ms = 60000
    },
    [](auto& event) {
        if (event.type == "done") { /* 子 Agent 完成 */ }
        if (event.type == "error") { /* 子 Agent 出错 */ }
        if (event.type == "progress") { /* 子 Agent 进度 */ }
    }
);
```

| 实现层 | 进程内 | 进程间 |
|--------|--------|--------|
| 机制 | ADR-0033 `SubtaskSession` + `ExecutionSession` | MCP `tasks/create` + `tasks/get` |
| 监控 | 父 Agent 通过 callback 接收事件 | 父 Agent 通过 `tasks/get` 轮询或 notifications |
| 隔离 | 子 Agent 拥有独立 TaskSession | 子 Agent 运行在独立进程/容器 |

**与 fork/join 区别**：④ 是**父-子关系**，父 Agent 持有子 Agent 句柄；⑤ 是**并发执行**，所有任务对等。

##### ⑤ parallel(tasks, options)（fork/join）

```cpp
// 并行处理多个独立任务
auto results = registry.parallel(
    "code_review/run",  // 工具名
    {                   // 任务列表
        {code: "file1.cpp", language: "cpp"},
        {code: "file2.cpp", language: "cpp"},
        {code: "main.py", language: "python"}
    },
    ParallelOptions{
        .max_concurrency = 4,
        .timeout_ms = 60000,
        .on_each_complete = [](auto& idx, auto& result) {
            // 每个任务完成时回调（可累加结果）
        },
        .fail_fast = false  // 任一失败是否中断其他
    }
);
// 等所有完成（或失败/超时），返回结果列表
```

| 实现层 | 进程内 | 进程间 |
|--------|--------|--------|
| 机制 | `DomainWorkerPool` (Sprint 3) + `execute_parallel` | MCP `tasks/create` (N 次) + `tasks/get` (等所有完成) |
| 并发控制 | `max_concurrency` + `DomainWorkerPool` jthread | 多个远程 MCP 客户端并发 |
| 失败策略 | `fail_fast=true` 取消其他 | MCP tasks cancel 取消其他 |

##### ⑥ open_stream(handler)（流式通道）— Phase 2

```cpp
auto stream = registry.open_stream("llm.generate", {prompt, model});
stream->on_token([](auto& e) { render_token(e["text"]); });
stream->on_done([](auto& e) { handle_response(e["response"]); });
stream->on_error([](auto& e) { handle_error(e); });
```

| 实现层 | 进程内 | 进程间 |
|--------|--------|--------|
| 机制 | `IInteractionBus` 流式订阅（token-by-token） | MCP `tools/call` + SSE streaming |
| 阶段 | Phase 2 | Phase 2 |

### 决策 2 — 透明路由：IToolRegistry 自动选择 backend

```cpp
// 调用方写一次代码，路由自动选择 backend
auto result = call_tool("loop/run", args);

ToolRegistry::call_tool(name, args):
    1. CapabilityRegistry.query(name) → 得到 agent_id + agent_metadata
    2. RemoteRegistry.is_remote(agent_id) 判断 backend
    3. 本地 backend:
       ├── PDK Plugin (C++) → 直接调用 .so
       ├── SKILL → SkillInterpreter
       └── Wasm → WasmRuntime::invoke
    4. 远程 backend:
       └── RemoteAgentAdapter::call_remote(agent_id, ...)
    5. 返回 ToolResult
```

**调用方对 backend 完全无感**——Agent 开发者只需写：

```cpp
auto result = call_tool("loop/run", {prompt, tools});
```

不管目标 Agent 是本地还是远程、SKILL 还是 Wasm 还是远程 MCP，调用代码完全一致。

### 决策 3 — Loop Agent 通信模型

Loop Agent 采用 **同步调用 + 异步事件流**的双重模型：

```cpp
// Loop Agent 的对外接口

// 1. 同步入口（call_tool）
//    阻塞直到整个 Loop 完成，返回最终结果
auto result = call_tool("loop/run", {
    {"prompt", "..."},
    {"tools", ["fs/read", "shell/exec"]},
    {"max_steps", 50}
});
// result = {response: "...", steps: 3, tokens_used: 1500}

// 2. 异步事件流（IInteractionBus）
//    Loop 执行期间持续 emit 中间状态
subscribe("llm.token", [](auto& e) { render_token(e["text"]); });
subscribe("tool.execution.start", [](auto& e) { show_spinner(e["name"]); });
subscribe("loop.done", [](auto& e) { render_final(e["response"]); });
```

**事件清单**（确认现有的 PDK Chat Demo §8 事件流）：

| 主题 | 时机 | 载荷 |
|------|------|------|
| `loop.turn.start` | 每步开始 | `{turn, step, loop_type}` |
| `llm.token` | LLM 流式输出 | `{text, model}` |
| `llm.response` | LLM 完成 | `{model, tokens_used, truncated}` |
| `tool.execution.start` | 工具调用开始 | `{name, args_keys}`（不含 args 值） |
| `tool.execution.end` | 工具调用结束 | `{name, duration_ms, ok}` |
| `loop.turn.end` | 每步结束 | `{turn, decision}` |
| `loop.done` | 循环完成 | `{response, total_steps, total_tokens}` |
| `loop.error` | 循环出错 | `{error, step}` |

**与 tau yield 的等价性**：
```
tau yield             →  HydraForge
────────────────────────────────────────
yield ThoughtEvent    →  emit("loop.turn.start")
yield TokenEvent      →  emit("llm.token")
yield ToolCallEvent   →  emit("tool.execution.start")
yield ToolResultEvent →  emit("tool.execution.end")
yield ResponseEvent   →  emit("loop.done")

功能等价，API 形状不同：
- tau: 单 API（yield）同时承载流 + 结果
- HF: 双 API（call_tool + bus），调用方可选择是否关注事件流
```

**不采用 tau yield 模式的原因**：
- C++20 无 generator 语义（`std::generator` 在 C++23，且灵活性不足）
- 双重模型功能完全等价，且更灵活（CLI 场景可完全忽略事件）
- 如果未来需要 yield 式 API，可以在 `call_tool` 返回值上加 `stream_handle` 字段

### 决策 4 — v1 实现范围

| 模式 | v1 实现 | Phase 2 |
|------|:------:|:------:|
| ① call（同步 RPC） | ✅ | — |
| ② call_async（异步 RPC） | ✅ | — |
| ③ emit/subscribe（pub/sub） | ✅ | — |
| ④ delegate（子 Agent） | ✅ | — |
| ⑤ parallel（fork/join） | ✅ | — |
| ⑥ open_stream（流式） | ❌ | Phase 2 |

**v1 全部在进程内实现**（进程间等 ADR-0059 落地）。

### 决策 5 — 编排层次

**v1 不造编排引擎**。编排逻辑直接用 main.cpp 写：

```cpp
// examples/pdk_chat_demo/main.cpp — v1 编排
int main() {
    // 1. 加载 Agent
    PluginLoader loader;
    loader.load_so("libLoopAgent.so");
    loader.load_so("libProviderAgent.so");
    loader.load_so("libFSTools.so");
    
    // 2. 注册事件驱动关系
    bus->subscribe("chat.input", loop_agent);
    bus->subscribe("chat.input", budget_agent);
    
    // 3. 进入交互循环（工具调用链）
    while (std::getline(std::cin, input)) {
        auto result = call_tool("loop/run", {prompt, tools});
        std::cout << result["response"];
    }
}
```

**Phase 2 目标**：`application.yaml` 声明式编排文件，由 `AgentOrchestrator` 自动执行。

### 决策 4 — 编排层次（已移到决策 5）

```
L3 Application Mesh（声明式编排 → Phase 2）
  |  描述：哪些 Agent 参与、如何连接、什么触发
  |  格式：YAML / JSON
  
L2 Agent Level（组合协议 → v1 已覆盖）
  |  四种模型：工具链 / 子图 / 事件 / Wasm
  |  通信：call_tool + IInteractionBus
  
L1 OS Level（基础设施 → 已有）
  |  IToolRegistry, IInteractionBus, TopoScheduler
  |  CapabilityRegistry, Lifecycle
```

## 替代方案

### 方案 A：纯事件驱动，不做同步 call_tool

**否决理由**：
- 简单 CLI 场景不需要事件驱动
- 请求-响应是最自然的人机交互模式
- 事件驱动增加了复杂度（需要关联 request/response）

### 方案 B：用 tau 的 yield 替代 call_tool + bus

**否决理由**：
- C++20 无原生 generator 支持
- 双重模型等价且更灵活

### 方案 C：Phase 2 的编排引擎现在做

**否决理由**：
- 编排引擎是很大的投入
- v1 的 main.cpp 编排已经够用
- 需要等 Agent Marketplace 需求明确后再设计编排格式

## 不变量

- 所有组合模型都通过同一 `IToolRegistry` 调度（统一路由）
- 事件驱动组合不绕过 `call_tool` 的安全检查（审批策略依然生效）
- Loop Agent 的事件与 call_tool 的返回是一致的（事件流 + 最终结果 = 同一执行）
- 声明式编排是 Phase 2 的目标，v1 不承诺

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 组合模型 v1 | A（工具链）+ C（事件） | 够用，简单 |
| Loop 通信 | call_tool + bus 双重 | 等价 tau yield，更灵活 |
| 编排引擎 v1 | 代码编排 | 减少投入 |
| 编排文件 | Phase 2 | 等 Market 需求 |

## 后续行动

- ADR-0059: 跨进程 Agent 协议（扩展组合到网络）
- Phase 2: `application.yaml` 声明式编排
- Phase 2: DSL 子图嵌入 + Wasm 混合组合

## 参考

- [ADR-0019 — IInteractionBus](../adr-0019-iinteraction-bus-mvp.md)
- [ADR-0054 — Capability Discovery](./adr-0054-capability-discovery.md)
- tau 架构: huggingface/tau — async yield event stream
- [PDK Chat Demo 设计 §8 事件流](../superpowers/specs/2026-07-16-pdk-chat-demo-design.md)