# AgentScope 调研报告 — 对 HydraForge 的可借鉴项

**调研项目**: [github.com/agentscope-ai/agentscope](https://github.com/agentscope-ai/agentscope) (原 modelscope/agentscope)
**调研日期**: 2026-07-22
**版本**: AgentScope 2.0 (production-ready)
**产出**: 本报告列出 18 个可借鉴的设计模式, 按与 HydraForge v1.2 架构的对齐度排序。

---

## 一、AgentScope 架构概览

AgentScope 是一个**生产级 Python Agent 框架**, 核心理念是 "为日益强大的 LLM 而设计" — 依赖模型的推理和工具使用能力, 而非硬编码工作流。

### 层架构 (5 层)

| AgentScope 层 | 组件 | 对应 HydraForge 层 |
|--------------|------|-------------------|
| Orchestration | `MsgHub`, `Pipeline` (sequential/fanout) | L4 Agent 应用服务 |
| Agent | `AgentBase`, `ReActAgent` | L4 Agent 应用服务 |
| Capability | `Toolkit`, `MemoryBase`, `KnowledgeBase` | L2 Plugin 工具 + L3 PDK |
| Infrastructure | `ChatModelBase`, OTel Tracing | L1 OS Services |
| Deployment | `AgentService` (FastAPI), multi-tenancy | L4 外部 API 面 |

### 核心抽象 (5 个)

| 抽象 | AgentScope | HydraForge 对应 |
|------|-----------|----------------|
| Agent | `AgentBase` (async, hook 系统) | `DEFINE_AGENT` 宏展开的类 |
| Message | `Msg` (多模态 content blocks) | `nlohmann::json` (ToolResult) |
| Model | `ChatModelBase` (统一 provider 接口) | `ILLMProvider` |
| Tool | `ToolBase` + `Toolkit` (分组管理) | `IToolRegistry` |
| Memory | `MemoryBase` (短/长/压缩三层) | `LayeredContext` (L1-L5) |

---

## 二、12 个可借鉴设计模式

### 🔴 P0 — 高优先级 (直接弥合现有差距)

#### 1. Agent 生命周期 Hook 系统

**AgentScope 模式**: `AgentBase` 提供 6 个 hook 位置:

```python
# 类级 hook (所有实例共享)
AgentBase._class_pre_reply_hooks
AgentBase._class_post_reply_hooks
AgentBase._class_pre_observe_hooks
AgentBase._class_post_observe_hooks
AgentBase._class_pre_print_hooks
AgentBase._class_post_print_hooks

# 实例级 hook (per-instance)
agent.register_instance_hook("pre_reply", "audit_log", my_logger)
```

**HydraForge 现状**: 当前 `DEFINE_AGENT` 宏展开的类无 hook 系统。所有编排逻辑硬编码在 `ReactLoop::run()` / `PlanExecuteLoop::run()` 中。

**建议**: 为 `DEFINE_AGENT` 宏新增可选 hook 注册参数:

```cpp
#define DEFINE_AGENT(name, loop_type, ...)  // 可选 hook 列表
  class name##Agent {
    // pre_reply / post_reply / pre_observe / post_observe hooks
  };
```

**收益**: 审计日志、租户隔离、成本追踪可通过 hook 注入, 无需修改 Agent 核心逻辑。

---

#### 2. Middleware 洋葱模型

**AgentScope 模式**: 6 个 hook 位置, 每个位置都是洋葱模型 (before → next_handler() → after):

```python
class AuditMiddleware(MiddlewareBase):
    async def on_reply(self, agent, input_kwargs, next_handler):
        log_start(agent.name, input_kwargs)
        async for event in next_handler(**input_kwargs):
            yield event
        log_end(agent.name)

    async def on_model_call(self, agent, input_kwargs, next_handler):
        result = await next_handler(**input_kwargs)
        track_tokens(result.usage)
        return result

agent = Agent(middlewares=[TracingMiddleware(), AuditMiddleware()])
```

**HydraForge 现状**: ILLMProvider 有 Decorator 链 (`CostTrackingDecorator` / `ComplianceDecorator` / `RateLimitDecorator`), 但 Agent 层面无 middleware。

**建议**: Agent 级别新增 middleware 栈, 与 ILLMProvider 的 Decorator 链形成双层可观测性:

```
Agent Middleware Stack (on_reply, on_reasoning, on_acting)
    └─ ILLMProvider Decorator Chain (cost, compliance, rate_limit)
```

**收益**: 观测性注入点更丰富, 分离 Agent 编排逻辑与横切关注点。

---

#### 3. 多租户 Agent Service (FastAPI 部署层)

**AgentScope 模式**: `AgentService` 是 FastAPI 应用, 每请求解析 `user_id`, 管理 7 种资源类型:

| 资源 | 说明 |
|------|------|
| User | 不透明租户标识, 从请求解析 |
| Credential | Provider 连接配置 (API key + 设置) |
| Agent | 模板: 显示名 + system prompt + 运行时配置 |
| Session | 用户-Agent 间的持续交互 |
| Workspace | Agent 运行时环境 (工作目录, MCP, skills) |
| Schedule | Cron 定时触发 Agent |
| MessageBus | Redis-backed 会话锁 + 重放日志 + inbox 队列 |

**HydraForge 现状**: 当前无 HTTP 部署层。`pdk_chat_demo` 的 `main.cpp` 是 CLI 交互循环, 不是服务。

**建议**: 借鉴 AgentScope 的 7 资源模型, 为 HydraForge 设计 L4 Agent 应用服务的 HTTP API 面:

```
POST   /api/agent/run          → IToolRegistry::call_tool()
GET    /api/agent/status/{id}   → temporal/poll
GET    /api/agent/sessions      → session/list
DELETE /api/agent/sessions/{id} → session/delete
```

**收益**: PKGM-Web 等第三方应用可通过标准 HTTP API 接入, 无需嵌入 C++ 运行时。

---

#### 4. 统一消息协议 (Msg + Content Blocks)

**AgentScope 模式**: `Msg` 对象支持多模态 typed content blocks:

```python
Msg(
    name="assistant",
    content=[
        TextBlock(text="I'll analyze the code."),
        ToolUseBlock(id="call_1", name="read_file", input={"path": "main.cpp"}),
        ThinkingBlock(thinking="Let me understand the structure first."),
    ],
    role="assistant",
)
```

**Content Block 类型**: `TextBlock`, `ThinkingBlock`, `ToolUseBlock`, `ToolResultBlock`, `AudioBlock`, `ImageBlock`, `VideoBlock`

**HydraForge 现状**: 工具调用使用 `nlohmann::json`, 消息传递使用 `ToolResult` 信封。无 typed content blocks。

**建议**: 扩展 `ToolResult` 为 typed content block 模型, 支持 thinking traces 和 multimodal:

```cpp
struct ContentBlock {
    enum Type { Text, Thinking, ToolUse, ToolResult, Image, Audio };
    Type type;
    nlohmann::json data;
};
```

**收益**: 支持 DeepSeek-R1 等 reasoning model 的 thinking traces, 支持多模态 Agent。

---

### 🟠 P1 — 强烈建议

#### 5. MsgHub 广播机制

**AgentScope 模式**: `MsgHub` 是 async context manager, 管理一组 Agent 的订阅关系:

```python
async with MsgHub(
    participants=[analyst1, analyst2, risk_mgr, pm],
    announcement=Msg("system", "Analyze Q3 performance", "system"),
) as hub:
    await analyst1()  # 自动广播给其他 3 个 Agent
    await analyst2()
    hub.add(new_analyst)  # 动态加入
    hub.broadcast(Msg("system", "Time check", "system"))
```

**HydraForge 现状**: `IInteractionBus` 提供 emit/subscribe, 但订阅关系是手动管理的。无 "群组自动广播" 语义。

**建议**: 在 `IInteractionBus` 之上封装 `MsgHub` 模式 (或作为 L4 实用程序):

```cpp
class MsgHub {
    void add_participant(AgentBase& agent);
    void broadcast(const ToolResult& msg);
    // 析构时自动清理订阅
};
```

**收益**: 简化多 Agent 辩论/讨论/会议场景的代码。

---

#### 6. Pipeline 模式 (Sequential / Fanout / Streaming)

**AgentScope 模式**: 三种 pipeline 作为语法糖:

```python
# Sequential: output of A → input of B
msg = await sequential_pipeline([agent_a, agent_b, agent_c], msg=input_msg)

# Fanout: 同一输入分发到多个 Agent (并发/顺序)
msgs = await fanout_pipeline([agent_a, agent_b, agent_c], msg=input_msg, enable_gather=True)

# Streaming: 捕获 print 输出为 async generator
async for msg, last in stream_printing_messages([agent_a], agent_a()):
    print(f"[{msg.name}] {msg.text}")
```

**HydraForge 现状**: `DEFINE_AGENT` 支持 `React` / `PlanExecute` / `ForkJoin` 三种 LoopType, 但 pipeline 模式需要在应用层手动实现。

**建议**: 在 L4 提供 pipeline 工具类 (可参考现有 `ForkJoinLoop`):

```cpp
class SequentialPipeline { /* output of A → input of B */ };
class FanoutPipeline   { /* same input → N agents, gather results */ };
```

**收益**: 减少多 Agent 编排的样板代码。

---

#### 7. Tool Group 动态激活/停用

**AgentScope 模式**: `Toolkit` 支持 tool groups, Agent 可通过 meta tool 在运行时激活/停用工具组:

```python
toolkit = Toolkit(
    tools=[read_file, write_file],  # "basic" group (always active)
    tool_groups=[
        ToolGroup("code_analysis", tools=[ast_parser, linter], description="Code analysis tools"),
        ToolGroup("web_search", tools=[search_web, fetch_url], description="Web tools"),
    ]
)
# Agent calls reset_tools(code_analysis=True, web_search=False) at runtime
```

**HydraForge 现状**: `IToolRegistry::register_tool_function()` 注册后全局可用, 无分组和动态激活。

**建议**: `IToolRegistry` 扩展分组概念:

```cpp
registry.register_tool_function("code/ast_parse", meta, handler, "code_analysis");
registry.activate_group("code_analysis");
registry.deactivate_group("web_search");
```

**收益**: 减少 LLM context window 中的工具 schema 噪音, 按需暴露工具。

---

#### 8. PlanNotebook 任务规划系统

**AgentScope 模式**: `PlanNotebook` 让 Agent 将复杂任务分解为可追踪的子任务:

```python
class Plan(BaseModel):
    goal: str
    subtasks: list[SubTask]

class SubTask(BaseModel):
    id: str
    description: str
    status: Literal["pending", "in_progress", "completed", "failed"]
    depends_on: list[str]  # DAG!
```

Agent 在每个 ReAct 轮次前注入当前计划状态作为 hint。

**HydraForge 现状**: `PlanExecuteLoop` 有基本的 plan→execute→verify 流程, 但无细粒度 `SubTask` DAG 追踪。

**建议**: 为 `PlanExecuteLoop` 增加 `PlanNotebook` 风格的子任务追踪和 hint 注入。

**收益**: 复杂多步任务的可视化和可调试性。

---

### 🟡 P2 — 可选增强

#### 9. 统一 Memory 抽象 (Short / Long / Compression)

**AgentScope 模式**: 三层 memory:

| 层 | 实现 | 用途 |
|----|------|------|
| Short-term (Working) | `InMemoryMemory`, `RedisMemory` | 当前 session 上下文 |
| Long-term (Semantic) | `Mem0`, `ReMe` | 跨 session 知识 |
| Compression | 自动压缩 history | Token limit 管理 |

**HydraForge 现状**: `LayeredContext` (L1-L5) 提供结构化上下文, 但无显式的 long-term memory 接口和自动压缩。

**建议**: 在 `LayeredContext` 基础上提取 `IMemory` 接口:

```cpp
class IMemory {
    virtual void store(const std::string& key, const nlohmann::json& value) = 0;
    virtual nlohmann::json retrieve(const std::string& query) = 0;
    virtual void compress(size_t max_tokens) = 0;
};
```

**收益**: 支持跨 session 知识积累和自动上下文管理。

---

#### 10. AgentScope Studio (可视化调试)

**AgentScope 模式**: 本地 Web UI, 提供:
- Runtime 可视化 (chatbot 风格交互)
- OpenTelemetry trace 可视化
- 多 Run 管理和对比
- Token 用量和成本统计

连接到 AgentScope 只需一行: `agentscope.init(studio_url="http://localhost:port")`

**HydraForge 现状**: 无可视化调试工具。依赖终端日志和 `IInteractionBus` 事件。

**建议**: Phase 2 考虑构建 HydraForge Studio (可复用 AgentScope Studio 的 OTLP 协议):

```
HydraForge Agent → OTLP traces → HydraForge Studio (Web UI)
```

**收益**: 显著降低开发和调试门槛。

---

#### 11. Permission System (细粒度权限)

**AgentScope 模式**: 细粒度、可配置的工具和资源权限控制:

```python
agent = Agent(
    permission="bypass",  # bypass | confirm | deny
    # 或 per-tool permissions
)
```

**HydraForge 现状**: `IExecutionPolicy` + `ApprovalHandler` 提供审批机制, 但无 per-tool 权限配置。

**建议**: 扩展 `ApprovalPolicy` 支持 per-tool 级别的默认策略覆盖。

---

#### 12. Workspace / Sandbox 隔离

**AgentScope 模式**: 多种 workspace 后端 (Local, Docker, E2B, OpenSandbox, Daytona):

```python
workspace = DockerWorkspace(image="python:3.11")
# Agent 的工具调用在隔离的 Docker 容器中执行
```

**HydraForge 现状**: `SkillInterpreter` 提供 posix_spawn + seccomp 隔离, 但无容器化 workspace。

**建议**: Phase 2 考虑 Docker/E2B workspace 后端作为 `SkillInterpreter` 的替代隔离策略。

---

## 六、优先级矩阵 (18 项)

| 借鉴项 | 优先级 | 对应 HydraForge 层 | 实施难度 | 收益 |
|--------|:------:|--------------------|:------:|------|
| 1. Agent Hook 系统 | 🔴 P0 | L4 Agent | 中 | 极高 |
| 2. Middleware 洋葱模型 | 🔴 P0 | L4 Agent + L1 | 高 | 极高 |
| 3. Multi-tenant Agent Service | 🔴 P0 | L4 外部 API | 高 | 极高 |
| 4. 统一消息协议 | 🔴 P0 | L3 PDK | 中 | 高 |
| 5. MsgHub 广播 | 🟠 P1 | L4 Agent | 低 | 高 |
| 6. Pipeline 模式 | 🟠 P1 | L4 Agent | 低 | 中 |
| 7. Tool Group 动态激活 | 🟠 P1 | L3 PDK | 中 | 高 |
| 8. PlanNotebook | 🟠 P1 | L4 Agent | 中 | 中 |
| 9. Agent as Tool | 🟠 P1 | L3 PDK | 低 | 高 |
| 10. Handoffs/Routing | 🟠 P1 | L4 Agent | 中 | 中 |
| 11. Tool Offloading | 🟠 P1 | L4+L2 | 中 | 高 |
| 12. Memory 抽象 | 🟡 P2 | L1 OS | 高 | 中 |
| 13. Studio 可视化 | 🟡 P2 | L4 外部 | 高 | 中 |
| 14. Permission System | 🟡 P2 | L1 OS | 中 | 低 |
| 15. Sandbox 隔离 | 🟡 P2 | L1 OS | 高 | 低 |
| 16. Actor 分布式 | 🟡 P2 | L1 OS | 高 | 低 |
| 17. A2A 协议 | 🟡 P2 | L4 外部 | 高 | 中 |
| 18. DAG 节点扩展 | 🟡 P2 | L0 DSL | 中 | 中 |

---

## 四、补充发现 (第二轮调研, 基于源码/论文)

### 13. Actor 分布式模型

AgentScope 基于 Actor 模型实现分布式部署, 关键特性:

- **Agent 级自动并行**: 无依赖的 Agent 可通过 `asyncio.gather()` 自动并行
- **两种进程模式**: 一对一 (Agent per process, 计算密集型) / 多对一 (多个 Agent 共享进程, I/O 密集型)
- **`to_dist()` 一键分布式**: 将集中式工作流自动转换为分布式部署
- **代理机制**: 中心节点保留 Proxy, 自动转发消息到分布式 Agent
- **Placeholder 机制**: 异步非阻塞, 允许主流程继续执行

**对 HydraForge 的启示**: 当前 HydraForge 是单进程模型。Phase 2+ 如需分布式, 可借鉴 Actor 模型的 Agent 级并行和 Proxy 转发模式, 而非引入重量级 RPC 框架。

---

### 14. A2A 协议 + AgentCard 服务发现

AgentScope 实现开放标准 A2A Protocol (JSON-RPC over HTTP/SSE):

```
A2AAgent (Client)  ←──JSON-RPC/SSE──→  AgentScopeA2a Server
    │                                        │
    ├─ AgentCard                             ├─ AgentRunner
    ├─ Client                                ├─ AgentRegistry
    └─ Event Handler                         └─ Transport
```

**AgentCard** 包含: 名称、版本、能力描述、传输端点。支持 Nacos 注册中心实现服务发现。

**对 HydraForge 的启示**: 当前 Agent 间通信通过 `IToolRegistry::call_tool()` (进程内) 和 `IInteractionBus` (emit/subscribe)。如需跨进程 Agent 通信, 可借鉴 AgentCard 的 Capability 描述格式作为 Agent 发现协议。

---

### 15. DAG 工作流 + 6 种节点类型

AgentScope 的 ASDiGraph 支持将应用描述为 JSON DAG, 含 6 种节点:

| 节点类型 | 用途 | HydraForge 对应 |
|---------|------|----------------|
| Model Node | LLM/Embedding 配置 | `ILLMProvider` |
| Agent Node | Agent 实例 | `DEFINE_AGENT` |
| Pipeline Node | MsgHub/顺序/循环 | `ReactLoop` / `ForkJoinLoop` |
| Service Node | 搜索/代码执行 | `IToolRegistry::call_tool()` |
| Copy Node | 复制父节点输出多路分发 | 无直接对应 |
| DistPg Node | 分布式并行 | 无直接对应 |

**对 HydraForge 的启示**: HydraForge 的 DSL (`.agent.md`) 已经是声明式 DAG, 但缺少 "Copy Node" 和形式化的 "Service Node" 概念。可以考虑在 DSL 中引入这两种节点类型以增强表达能力。

---

### 16. Agent as Tool (Agent 即工具)

AgentScope 的核心模式之一: 一个 Agent 可以直接注册为另一个 Agent 的工具:

```python
# Agent B 作为 Agent A 的工具
toolkit = Toolkit()
toolkit.register_agent_as_tool(agent_b, name="code_reviewer")
agent_a = ReActAgent(toolkit=toolkit)
```

Agent A 调用 `code_reviewer(input)` 时, 内部触发 Agent B 的完整 `__call__` 生命周期。

**对 HydraForge 的启示**: 这与 ADR-0051 Spike 的 G1→G3 模式完全一致 (`call_tool("knowledge_base/query")`)。可以形式化此模式为 `register_agent_as_tool()`, 让 Agent 递归组合更直观。

---

### 17. Handoffs + Routing + Supervisor 编排模式

除了基础的 Pipeline/MsgHub, AgentScope 还支持 3 种高级编排:

- **Handoffs**: 状态驱动路由, Agent 间交接 (销售→支持). 类似 "transfer to agent" 语义。
- **Routing**: 分类器 → 专家 → 合并. 请求路由到不同领域 Agent。
- **Supervisor**: 中央调度 Agent + 专家 Agent 作为工具, Supervisor 决定调用哪个专家。

**对 HydraForge 的启示**: 当前 HydraForge 的 `ForkJoinLoop` 最接近 Routing 模式, 但 Handoffs 和 Supervisor 尚未有内置支持。Handoffs 可能对 PKGM-Web 场景特别有用——根据用户意图将请求路由到不同的知识管线 Agent。

---

### 18. Tool Offloading (后台工具执行)

AgentScope 的 `ToolOffloadMiddleware` 将超时工具移到后台执行:

- 工具超时 → 移到后台 watcher task
- 返回占位符给 Agent, 继续 loop
- 后台完成后 → 结果推入 session inbox → wakeup 信号触发下一次 run

**对 HydraForge 的启示**: 这对 Temporal Agent PoC 的 async polling 模式有直接参考价值。可以设计类似的 `BackgroundToolExecutor`, 在工具超时时不阻塞 Agent 主循环。

---

## 五、与 HydraForge v1.2 架构的对齐关系 (更新)

```
AgentScope 概念         →  HydraForge v1.2 层
──────────────────────────────────────────────
AgentService (FastAPI)  →  L4 Agent 应用服务
MsgHub / Pipeline       →  L4 Agent 应用服务 (编排)
AgentBase / ReActAgent  →  L4 Agent 应用服务 (Agent 实例)
Agent as Tool           →  L3 PDK (register_agent_as_tool)
Handoffs / Supervisor   →  L4 Agent 应用服务 (高级编排)
Middleware              →  L4 + L1 横切关注点
ToolBase / Toolkit      →  L3 PDK 接口契约 + L2 Plugin 工具
Tool Offloading         →  L4 Agent + L2 Plugin 工具
PlanNotebook            →  L4 Agent 应用服务
A2AAgent / AgentCard    →  L4 外部 (跨进程 Agent 发现)
A2A Server              →  L4 外部 API 面
ChatModelBase           →  L1 ILLMProvider
MemoryBase              →  L1 LayeredContext (可扩展)
OTel Tracing            →  L1 IInteractionBus (可扩展)
Actor Distribution      →  L1 OS (跨进程调度)
DAG Nodes (6 types)     →  L0 DSL (`.agent.md` 节点类型)
```

**关键差异**: AgentScope 是 Python async generator 驱动, HydraForge 是 C++20 同步/线程模型。Hook/Middleware 模式可直接借鉴, 但实现需适配 C++ 的 RAII + `std::function` 风格而非 Python 的 async generator。

---

## 六、立即行动建议

| 序号 | 行动 | 关联 |
|------|------|------|
| 1 | 为 `DEFINE_AGENT` 宏新增 `pre_reply`/`post_reply` hook 注册 | Agent Hook 系统 |
| 2 | 设计 L4 Agent HTTP API 面 (`POST /api/agent/run` 等) | Multi-tenant Service |
| 3 | 在 `IInteractionBus` 之上实现 `MsgHub` 工具类 | MsgHub 广播 |
| 4 | `IToolRegistry` 扩展 tool groups 概念 | Tool Group 动态激活 |
| 5 | 考虑 `register_agent_as_tool()` API (Agent as Tool 模式) | Agent 递归组合 |
| 6 | 评估 ToolOffloadMiddleware 对 Temporal PoC 的参考价值 | Async polling |

---

*调研基于 AgentScope 2.0 (2026-07) 公开文档和源码。*
