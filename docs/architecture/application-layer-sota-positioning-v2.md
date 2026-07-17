# 应用层 SOTA 架构走向分析 v2

**日期**: 2026-07-16
**状态**: 🟡 Proposed (架构讨论中)
**作者**: Architecture Working Group
**关联**: `docs/architecture/agent-as-plugin-architecture-v1.1.md`, `docs/architecture/agent-evolution-pipeline.md`

**前置文档**: `docs/architecture/application-layer-sota-positioning.md` v1

---

## 一、分析目标

回答核心问题：**基于 Agent-as-Plugin 架构，HydraForge 的应用层应该走向什么形态？**

本文档不是对 `docs/specs/architecture.md` v2.2 的替代，而是从**应用层**（Layer 3 / Layer 6）视角出发的 SOTA 趋势分析和架构方向建议。

**调研范围**：40+ 篇论文、规范、项目，覆盖 MCP 2026-07-28 RC、LangGraph、CrewAI、Microsoft Agent Framework、Google ADK、OSGi、FIPA/JADE、AIOS、AOS、Agent libOS、AgenticOS、Pipecat、NeoGraph、AgentCore、ATD、SW4RM、AgentIDL、FLUX Runtime、FLUID SDK、AgentForge SDLC 等。

---

## 二、2025-2026 应用层 SOTA 趋势

### 2.1 四个相互渗透的阵营

```
┌─────────────────────────────────────────────────────────────────┐
│  阵营 1: 协议基础设施层 (Protocol Substrate)                    │
│  - MCP 2026-07-28 RC | A2A | ACP | SW4RM | ATD                  │
│  - 解决: 跨框架/跨语言的"插件互联"协议                          │
├─────────────────────────────────────────────────────────────────┤
│  阵营 2: 多 Agent 编排框架 (Orchestration Frameworks)            │
│  - LangGraph (38% 生产部署) | CrewAI | Microsoft Agent Framework│
│  - Google ADK | OpenAI Agents SDK | AutoGen/AG2                 │
│  - 解决: 怎么把多个 agent 拼成可工作系统                        │
├─────────────────────────────────────────────────────────────────┤
│  阵营 3: Agent OS 论文 (运行时理论)                             │
│  - AIOS | AOS | Agent libOS | AgenticOS | Qualixar OS | STEM    │
│  - 解决: Agent 需要什么样的"操作系统级"运行时/安全/调度          │
├─────────────────────────────────────────────────────────────────┤
│  阵营 4: 领域 Agent 运行时 (Domain Runtimes)                     │
│  - Pipecat (语音) | LiveKit Agents | NeoGraph (C++) | AgentCore │
│  - Loom DSL | FLUX Runtime | FLUID SDK                          │
│  - 解决: 特定场景下的可运行实现                                  │
└─────────────────────────────────────────────────────────────────┘
```

**关键观察**：阵营之间正在**标准化收敛** —— MCP / A2A / ACP 三大协议都被 LangGraph / Microsoft Agent Framework / Google ADK / CrewAI 共同支持。SOTA 共识是 **"框架可选、协议必选"**。

### 2.2 六种主流应用层范式

| 范式 | 代表 | Agent 形态 | 编排方式 | 核心特征 |
|------|------|-----------|---------|---------|
| **① 图编排** | LangGraph, DSPy | 函数节点 | State Graph | 开发者定义状态图，框架执行 |
| **② 角色协作** | CrewAI, AutoGen | Crew/Agent | 角色对话 | 多人格 Agent 协商完成任务 |
| **③ Tool-First** | OpenAI Agents SDK, MCP | Handoff Agent | 工具调用链 | Agent 持有工具集，可 handoff 给其他 Agent |
| **④ Agent OS** | pi-mono, tau, AgentOS | pi-ai/pi-agent | AgentLoop + Harness | Agent 循环是核心抽象，工具/Provider 可插拔 |
| **⑤ Service Mesh** | Kubernetes Operator, Istio | Operator/Controller | 事件驱动 | Agent 是自治控制器，声明式配置 |
| **⑥ Microkernel** | Eclipse OSGi, FIPA-OS | Bundle/Agent | Service Registry | Agent 是独立 bundle，动态发现/加载/组合 |

