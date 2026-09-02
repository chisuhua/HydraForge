# Agent 间协作模式架构指南

> **目的**：快速理解 HydraForge 中 **Agent 与 Agent 之间** 的协作模式架构、协议抽象与设计取舍。
> **范围**：覆盖 6 种协作模式、4 通道 Plugin 通信、3 种 Agent Loop 协作、Phase 6 服务组合探索、Hook/横切/归因/进化协作。
> **关系**：与 [`plugin-and-agent-architecture.md`](./plugin-and-agent-architecture.md) 配套 —— 后者讲"Plugin 与 Agent 是什么/怎么构建"，本指南讲"多个 Agent 如何协作"。
> **依据**：ADR-0060（协作模式）/ ADR-0046（Plugin 通信协议）/ ADR-0045（编排 Plugin）/ ADR-0021（Agent Loop）/ ADR-0051（Phase 6 服务组合）/ ADR-0081（Hook）/ ADR-0085（横切）/ ADR-0086（信用分配）/ ADR-0061（进化）/ ADR-0071（LLM-native）。

---

## 一、核心结论

Agent 间协作不是单一抽象，而是**多层、6 维度**的体系：

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
│ 并发原语：DomainWorkerPool（jthread + FIFO 队列）       │
├────────────────────────────────────────────────────────┤
│ 服务层：Phase 6 PDK Composition（ADR-0051）             │
│   G1↔G3 reference impl + 5 escalation triggers         │
├────────────────────────────────────────────────────────┤
│ 拦截层：Agent Hook（ADR-0081）                          │
│   pre-step / post-step + agent_glob 路由               │
├────────────────────────────────────────────────────────┤
│ 横切层：Cross-Cutting Pattern（ADR-0085）               │
│   ICrossCuttingPattern + *.cc.md                       │
├────────────────────────────────────────────────────────┤
│ 归因层：Credit Assignment（ADR-0086）                   │
│   VersionPairDiff + 4 态判定                           │
├────────────────────────────────────────────────────────┤
│ 进化层：MCTS / GEPA / Trajectory IR（ADR-0061）         │
└────────────────────────────────────────────────────────┘
```

**关键设计哲学**：

1. **统一抽象**：Agent 开发者只需学一次 `IToolRegistry` API，路由层透明选择进程内/进程间/跨框架
2. **正交分层**：Tool Hook vs Agent Hook / Tool Layer vs Event Layer —— 每一层独立，避免双标准
3. **物理 vs 逻辑隔离**：v1 进程内协作 + jthread per-agent 线程隔离；Phase 2+ 进程/容器隔离
4. **安全优先**：fail-closed 默认 + 5 escalation triggers + RAII guard 嵌套防护 + ToolCoordinator 审批
5. **可演进**：从 v1 同步 RPC → 异步 → pub/sub → 父子 → 并行 → 流式，逐步开放能力

---

## 二、6 种 Agent 协作模式（ADR-0060）

**ADR-0060 决策 1**：协议无关的协作模式，由 `IToolRegistry` 透明路由。

| # | 模式 | v1 实现 | 进程内机制 | 进程间机制 | 适合场景 |
|---|---|:---:|---|---|---|
| ① | `call(req) → response` | ✅ | `IToolRegistry::call_tool()` | MCP `tools/call` | 同步 RPC、叶子工具调用 |
| ② | `call_async(req, cb)` | ✅ | `bus.emit + subscribe(req_id)` | MCP + notifications | 异步 RPC、回调式 |
| ③ | `emit(topic, payload)` | ✅ | `IInteractionBus::emit/subscribe` | MCP `notifications` | pub/sub、多对多广播 |
| ④ | `delegate(spec, monitor)` | ✅ | `SubtaskSession + ExecutionSession` | MCP `tasks/create + tasks/get` | 父子关系、长生命周期子 Agent |
| ⑤ | `parallel(tasks, opts)` | ✅ | `DomainWorkerPool` + `execute_parallel` | MCP `tasks/create × N + tasks/get` | fork/join 并行聚合 |
| ⑥ | `open_stream(handler)` | ⏳ Phase 2 | 流式订阅（token-by-token） | MCP + SSE streaming | LLM token 流 |

### 2.1 实际代码 API 形态

```cpp
// ① 同步 RPC
auto result = call_tool("loop/run", {prompt, tools});

