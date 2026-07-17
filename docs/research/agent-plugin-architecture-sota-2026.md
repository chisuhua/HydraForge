# Agent-as-Plugin 架构 SOTA 调研摘要（2025-2026）

**调研日期**: 2026-07-16
**来源**: Librarian 后台调研（Task `bg_4eaaa5dd`）+ 架构组综合
**范围**: 40+ 论文、规范、项目

---

## 一、总体格局：四个相互渗透的阵营

```
┌─────────────────────────────────────────────────────────────────┐
│  阵营 1: 协议基础设施层 (Protocol Substrate)                    │
│  - MCP 2026-07-28 RC | A2A | ACP | SW4RM | ATD                  │
├─────────────────────────────────────────────────────────────────┤
│  阵营 2: 多 Agent 编排框架 (Orchestration Frameworks)            │
│  - LangGraph | CrewAI | Microsoft Agent Framework | Google ADK   │
│  - OpenAI Agents SDK | AutoGen/AG2                              │
├─────────────────────────────────────────────────────────────────┤
│  阵营 3: Agent OS 论文 (运行时理论)                             │
│  - AIOS | AOS | Agent libOS | AgenticOS | Qualixar OS | STEM    │
├─────────────────────────────────────────────────────────────────┤
│  阵营 4: 领域 Agent 运行时 (Domain Runtimes)                     │
│  - Pipecat | LiveKit Agents | NeoGraph | AgentCore              │
│  - Loom DSL | FLUX Runtime | FLUID SDK                          │
└─────────────────────────────────────────────────────────────────┘
```

**关键共识**：框架可选，协议必选。MCP / A2A / ACP 正成为跨框架互联的事实标准。

---

## 二、六大应用层范式

| 范式 | 代表 | 编排方式 | 核心特征 |
|------|------|---------|---------|
| 图编排 | LangGraph, DSPy | State Graph | 开发者定义状态图，框架执行 |
| 角色协作 | CrewAI, AutoGen | 角色对话 | 多人格 Agent 协商 |
| Tool-First | OpenAI Agents SDK, MCP | 工具调用链 | Agent 持有工具集，可 handoff |
| Agent OS | pi-mono, tau, AgentOS | AgentLoop + Harness | Agent 循环是核心抽象 |
| Service Mesh | K8s Operator, Istio | 事件驱动 | Agent 是自治控制器 |
| Microkernel | OSGi, FIPA-OS | Service Registry | Agent 是独立 bundle |

---

## 三、12 条 SOTA 架构原则

| # | 原则 | 证据来源 | HydraForge 状态 |
|---|------|---------|----------------|
| 1 | Agent 即一等公民 | OpenAI/Google ADK | ✅ PDK Plugin 范式 |
| 2 | Manifest-first | Zylos, MCP, OSGi, ATD, VS Code | ⚠️ 待实现 `pdk_manifest()` |
| 3 | 多实现不锁死 | MCP 多语言 | ✅ Skill/DSL/C++/Wasm |
| 4 | 声明式组合 | LangGraph DSL, K8s CRD | ✅ YAML + .agent.md |
| 5 | 可观测是默认 | OpenTelemetry, LangSmith | ⚠️ 仅 audit events |
| 6 | 预算/成本硬约束 | OpenAI CostConfig | ⚠️ 原子层已有 |
| 7 | 本地优先/离线 | pi-mono, llama.cpp | ✅ |
| 8 | Schema-first contracts | MCP, ATD, SW4RM | ⚠️ 需扩展 ToolMetadata |
| 9 | Capability-based security | ATD, Agent libOS | ⚠️ 部分 |
| 10 | 进程隔离 | MCP, AOS, Agent libOS | ⚠️ SKILL 需隔离 |
| 11 | Lifecycle-aware | OSGi, VS Code, MCP | ⚠️ 待实现 |
| 12 | Transport-agnostic | MCP, ATD, SW4RM | ⚠️ 当前仅进程内 |

---

## 四、关键技术标准深度解析

### 4.1 MCP 2026-07-28 RC

**来源**: `blog.modelcontextprotocol.io/posts/2026-07-28-release-candidate/`

**重大转向**：
- 完全无状态：移除 `initialize` 和 `Mcp-Session-Id`
- `Mcp-Method` header 路由：网关无需解析 body
- `ttlMs` + `cacheScope` 缓存控制
- Extensions Framework：reverse-DNS 标识，opt-in 协商

**对 HydraForge 的影响**：
- 工具必须暴露 `input_schema` / `output_schema`
- 工具命名采用 reverse-DNS（与现有 `inference.engine.*` 一致）
- 未来网关设计向无状态靠拢

### 4.2 OSGi Service Registry

**来源**: OSGi Core R8 规范