### 2.3 SOTA 收敛的 12 条架构原则

| # | 原则 | 证据 | HydraForge 对应 |
|---|------|------|----------------|
| 1 | **Agent 即一等公民** | OpenAI Agents SDK, Google ADK 都将 Agent 作为核心类 | ✅ Agent Plugin 范式 |
| 2 | **Manifest-first** | Zylos 2026, MCP 2026-07-28 RC, OSGi, ATD, VS Code | ⚠️ 待实现 `pdk_manifest()` |
| 3 | **多实现不锁死** | MCP: 同一工具可 MCP/Python/JS 实现 | ✅ 四形态（Skill/DSL/C++/Wasm） |
| 4 | **声明式组合** | LangGraph DSL, CrewAI YAML, K8s CRD | ✅ YAML 编排 + .agent.md |
| 5 | **可观测是默认** | OpenTelemetry, LangSmith, LangFuse | ⚠️ ADR-0031 audit events 初版 |
| 6 | **预算/成本硬约束** | OpenAI Agents SDK CostConfig | ⚠️ IBudgetController 原子层已有 |
| 7 | **本地优先 / 离线** | pi-mono offline, llama.cpp | ✅ LlamaAdapter + MockLLMProvider |
| 8 | **Schema-first contracts** | MCP, ATD, SW4RM | ⚠️ ToolMetadata 需扩展 schema |
| 9 | **Capability-based security** | ATD, Agent libOS, Kaman MCP | ⚠️ 部分（ToolMetadata V2） |
| 10 | **进程隔离** | MCP, VS Code, AOS, Agent libOS | ⚠️ Skill 需隔离运行时 |
| 11 | **Lifecycle-aware** | OSGi, VS Code, MCP | ⚠️ 待实现 activation/hot-reload |
| 12 | **Transport-agnostic** | MCP, ATD, SW4RM | ⚠️ 当前仅进程内 |

### 2.4 与 pi-mono / tau 的深度对比

| 维度 | pi-mono | tau | HydraForge（本架构） |
|------|---------|-----|---------------------|
| **Agent 定义** | Python class `Agent` | Python `AgentHarness` | PDK Plugin (.so/.wasm) |
| **循环引擎** | `agentLoop()` 函数 | `run_agent_loop()` | DSL 图 / C++ `DEFINE_AGENT` |
| **Provider** | `pi-ai` Python lib | `tau_ai` Python lib | `ILLMProvider` C++ 接口 + Plugin |
| **Session** | `AgentSession` class | `AgentHarness` 内建 | ADR-0033 三层 + Session Agent Plugin |
| **工具** | 函数 + `@tool` 装饰器 | Python callable | `DECLARE_TOOL` / `IToolRegistry` |
| **多模型** | `Models` collection | 多 Client 持有 | `model_router` Plugin |
| **可扩展** | Fork+修改 | Python 包 | **Plugin 热加载** |
| **性能** | Python (GIL) | Python | **C++ 核心** |
| **热更新** | ❌ | ❌ | ✅ SKILL.md / .agent.md 免编译 |
| **进化路径** | ❌ | ❌ | ✅ SKILL→DSL→C++→Wasm |
| **边缘 Wasm** | ❌ | ❌ | ✅ 可移植分发 |

### 2.5 与 SOTA 框架的直接对比