// ② 异步 RPC（进程内）
bus->emit("chat.request", {prompt, tools, request_id});
bus->subscribe("chat.response." + request_id, [this](auto& e) { ... });

// ③ pub/sub
bus->emit("user.input", {text: "..."});
bus->subscribe("user.input", [](auto& e) {
    auto result = call_tool("loop/run", {prompt: e["text"]});
    bus->emit("loop.response", result);
});

// ④ 委派子 Agent（父-子关系：父持有子句柄、监控生命周期）
auto sub_id = orchestrator.delegate(
    SubAgentSpec{.agent_id="code.review", .task={...}, .max_lifetime_ms=60000},
    [](auto& event) {
        if (event.type == "done")   { /* 子 Agent 完成 */ }
        if (event.type == "error")  { /* 子 Agent 出错 */ }
        if (event.type == "progress") { /* 子 Agent 进度 */ }
    }
);

// ⑤ fork/join 并行（任务对等：无父子关系）
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

### 2.2 关键对比

| 维度 | ④ delegate | ⑤ parallel |
|---|---|---|
| 关系 | **父-子**（父持有子句柄） | **对等**（所有任务同级） |
| 生命周期 | 长（max_lifetime_ms） | 短（每个任务完成即结束） |
| 监控 | callback 接收 done/error/progress | `on_each_complete` 回调 |
| 隔离 | 子 Agent 独立 TaskSession | 无独立 Session |
| 适用 | 长任务、需监控、需取消 | 批量并行、独立子任务 |

### 2.3 透明路由（ADR-0060 决策 2）

```cpp
ToolRegistry::call_tool(name, args):
    1. CapabilityRegistry.query(name) → agent_id + metadata
    2. RemoteRegistry.is_remote(agent_id) 判断 backend
    3. 本地 backend:
       ├── PDK Plugin (C++) → 直接调用 .so
       ├── SKILL → SkillInterpreter
       └── Wasm → WasmRuntime::invoke
    4. 远程 backend:
       └── RemoteAgentAdapter::call_remote(agent_id, ...)
    5. 返回 ToolResult
```

调用方对 backend 完全无感 —— Agent A 写一次代码，路由自动选择。

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

| 主题 | 时机 | 载荷 |
|---|---|---|
| `loop.turn.start` | 每步开始 | `{turn, step, loop_type}` |
| `llm.token` | LLM 流式输出 | `{text, model}` |
| `llm.response` | LLM 完成 | `{model, tokens_used, truncated}` |
| `tool.execution.start` | 工具调用开始 | `{name, args_keys}`（不含 args 值） |
| `tool.execution.end` | 工具调用结束 | `{name, duration_ms, ok}` |
| `loop.turn.end` | 每步结束 | `{turn, decision}` |
| `loop.done` | 循环完成 | `{response, total_steps, total_tokens}` |
| `loop.error` | 循环出错 | `{error, step}` |

**关键事件清单源自 ADR-0068**（Canonical Topic Registry + 7 幻影主题强制发射点）。

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
- **不再使用**：嵌套 `{"error": {"code": ..., "message": ...}}` 格式（ADR-0023 §C.7 已修正）

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
| **ILLMProvider 桥接** | ✅（包装） | ✅（实现） | ❌ |

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

---

## 六、DomainWorkerPool 并发原语

**Sprint 3（ADR-0020 §2.2.1 ✅ Resolved）**：`DomainWorkerPool` 是 `ForkJoinLoop` + `PlanExecuteLoop` 并行分支的底层并发原语。

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
- `PlanExecuteLoop` verify_phase 并行验证多个 hypothesis
- Phase 6 PDK Composition 并行 fan-out

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

