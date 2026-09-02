# Agent 协作与运行时架构指南

> **目的**：快速理解 HydraForge 中 **Agent 与 Agent 之间** 的协作模式架构、协议抽象、运行时基底与设计取舍。
> **范围**：
> - **抽象层**：6 种协作模式（ADR-0060）+ 4 通道 Plugin 协议（ADR-0046）+ 3 种 Agent Loop（ADR-0021）
> - **运行时基底**：CognitiveWorker ↔ DomainWorkerPool 双 worker 模型（ADR-0020）+ 动态子图生成（`GenerateSubgraphNode` + `PlanExecuteLoop`）+ 进程/线程隔离
> - **横切治理**：Hook（ADR-0081/0085/0086）+ Phase 6 服务组合（ADR-0051）+ LLM-native（ADR-0071）
> - **自进化协作**：MCTS（ADR-0061-08）/ GEPA（ADR-0061-09）/ SkillCompiler（ADR-0061-03）/ Distillation（ADR-0061-13）等与基础模式的耦合关系
> **关系**：与 [`plugin-and-agent-architecture.md`](./plugin-and-agent-architecture.md) 配套 —— 后者讲"Plugin 与 Agent 是什么/怎么构建"，本指南讲"多个 Agent 如何协作 + 在什么运行时基底上执行 + 如何自进化"。
> **依据**：ADR-0019（IInteractionBus）/ ADR-0020（线程模型）/ ADR-0021（PDK）/ ADR-0033（Session 层级）/ ADR-0045（编排 Plugin）/ ADR-0046（Plugin 通信）/ ADR-0060（协作模式）/ ADR-0061（自进化系列）/ ADR-0066（SkillInterpreter 物理隔离）/ ADR-0068（事件契约）/ ADR-0071（LLM-native）/ ADR-0079（Session 4-Scope）/ ADR-0080（AppendOnlyEventLog）/ ADR-0081（Hook）/ ADR-0083（评估契约）/ ADR-0084（Mutation Governance）/ ADR-0085（横切）/ ADR-0086（信用分配）。

---

## 一、核心结论

Agent 协作不是单一抽象，而是**多层、10 维度**的体系（含运行时基底 + 自进化）：

```
┌────────────────────────────────────────────────────────┐
│ 顶层：6 种调用范式（ADR-0060）                          │
│   call / call_async / emit / delegate / parallel / stream │
├────────────────────────────────────────────────────────┤
│ 通信层：4 通道 Plugin 协议（ADR-0046）                  │
│   Tool / Event / Config / Query                        │
├────────────────────────────────────────────────────────┤
│ 执行层：3 种 Agent Loop（ADR-0021）                     │
│   React / PlanExecute / ForkJoin                       │
├────────────────────────────────────────────────────────┤
│ 动态子图：GenerateSubgraphNode（v3.1, §5A）             │
│   LLM 实时生成 + signature_validation（strict/warn/ignore）│
├────────────────────────────────────────────────────────┤
│ 运行时基底：双 Worker 模型（ADR-0020 §3.1 + §2.2.1, §6A）│
│   CognitiveWorker（思考/单线程） + DomainWorkerPool（执行/N jthread）│
│   + SkillInterpreter 子进程（ADR-0066, §6A.6）         │
├────────────────────────────────────────────────────────┤
│ Session：三层模型（ADR-0033）+ 4-Scope（ADR-0079, §6C） │
├────────────────────────────────────────────────────────┤
│ 拦截层：Agent Hook（ADR-0081；V1 骨架 ship，loop 集成推迟 Sprint 24+）│
│   pre-step / post-step + agent_glob 路由               │
├────────────────────────────────────────────────────────┤
│ 横切层：Cross-Cutting Pattern（ADR-0085）               │
│   ICrossCuttingPattern + *.cc.md                       │
├────────────────────────────────────────────────────────┤
│ 归因层：Credit Assignment（ADR-0086）                   │
│   VersionPairDiff + 4 态判定                           │
├────────────────────────────────────────────────────────┤
│ 自进化层：MCTS / GEPA / SkillCompiler / Trajectory IR    │
│   （ADR-0061 系列，§6B，正交于 §6A 运行时基底）         │
└────────────────────────────────────────────────────────┘
```

**关键设计哲学**：

1. **统一抽象**：Agent 开发者只需学一次 `IToolRegistry` API，路由层透明选择后端（**v1 进程内 backend 已 ship；进程间/跨框架 backend 为 Phase 2 目标，依赖 ADR-0059 未落地**）
2. **正交分层**：Tool Hook vs Agent Hook / Tool Layer vs Event Layer / **§6A 业务运行时 vs §6B 自进化治理** —— 每一层独立，避免双标准
3. **物理 vs 逻辑隔离**：v1 = 双 Worker 逻辑隔离（jthread） + SkillInterpreter 进程级物理隔离（seccomp 已 ship）；Phase 2+ 容器级隔离待 Phase 6+ 触发
4. **安全优先**：fail-closed 默认 + 5 escalation triggers + RAII guard 嵌套防护 + ToolCoordinator 审批
5. **可演进**：从 v1 同步 RPC → 异步 → pub/sub → 父子 → 并行 → 流式，逐步开放能力
6. **正交架构层**：§6A（业务运行时）与 §6B（自进化治理）通过 IEvaluator/IMutationGovernor/IInteractionBus 共享事件流与评估语义，但不共享线程模型

---

## 二、6 种 Agent 协作模式（ADR-0060）

**ADR-0060 决策 1**：协议无关的协作模式，由 `IToolRegistry` 透明路由。

| # | 模式 | ADR 决策 4 标注 | v1 实现 | 进程内机制（v1 真实 API） | 进程间机制 | 适合场景 |
|---|---|:---:|:---:|---|---|---|
| ① | `call(req) → response` | ✅ v1 | ✅ | `IAgentComposition::call` / `IToolRegistry::call_tool()` | MCP `tools/call` | 同步 RPC、叶子工具调用 |
| ② | `call_async(req, cb)` | ✅ v1 | ✅ | `IAgentComposition::call_async` / `IInteractionBus::emit+subscribe` | MCP + notifications | 异步 RPC、回调式 |
| ③ | `emit(topic, payload)` | ✅ v1 | ✅ | `IInteractionBus::emit/subscribe`（不在 IAgentComposition） | MCP `notifications` | pub/sub、多对多广播 |
| ④ | `delegate(spec, monitor)` | ✅ v1 | ✅（受限） | `IAgentComposition::delegate(agent_id, task, priority)`（v1 **无** monitor callback / max_lifetime_ms；仅返回 `TaskHandle{task_id, cancel}`） | MCP `tasks/create + tasks/get` | 父子关系、长生命周期子 Agent |
| ⑤ | `parallel(tasks, opts)` | ✅ v1 | ✅（受限） | **无统一 `parallel()` 方法**；`DomainWorkerPool` + `ForkJoinLoop` + 调用方手工聚合 | MCP `tasks/create × N + tasks/get` | fork/join 并行聚合 |
| ⑥ | `open_stream(handler)` | ❌ Phase 2 | ❌ Phase 2 | `IAgentComposition::stream` 占位（抛 `std::logic_error`） | MCP + SSE streaming | LLM token 流 |