| 框架 | Agent 形态 | 内部实现 | 通信 | 可扩展 | HydraForge 优势 |
|------|-----------|---------|------|--------|----------------|
| **LangGraph** | Graph 节点 | Python lambda | 状态传递 | Python 包 | DSL 图可热更新；C++ 性能 |
| **CrewAI** | Crew Agent | Python class | 共享状态 | 自定义 Agent | 多形态；编译时安全 |
| **AutoGen** | Agent | Python class | 消息传递 | 自定义 Agent | Plugin ABI 稳定；可热加载 |
| **Google ADK** | Agent | Python | 事件通信 | 插件系统 | C++ 核心性能；DSL 可审计 |
| **OpenAI Agents SDK** | Agent | Python | Handoff | 自定义 Agent | 本地推理；预算控制 |
| **MCP Server** | Server | 任意语言 | JSON-RPC | 任意 | Agent 内嵌 OS；无网络开销 |
| **OSGi** | Bundle | Java class | 服务注册 | 动态 bundle | C++ 化；AI 语义 |
| **ROS 2** | Managed Node | C++ / Python | 消息 + 服务 | 共享库 | AI Agent 语义；LLM 编排 |

---

## 三、关键技术标准深度解析

### 3.1 MCP 2026-07-28 RC（协议基础设施 SOTA）

来自 `blog.modelcontextprotocol.io/posts/2026-07-28-release-candidate/`：

**重大转向**：
- **完全无状态**：移除 `initialize` 握手和 `Mcp-Session-Id` header，任何服务器实例可处理任何请求
- **`Mcp-Method` header 路由**：网关无需解析 body 即可路由
- **`ttlMs` + `cacheScope` 缓存控制**：模型 HTTP `Cache-Control`
- **Extensions Framework**：reverse-DNS 标识，opt-in 协商，extensions 与 spec 独立版本
- **Tool Metadata 完整字段**：`input_schema` / `output_schema` / `description` / `annotations`

**对 HydraForge 的含义**：
- `pdk/` 接口设计应**预留** `input_schema`/`output_schema` 字段
- `inference.engine.*` 这种 reverse-DNS 命名方式已经是好兆头
- 如果未来做 MCP 兼容网关，host function 设计要向无状态靠拢

### 3.2 OSGi Service Registry（动态组合 SOTA）

来自 OSGi Core R8 规范：

> "The OSGi Service Layer defines a dynamic collaborative model... The service model is a publish, find and bind model. A service is a normal Java object that is registered under one or more Java interfaces with the service registry."

**关键属性**：
- **服务作用域**：`singleton` / `bundle` / `prototype`
- **服务属性字典**：支持 LDAP 风格 filter 查询
- **service.ranking**：多实现排序
- **Declarative Services (DS)**：注解式声明，无需手写注册代码
- **Lifecycle**：`install → resolve → start → stop → update → uninstall`

**对 HydraForge 的映射**：
- OSGi Service → HydraForge Agent Plugin
- Service Registry → `CapabilityRegistry`
- Service Properties → `AgentDescriptor` + `pdk_manifest.json`
- Bundle Lifecycle → Plugin Lifecycle
- DS → YAML 声明式编排

### 3.3 FIPA / JADE（分布式 Agent 系统 SOTA）

来自 FIPA 97 规范与 Bellifemine 2001 论文：

**三大系统 Agent**：
- **AMS**（Agent Management System）：白页服务，监督生命周期
- **DF**（Directory Facilitator）：黄页服务，按服务能力查询
- **ACC**（Agent Communication Channel）：消息路由

**关键设计原则**：
> "The internal design of the agent and agent platform is outside the scope of this specification."

**对 HydraForge 的映射**：
- AMS → `ManifestRegistry`
- DF → `CapabilityRegistry`
- ACC → `IInteractionBus`
- 内部实现自由 → 支持 SKILL/DSL/C++/Wasm 四形态

### 3.4 ROS 2 Managed Node（生命周期 SOTA）

来自 `design.ros2.org/articles/node_lifecycle.md`：

> "The most important concept of this document is that a managed node presents a known interface, executes according to a known life cycle state machine, and otherwise can be considered a black box."

**状态机**：`Unconfigured → Inactive → Active → Finalized` + 6 过渡状态
**Composition 模式**：每个组件编译为 shared library，可在同一进程或不同进程运行