**核心属性**：
- 发布-查找-绑定模型
- 服务作用域：`singleton` / `bundle` / `prototype`
- 属性字典支持 LDAP 风格 filter
- `service.ranking` 多实现排序
- Declarative Services (DS) 注解式声明
- 生命周期：`install → resolve → start → stop → update → uninstall`

**对 HydraForge 的映射**：
- Service → Agent Plugin
- Service Registry → `CapabilityRegistry`
- Service Properties → `pdk_manifest.json`
- Bundle Lifecycle → Plugin Lifecycle
- DS → YAML 编排

### 4.3 FIPA / JADE

**来源**: FIPA 97 规范 + Bellifemine 2001 论文

**三大系统 Agent**：
- AMS: Agent Management System（白页/生命周期）
- DF: Directory Facilitator（黄页/能力查询）
- ACC: Agent Communication Channel（消息路由）

**关键原则**：内部实现对平台不可见。

**对 HydraForge 的映射**：
- AMS → `ManifestRegistry`
- DF → `CapabilityRegistry`
- ACC → `IInteractionBus`

### 4.4 ROS 2 Managed Node

**来源**: `design.ros2.org/articles/node_lifecycle.md`

**核心属性**：
- 状态机：`Unconfigured → Inactive → Active → Finalized`
- Managed Node 是 black box：接口固定，内部自由
- Composition：每个组件是 shared library，可同进程或跨进程

**对 HydraForge 的映射**：
- Managed Node → Agent Plugin
- Known Interface → `IAgent` 契约
- Black Box → 内部形态透明（SKILL/DSL/C++/Wasm）

### 4.5 Kubernetes Operator

**来源**: Kubebuilder Book

**核心模式**：CRD + Controller + Reconciliation Loop

**对 HydraForge 的映射**：
- CRD → `application.yaml` / `pdk_manifest.json`
- Controller → `AgentOrchestrator`
- Reconciler → 持续监控 Agent 健康状态

### 4.6 AgenticOS 论文

**来源**: `arxiv.org/pdf/2606.21129v1`

**四层安全架构**：
- Ghost Kernel（可信基座）
- Logic Shutter（安全策略执行点）
- Agent Capsule（沙箱化执行环境）
- Semantic Boundary Gateway（跨域数据安全网关）

**对 HydraForge 的映射**：
- Logic Shutter → `ApprovalHandler` / `ToolCoordinator`
- Agent Capsule → `WasmRuntime` / `SkillInterpreter`
- Semantic Boundary Gateway → `IInteractionBus` + `LayeredContext`

### 4.7 Agent libOS

**来源**: `arxiv.org/html/2606.03895v1`

**关键抽象**：
- `AgentImage`: 镜像（默认工具、系统 prompt、安全 profile）
- `AgentProcess`: 进程实例（PID、lifecycle_status、resource budget）
- 123 回归测试验证安全声明

**对 HydraForge 的映射**：
- `AgentImage` → `AgentDescriptor` + `pdk_manifest.json`
- `AgentProcess` → `ExecutionSession`
- `Object Memory` → `LayeredContext`
- `Resource Budget` → `IBudgetController`

---

## 五、其他关键项目速览

| 项目 | 关键特性 | 借鉴点 |
|------|---------|--------|
| Pipecat | Worker Bus + Job RPC + Frame pipeline | `IInteractionBus` 增加 Job RPC 模式 |
| NeoGraph | C++20 graph engine + asio + MCP client | 最接近 HydraForge 的 C++ 竞品 |
| AgentCore | C++20 + Python + persistent subgraph sessions | persistent session 设计 |
| FLUX Runtime | Markdown-to-bytecode for polyglot agents | DSL → Wasm 编译参考 |
| Loom DSL | Neuro-symbolic orchestration | 强化 DSL 与 LLM 边界分离 |
| ATD | 单 schema 多传输 + binding trait | `IHydraBinding` 抽象 |
| SW4RM | gRPC agent protocol + Jaeger tracing | 分布式 trace 和 protocol 抽象 |
| AgentIDL | WebIDL + intent/proof/capability annotations | 工具契约标注 |

---

## 六、HydraForge 的 SOTA 定位

**领先领域**：
- C++20 native graph engine
- 14-header contract/policy 层
- 三层会话层级（ADR-0033）
- 5-层结构化上下文（ADR-0008）
- ToolMetadata V2 多维策略
- 真 .so PDK Plugin
- 94 PDK 测试（React/PlanExecute/ForkJoin）

**主要差距**（按优先级）：
- P0: 无 manifest 体系
- P0: 无 capability-based discovery
- P0: 无 Skill 隔离运行时
- P0: 无 Wasm 运行时
- P0: 无跨进程/跨网络协议
- P1: 无 hot-reload / lazy-load
- P1: 无 semver 版本约束
- P1: 无 tool schema 强制校验
- P1: 无 distributed bus
- P1: 无 OpenTelemetry trace
- P2: 无 conformance test suite
- P2: 无多语言 PDK

---

## 七、参考来源索引

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