> **状态列说明**：第二列「ADR 决策 4 标注」严格依据 [ADR-0060 决策 4 表](../adr/adr-0060-agent-composition.md#决策-4--v1-实现范围)（5 个 ✅ v1 + 1 个 ❌ Phase 2）。第三列「v1 实现」补充 v1 真实落地状态（与 §2.1 对照表 L164-168 对齐）：①/②/③ 完整可用；④/⑤ 仅部分能力落地（字段缺失或方法缺失，详见 §2.1 B）；⑥ Phase 2 占位。

### 2.1 API 形态（ADR 目标 vs v1 实际 ship）

> ⚠️ **重要区分**：下列两组代码**不可混用**。第一组是 **ADR-0060 决策 1 的目标 API**（含 SubAgentSpec/ParallelOptions/monitor callback 等字段，仅作契约目标）；第二组是 **v1 已 ship 的真实 API**（来自 `include/agenticdsl/contract/iagent_composition.h`，Sprint 22 P8 ship，OpenSpec change `adr-0060-p2-p3-patterns`）。

#### A. ADR-0060 决策 1 目标 API（Phase 2 部分字段未落地）

```cpp
// ① 同步 RPC（v1 ✅ / Phase 2 同样语义）
auto result = call_tool("loop/run", {prompt, tools});

// ② 异步 RPC（进程内）
bus->emit("chat.request", {prompt, tools, request_id});
bus->subscribe("chat.response." + request_id, [this](auto& e) { ... });

// ③ pub/sub（直接走 IInteractionBus，不在 IAgentComposition 中）
bus->emit("user.input", {text: "..."});
bus->subscribe("user.input", [](auto& e) {
    auto result = call_tool("loop/run", {prompt: e["text"]});
    bus->emit("loop.response", result);
});

// ④ 委派子 Agent（ADR-0060 决策 1 目标：SubAgentSpec + monitor callback）
//    ⚠️ v1 未落地：monitor callback / max_lifetime_ms 字段尚不存在
auto sub_id = orchestrator.delegate(
    SubAgentSpec{.agent_id="code.review", .task={...}, .max_lifetime_ms=60000},
    [](auto& event) {
        if (event.type == "done")   { /* 子 Agent 完成 */ }
        if (event.type == "error")  { /* 子 Agent 出错 */ }
        if (event.type == "progress") { /* 子 Agent 进度 */ }
    }
);

// ⑤ fork/join 并行（ADR-0060 决策 1 目标：ParallelOptions）
//    ⚠️ v1 未落地：registry.parallel() 统一方法尚不存在
auto results = registry.parallel(
    "code_review/run",
    {task1, task2, task3},
    ParallelOptions{
        .max_concurrency = 4,
        .timeout_ms = 60000,
        .fail_fast = false,
        .on_each_complete = [](auto& idx, auto& result) { ... }
    }
);
```

#### B. v1 实际 ship API（`include/agenticdsl/contract/iagent_composition.h`）

```cpp
// ① 同步 RPC（v1 ✅ IAgentComposition::call）
auto composition = agenticdsl::make_agent_composition(registry);
auto result = composition->call(
    "react-loop-v1",                       // agent_id（必填）
    "{prompt:\"review src/main.cpp\"}",    // args 序列化为字符串
    std::chrono::seconds(30));             // timeout（默认 30s）
// 返回 AgentResult<std::string> {ok, value, error_code?, message}

// ② 异步 RPC（v1 ✅ IAgentComposition::call_async）
auto fut = composition->call_async(
    "react-loop-v1",
    args,
    [](agenticdsl::AgentResult<std::string> r) { /* 回调 */ },  // 可选 callback
    std::chrono::seconds(30));
auto result = fut.get();  // 或异步等待

// ④ 委派子 Agent（v1 ✅ IAgentComposition::delegate — 注意：仅 3 参数）
auto task_handle = composition->delegate(
    "react-loop-v1",                       // agent_id
    "review src/main.cpp",                 // task
    "normal");                             // priority（默认 "normal"）
// 返回 TaskHandle { task_id, cancel }
// ⚠️ v1 不支持：monitor callback / max_lifetime_ms / event subscription
//    取消语义通过 TaskHandle::cancel() 触发（无进度事件回调）

// ⑤ fork/join 并行（v1 通过 ForkJoinLoop + DomainWorkerPool 组合，无统一 parallel()）
//    参见 §六 + ADR-0020 §2.2.1：调用方手工组合 N 个 call_tool/call + 聚合
//    ForkJoinLoop 直接接受 std::vector<std::string> 分支列表（fork_join_loop.h:138）
//    备注：「按逗号分隔」语义属于 AgentRunner 适配层（配套指南 §3.2），非 run() 直 API
auto results = fork_join_loop.run({"branch1 prompt", "branch2 prompt", "branch3 prompt"}, ctx);

// ⑥ 流式通道（v1 ❌ Phase 2 — 当前实现抛 std::logic_error）
auto stream_handle = composition->stream("react-loop-v1", args);
// throws: "Phase 2 - stream not yet implemented"
```

**v1 ↔ 决策表对照**（[ADR-0060 决策 4](../adr/adr-0060-agent-composition.md#决策-4--v1-实现范围) 决策 4）：

| 模式 | ADR 决策 4 标注 | v1 实际代码落地 |
|---|---|---|
| ① call | ✅ v1 | `IAgentComposition::call` ✅ |
| ② call_async | ✅ v1 | `IAgentComposition::call_async` ✅ |
| ③ emit/subscribe | ✅ v1 | `IInteractionBus::emit/subscribe` ✅（不在 `IAgentComposition` 内） |
| ④ delegate | ✅ v1 | `IAgentComposition::delegate` ✅（但**仅 3 参数**，无 monitor/max_lifetime_ms） |
| ⑤ parallel | ✅ v1 | **未提供统一 `parallel()` 方法**，由 `ForkJoinLoop` + `DomainWorkerPool` + 调用方编排承担 |
| ⑥ open_stream | ❌ Phase 2 | `IAgentComposition::stream` 占位（抛 `logic_error`） |

### 2.2 关键对比（v1 实际 ship 能力）

| 维度 | ④ delegate | ⑤ parallel |
|---|---|---|
| 关系 | **父-子**（父持有 `TaskHandle`） | **对等**（所有任务同级） |
| 生命周期 | 由 `TaskHandle::cancel()` 控制（**无 max_lifetime_ms**） | 短（每个任务完成即结束） |
| 监控 | **无 callback / 无 event stream**；调用方通过 `TaskHandle::task_id` + bus 事件 `loop.done`/`loop.error` 自查（v1 限制） | 调用方手工编排（**无 `on_each_complete` 回调**；v1 通过 `ForkJoinLoop.run()` 同步聚合结果） |
| 隔离 | 子 Agent 由 `IAgentComposition::delegate` 创建（v1 实际依赖 `TestDoubleAgentRegistry`——`include/agenticdsl/contract/test_double_registry.h` P8 test-double；与 ADR-0082 `IAgentRegistry` 的接线为后续工作） | 无独立 Session |
| 适用 | 长任务、需取消（监控能力弱） | 批量并行、独立子任务 |

### 2.3 透明路由（ADR-0060 决策 2 — Phase 2 愿景）

> ⚠️ **决策 2 为 Phase 2 目标架构**，当前 v1 **未实现**。下列伪码与 `CapabilityRegistry` / `RemoteRegistry` / `RemoteAgentAdapter` / `WasmRuntime` 等类**在全代码库零匹配**（仅存在 ADR-0060 决策 2 文本中）。v1 实际行为：所有 tool 调用走 `IToolRegistry::call_tool()` 直调（v1 进程内，ADR-0060 决策 4 明确"v1 全部在进程内实现，进程间等 ADR-0059 落地"）。

```cpp
// ADR-0060 决策 2 目标架构（Phase 2 愿景，v1 未实现）
ToolRegistry::call_tool(name, args):
    1. CapabilityRegistry.query(name) → agent_id + metadata       // ⚠️ 类不存在
    2. RemoteRegistry.is_remote(agent_id) 判断 backend             // ⚠️ 类不存在
    3. 本地 backend:
       ├── PDK Plugin (C++) → 直接调用 .so                        // ✅ v1 可用
       ├── SKILL → SkillInterpreter                               // ✅ v1 可用
       └── Wasm → WasmRuntime::invoke                             // ⚠️ WasmRuntime 不存在（ADR-0056 V2 deferred Phase 8+）
    4. 远程 backend:
       └── RemoteAgentAdapter::call_remote(agent_id, ...)         // ⚠️ 类不存在
    5. 返回 ToolResult
```

**v1 实际行为**：调用方对本地 PDK Plugin 直调透明（v1 已 ship），进程间/跨框架路由为 Phase 2 目标（依赖 ADR-0059，未启动）。Agent A 写一次代码（`call_tool("loop/run", args)`）在 v1 中**仅**透明选择本地 backend；远程 backend 调用需调用方手工编排（与 ADR-0060 决策 4 "v1 全部在进程内实现" 一致）。

### 2.4 Loop Agent 双重模型（ADR-0060 决策 3）

```
┌──────────────────────────┬────────────────────────────────┐
│ tau yield                │ HydraForge                      │
├──────────────────────────┼────────────────────────────────┤
│ yield ThoughtEvent       │ emit("loop.turn.start")         │
│ yield TokenEvent         │ emit("llm.token")               │
│ yield ToolCallEvent      │ emit("tool.execution.start")    │
│ yield ToolResultEvent    │ emit("tool.execution.end")      │
│ yield ResponseEvent      │ emit("loop.done")               │
└──────────────────────────┴────────────────────────────────┘

功能等价，API 形状不同：
- tau: 单 API（yield）同时承载流 + 结果
- HF: 双 API（call_tool + bus），调用方可选择是否关注事件流
```

**事件清单**：

| 主题 | 时机 | ADR-0060 决策 3 逻辑字段 | ADR-0068 Appendix A 注册载荷 |
|---|---|---|---|
| `loop.turn.start` | 每步开始 | `{turn, step, loop_type}` | `turn`, `step` |
| `loop.turn.end` | 每步结束 | `{turn, decision}` | `turn`, `decision` |
| `loop.decision` | 决策点 | （未列） | `decision`, `tool?` |
| `loop.done` | 循环完成 | `{response, total_steps, total_tokens}` | `session_id` |
| `loop.error` | 循环出错 | `{error, step}` | `error_code`, `message` |
| `llm.request` | LLM 调用前 | （未列） | `model`, `prompt_hash` |
| `llm.response` | LLM 完成 | `{model, tokens_used, truncated}` | `tokens`, `duration_ms`, `error_code?` |
| `llm.token` | LLM 流式输出 | `{text, model}` | `session_id`, `token`, `index` |
| `llm.token.done` | 流式完成 | （未列，ADR-0060 决策 3 仅提 `loop.done`） | `total_tokens` |
| `llm.token.error` | 流式错误 | （未列） | `error_code`, `message` |
| `tool.execution.start` | 工具调用开始 | `{name, args_keys}`（不含 args 值） | `tool`, `layer` |
| `tool.execution.end` | 工具调用结束 | `{name, duration_ms, ok}` | `tool`, `ok`, `duration_ms` |

**关键事件清单源自 [ADR-0060 决策 3](../adr/adr-0060-agent-composition.md#决策-3--loop-agent-通信模型)**（Loop Agent 双重模型）。**运行时生命周期主题的注册载荷以 [ADR-0068 Appendix A](../adr/adr-0068-event-emission-contract.md#附录-acanonical-topic-registry-v20-2026-08-31) 为准**（Canonical Topic Registry + 7 幻影主题强制发射点）。上表"ADR-0060 决策 3 逻辑字段"列描述**业务含义**；"ADR-0068 Appendix A 注册载荷"列描述**真实契约 schema**（含字段重命名与省略差异，例如 `llm.token` 业务字段 `text` 在注册载荷中重命名为 `token`；`loop.done` 业务字段 `response/total_steps/total_tokens` 未出现在注册载荷中）。

---

## 三、4 通道 Plugin 通信协议（ADR-0046）

**ADR-0046 决策**：编排 Plugin ↔ 推理 Plugin 的四通道架构，零框架改动，复用现有基础设施。

```
编排 Plugin                         推理引擎 Plugin
  │                                     │
  ├─ ① Tool Layer ─────────────────────►│  sync: call_tool("inference/*")
  │                                     │
  │◄─ ② Event Layer ──────────────────┤  async: emit("inference.*", payload)
  │                                     │
  ├─ ③ Config Layer ──────────────────►│  sync: call_tool("inference/configure")
  │                                     │
  ├─ ④ Query Layer ◄───────────────────│  sync: call_tool("inference/get/*")
  │                                     │
```

| 通道 | 方向 | 同步性 | 频率 | 走审批? | 数据格式 |
|---|---|:---:|:---:|:---:|---|
| **① Tool** | 编排→推理 | 同步 | 中 | ✅外部 / ❌ 内部豁免 | `unordered_map<string,string> → nlohmann::json` |
| **② Event** | 推理→编排 | 异步 | 低（每推理 1-2 次） | ❌ | `ToolResult` 信封 |
| **③ Config** | 编排→推理 | 同步 | 低（变化时） | ✅ 外部 / ❌ 内部豁免 | `{applied, requires_restart, current}` |
| **④ Query** | 编排→推理 | 同步 | 中（决策时） | ❌（ReadOnly） | atomic snapshot |

### 3.1 命名约定（ADR-0043 + ADR-0046）

| 用途 | 分隔符 | 例子 |
|---|---|---|
| **PDK tool names** | slash `/` | `inference/generate`, `model_router/cost` |
| **EventBus topics** | dot `.` | `inference.lifecycle.idle`, `tool.audit.invoked` |
| **DSL module namespace** | dot `.` 或 `::` | `inference::engine` |

### 3.2 错误处理约定（ADR-0046 §1）

- **成功**：返回 `ToolResult(ok=true)` 或直接 `nlohmann::json`（由 ToolRegistry 包装）
- **业务错误**：返回 `ToolResult.error(ErrorCode, message, meta)`（Per ADR-0023 P2 enum ErrorCode）
- **协议错误**：由 ToolCoordinator 处理（工具未注册 → `ErrorCode::ToolNotRegistered`）
- **不再使用**：嵌套 `{"error": {"code": ..., "message": ...}}` 格式（[ADR-0023 §C.7](../adr/adr-0023-tool-result-standard.md#c7-已知遗留) 标注为**已知遗留**：实现采用扁平 `error_code`（顶层） + `meta.error_message`（兼容层），修正需独立 OpenSpec change）

### 3.3 Event Layer Topic 规范（ADR-0046 §2）

```
inference.lifecycle.{state}              ← idle, running, model_loaded, context_overflow
inference.model.{action}                 ← loaded, unloaded, switched
inference.error.{code}                   ← oom, network, cancelled, context_overflow
orchestration.audit.internal.{tool}      ← 编排内部调用 audit（ADR-0045 §6.3）
orchestration.audit.llm.generate         ← 编排 ILLMProvider → 推理 内部 audit
```

**频率限制**：EventBus 承载每推理 ~1-2 次的生命周期事件。性能指标（t/s、KV cache%、GPU mem）通过通道 ④ query 按需拉取。1000+ events/sec 触发 Sprint 12 bridge 背压，**严禁高频指标推送**。

---

## 四、编排 Plugin 角色边界（ADR-0045）

**ADR-0045 决策 1**：编排 Plugin 是 Agent 协作的核心路由器。职责矩阵：

| 职责 | 编排 Plugin | 推理 Plugin | Core |
|---|:---:|:---:|:---:|
| **模型选择** | ✅ | ❌ | ❌ |
| **推理执行**（decode） | ❌ | ✅ | ❌ |
| **提示词构建** | ✅ | ❌ | ❌ |
| **Agent 循环**（React/PlanExec/ForkJoin） | ✅ | ❌ | ❌ |
| **工具调用编排** | ✅ | ❌ | ❌ |
| **Session 管理** | ✅ | 部分 | ✅ |
| **负载均衡** | ✅ | ❌ | ❌ |
| **超时/重试策略** | ✅ | ❌ | ❌ |
| **LLM 调用**（ILLMProvider interface） | ✅（包装） | ✅（实现） | ❌ |

### 4.1 Dual Consumer Model（ADR-0045 §2.1）

```
┌────────────────── Orchestration Plugin ──────────────────┐
│                                                           │
│  ┌────────────────────────────┐  ┌─────────────────────┐ │
│  │ OrchestrationILLMProvider  │  │ Agent Loops         │ │
│  │ (路由 + 会话管理)            │  │ (ReAct/PlanExec/    │ │
│  │ 消费者: DSLEngine/          │  │  ForkJoin)          │ │
│  │          NodeExecutor       │  │ 消费者: 循环自身      │ │
│  │ 直连推理 (no Tool dispatch)│  │ 直连推理 (no dispatch)│ │
│  └──────────┬─────────────────┘  └──────────┬──────────┘ │
│             │                                 │            │
│             ▼                                 ▼            │
│        ┌──────────────────────────────────────────────┐    │
│        │  推理 Plugin ILLMProvider                   │    │
│        └────────────────────┬─────────────────────────┘    │
└────────────────────────────┼───────────────────────────────┘
                              ▼
                         llama_decode()
```

**关键洞察**：LLM generate 路径**直连** `inference_provider_->generate()`，**不经** ToolCoordinator audit pipeline（避免语义误用）。仅 DSL workflow 的 tool call 经 ToolCoordinator 审批。

### 4.2 编排 Plugin 注册的工具（ADR-0045 §5）

| 工具名 | 功能 | 映射到推理 Plugin |
|---|---|---|
| `orchestration/route` | 根据任务描述选择推理模型 | `inference/get/models` + ADR-0034 IModelRouter |
| `orchestration/execute` | 执行一次 Agent 任务 | `inference/generate` + Agent loop |
| `orchestration/status` | 查询编排状态 | `inference/get/status` |
| `orchestration/configure` | 动态调节编排策略 | 编排自身状态 |

### 4.3 路由策略复用（ADR-0034）

`orchestration/route` 内部直接调用 `IModelRouter::route()`，不引入新路由抽象。Strategy 实现由 `pdk/model_router/` 提供（cost / quality / latency 三个 `.so`）。

---

## 五、3 种 Agent Loop 协作语义（ADR-0021）

### 5.1 Loop 内部协作形态

| Loop | 状态机 | 协作模式 |
|---|---|---|
| **React** | 思考 → 行动 → 观察 | 单 Agent 内部循环；通过工具调用其他 Agent 提供的 tool（无显式 peer 关系） |
| **PlanExecute** | Planning → Executing → Verifying → Done/Retry | execute_phase 可调用其他 Agent 工具；verify_phase 可委派 verify 子 Agent（peer 关系） |
| **ForkJoin** | Forking → Executing → Joining → Done | N 个分支 peer 同时执行 → 主循环 join 等待 → 合并结果 |

### 5.2 定义方式

```cpp
#include <agenticdsl/pdk/pdk.h>
using namespace hydraforge::pdk;

DEFINE_AGENT(coding_assistant, AgentLoopType::React);     // 单 agent ReAct
DEFINE_AGENT(planner,           AgentLoopType::PlanExecute); // 规划→执行→验证
DEFINE_AGENT(parallel_analyzer, AgentLoopType::ForkJoin);    // 多 worker 并行
```

完整说明参见 [`plugin-and-agent-architecture.md`](./plugin-and-agent-architecture.md) §3.3。

### 5A. 动态子图生成（GenerateSubgraphNode）— LLM 驱动协作的物质载体

> **本节是协作的关键补充**：§5.1 描述了 `PlanExecuteLoop` 的状态机，但**未说明其 "Planning 阶段" 的核心机制是 LLM 实时生成新子图，再由 DSLEngine 解析执行**。这正是 §11 自进化（AFlow MCTS / GEPA）的物理载体。

#### 5A.1 GenerateSubgraphNode 定义（`src/core/types/node.h:198-207`）

```cpp
struct GenerateSubgraphNode : public Node {
    std::string prompt_template;                              // 提示模板（含目标 + 上下文占位符）
    std::vector<std::string> output_keys;                     // e.g., ["generated_graph_path"]
    std::string signature_validation = "strict";              // strict | warn | ignore
    std::optional<NodePath> on_signature_violation;           // 违反时跳转路径
    LayeredContext execute(LayeredContext& ctx) override;     // 实现 in NodeExecutor
};
```

| 字段 | 语义 |
|---|---|
| `prompt_template` | 拼装 LLM 调用 prompt（`Goal` + `Context.dump()` + 节点占位符） |
| `output_keys` | 生成子图写入 `LayeredContext.working[output_keys[i]]`（典型：`generated_graph_path`） |
| `signature_validation` | 三态：strict（违反则 fail）/ warn（警告并继续）/ ignore（不校验） |
| `on_signature_violation` | strict 模式下违反契约时跳转的备选节点路径 |

**NodeType 枚举**：`NodeType::GENERATE_SUBGRAPH`（v3.1 引入，与 ForkNode/JoinNode 同代）

#### 5A.2 实际调用链：`PlanExecuteLoop::execute_phase` 是核心消费者

> **重要修正（Oracle session `ses_f9e927788ffeFwJ26EQHrm8YT7` 实证）**：PlanExecuteLoop 路径**不经过 GenerateSubgraphNode**。两者仅共享 `engine_->continue_with_generated_dsl()` 入口——PlanExecuteLoop 调用该方法解析 + append markdown（`plan_execute_loop.h:232-235` 明示「不实际 engine_->run()」），而 GenerateSubgraphNode 的 signature 校验只在运行时 NodeExecutor 路径执行（`node_executor.cpp:314-320`）。

```
PlanExecuteLoop::plan_phase()                           // ① LLM 生成 DSL markdown
  ↓ LLM 返回 markdown 字符串
PlanExecuteLoop::execute_phase(generated_dsl)           // ② 动态注入新子图
  ↓ engine_->continue_with_generated_dsl(generated_dsl) // ③ DSLEngine 仅 parse + append（`engine.cpp:390-399` 注释「signature 校验暂略」）
DSLEngine::continue_with_generated_dsl()                // ④ append 到 graph registry
  ↓
PlanExecuteLoop::verify_phase()                         // ⑤ LLM yes/no 验证

// 注：GenerateSubgraphNode 的 signature_validation 仅在 NodeExecutor 运行时路径触发
// （独立路径，与 PlanExecuteLoop 无关）
```

**关键代码实证**（`include/agenticdsl/pdk/agent_loops/plan_execute_loop.h:232-248`）：

```cpp
bool execute_phase(const std::string& generated_dsl, ...) {
    try {
        engine_->continue_with_generated_dsl(generated_dsl);  // ← 动态子图入口
        result.final_context.working["meta"]["plan_appended"] = true;
        return true;
    } catch (const std::exception& e) {
        result.final_context.working["meta"]["execute_error"] = e.what();
        return false;
    }
}
// 注：本实现仅调用 engine_->continue_with_generated_dsl (parse + append)，
// 不实际 engine_->run(). 因为 LLM 生成的 DSL 通常是新子图 (例如 /plan_1)，
// 与初始 /main 不冲突; 实际 run 由调用方在 verify 之后决定.
```

#### 5A.3 与 §5.1 的关系修正

**之前**：§5.1 表格描述 "PlanExecute: execute phase 可调用其他 Agent 工具"

**修正**：PlanExecuteLoop **不是通过工具调用其他 Agent**（这是 ReactLoop 的特征），而是通过 **LLM 实时生成子图**（这是 ReactLoop 完全没有的能力）。这是 PlanExecuteLoop 与 ReactLoop 的本质区分。

#### 5A.4 与 §6B 自进化的衔接

`GenerateSubgraphNode` + `engine_->continue_with_generated_dsl()` 入口是以下组件的共同物质基础：

- **AFlow MCTS（ADR-0061-08, T20 ship 2026-08-28）**：通过 `Materializer::materialize_to_dsl()`（`workflow_materializer.h:19`）产出 DSL 文本 → `continue_with_generated_dsl`，**不经 GenerateSubgraphNode 节点运行时路径**
- **GEPA Loop（ADR-0061-09, T19 ship 2026-08-27）**：反思候选通过 SkillCompiler + MutationGovernor 治理，**与 GenerateSubgraphNode 无直接耦合**
- **SkillCompiler（ADR-0061-03, T17 ship 2026-08-27）**：SKILL.md → 纯函数式 CompiledSkill（**不构建 ParsedGraph**，V1 不走 GenerateSubgraphNode 路径）

**唯一直接触发 GenerateSubgraphNode 节点运行时路径的场景**：DSL workflow 中显式声明 `type: generate_subgraph` 节点时，NodeExecutor::execute_generate_subgraph（`node_executor.cpp:314-320`）按 `signature_validation` 模式校验。

详见 [§6B](#6b-自进化与协作模式的关系分析)。

---

## 六、DomainWorkerPool 并发原语

**Sprint 3（ADR-0020 §2.2.1 ✅ Resolved）**：`DomainWorkerPool` 是 `ForkJoinLoop` + `PlanExecuteLoop` 并行分支的底层并发原语，也是 [§6A](#6a-cognitiveworker--domainworkerpool-协作运行时基底) 中 Domain 层的核心实现。

```cpp
class DomainWorkerPool {
    // N 个 std::jthread worker + 共享 FIFO 任务队列
    // 多消费者模式 + shared_mutex 保护 handler
    // 异常隔离（try-catch + catch(...)）+ queue 排空策略
};
```

**位置**：`include/agenticdsl/cognitive/domain_worker_pool.h`

**应用**：
- `ForkJoinLoop` 默认 4 worker，按 `num_threads` 参数注入
- `PlanExecuteLoop` **无并行假设验证**：v1 实现为单次同步 `bool verify_phase(goal, result, llm, token)`（`include/agenticdsl/pdk/agent_loops/plan_execute_loop.h:254`），验证失败触发单条 Retry 路径，无 `DomainWorkerPool` 依赖（头文件 L17-21 include 列表可证）
- Phase 6 PDK Composition 并行 fan-out（ADR-0051 spike 内 `parallel()` 编排为单一 agent 工具调用 + manual fan-out，非原生并行抽象）

### 6A. CognitiveWorker ↔ DomainWorkerPool 协作（运行时基底）

> **本节是协作的核心补充**：§六 仅把 `DomainWorkerPool` 当作"并发原语"描述，遗漏了 Agent 协作的真正运行时骨架 —— **双 worker 模型的协作模式**。本节依据 [ADR-0020](../adr/adr-0020-thread-model-isolation.md) §2.2.1 + §3.1 还原完整图景。

#### 6A.1 双 Worker 模型对比表

| 维度 | **CognitiveWorker**（思考层） | **DomainWorkerPool**（执行层） |
|---|---|---|
| **位置** | `include/agenticdsl/cognitive/cognitive_worker.h` | `include/agenticdsl/cognitive/domain_worker_pool.h` |
| **ADR 定位** | ADR-0020 §3.1 | ADR-0020 §2.2.1 |
| **Sprint** | Sprint 2 (2026-06-18) | Sprint 3 (2026-06-19) |
| **状态机** | `idle / running / stopped`（atomic State） | `idle / running / stopped`（atomic State） |
| **线程模型** | **单线程**消费任务队列 | **N 个 jthread worker**（默认 4，可配置）共享 FIFO（多消费者） |
| **任务模型** | `(task_id, prompt)` 元组 | `DomainTask{domain, tool_name, arguments, output_key}` |
| **每实例隔离** | **独占一个 DSLEngine**（per-agent 隔离） | 共享 N 个 worker + handler 注册表 |
| **总线接入** | **构造时强制注入**（F7 顺序契约，立即 `set_interaction_bus` 到 engine） | **可选注入**（向后兼容：无 bus 版本仍可用） |
| **可选评估器** | `set_evaluator(IEvaluator*)` —— 完成后发射 `evaluation.result` | `set_evaluator(IEvaluator*)` —— 同上 |
| **事件主题** | `cognitive.task.{started,completed}` | `domain.task.{started,completed,failed}` |
| **handler 签名** | 内置 `SimpleCognitiveOrchestrator::process(task_id, prompt)`（Sprint 2） | 注册 `nlohmann::json(const DomainTask&)` lambda |
| **典型消费者** | v1 **零生产消费者**（grep 实证：src/ 仅 cognitive 模块自引用，examples/ 无使用；`ReactLoop` 直连 `SimpleCognitiveOrchestrator`，不经 CognitiveWorker） | `ForkJoinLoop` 分支执行（`fork_join_loop.h:45` num_threads=4）/ 自进化组件 v1 不经此路径（详见 §6B.3） |

#### 6A.2 协作链：Thinking → Execution → Event 流

```
┌───────────────────────┐                                ┌──────────────────────────┐
│  CognitiveWorker      │                                │  DomainWorkerPool        │
│  (思考层)              │                                │  (执行层)                  │
│                       │                                │                          │
│  submit_task(         │                                │  submit_task(            │
│    task_id, prompt)   │  ──── 单线程处理 ────▶         │    DomainTask{           │
│  )                    │       ReactLoop.run()         │      "fs", "fs/read",   │
│  ↓                    │       完成一次 ReAct           │      args, "result"     │
│  engine_->run(ctx)    │                                │    }                     │
│  ↓                    │                                │  )                       │
│  emit("cognitive.     │                                │  ↓ worker 抢到           │
│       task.started") │                                │  process_task()          │
│  ↓                    │                                │  ↓                       │
│  SimpleCognitive      │                                │  handler(args)           │
│  Orchestrator::       │                                │  ↓                       │
│  process()            │       (可选) subscribe          │  emit("domain.task.      │
│  ↓                    │       "domain.task.*"          │       started")       │
│  emit("cognitive.     │                                │  ↓                       │
│       task.completed")│ ───────── bus ────────────▶   │  ...handler 异常隔离...  │
│                       │                                │  ↓                       │
│                       │                                │  emit("domain.task.      │
│                       │                                │       completed/failed")│
└───────────────────────┘                                └──────────────────────────┘
```

**协作的三种触发模式**（实证 `cognitive_worker.cpp` + `domain_worker_pool.cpp`）：

1. **直接 submit 模式**：调用方自行决定何时向 DomainWorkerPool 提交 task（最常见）
2. **事件订阅模式**：CognitiveWorker 通过 `bus_->subscribe("domain.task.*", ...)` 被动接收 DomainWorkerPool 事件（理论可行，代码未见显式接线）
3. **混合模式**：`PlanExecuteLoop::execute_phase` 触发 DSL workflow 执行，DSL workflow 中的 `tool_call` 节点内部可调用 DomainWorkerPool

#### 6A.3 IInteractionBus 共享的角色

**F7 顺序契约**（`cognitive_worker.cpp:75-87`）：
```cpp
CognitiveWorker::CognitiveWorker(unique_ptr<DSLEngine> engine,
                                 shared_ptr<IInteractionBus> bus)
    : engine_(std::move(engine)), bus_(std::move(bus)) {
    // ...
    engine_->set_interaction_bus(bus_);  // 构造时立即注入
}
```

**含义**：
- CognitiveWorker 持有的 DSLEngine 与 CognitiveWorker 共享同一 `IInteractionBus`
- 该 bus 与 DomainWorkerPool 共享 → **三层共享同一事件流**：
  - CognitiveWorker 发射 `cognitive.task.*`
  - DomainWorkerPool 发射 `domain.task.*`
  - DSLEngine 发射 `dsl.call.*` / `tool.execution.*`
- 任一消费者通过 `subscribe` 即可观察全局事件（审计、监控、Replay 场景）

#### 6A.4 与 6 协作模式的衔接

| 协作模式 | 与双 Worker 模型的衔接 |
|---|---|
| ① `call` | 调用方直接 `composition->call()` 同步阻塞 —— v1 未走 CognitiveWorker 路径 |
| ② `call_async` | 通过 bus 实现异步回调（理论可由 CognitiveWorker 持有回调） |
| ③ `emit` | DomainWorkerPool `emit("domain.task.*")` 即为 ③ emit 模式实例 |
| ④ `delegate` | CognitiveWorker 持有子 CognitiveWorker（per-agent DSLEngine 链）即为 ④ delegate 实例 |
| ⑤ `parallel` | ForkJoinLoop 默认 4 worker → 即为 ⑤ parallel + DomainWorkerPool 实例 |
| ⑥ `open_stream` | Phase 2，未实现 |

**核心洞察**：§2 的 6 协作模式是 **API 抽象**，而本节的双 Worker 模型是 **运行时实现**。两者正交：API 调用通过运行时基底落地，运行时基底暴露 API 给上层。

#### 6A.5 物理 vs 逻辑隔离（ADR-0020 §2.2）

> **修正（Oracle session `ses_f9e927788ffeFwJ26EQHrm8YT7` 实证）**：物理隔离相关 ADR 是 **ADR-0055/0066**（SkillInterpreter），非 ADR-0056（wasm 运行时）；且 SkillInterpreter V1 已 ship posix_spawn + IPC + seccomp(BPF)（Sprint 22），非 Phase 2+。

| 隔离维度 | v1（ADR-0020 §2.2.1 + §3.1 + ADR-0066 V1 ship） | Phase 2+（容器级） |
|---|---|---|
| **进程边界** | 同一进程（共享地址空间） + SkillInterpreter 子进程例外 | 独立容器（K8s / Docker per agent） |
| **线程模型** | CognitiveWorker 单线程 `std::thread` + DomainWorkerPool `std::jthread` × N | 进程 per agent |
| **故障隔离** | `try-catch + catch(...)` 异常隔离 + event 失败事件 | seccomp(BPF)（已 ship via SkillInterpreter）+ namespace + cgroup |
| **资源隔离** | `shared_mutex` 保护 handler 注册表 | cgroup 资源限制 |
| **当前实证** | ADR-0051 G1↔G3 spike（同进程 jthread 协作） + SkillInterpreter（Sprint 22，posix_spawn + seccomp + 4 host functions） | K8s/Docker per-agent 编排（待 Phase 6+ 触发） |

**结论**：v1 是**逻辑隔离**（共享进程 + jthread 隔离），**物理隔离已部分 ship**（SkillInterpreter 子进程边界 + seccomp）。完整容器级物理隔离待 Phase 6+ 触发。

#### 6A.6 SkillInterpreter：唯一已 ship 的物理隔离组件（ADR-0066）

> **本节依据 Oracle 评审建议补入**：SkillInterpreter 是全库**唯一已 ship**的物理隔离实现（posix_spawn + IPC + seccomp），但未在任何章节中充分描述。

**位置**：`include/agenticdsl/skill/skill_interpreter.h` + `src/modules/skill_interpreter/skill_interpreter.cpp`

**关键设计**（ADR-0055/0066）：
- **进程隔离**：`posix_spawn` + `execve(/proc/self/exe, --skill-child)` 启动子进程执行 SKILL.md
- **IPC**：pipe + BusEvent 信封序列化
- **沙箱**：`seccomp(BPF)` 限制 syscall（实际已 ship，非 Phase 2+）
- **4 host functions**（子进程通过 IPC 调用）：
  1. `call_tool(name, args)`
  2. `emit_event(topic, payload)`
  3. `consume_budget(amount)`
  4. `llm_generate(prompt)`

**与 §6A 双 Worker 模型的关系**：
- SkillInterpreter 是**子进程粒度**的隔离（vs CognitiveWorker/DomainWorkerPool 的**线程粒度**）
- SkillInterpreter 子进程内**仍然使用** CognitiveWorker/DomainWorkerPool 编排业务逻辑（隔离层级在外）
- 这是 ADR-0020 §2.2 「v1 = 逻辑隔离」**唯一例外**——SkillInterpreter 提供**进程级隔离**

**与 §6B 自进化的关系**：
- SkillCompiler 编译的 SKILL.md 通过 SkillInterpreter 执行（即 Phase 3 编译 → Phase 4 执行的真实路径）
- 自进化组件可选择将生成的 Skill **通过 SkillInterpreter 隔离执行**以避免污染主进程

### 6B. 自进化与协作模式的关系分析

> **本节是协作的深度补充**：§十一仅列出 ADR-0061 子项的 ship 状态，**未揭示它们与前 6 章基础协作模式的双向耦合**。本节基于 [ADR-0061-03](../adr/skill/adr-0061-03-skill-compiler.md) / [0061-08](../adr/skill/adr-0061-08-aflow-search.md) / [0061-09](../adr/skill/adr-0061-09-gepa-loop.md) / [0061-13](../adr/skill/adr-0061-13-distillation-output-format.md) 状态行 + 实证代码还原完整图景。

#### 6B.1 自进化管线的四阶段（ADR-0061 + ADR-0086）

```
┌────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ Phase 1    │    │ Phase 2      │    │ Phase 3      │    │ Phase 4      │
│ SKILL.md   │ →  │ DSL Workflow │ →  │ C++ Compile  │ →  │ Wasm Binary  │
│ (Anthropic │    │ (Generate-   │    │ (Skill-      │    │ (wasi-sdk,   │
│ Skills 对齐)│    │ Subgraph 生成)│   │ Compiler V1) │    │ ADR-0061-05) │
│ ADR-0061-01│    │ ADR-0061-06+ │    │ ADR-0061-03  │    │ ADR-0061-11  │
└────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
       ↑                  ↑                  ↑
       │ 自检              │ MCTS 搜索        │ 蒸馏
       │ ADR-0061-02       │ ADR-0061-08      │ ADR-0061-13
       │ (T14 ship)        │ (T20 ship)       │ (Phase 0-2 ship)
       │                   │
       └───────────────────┴── 反思 (GEPA, T19 ship, ADR-0061-09)
```

**关键洞察**：4 个阶段不是单向流水线，是**自反馈环**——Phase 2 的 MCTS 搜索结果会生成新的 DSL 候选，回流到 Phase 1 SKILL.md；Phase 3 编译后的 C++ 通过 `SkillCompiler` 重新生成 CompiledSkill，反哺 Phase 2 DSL 模板。

#### 6B.2 自进化组件与基础协作模式的双向耦合矩阵

> **本表区分「ADR-0060 协作模式」与「运行时宿主」两列的真实状态**：v1 自进化组件**全部为同步单进程实现**，**未通过 ⑤ parallel 多 worker 编排**（详见 §6B.3 注记）。

| 自进化组件 | ADR | 调用方协作模式 | 内部协作模式 | 运行时宿主（v1 实证） |
|---|---|---|---|---|
| **SkillCompiler** | 0061-03 | 外部程序调用 `compile(skill_path)` 走 ① call | emit `skill.compilation.{started,succeeded,failed}`（ADR-0068 v1.2.2）| 纯函数式同步调用（`skill_compiler.h:3` "V1 pure functional"）；**无 CognitiveWorker**（grep 实证 src/ 中 CognitiveWorker 零引用） |
| **MCTSWorkflowSearch** | 0061-08 | 外部程序调用 `search()` 走 ① call | emit `mcts.search.{started,iteration,completed,failed}`（ADR-0068 v1.5）| **同步单线程** `search()`（`mcts_workflow_search.h:186`）；**零 CognitiveWorker 引用 / 零 DomainWorkerPool 引用**（grep 实证） |
| **GEPALoop** | 0061-09 | 外部程序调用 `reflection_loop()` 走 ① call | emit `gepa.reflection.{started,completed,failed}` + `gepa.commit.{proposed,committed,denied}`（**精确 6 主题**，ADR-0068 L225-230）| **同步单 agent** 反思循环（`gepa_loop.h:24` "single-agent sync reflection loop"）；**零 CognitiveWorker / DomainWorkerPool 引用** |
| **MutationGovernor** | 0084 | 接受 `mutation.proposed` 事件 → 走 ④ commit 链路 | emit `mutation.{proposed,committed,reverted,denied}`（ADR-0068 v1.2） | 与总线共享 bus（独立模块，非 CognitiveWorker） |
| **IEvaluator** | 0083 | 注入 CognitiveWorker / DomainWorkerPool → 任务完成后评估 | emit `evaluation.result`（ADR-0068 v1.2） | 通过 `set_evaluator()` 注入 worker（§6A 已 ship），自进化组件通过 `evaluator_->evaluate()` 调用 |
| **SkillRegistry** | 0061-03 §决策 4 | 走 ③ emit/subscribe 注册/重载 | （V1 未实施，注册触发器 deferred V2） | 外部调用方编排 |
| **Trajectory IR** | 0061-06 v1.1 | 走 ③ emit `dsl.call.*` 捕获轨迹 → 独立序列化 | `trajectory.*` 主题（ADR-0068 deferred，待 v1.1 完整注册） | DSLEngine 内部捕获 + AppendOnlyEventLog（ADR-0080） |
| **DistillationWriter** | 0061-13 | 走 ③ emit 触发 capture → 写 JSONL | emit `event_log.capture_mode_downgrade`（ADR-0068 v1.7） | AppendOnlyEventLog 旁路；**CaptureMode 三态**：Online/Training/Off + Training fail-open（ADR-0080 v1.2 D10 解耦） |
| **BehavioralRegression** | 0061-02 (T14) | 外部调用 `run_regression()` 走 ① call | （V1 简化为指纹比对，无独立事件主题）| **自由函数库**（`compute_fingerprint` + `hotelling_t2_test`，`tests/test_mcts_workflow_search.cpp` 调用）；无 worker 编排 |

#### 6B.3 自进化与 §6A 双 Worker 模型的关系

> **关键发现（2026-09-02 Oracle session `ses_f9e927788ffeFwJ26EQHrm8YT7` 实证）**：v1 自进化组件**未通过 CognitiveWorker / DomainWorkerPool 编排**——它们都是同步单线程实现，注入 IEvaluator / IMutationGovernor / IInteractionBus 即可。**§6A 双 Worker 模型与 §6B 自进化组件是正交关系**，不是「自进化子运行在子上子子」。

| 自进化阶段 | 与 §6A 双 Worker 的实际关系 |
|---|---|
| **SkillCompiler.compile()** | 纯函数同步调用，**不持有** CognitiveWorker（`skill_compiler.h:3` 纯函数式，无 runtime）；5 轴 TemplateEngine **deferred V2**（[ADR-0061-03 §实施 L97](../adr/skill/adr-0061-03-skill-compiler.md)） |
| **MCTSWorkflowSearch.search()** | 同步单线程编排（`mcts_workflow_search.h:186`）；通过 `evaluator_->evaluate()` + `governor_->commit()` 接入自进化基础设施，**无 worker 多实例** |
| **GEPALoop.reflection_loop()** | 同步单 agent 反思（`gepa_loop.h:24`）；同 MCTS 模式 |
| **MutationGovernor.commit()** | 总线监听 `mutation.proposed` → 走评估门 → emit `mutation.committed/reverted/denied`；与 CognitiveWorker 共享 bus 但不持有引用 |
| **BehavioralRegression.run()** | 自由函数库；外部调用方可选择串行或 `std::async` 并行（**未与 §6A worker 集成**） |

**关键洞察（修正）**：自进化组件通过 `IEvaluator` + `IMutationGovernor` + `IInteractionBus` **3 个独立接入点**（非 §6A worker 宿主）实现「评估门 + 治理门 + 事件审计」三道关卡。**§6A worker 与 §6B 自进化是正交架构层**——前者承载业务 Agent 运行时，后者承载 Agent 自身的演进治理。两者通过总线共享事件流，通过 IEvaluator 共享评估语义，但**不共享线程模型**。

#### 6B.4 自进化的实施现状

**已 ship 的组件**（截至 2026-08-29，与 active-status.md / README 一致）：

| ADR | 组件 | Ship 日期 | 实证 | 文档位置 |
|---|---|---|---|---|
| 0061-02 | BehavioralRegression Suite | T14, 2026-08-25 | 6 cases / 13 assertions PASS | §11.6 ✅ |
| 0061-03 | SkillCompiler V1 | T17, 2026-08-27 | 15 cases / 61 assertions PASS | §11.4 ✅ |
| 0061-06 v1.1 | Trajectory IR | T15, 2026-08-27 | 9 cases / 55 assertions PASS | §11.3 ✅ |
| 0061-08 | MCTSWorkflowSearch V1 | T20, 2026-08-28 | 17 cases / 65 assertions PASS | §11.1 ✅ |
| 0061-09 | GEPALoop V1 | T19, 2026-08-27 | 14 cases（per ADR-0071 L696） | §11.2 ✅ |
| 0061-13 | DistillationWriter V1 | Phase 0-2, 2026-08-29 | 21 cases PASS | §11.5 ✅ |

所有 ADR-0061 ship 状态均在 §11.1-§11.6 覆盖；本轮同步增补 0061-02/0061-13 已消除先前文档脱钩。

#### 6B.5 自进化的关键不变量（跨组件共享）

1. **评估门不可绕过**：所有 mutation 在 commit 前必须通过 IEvaluator + BehavioralRegression Gate（ADR-0061-02 + ADR-0083 + ADR-0084）
2. **事件审计完整**：所有自进化操作产生 ADR-0068 注册主题事件（emission-only + mutation 终态）
3. **fail-closed 默认**：GEPALoop / MutationGovernor / SkillCompiler 失败时全部 fail-closed（无 silent skip）
4. **人类可中断**：append-only event log + capture-mode 三态（ADR-0080 v1.2 D10 解耦：Online/Training/Off + Training fail-open 三重保护）保证所有自进化可被人类回滚
5. **正交于 §6A 运行时**：自进化组件不自建线程模型，通过 IEvaluator + IMutationGovernor + IInteractionBus 3 个独立接入点与 CognitiveWorker/DomainWorkerPool 共享事件流与评估语义（详见 §6B.3）

#### 6B.6 自进化的未来轴（ADR-0061-08-V1.1 Axis6 cognitive_domain composition chain）

**Axis6 = 第 6 维度**（`ADR-0061-08-V1.1` 第 6 维度的 cognitive_domain composition chain）：
- 前 5 轴已 ship：Axis1Template（工作流结构）/ Axis2Param（LLM 参数）/ Axis3Tool（工具选择）/ Axis4Control（控制流）/ Axis5Error（错误处理）——MCTS 搜索的 5 维度空间
- Axis6 新增"认知域组合链"作为第 6 维度（节点级属性）
- `CognitiveDomainChainConfig` 配置 + 单主体 commit 路径
- 与 ADR-0068 v1.8 归口 + W4 双发射语义分离（governance `mutation.*` + axis6 专属 `axis6.*`）

**含义**：未来自进化不仅在「模板实例化」层面搜索，还在「认知域组合顺序」层面搜索。即 MCTS 不只选工作流结构，还选 thinking 阶段的认知域组合。

---

### 6C. Session 层级与协作（运行时第三维度）

> **本节简要补充**：§2.1 §2.2 多次引用 `SubtaskSession + ExecutionSession`，但**未说明 Session 层级在协作中的精确角色**。本节依据 [ADR-0033](../adr/adr-0033-session-hierarchy.md) + [ADR-0079](../adr/adr-0079-unified-session-4scope.md) v1.1 还原。

#### 6C.1 三层 Session 模型（ADR-0033）

| 层 | 抽象 | 角色 | 协作中的位置 |
|---|---|---|---|
| **UserSession** | 顶层用户会话 | 持有 `deque<TaskSession>` + `vector<ToolResult> messages` + `current_task_session_` | 跨 TaskSession 复用 |
| **TaskSession** | 任务会话 | 持有 `deque<SubtaskSession>` + `shared_ptr<IExecutionPolicy>` + `failure_count_` | 失败重试：3 次 retry 后分裂 NewSession |
| **SubtaskSession** | 原子执行单元 | POD-like，最小 Fork/Join 单位 | Fork/JoinLoop 分支 / ④ delegate 子 Agent |

**容器选择 `std::deque`**（非 vector）：保证地址稳定性，避免 CognitiveWorker 持有的 DSLEngine 引用 SubtaskSession 时被 vector reallocation 失效（Metis F1/F2）。

**失败重试模型**（Oracle R6）：
- `failure_count_` 仅对**可重试错误**递增（`Retry / Timeout / ResourceExhausted`）
- `<3` → KeepSession（同 TaskSession 重试）
- `≥3` → NewSession（TaskSession 分裂，避免污染上下文）

#### 6C.2 4-Scope 模型（ADR-0079 v1.1）— 与三层并存的层级

`ADR-0079` v1.1 引入 `Conversation/Attempt/Step/Execution` 4-Scope + `ConvergenceEntry`，与 ADR-0033 三层**正交叠加**：
- **Conversation 范围**：跨多个 UserSession（如同一用户多日协作）
- **Attempt 范围**：单次 UserSession 内的多次尝试
- **Step 范围**：单次 Attempt 内的多次 step
- **Execution 范围**：单 step 内的多次 tool/LLM 调用
- **ConvergenceEntry**：4-Scope 收敛点（用于跨 scope 信息共享）

#### 6C.3 Session 与协作模式衔接

| 协作模式 | Session 衔接 |
|---|---|
| ① `call` | 调用方 TaskSession 持有结果 `ToolResult` |
| ② `call_async` | 调用方 TaskSession 持有 future + callback；callback 写入 SubtaskSession |
| ③ `emit` | 事件可携带 `session_id` + `node_id` + `branch_id` + `timestamp`（ADR-0068 `session.persisted` 注册载荷） |
| ④ `delegate` | 父 TaskSession 创建子 SubtaskSession（独立生命周期） |
| ⑤ `parallel` | Fork/Join 创建 N 个 SubtaskSession 并行 → Join 合并 |
| ⑥ `open_stream` | Session 持有流句柄（Phase 2） |

#### 6C.4 Session 与 §6A 双 Worker 衔接

- CognitiveWorker 持有 DSLEngine → DSLEngine 持有当前 TaskSession（per-agent isolation）
- DomainWorkerPool worker 处理 `DomainTask.output_key` → 写入 result.data[output_key]（不直接持有 Session 引用）
- Session 持久化走 `SessionManager`（独立组件，详见 [ADR-0079](../adr/adr-0079-unified-session-4scope.md) JSONL 树存储）
- 事件审计走 [ADR-0080](../adr/adr-0080-append-only-event-log.md) `AppendOnlyEventLog`（与 SessionManager 正交：前者审计，后者持久化）

---

## 七、Phase 6 PDK Composition Spike（ADR-0051）

**状态**：✅ Approved (experimental, 2026-07-15 — C19 ship)

**目的**：验证 PDK Agent 互相提供服务的可行性（**PDK 内组合**，Phase 6 正式启动仍需 ADR-0050 §启动条件 5 项）。

### 7.1 G1↔G3 Reference Implementation

```
G1 (coding_assistant, AgentLoopType::React)
 └─ 2-step: invoke G3 → synthesize comment
   ↓
G3 (knowledge_base, retrieval + LLM Q&A)
```

### 7.2 Spike v1 契约（Decision 7）

| 维度 | 规格 |
|---|---|
| **注册模式** | `IToolRegistry::register_tool_function(name, metadata, lambda)` |
| **工具命名** | ADR-0043 slash-only (`knowledge_base/query`, `coding_assistant/review`) |
| **Args 签名** | `unordered_map<string,string> → nlohmann::json` |
| **Error Schema** | 强制 `{success: bool, answer?: string, error?: string}` |
| **Transport** | 进程内 `IToolRegistry::call_tool()` |
| **G3 ToolCategory** | `Execute`, `allowed_layers={Workflow}` |
| **G1 Loop** | `React`, 2-step |

### 7.3 5 Escalation Triggers（嵌套/环路防护）

| Trigger | 条件 | 响应 | 测试 |
|---|---|---|---|
| **T-1** | 嵌套深度 > 2（G1→G3→G3 再加一层） | HARD KILL (`runtime_error`) | `nesting_depth_exceeds_2_kills` |
| **T-2** | 环检测（thread_local call stack 同名工具） | HARD KILL + `cycle_detected_log` | `cycle_detection_kills` |
| **T-3** | G3 session store > 1K entries | Escalation log + 清理警告 | `session_store_size_triggers_1k` |
| **T-4** | G3 error-as-success ratio > 10% | Escalation log + 健康告警 | `error_ratio_triggers_10_percent` |
| **T-5** | 2+ awkward pattern 类别 | ADR-0052 draft proposal | `design_review_trigger` |

### 7.4 关键不变量

- **逻辑隔离非物理隔离**：v1 所有 agent plugin 同一进程、共享地址空间（不是进程级沙箱）
- **ToolCoordinator RAII guard 跨线程无效**：`thread_local` 绑定 jthread worker；跨线程 cycle 不可检测（v1 known limitation，v2 评估全局 cycle graph）
- **G3 MockLLMProvider per-test-instance**：避免 ctest 并行测试数据竞争

### 7.5 ADR-0051 Layer 3 Dual Memos 共识发现

| # | 发现 | 严重度 |
|---|---|---|
| O-1 | LLM callback 签名 `string→string` 不支持 error return | 🔴 P0 |
| O-2 | 魔术字符串伪装有效 answer 流入正常 data flow | 🔴 P0 |
| O-3 | Hardcoded tool name 无编译时检查 | 🟠 P1 |
| O-4 | 缺失 shared contract header（合约在 README 而非 .h） | 🟠 P1 |
| O-5 | LLM callback pattern copy-paste | 🟡 P2 |
| O-6 | `call_tool` 签名保真度不足（string→json 无类型验证） | 🟠 P1 |

**对设计的指导意义**：6 项共识发现推动 DECLARE_SERVICE 宏提案（推迟到 Phase 6 v2+，待 2+ 不同类别 awkward pattern 涌现触发；**注**：早期 ADR-0051 §T-5 前向引用编号 0052 给该提案，但实际 `docs/adr/adr-0052-agent-plugin-manifest.md` 是 Agent Plugin Manifest 规范，全文无 DECLARE_SERVICE 提及。当前 ADR 编号下 DECLARE_SERVICE 尚无独立 ADR 立项）。

---

## 八、Agent Hook 拦截模式（ADR-0081）

**状态**：✅ Approved (2026-08-21)，**V1 仅骨架 ship，Agent loop 集成推迟 Sprint 24+**

**目的**：与 Tool Hook 正交的 Agent 级拦截点。

> ⚠️ **实施现状**：[ADR-0081](../adr/adr-0081-pre-step-hook-contract.md) V1 仅交付 `IAgentHookRegistry` 契约层 + InMemory 参考实现 + 工厂函数（`make_in_memory_agent_hook_registry()`）；**Agent loop 集成（ReactLoop/PlanExecuteLoop/ForkJoinLoop 触发 hook 调用）推迟至 Sprint 24+**。当前代码可在 Agent 框架外独立测试 hook 调度逻辑，但运行 Agent 时 hook 不会被触发。

### 8.1 接口形态

```cpp
struct AgentPreHookResult {
    enum Action { Continue, Deny, ModifyContext } action = Continue;
    std::string deny_reason;
    std::unordered_map<std::string, std::string> modified_context;
};

struct AgentPostHookResult {
    bool modify_result = false;
    std::string modified_output;
};

class IAgentHookRegistry {
public:
    virtual void register_pre_hook(const std::string& agent_glob,
                                   AgentPreHook hook,
                                   int priority,
                                   HookErrorPolicy policy) = 0;
    virtual void register_post_hook(const std::string& agent_glob,
                                    AgentPostHook hook,
                                    int priority,
                                    HookErrorPolicy policy) = 0;
    virtual AgentPreHookResult apply_pre_hooks(
        const IAgent& agent, const std::string& step_input,
        std::vector<std::string>& warnings) const = 0;
    virtual AgentPostHookResult apply_post_hooks(
        const IAgent& agent, const std::string& step_output,
        std::vector<std::string>& warnings) const = 0;
};
```

### 8.2 不变量

- **拦截点不修改 core 行为**：hook 失败/异常不阻断主流程
- **fail-closed 安全语义**：`Deny` 决策不可被后续 hook 覆盖
- **agent_glob 通配**：如 `"react-loop/*"`、`"*"`（与 ADR-0043 tool_glob 约定一致）
- **HookErrorPolicy 复用 ADR-0069**：避免双标准。**当前枚举仅 2 值**（`include/agenticdsl/contract/itool_hook_registry.h:18` — `enum class HookErrorPolicy { FailClosed, FailOpen }`），未来若需 LogAndContinue 需独立 ADR 扩展。

### 8.3 与 ToolHookRegistry 的正交

```
Tool hook:    per-tool 调用（`tools/*`）
Agent hook:   per-agent step（`agent/*`）
调用顺序：    agent step → tool call（hook 触发点不重叠）
```

完整位置：`include/agenticdsl/contract/iagent_hook_registry.h`

---

## 九、Cross-Cutting Pattern PDK（ADR-0085）

**状态**：✅ Approved (2026-08-28，Oracle 3 轮复审全部通过) + **V1 ship ✅ 2026-08-28（T26，18 cases PASS）**

**目的**：4 种横切范式 PDK Pattern（类比 PDK Loop Agent 模式），AOP 风格的横切关注点统一抽象。

```cpp
// include/agenticdsl/pdk/cross_cutting/ — V1 已 ship
ICrossCuttingPattern          // 统一抽象（name() + apply(json, ctx)）
CrossCuttingOrchestrator       // 无状态 dispatcher（运行期 JSON 分发）
decorator_pattern / hook_pattern / composition_pattern / bus_pattern   // 4 Pattern class
*.cc.md                        // 横切功能 DSL（YAML 格式，类比 *.agent.md）
```

**与 Loop Agent 的真实类比**（[ADR-0085 §决策 5](../adr/adr-0085-cross-cutting-pattern-pdk.md#决策-5--v1-不强制-meta-agent-自管理)）：
- `LoopResult` ↔ `ICrossCuttingPattern`（统一抽象接口）
- `ReactLoop` / `PlanExecuteLoop` / `ForkJoinLoop` ↔ `DecoratorPattern` / `HookPattern` / `CompositionPattern` / `BusPattern`（4 独立 class）
- `LoopDispatcher<>` 编译期模板特化 ↔ `CrossCuttingOrchestrator` 运行期 JSON 分发（关键差异：横切功能运行时按需启用，非编译期模板）
- `*.agent.md` DSL ↔ `*.cc.md` DSL（YAML 格式，schema 校验复用 ADR-0073）

**类型识别方式**：4 个 Pattern 通过**字符串常量**识别（[ADR-0085 §决策 1](../adr/adr-0085-cross-cutting-pattern-pdk.md#决策-1--4-范式独立-pdk-pattern-class)）：

```cpp
namespace hydraforge::pdk::cross_cutting_pattern {
    constexpr const char* Decorator   = "decorator-v1";
    constexpr const char* Hook        = "hook-v1";
    constexpr const char* Composition = "composition-v1";
    constexpr const char* Bus         = "bus-v1";
}
```

> ⚠️ **注意**：不存在 `DEFINE_CROSSCUTTING` 宏，亦不存在 `CrossCuttingPatternType` 枚举。Orchestrator 通过运行时 JSON 配置中的 `type` 字段（值如 `"decorator-v1"`）匹配 Pattern，非编译期模板特化。代码位置：`include/agenticdsl/pdk/cross_cutting/{icross_cutting_pattern,decorator_pattern,hook_pattern,composition_pattern,bus_pattern,cross_cutting_orchestrator,cross_cutting_config}.h`（7 个头文件，V1 全 ship）。

---

## 十、信用分配契约（ADR-0086）

**状态**：🔍 Proposed (2026-08-31 — self-evolution §七 #6 立项)

**目的**：协作链路的信用归因，**Axis6 cognitive_domain composition chain** 的前置（`adr-0061-08-v1-1-amendment-axis6.md`）。

### 10.1 评估层 vs 归因层划界

| 层 | 抽象 | 位置 |
|---|---|---|
| **评估层** | `IEvaluator` / `RewardSignal` | ADR-0083 |
| **归因层** | `AttributionRecord` / VersionPairDiff | ADR-0086 |

### 10.2 V1 归因方法

- **VersionPairDiff V1**：通过版本对比计算归因
- **ConfounderRecord**：混杂分层记录
- **4 态判定**：
  - `Attributed` ← 成功归因
  - `Confounded` ← 混杂干扰
  - `Insufficient` ← 数据不足
  - `NotAttempted` ← 未尝试
- **默认 fail-closed**

---

## 十一、进化期协作（ADR-0061 Skill 子项）

Agent 协作不仅是运行时，还有**进化期**协作。本章**仅列出 ship 状态**；与基础协作模式的双向耦合关系分析详见 [§6B](#6b-自进化与协作模式的关系分析)，运行时宿主（思考/执行层）映射详见 [§6B.3](#6b3-自进化与双-worker-模型的精确对应)。

### 11.1 MCTS Workflow Search（ADR-0061-08 + V1.1 Axis6）

- `MCTSWorkflowSearch` (T20 ship, 2026-08-28, 17 cases / 65 assertions PASS)
- 基于 Monte Carlo Tree Search 搜索工作流空间
- Axis6 = 第 6 维度（cognitive_domain composition chain，节点级属性）
- commit/revert 事件语义统一（[ADR-0061-08-V1.1 决策 6](../adr/skill/adr-0061-08-v1-1-amendment-axis6.md)）+ W4 双发射语义分离（governance 层 `mutation.*` + axis6 专属 `axis6.*`）
- 关联变更：`openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` v2.1

### 11.2 GEPA Loop（ADR-0061-09）

- `GEPALoop` (T19 ship, 2026-08-27, 14 cases PASS)
- 反思循环：agent 自反思 → 改进策略 → 重试
- 类比 GEPA（Genetic-Pareto）的多目标反思

### 11.3 Trajectory IR（ADR-0061-06 V1.1）

- 独立的轨迹序列化视图
- 不改 `ParsedGraph`（v1 标题耦合修正）
- T15 ship 2026-08-27（9 cases / 55 assertions PASS）

### 11.4 Skill Compiler（ADR-0061-03）

- `SkillCompiler` 实施
- T17 V1 ship 2026-08-27（15 cases / 61 assertions PASS）

### 11.5 Distillation Output Format（ADR-0061-13）

- `IDistillationWriter` + `DistillationRecord` + trajectory/policy/meta 三文件分离
- Phase 0-2 ship 2026-08-29（21 cases PASS, OpenSpec `capture-mode-and-distillation-writer-v1` archived）
- v1.7 事件 `event_log.capture_mode_downgrade` 注册于 ADR-0068 Appendix A

### 11.6 Behavioral Regression Suite（ADR-0061-02）

- AgentAssay-style 行为回归套件
- T14 ship 2026-08-25（6 cases / 13 assertions PASS）
- v1 简化为指纹比对，与 IEvaluator V2（ADR-0083）协同

---

## 十二、LLM-native 协作架构（ADR-0071）

**状态**：✅ Approved (2026-08-02 顶层方向 ADR；Promotion 评审通过 2026-08-25)

**3 平面协作架构**：

```
Operator Plane   ─── 人类 + LLM Operator
       ↓
DSL Plane        ─── 中间表示（LLM 生成的 DSL）
       ↓
Backend Plane    ─── 多 inference backend 协同
```

派生 **6 个子 ADR**（[ADR-0071 §下游派生](../adr/adr-0071-llm-native-agenticdsl-architecture.md#下游派生-待创建-本-adr-列举) L29-35），锚定 Phase 6+ 演化方向：

- **ADR-0072** DSL 节点扩展（try/catch / `backend:` / `$var` / declarative style）— Wave 2 GATED
- **ADR-0073** Tool JSON Schema 契约（JSON Schema 2020-12, nlohmann validator）— Wave 2 FIRST
- **ADR-0074** Prompt Engineering + Evidence Gate — Wave 2 SECOND
- **ADR-0075** EnvBackend 多环境执行（Local + Docker first）— Wave 3
- **ADR-0076** DSL Engine as MCP Server（控制面）— Wave 3，gated by Candidate B 启动条件
- **ADR-0077** gRPC Data Plane（数据面，**descoped pending consumer**）— Wave 4

> ⚠️ **ADR-0078** Fine-tune 基模选型**不在 ADR-0071 派生清单内**——按 [ADR-0071 §D9](../adr/adr-0071-llm-native-agenticdsl-architecture.md#d9-训练基模延后决策) 决议，ADR-0078 需 AgenticMind 项目 fine-tune 探索完成后**独立新建**，不在 Wave 2-4 派生路径上。

---

## 十三、关键设计原则总结

### 13.1 透明路由 vs 显式 backend

> ⚠️ **「透明路由」v1 落地范围**：v1 仅在本地 backend 透明（`call_tool` 不感知 PDK Plugin vs SKILL 差异）；ADR-0060 决策 2 的全量路由（`CapabilityRegistry`/`RemoteRegistry`/`RemoteAgentAdapter`/`WasmRuntime`）属 Phase 2 愿景，全库零匹配，详见 [§2.3](#23-透明路由adr-0060-决策-2--phase-2-愿景)。

```cpp
// 推荐：本地透明调用（v1 ✅，ADR-0060 决策 4 进程内范围）
auto result = call_tool("loop/run", args);  // 一致 API（本地 backend 透明）

// 不推荐：显式 backend 选择
auto result = orchestrator.parallel("code_review", tasks);  // 与 call 重叠
auto result = call_remote_agent("loop/run", args);          // Phase 2 才需暴露 transport
```

### 13.2 进程内 vs 进程间统一 API

```cpp
// 进程内（v1 已 ship）
auto result = call_tool("loop/run", {prompt, tools});

// 进程间（Phase 2 — ADR-0059 落地后）
auto result = remote.call_tool("loop/run", {prompt, tools}); // 同一 API
```

### 13.3 父子关系 vs 并行聚合

> ⚠️ **代码示意 ADR-0060 决策 1 目标 API**；v1 实际签名与限制见 [§2.1 B](#21-api-形态adr-目标-vs-v1-实际-ship)（`delegate` 仅 3 参数无 monitor/max_lifetime_ms；无统一 `parallel()` 方法）。

```cpp
// delegate: 长任务、监控、取消（父持有子句柄）—— 目标 API
delegate(SubAgentSpec{.max_lifetime_ms=60000}, monitor);

// parallel: 批量并行、独立任务（对等关系）—— 目标 API
registry.parallel("code_review", [t1, t2, t3]);
```

### 13.4 Hook vs Tool 重构

- **能通过 Tool 重写的逻辑** → 不引入 Agent Hook
- **需要 per-agent step 拦截的逻辑** → Agent Hook
- **需要 per-tool call 拦截的逻辑** → Tool Hook（ADR-0069）

### 13.5 物理隔离 vs 逻辑隔离

- **v1（ADR-0051 Spike）**：进程内协作 + jthread per-agent 线程隔离（ADR-0020）；**例外**：`SkillInterpreter` 提供进程级物理隔离（`include/agenticdsl/skill/skill_interpreter.h`，Sprint 22 V1 ship，ADR-0066 🟡 Partial，V2 deferred）—— SKILL.md 执行通过 `posix_spawn` + execve(/proc/self/exe, --skill-child) + seccomp(BPF) + pipe IPC 实现子进程隔离
- **Phase 2+**：进程/容器隔离 + DECLARE_SERVICE 宏（**尚无独立 ADR 立项**，详见 [§7.5](#75-adr-0051-layer-3-dual-memos-共识发现) / [§十四](#十四实施优先级建议)）

---

## 十四、实施优先级建议

| 优先级 | 模式 / 组件 | 状态 | 适用场景 |
|---|---|---|---|
| ⭐⭐⭐ | `call` + `emit` + `parallel` | v1 ✅（`parallel` 由 `ForkJoinLoop` + `DomainWorkerPool` 组合承担，非统一 `parallel()` 方法） | 几乎所有场景 |
| ⭐⭐⭐ | 3 Agent Loop（React/PlanExec/ForkJoin） | ✅ Sprint 20 ship | 单 Agent 任务 |
| ⭐⭐⭐ | CognitiveWorker ↔ DomainWorkerPool 双 worker 模型（§6A） | ✅ Sprint 2-3 ship | 运行时基底 |
| ⭐⭐ | `call_async` | v1 ✅ | 长任务、防阻塞 |
| ⭐⭐ | Agent Hook（ADR-0081） | ✅ 骨架（V1 ship；loop 集成推迟 Sprint 24+） | 跨 Loop 拦截、治理 |
| ⭐⭐ | 4 通道 Plugin 通信（ADR-0046） | 🔍 提案 | 多 Plugin 协作 |
| ⭐⭐ | GenerateSubgraphNode + PlanExecute 动态子图生成（§5A） | ✅ v3.1 ship | LLM 驱动协作 |
| ⭐ | `delegate` | v1 ✅（仅 3 参数 `IAgentComposition::delegate`，无 monitor callback） | 父子任务、需要取消 |
| ⭐ | Cross-Cutting Pattern（ADR-0085） | ✅ V1 ship 2026-08-28（T26，18 cases） | AOP 风格横切 |
| ⭐ | MCTS / GEPA / SkillCompiler / TrajectoryIR / Distillation（ADR-0061） | ✅ V1 ship T14/T15/T17/T19/T20/Phase 0-2 | Agent 进化期协作 |
| ⭐ | SkillCompiler 5 轴 TemplateEngine | 🔍 deferred V2（[ADR-0061-03 §实施 L97](../adr/skill/adr-0061-03-skill-compiler.md)；T17 V1 ship 仅含纯函数式编译 + 6 字段 metadata + IEvaluator 质量门 + G11 emit-only） | SKILL → DSL 编译 |
| 🔮 | `open_stream` | Phase 2（`IAgentComposition::stream` 抛 `logic_error`） | LLM token 流 |
| 🔮 | DECLARE_SERVICE | Phase 6 v2+，尚无独立 ADR 立项 | 跨进程服务化 |
| 🔮 | Axis6 cognitive_domain composition chain | ADR-0061-08-V1.1 ✅；实施载体 v2.1 active | 第 6 维自进化 |

---

## 十五、关联 ADR 索引

| ADR | 议题 | 状态 |
|---|---|---|
| [ADR-0019](../adr/adr-0019-iinteraction-bus-mvp.md) | IInteractionBus 接口与 TUI Chat MVP | 🟡 Partial |
| [ADR-0020](../adr/adr-0020-thread-model-isolation.md) | 多智能体线程模型与隔离策略 | ✅ |
| [ADR-0021](../adr/adr-0021-pdk-design.md) | PDK 设计 | ✅ |
| [ADR-0022](../adr/adr-0022-plugin-loading.md) | 插件加载机制 | ✅ |
| [ADR-0023](../adr/adr-0023-tool-result-standard.md) | ToolResult 标准化 | ✅ |
| [ADR-0033](../adr/adr-0033-session-hierarchy.md) | 会话层次结构 | ✅ |
| [ADR-0034](../adr/plugin/adr-0034-model-router.md) | IModelRouter 模型路由接口 | ✅ |
| [ADR-0035](../adr/adr-0035-inference-engine-plugin-spec.md) | 推理引擎 PDK Plugin 规范 | ✅ |
| [ADR-0043](../adr/adr-0043-pdk-tool-naming-convention.md) | PDK 工具命名约定 | ✅ |
| [ADR-0045](../adr/adr-0045-orchestration-plugin-spec.md) | 编排 PDK Plugin 规范 | 🔍 |
| [ADR-0046](../adr/adr-0046-plugin-communication-protocol.md) | PDK 插件间通信协议 | 🔍 |
| [ADR-0050](../adr/adr-0050-phase6-strategic-evaluation.md) | Phase 6 战略评估 | ✅ |
| [ADR-0051](../adr/adr-0051-phase6-pdk-composition-spike.md) | Phase 6 PDK Composition Spike | ✅ |
| [ADR-0053](../adr/adr-0053-agent-descriptor-interface.md) | Agent Descriptor 接口 | ✅ |
| [ADR-0054](../adr/adr-0054-capability-discovery.md) | Capability Discovery | ✅ |
| [ADR-0060](../adr/adr-0060-agent-composition.md) | Agent 组合协议与声明式编排 | ✅ |
| [ADR-0066](../adr/adr-0066-skill-interpreter-arch.md) | SkillInterpreter 架构（§13.5 物理隔离例外引用；V1 ship 2026-07-22, V2 deferred） | 🟡 |
| [ADR-0068](../adr/adr-0068-event-emission-contract.md) | 事件发射契约 | ✅ |
| [ADR-0069](../adr/adr-0069-tool-coordinator-hooks.md) | ToolCoordinator Hook | 🟡 |
| [ADR-0071](../adr/adr-0071-llm-native-agenticdsl-architecture.md) | LLM-native AgenticDSL 架构 | ✅ |
| [ADR-0072](../adr/adr-0072-dsl-node-extensions.md) | DSL 节点扩展（§十二 ADR-0071 派生；Wave 2 GATED by Evidence Gate） | 🔍 |
| [ADR-0073](../adr/adr-0073-tool-json-schema-contract.md) | Tool JSON Schema 契约（§十二 ADR-0071 派生；JSON Schema 2020-12） | 🟡 |
| [ADR-0074](../adr/adr-0074-prompt-evidence-gate.md) | Prompt Engineering + Evidence Gate | ✅ |
| [ADR-0075](../adr/adr-0075-env-backend-local-docker.md) | EnvBackend 多环境执行 | ✅ |
| [ADR-0076](../adr/adr-0076-dsl-engine-mcp-server.md) | DSL Engine as MCP Server 控制面（§十二 ADR-0071 派生；Wave 3, gated by Candidate B） | 🔍 |
| [ADR-0077](../adr/adr-0077-grpc-data-plane.md) | gRPC Data Plane 数据面（§十二 ADR-0071 派生；Wave 4 descoped pending consumer） | 🔍 |
| [ADR-0079](../adr/adr-0079-unified-session-4scope.md) | 统一会话模型与 4-Scope 存储（Conversation/Attempt/Step/Execution） | ✅ |
| [ADR-0080](../adr/adr-0080-append-only-event-log.md) | AppendOnlyEventLog 作为核心审计日志（v1.1 D10 Distillation Capture） | ✅ |
| [ADR-0081](../adr/adr-0081-pre-step-hook-contract.md) | Pre-Step Hook Contract（V1 骨架 ship，loop 集成推迟 Sprint 24+） | ✅ |
| [ADR-0082](../adr/adr-0082-agent-first-class-registry.md) | Agent as First-Class Registry | ✅ |
| [ADR-0083](../adr/adr-0083-evaluator-reward-contract.md) | IEvaluator/RewardSignal 评估契约（§十.1 引用） | ✅ |
| [ADR-0085](../adr/adr-0085-cross-cutting-pattern-pdk.md) | Cross-Cutting Pattern PDK | ✅ |
| [ADR-0086](../adr/adr-0086-credit-assignment-contract.md) | 信用分配契约 | 🔍 |
| [ADR-0061-03](../adr/skill/adr-0061-03-skill-compiler.md) | Skill Compiler | ✅ |
| [ADR-0061-06](../adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md) | Trajectory IR（v1.1 amendment；基准 ADR-0061-06 v1 ⛔ Superseded） | ✅ |
| [ADR-0061-13](../adr/skill/adr-0061-13-distillation-output-format.md) | Distillation Output Format（IDistillationWriter + DistillationRecord） | ✅ |
| [ADR-0061-08](../adr/skill/adr-0061-08-aflow-search.md) | AFlow MCTS 工作流搜索 | ✅ |
| [ADR-0061-08-V1.1](../adr/skill/adr-0061-08-v1-1-amendment-axis6.md) | MCTS Axis6 cognitive_domain | ✅ |
| [ADR-0061-09](../adr/skill/adr-0061-09-gepa-loop.md) | GEPA-style 反思循环 | ✅ |

---

## 十六、相关文档

- [`plugin-and-agent-architecture.md`](./plugin-and-agent-architecture.md) — Plugin 与 Agent 基础架构（必读前置）
- [`developer-guide.md`](./developer-guide.md) — 开发者规范与最佳实践
- [`contract-template.md`](./contract-template.md) — Agent 间交互契约模板
- [`reference.md`](./reference.md) — DSL 语法快速参考
- [`../specs/architecture.md`](../specs/architecture.md) — 五层架构规范（L0~L4 + R1~R5）

---

## 修订记录

| 日期 | 修订 | 依据 |
|---|---|---|
| 2026-09-02 | 初始版本，基于 ADR-0060/0046/0045/0051/0081/0085/0086/0061/0071 综合 | |
| 2026-09-02 | 一致性修正：5 🚨 严重错误 + 7 ⚠️ 中度问题 + 4 💡 轻量建议（详见 Oracle session `ses_f9f0033d5ffeBb7jHMzWPnhrdq`）。关键变更：① §2.1 区分 ADR 目标 API vs v1 实际 ship API（IAgentComposition 4 方法）；② §2.3 标注 4 路由类为 Phase 2 愿景；③ §8.2 删除 `HookErrorPolicy` 虚构 `LogAndContinue`；④ §六 删除 `PlanExecuteLoop verify_phase` 并行虚构；⑤ §九 删除 `DEFINE_CROSSCUTTING` 宏 / `CrossCuttingPatternType` 枚举虚构，补 V1 ship 2026-08-28（T26, 18 cases）注记；⑥ §十二 修正 ADR-0071 派生清单（0072-0077，0078 非派生）；⑦ §十四/§7.5 修复 `DECLARE_SERVICE/ADR-0052` 过期引用；⑧ §2.4 区分 ADR-0060 决策 3 逻辑字段 vs ADR-0068 Appendix A 注册载荷；⑨ §八/§十四 补 Hook V1 骨架 + loop 集成推迟披露；⑩ §十五 补 ADR-0083/ADR-0061-13 索引 | Oracle session `ses_f9f0033d5ffeBb7jHMzWPnhrdq` + 实测源文件 `iagent_composition.h` / `iagent_hook_registry.h` / `plan_execute_loop.h` |
| 2026-09-02 | 二轮审查补漏（Oracle session `ses_f9ebfec9affeg7TSuR90WKBwjr`，评级 B）：① §2.2 修正 `IAgentRegistry` 表述 → v1 实际依赖 `TestDoubleAgentRegistry`（P8 test-double），ADR-0082 接线为后续工作；② §13.3 加 ⚠️ v1 限定标注，与 §2.1 严谨性对齐 | Oracle session `ses_f9ebfec9affeg7TSuR90WKBwjr` + `include/agenticdsl/contract/test_double_registry.h` |
| 2026-09-02 | 三轮审查补漏（Oracle session `ses_f9e6abd56ffeCtK1u6e319zIHZ`，评级 A- → A）：① ⚠️ §二 顶表状态符号冲突：拆分「ADR 决策 4 标注」列与「v1 实现」列 + 脚注；② ⚠️ §13.1 补决策 2 Phase 2 限定（避免误导读者 ADR-0060 决策 2 是 v1）；③ ⚠️ §13.5 补 SkillInterpreter 物理隔离例外（ADR-0066 🟡）+ DECLARE_SERVICE 无立项限定；④ ⚠️ §十五 索引补 7 行：0066/0072/0073/0076/0077/0079/0080（§十二派生 + §6A.6 + §6C 引用）；⑤ 💡 §一「9 维度」→「10 维度」（架构栈图实际 10 框）；⑥ 💡 §一 拦截层补 ADR-0081 V1 骨架限定；⑦ 💡 §2.1 B L152 修正 ForkJoinLoop「按逗号分隔」错误注释 → 真实签名为 `std::vector<std::string>` 直传；⑧ 💡 §2.4 补 `llm.token.done` / `llm.token.error` 主题（ADR-0068 v1.4 真实发射） | Oracle session `ses_f9e6abd56ffeCtK1u6e319zIHZ` |
| 2026-09-02 | **三章新增 + 标题升级**（用户请求：A+A 实施补充 + 独立分析）：① 文档标题升级 `Agent 间协作模式架构指南` → `Agent 协作与运行时架构指南`，范围扩展到运行时基底 + 自进化；② **新增 §5A 动态子图生成**（`GenerateSubgraphNode` 定义 + `PlanExecuteLoop::execute_phase` 调用链 + 与 §5.1 关系修正 + 与 §6B 自进化衔接）；③ **新增 §6A CognitiveWorker ↔ DomainWorkerPool 协作**（双 Worker 对比表 + thinking→execution 协作链 + IInteractionBus F7 顺序契约 + 与 6 协作模式衔接 + 物理/逻辑隔离对比）；④ **新增 §6B 自进化与协作模式的关系分析**（4 阶段管线 + 9 组件 × 3 协作模式耦合矩阵 + 双 Worker 模型精确对应 + ship 现状与文档脱钩 + 5 条关键不变量 + Axis6 未来轴）；⑤ **新增 §6C Session 层级与协作**（ADR-0033 三层 + ADR-0079 4-Scope + 与协作模式衔接 + 与 §6A 双 Worker 衔接）；⑥ §十一 增补 ADR-0061-02/0061-13 ship 状态 + 11.5/11.6 新增子节；⑦ §十四 优先级表新增 4 行（双 Worker、GenerateSubgraph、SkillCompiler 5 轴、Axis6）；⑧ §六与 §6A 互相锚定链接 | 独立分析依据：`src/core/types/node.h:198-207` + `cognitive_worker.cpp:75-87` + `domain_worker_pool.cpp` + `adr-0061-02/03/06/08/09/13` + `adr-0033/0079` 状态行 |
| 2026-09-02 | **三轮审查修正**（Oracle session `ses_f9e927788ffeFwJ26EQHrm8YT7`，评级 C → 修订后 B）：**🚨 严重（4 项）**：① §6B.2 矩阵 3 行虚构耦合——MCTS/GEPA/BehavioralRegression 实际均为同步单线程（`mcts_workflow_search.h:186` / `gepa_loop.h:24` / `compute_fingerprint` 自由函数库），不是「⑤ parallel + 双 Worker」编排；③ §5A.2 调用链叙事错误——PlanExecuteLoop 路径**不经过** GenerateSubgraphNode（两者仅共享 `engine_->continue_with_generated_dsl` 入口）；④ §6B.4 vs §11.5/11.6 自相矛盾——脱钩列改为「§11.5/§11.6 ✅」，删除原建议句；⑥ §十四 SkillCompiler 5 轴标为 deferred V2（[ADR-0061-03 L97](../adr/skill/adr-0061-03-skill-compiler.md)），非 T17 ship 内容；⑦ §6A.1 ReactLoop 不走 CognitiveWorker 实证披露；⑧ §6A.5 ADR-0056 误标——物理隔离属 ADR-0055/0066，SkillInterpreter V1 已 ship seccomp；⑨ §6C.4 SessionManager 是 ADR-0079 域，非 ADR-0080；⑩ §6B.6 五轴名称按 `ADR-0061-08-V1.1` 实证改为 Axis1Template/Axis2Param/Axis3Tool/Axis4Control/Axis5Error；⑪ **§6A.6 SkillInterpreter 物理隔离专拆**（采纳 Oracle 建议）；⑫ §一 核心图同步新增「动态子图 / 双 Worker 运行时基底 / Session 三层」3 行 | Oracle session `ses_f9e927788ffeFwJ26EQHrm8YT7` + `src/modules/cognitive/{mcts_workflow_search,gepa_loop}.{h,cpp}` + `include/agenticdsl/skill/skill_interpreter.h` + ADR-0061-03 §实施 L97 |

> **维护责任**：架构组（Sprint 24 pre-launch governance）
> **审查频率**：每 Sprint 启动时检查 ADR 状态变化（Proposed → Approved → Archived）