**对 HydraForge 的映射**：
- Managed Node → Agent Plugin
- Known Interface → `IAgent` 契约
- Black Box → 内部形态透明（SKILL/DSL/C++/Wasm）
- Composition → Agent 组合模型

### 3.5 Kubernetes Operator（声明式编排 SOTA）

来自 Kubebuilder Book：

> "Controllers are the core of Kubernetes, and of any operator. It's a controller's job to ensure that, for any given object, the actual state of the world matches the desired state in the object."

**核心模式**：CRD + Controller + Reconciliation Loop

**对 HydraForge 的映射**：
- CRD → `application.yaml` / `pdk_manifest.json`
- Controller → `AgentOrchestrator`（未来组件）
- Reconciliation Loop → 持续监控 Agent 健康状态，必要时重启/替换

### 3.6 AgenticOS 论文（安全 SOTA）

来自 `arxiv.org/pdf/2606.21129v1`：

**四层安全架构**：
- **Ghost Kernel**：可信基座
- **Logic Shutter**：安全策略执行点
- **Agent Capsule**：沙箱化 Agent 执行环境
- **Semantic Boundary Gateway**：跨域数据安全网关

**对 HydraForge 的含义**：
- `ApprovalHandler` / `ToolCoordinator` → Logic Shutter
- `WasmRuntime` / `SkillInterpreter` → Agent Capsule
- `IInteractionBus` + `LayeredContext` → Semantic Boundary Gateway
- `DSLEngine` / `TopoScheduler` → Ghost Kernel 之下的执行层

### 3.7 Agent libOS（进程模型 SOTA）

来自 `arxiv.org/html/2606.03895v1`：

> "An AgentProcess is the unit of execution; namespace-local Object Memory replaces unstructured scratch transcripts; capabilities and humans are explicit sources of authority."

**关键抽象**：
- `AgentImage` —— 镜像（默认工具、系统 prompt、context policy、安全 profile）
- `AgentProcess` —— 进程实例（PID、parent_id、image_id、lifecycle_status、tool table、checkpoint head、resource budget）
- **123 回归测试** 验证安全声明

**对 HydraForge 的映射**：
- `AgentImage` → `AgentDescriptor` + `pdk_manifest.json`
- `AgentProcess` → `ExecutionSession`（已 PIMPL-lite 拆分）
- `Object Memory` → `LayeredContext`
- `Resource Budget` → `IBudgetController`

### 3.8 其他关键项目速览

| 项目 | 关键特性 | 对 HydraForge 的借鉴 |
|------|---------|---------------------|
| **Pipecat** | Worker Bus + Job RPC + Frame-based pipeline | 为 `IInteractionBus` 增加 Job RPC 模式 |
| **NeoGraph** | C++20 graph engine + asio + MCP client | 最接近 HydraForge 的 C++ 竞品 |
| **AgentCore** | C++20 + Python + persistent subgraph sessions | 参考 persistent session 设计 |
| **FLUX Runtime** | Markdown-to-bytecode for polyglot agents | 参考 DSL → Wasm 编译 |
| **Loom DSL** | Neuro-symbolic orchestration | 强化 DSL 与 LLM 的边界分离 |
| **ATD** | 单 schema 多传输 + binding trait | 参考 `IHydraBinding` 抽象 |
| **SW4RM** | gRPC agent protocol + Jaeger tracing | 参考分布式 trace 和 protocol 抽象 |
| **AgentIDL** | WebIDL + intent/proof/capability annotations | 参考工具契约标注 |

---

## 四、HydraForge 应用层三阶段演进

### 4.1 Phase A（当前 → 6 个月）：范式验证

构建 `examples/pdk_chat_demo` 验证核心命题：