**对设计的指导意义**：6 项共识发现推动 `DECLARE_SERVICE` 宏提案（推迟到 Phase 6 v2+，待 2+ 不同类别 awkward pattern 涌现触发）。

---

## 八、Agent Hook 拦截模式（ADR-0081）

**状态**：✅ Approved (2026-08-21)

**目的**：与 Tool Hook 正交的 Agent 级拦截点。

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
- **HookErrorPolicy 复用 ADR-0069**：避免双标准（FailOpen / FailClosed / LogAndContinue）

### 8.3 与 ToolHookRegistry 的正交

```
Tool hook:    per-tool 调用（`tools/*`）
Agent hook:   per-agent step（`agent/*`）
调用顺序：    agent step → tool call（hook 触发点不重叠）
```

完整位置：`include/agenticdsl/contract/iagent_hook_registry.h`

---

## 九、Cross-Cutting Pattern PDK（ADR-0085）

**状态**：✅ Approved (2026-08-28)

**目的**：4 种横切范式 PDK Pattern（类比 PDK Loop Agent 模式），AOP 风格的横切关注点统一抽象。

```cpp
ICrossCuttingPattern     // 统一抽象
CrossCuttingOrchestrator // 无状态 dispatcher
*.cc.md                  // 横切功能 DSL
```

**类比 Loop Agent**：
- `AgentLoopType` 枚举（React/PlanExecute/ForkJoin） → 各种 Cross-CuttingPatternType 枚举
- `DEFINE_AGENT(name, LoopType)` → `DEFINE_CROSSCUTTING(name, PatternType)`

**实施载体**：OpenSpec change `pdk-cross-cutting-patterns`（~2.2 sprint）

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

Agent 协作不仅是运行时，还有**进化期**协作。

### 11.1 MCTS Workflow Search（ADR-0061-08 + V1.1 Axis6）

- `MCTSWorkflowSearch` (T20 ship, 2026-08-28)
- 基于 Monte Carlo Tree Search 搜索工作流空间
- Axis6 = 第 6 维度（cognitive_domain composition chain）
- 单主体 commit 路径 + W4 双发射语义分离
- 关联变更：`openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` v2.1

### 11.2 GEPA Loop（ADR-0061-09）

- `GEPALoop` (T19 ship, 2026-08-27)
- 反思循环：agent 自反思 → 改进策略 → 重试
- 类比 GEPA（Genetic-Pareto）的多目标反思

### 11.3 Trajectory IR（ADR-0061-06 V1.1）

- 独立的轨迹序列化视图
- 不改 `ParsedGraph`（v1 标题耦合修正）
- T15 ship 2026-08-27

### 11.4 Skill Compiler（ADR-0061-03）

- `SkillCompiler` 实施
- T17 V1 ship 2026-08-27（15 cases / 61 assertions PASS）

---

## 十二、LLM-native 协作架构（ADR-0071）

**状态**：✅ Approved (2026-08-25)

**3 平面协作架构**：

```
Operator Plane   ─── 人类 + LLM Operator
       ↓
DSL Plane        ─── 中间表示（LLM 生成的 DSL）
       ↓
Backend Plane    ─── 多 inference backend 协同
```

派生 6 个子 ADR/Change，锚定 Phase 6+ 演化方向。

关联：
- **ADR-0074** Prompt Engineering + Evidence Gate
- **ADR-0075** EnvBackend 多环境执行
- **ADR-0076** DSL Engine as MCP Server（控制面）
- **ADR-0077** gRPC Data Plane（高吞吐通道）
- **ADR-0078** Fine-tune 基模与训练管线

---

## 十三、关键设计原则总结

### 13.1 透明路由 vs 显式 backend

```cpp
// 推荐：透明路由（ADR-0060 决策 2）
auto result = call_tool("loop/run", args);  // 一致 API

// 不推荐：显式 backend 选择
auto result = orchestrator.parallel("code_review", tasks);  // 与 call 重叠
auto result = call_remote_agent("loop/run", args);          // 暴露 transport
```

### 13.2 进程内 vs 进程间统一 API

```cpp
// 进程内（v1 已 ship）
auto result = call_tool("loop/run", {prompt, tools});

// 进程间（Phase 2 — ADR-0059 落地后）
auto result = remote.call_tool("loop/run", {prompt, tools}); // 同一 API
```

### 13.3 父子关系 vs 并行聚合

```cpp
// delegate: 长任务、监控、取消（父持有子句柄）
delegate(SubAgentSpec{.max_lifetime_ms=60000}, monitor);

// parallel: 批量并行、独立任务（对等关系）
registry.parallel("code_review", [t1, t2, t3]);
```

### 13.4 Hook vs Tool 重构

- **能通过 Tool 重写的逻辑** → 不引入 Agent Hook
- **需要 per-agent step 拦截的逻辑** → Agent Hook
- **需要 per-tool call 拦截的逻辑** → Tool Hook（ADR-0069）

### 13.5 物理隔离 vs 逻辑隔离

- **v1（ADR-0051 Spike）**：进程内协作 + jthread per-agent 线程隔离（ADR-0020）
- **Phase 2+**：进程/容器隔离 + DECLARE_SERVICE 宏

---

## 十四、实施优先级建议

| 优先级 | 模式 | 状态 | 适用场景 |
|---|---|---|---|
| ⭐⭐⭐ | `call` + `emit` + `parallel` | v1 ✅ | 几乎所有场景 |
| ⭐⭐⭐ | 3 Agent Loop（React/PlanExec/ForkJoin） | ✅ | 单 Agent 任务 |
| ⭐⭐ | `call_async` | v1 ✅ | 长任务、防阻塞 |
| ⭐⭐ | Agent Hook（ADR-0081） | ✅ | 跨 Loop 拦截、治理 |
| ⭐⭐ | 4 通道 Plugin 通信（ADR-0046） | 🔍 提案 | 多 Plugin 协作 |
| ⭐ | `delegate` | v1 ✅ | 父子任务、需要监控 |
| ⭐ | Cross-Cutting Pattern（ADR-0085） | ✅ | AOP 风格横切 |
| ⭐ | MCTS / GEPA（ADR-0061） | ✅ | Agent 进化期协作 |
| 🔮 | `open_stream` | Phase 2 | LLM token 流 |
| 🔮 | DECLARE_SERVICE（ADR-0052） | Phase 6 v2+ | 跨进程服务化 |

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
| [ADR-0068](../adr/adr-0068-event-emission-contract.md) | 事件发射契约 | ✅ |
| [ADR-0069](../adr/adr-0069-tool-coordinator-hooks.md) | ToolCoordinator Hook | 🟡 |
| [ADR-0071](../adr/adr-0071-llm-native-agenticdsl-architecture.md) | LLM-native AgenticDSL 架构 | ✅ |
| [ADR-0074](../adr/adr-0074-prompt-evidence-gate.md) | Prompt Engineering + Evidence Gate | ✅ |
| [ADR-0075](../adr/adr-adr-0075-env-backend-local-docker.md) | EnvBackend 多环境执行 | ✅ |
| [ADR-0081](../adr/adr-0081-pre-step-hook-contract.md) | Pre-Step Hook Contract | ✅ |
| [ADR-0082](../adr/adr-0082-agent-first-class-registry.md) | Agent as First-Class Registry | ✅ |
| [ADR-0085](../adr/adr-0085-cross-cutting-pattern-pdk.md) | Cross-Cutting Pattern PDK | ✅ |
| [ADR-0086](../adr/adr-0086-credit-assignment-contract.md) | 信用分配契约 | 🔍 |
| [ADR-0061-03](../adr/skill/adr-0061-03-skill-compiler.md) | Skill Compiler | ✅ |
| [ADR-0061-06](../adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md) | Trajectory IR | ✅ |
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
| 2026-09-02 | 初始版本，基于 ADR-0060/0046/0045/0051/0081/0085/0086/0061/0071 综合 |

> **维护责任**：架构组（Sprint 24 pre-launch governance）
> **审查频率**：每 Sprint 启动时检查 ADR 状态变化（Proposed → Approved → Archived）