| 验证项 | 成功标准 | 当前状态 |
|--------|---------|---------|
| Agent Plugin 注册/加载 | PluginLoader 加载 .so，工具可调用 | ✅ model_router/llama_engine 已验证 |
| 四形态 Agent 可互换 | Skill/DSL/C++/Wasm 暴露相同契约 | 🟡 DSL/C++ 已验证；Skill/Wasm 待实现 |
| Agent 组合编排 | Chat Agent 调用 Loop Agent，后者调用 Tool Agents | 🟡 设计完成 |
| 预算控制贯穿 | Agent 调用消耗预算，超预算自动停止 | ✅ IBudgetController 原子层 |
| Mock 模式 CI | --mock 端到端测试通过 | 🟡 需实现 demo |

### 4.2 Phase B（6-18 个月）：Agent Marketplace

```
┌─ Agent Marketplace ─────────────────────────────────────────┐
│                                                              │
│  ┌─ Agent Package ────────────────────────────────────────┐   │
│  │  my_agent-0.2.0.hfpkg                                   │   │
│  │  ├── manifest.json         # 元数据 + 签名              │   │
│  │  ├── libMyAgent.so         # Plugin 二进制              │   │
│  │  ├── agents/*.agent.md     # DSL 工作流                 │   │
│  │  ├── skills/*/SKILL.md     # 技能定义                   │   │
│  │  ├── wasm/*.wasm            # 固化产物                   │   │
│  │  ├── config/default.json   # 默认配置                   │   │
│  │  └── tests/test_*.cpp      # 安装时测试                 │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                              │
│  • 签名验证 (ED25519 / secp256k1)                            │
│  • 沙箱隔离 (cgroups + seccomp + Firecracker)                │
│  • 版本管理 (SemVer + 兼容性矩阵)                             │
│  • 依赖解析 (Agent A requires Agent B >= 1.0)                │
│  • 声誉系统 (Layer 4.5 集成)                                  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 4.3 Phase C（18-36 个月）：自进化平台

当 HydraForge 足够成熟时，Agent 可以：

1. **自我生成 DSL 工作流** — LLM 生成 .agent.md 响应新模式
2. **自我修改 SKILL.md** — Agent 根据反馈优化自己的技能
3. **自我组合** — Agent 动态发现并组合其他 Agent 完成新任务
4. **自举** — 用 HydraForge 构建 HydraForge

```
LLM Agent → 生成 .agent.md → DSLEngine 验证 → 热加载 → 执行 → 反馈 → 优化
     ↑                                                               │
     └──────────────────── 自进化循环 ─────────────────────────────────┘
```

---

## 五、应用层编排模式演进

### 5.1 当前：静态编排

```yaml
agents: [chat, loop, provider, session, budget]
orchestration: event_driven
entry: chat
```

Agent 列表在启动时确定，运行期不变。

### 5.2 近期：动态编排

```yaml
orchestration: dynamic
discovery:
  - directory: ./plugins/
  - marketplace: https://agents.hydraforge.io
  
routing:
  router_agent: intent_router
  select_by: user_intent_embedding
```

Agent 在运行时可加载/卸载，路由 Agent 决定调用组合。

### 5.3 远期：自适应编排

```yaml
orchestration: adaptive
evolution:
  self_optimize: true
  feedback_loop:
    - metric: user_satisfaction
    - metric: budget_efficiency
    - metric: task_completion_rate
  mutation:
    - mutate_skills: true       # 允许修改 SKILL.md
    - mutate_agents: true       # 允许修改 .agent.md
    - compose_new: true         # 允许组合新 Agent
```

Agent 不仅被编排，还参与编排决策。

---

## 六、关键技术缺口

从当前状态到 SOTA 应用层，需要填补的技术缺口：

| 缺口 | 当前状态 | 目标 | 估时 | SOTA 参考 |
|------|---------|------|------|-----------|
| **SKILL.md Interpreter** | examples/skill_porting/ 有格式，无运行时 | OS 内置 SkillInterpreter | 2-3 Sprint | OpenCode Skills |
| **Agent Descriptor** | 无（Plugin 只注册工具） | `pdk_register_agent()` + Discovery | 1 Sprint | FIPA DF, OSGi |
| **pdk_manifest** | 无 | 机器可读 manifest | 1 Sprint | MCP, Zylos, ATD |
| **.agent.md 循环原语** | goto 有限支持 | while/for/break DSL 内建 | 2 Sprint | LangGraph |
| **Agent 热加载** | PluginLoader 有 load/unload | 运行时 swap Plugin | 1 Sprint | OSGi |
| **SKILL → DSL 编译** | 无 | SKILL.md → .agent.md 自动翻译 | 3 Sprint | Loom DSL |
| **Wasm 运行时** | 无 | WasmRuntime + host functions | 4 Sprint | wasi-sdk |
| **Agent Sandbox** | 进程级 (ADR-0021 §3.3) | OS-level (seccomp/cgroups/Wasm) | 3 Sprint | AgenticOS, Agent libOS |
| **Network Transport** | 进程内 (ADR-0051 §不变量) | MCP / A2A / OpenAI API | 4 Sprint | MCP, A2A |
| **Agent Observability** | audit events (ADR-0031) | OpenTelemetry integration | 2 Sprint | MCP traceparent |
| **Capability Registry** | 无 | 按 input/output schema 发现 Agent | 2 Sprint | FIPA DF, OSGi |
| **Lazy Load / Activation Events** | 无 | 条件满足时才注册工具 | 1 Sprint | VS Code |
| **Semver 版本约束** | 无 | `min_host_version` / `max_host_version` | 1 Sprint | OSGi |
| **Tool Schema 强制校验** | 注册时部分检查 | 调用时 JSON Schema 校验 | 2 Sprint | MCP, ATD |
| **Conformance Test Suite** | 29/29 ctest | 外部可消费 conformance kit | 3 Sprint | MCP SEP-2484 |

---

## 七、设计原则总结

从 SOTA 趋势推导的 10 条设计原则：

| # | 原则 | 含义 | 来源 |
|---|------|------|------|
| 1 | **Agent 即一等公民** | Agent 是 API 核心，不是附件 | OpenAI/Google 共识 |
| 2 | **Manifest-first** | 每个插件自带机器可读描述 | MCP, Zylos, OSGi, ATD |
| 3 | **多形态不锁死** | 同一 Agent 可 Skill/DSL/C++/Wasm 实现 | MCP 多语言 + 本架构进化路径 |
| 4 | **Capability-based discovery** | 按能力而非名字发现 Agent | FIPA DF + OSGi |
| 5 | **声明式优于命令式** | YAML/.agent.md 描述 > 代码编排 | K8s/LangGraph |
| 6 | **可观测是默认** | 每个 Agent 调用自动 trace | LangSmith/OTEL/MCP |
| 7 | **预算是硬约束** | Agent 不可绕过预算控制 | OpenAI CostConfig |
| 8 | **本地优先** | 离线可用，联网增强 | pi-mono/tau 共识 |
| 9 | **进程隔离** | 解释执行不可信代码必须沙箱 | MCP, AOS, Agent libOS |
| 10 | **Schema-first contracts** | 输入输出必须有机器可读 schema | MCP, ATD, SW4RM |

---

## 八、与现有架构文档的关系

| 文档 | 关系 |
|------|------|
| `docs/specs/architecture.md` v2.2 | **互补** — 八层规范是"深度分层"，本文档是"应用视角" |
| `docs/guides/multi-domain-agent-architecture.md` | **演进** — 本文档将 Cognitive/Domain Worker 模式提升为 Agent-as-Plugin 范式 |
| `docs/adr/adr-0021-pdk-design.md` | **扩展** — PDK 从"工具脚手架"扩展为"Agent 脚手架" |
| `docs/adr/adr-0051-*.md` | **落地** — Spike 发现的 awkward pattern 是本文档的实践反馈 |
| `docs/architecture/agent-as-plugin-architecture-v1.1.md` | **基础** — 本文档建立在 Plugin 架构文档之上 |
| `docs/architecture/agent-evolution-pipeline.md` | **核心机制** — 进化管线是应用层从 Phase A 到 Phase C 的技术路径 |

---

## 九、下一步

1. **对齐讨论** — 团队确认核心命题："万物皆 Agent，Agent 皆 Plugin，Agent 可进化"
2. **Phase A 实施** — 构建 `examples/pdk_chat_demo` 验证范式
3. **ADR-0052 提案** — `AgentDescriptor` + `pdk_register_agent` + `pdk_manifest` 形式化
4. **SkillInterpreter 立项** — 设计隔离模型和语法子集
5. **WasmRuntime 预研** — 评估 Emscripten/wasi-sdk 技术栈
6. **SOTA 持续跟踪** — 每季度更新本文档 §二 趋势分析

---

## 十、SOTA 来源索引

### 论文
- AIOS: `arxiv.org/abs/2403.16971`
- AOS: `arxiv.org/abs/2606.01508v1`
- Agent libOS: `arxiv.org/html/2606.03895v1`
- AgenticOS: `arxiv.org/pdf/2606.21129v1`
- AgentOS (Personal): `arxiv.org/abs/2603.08938v1`
- Qualixar OS: `arxiv.org/html/2604.06392`
- STEM Agent: `arxiv.org/pdf/2603.22359`
- Bellifemine JADE 2001: `jmvidal.cse.sc.edu/library/bellifemine01a.pdf`

### 规范与官方文档
- MCP 2026-07-28 RC: `blog.modelcontextprotocol.io/posts/2026-07-28-release-candidate/`
- MCP Spec: `modelcontextprotocol.io/specification/draft.md`
- OSGi Core R8: `osgi.github.io/osgi/core/framework.service.html`
- ROS 2 Lifecycle: `design.ros2.org/articles/node_lifecycle.html`
- Kubebuilder Book: `book.kubebuilder.io/cronjob-tutorial/controller-overview`
- FIPA 97: `ptacts.uspto.gov/...` (见 ADR 关联)

### 项目
- NeoGraph: `github.com/fox1245/NeoGraph`
- AgentCore: `github.com/mavin2009/agentcore`
- Pipecat: `github.com/pipecat-ai/pipecat`
- ATD: `github.com/downsea/atd`
- SW4RM: `sw4rm.ai/protocol/`
- AgentIDL: `github.com/s-agent-comm/agent-idl`
- Loom DSL: `github.com/srijithunni7182/llm4j/tree/main/loom`
- FLUX Runtime: `github.com/SuperInstance/flux-runtime`
- FLUID SDK: `github.com/Agenticstiger/forge-cli-sdk`

### 综述
- Zylos Plugin Architecture 2026: `zylos.ai/research/2026-02-21-ai-agent-plugin-extension-architecture`
- LangGraph vs CrewAI vs ADK vs MS Agent: `rpabotsworld.com/langgraph-vs-crewai-vs-microsoft-agent-framework-vs-google-adk/`
- Multi-Agent Orchestration 2026: `presenc.ai/research/multi-agent-orchestration-frameworks-2026`
- Agentic Design Patterns 2026: `sitepoint.com/the-definitive-guide-to-agentic-design-patterns-in-2026/`

---

**核心结论**：HydraForge 在 C++20 native + 强类型契约 + 真 .so PDK 这条赛道上**几乎无直接竞品**。SOTA 的差距主要在 **manifest 标准化、hot-reload、跨进程协议、Skill 隔离、Wasm 运行时** 这五块，建议在下一个 Sprint 优先补齐。

**HydraForge 独特定位建议写入官方文档**：

> **HydraForge is the C++20-native, capability-controlled agent operating system where every application agent is a first-class PDK plugin (.so/.dll/.wasm), internally evolvable from SKILL.md to AgenticDSL to C++ to WebAssembly, connected through a contract-first policy layer, operating on a 5-layer structured context, and orchestrated via neuro-symbolic DSL with hard trust boundaries.**
