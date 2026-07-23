# 应用层 SOTA 架构走向分析

**日期**: 2026-07-16
**状态**: ⛔ Superseded (v1, 被 `application-layer-sota-positioning-v2.md` 替代)
**作者**: Architecture Working Group

---

## 一、分析目标

回答核心问题：**基于 Agent-as-Plugin 架构，HydraForge 的应用层应该走向什么形态？**

本文档不是对 `docs/specs/architecture.md` v2.2 的替代，而是从**应用层**（Layer 6）视角出发的 SOTA 趋势分析和架构方向建议。

---

## 二、2025-2026 应用层 SOTA 趋势

### 2.1 六种主流范式

| 范式 | 代表 | Agent 形态 | 编排方式 | 核心特征 |
|------|------|-----------|---------|---------|
| **① 图编排** | LangGraph, DSPy | 函数节点 | State Graph | 开发者定义状态图，框架执行 |
| **② 角色协作** | CrewAI, AutoGen | Crew/Agent | 角色对话 | 多人格 Agent 协商完成任务 |
| **③ Tool-First** | OpenAI Agents SDK, MCP | Handoff Agent | 工具调用链 | Agent 持有工具集, 可 handoff 给其他 Agent |
| **④ Agent OS** | pi-mono, tau | pi-ai/pi-agent | AgentLoop + Harness | Agent 循环是核心抽象, 工具/Provider 可插拔 |
| **⑤ Service Mesh** | Kubernetes Operator, Istio | Operator/Controller | Event 驱动 | Agent 是自治控制器, 声明式配置 |
| **⑥ Microkernel** | Eclipse OSGi, FIPA-OS | Bundle/Agent | Service Registry | Agent 是独立 bundle, 动态发现/加载/组合 |

### 2.2 SOTA 收敛趋势

2025-2026 年六个明确的收敛方向：

| 趋势 | 证据 | HydraForge 对应 |
|------|------|----------------|
| **Agent-as-first-class-citizen** | OpenAI Agents SDK, Google ADK 都将 Agent 作为核心类 | ✅ Agent Plugin 范式 |
| **Multi-implementation** | MCP: 同一工具可 MCP/Python/JS 实现 | ✅ 三形态 (Skill/DSL/C++) |
| **Declarative composition** | LangGraph DSL, CrewAI YAML, K8s CRD | ✅ YAML 编排 + .agent.md |
| **Observability built-in** | OpenTelemetry, LangSmith, LangFuse | ⚠️ ADR-0031 audit events 初版 |
| **Budget/cost control** | OpenAI Agents SDK CostConfig | ⚠️ IBudgetController 原子层已有 |
| **Local-first / offline** | pi-mono offline, llama.cpp | ✅ LlamaAdapter + MockLLMProvider |

### 2.3 与 pi-mono / tau 的深度对比

| 维度 | pi-mono | tau | HydraForge (本架构) |
|------|---------|-----|---------------------|
| **Agent 定义** | Python class `Agent` | Python `AgentHarness` | PDK Plugin (.so) |
| **循环引擎** | `agentLoop()` 函数 | `run_agent_loop()` | DSL 图 / C++ `DEFINE_AGENT` |
| **Provider** | `pi-ai` Python lib | `tau_ai` Python lib | `ILLMProvider` C++ 接口 + Plugin |
| **Session** | `AgentSession` class | `AgentHarness` 内建 | ADR-0033 三层 + Session Agent Plugin |
| **工具** | 函数 + `@tool` 装饰器 | Python callable | `DECLARE_TOOL` / `IToolRegistry` |
| **多模型** | `Models` collection | 多 Client 持有 | `model_router` Plugin |
| **可扩展** | Fork+修改 | Python 包 | **Plugin 热加载** |
| **性能** | Python (GIL) | Python | **C++ 核心** |
| **热更新** | ❌ | ❌ | ✅ SKILL.md / .agent.md 免编译 |

**HydraForge 的独特优势**：
1. 唯一的 **C++ 核心 + 多形态 Agent** 架构
2. 唯一的 **DSL 图可审计** + **热更新** 双重能力
3. 唯一的 **预算控制内建于 Agent 调用链**

---

## 三、HydraForge 应用层架构演进方向

### 3.1 三阶段演进

```
Phase A (当前 → 6个月)         Phase B (6-18个月)          Phase C (18-36个月)
━━━━━━━━━━━━━━━━━━━━━━━      ━━━━━━━━━━━━━━━━━━━━━━━    ━━━━━━━━━━━━━━━━━━━━━━━
Plugin Chat Demo               Agent Marketplace           Self-Evolving Platform
                              
• 5-10 个 Agent Plugin         • Agent 市场 (Layer 6)      • LLM 自我生成 .agent.md
• CLI/TUI 交互                 • 签名验证 + 沙箱隔离        • Agent 自我修改 SKILL.md
• MockLLMProvider              • 网络 RPC (ADR-0050 候选B)  • 自举: Agent 修改 OS
• 事件驱动编排                  • 跨进程 Agent 通信          • 形式化验证
• --mock CI                    • Cloud plugin 部署          • 分布式部署
```

### 3.2 Phase A 目标：验证范式可行性

构建 `examples/pdk_chat_demo` 验证核心命题：

| 验证项 | 成功标准 | 当前状态 |
|--------|---------|---------|
| Agent Plugin 注册/加载 | PluginLoader 加载 .so, 工具可调用 | ✅ model_router/llama_engine 已验证 |
| 三形态 Agent 可互换 | DSL/SKILL/C++ Agent 暴露相同契约 | 🟡 仅 C++ 已验证 |
| Agent 组合编排 | Chat Agent 调用 Loop Agent, 后者调用 Tool Agents | 🟡 设计完成 |
| 预算控制贯穿 | Agent 调用消耗预算, 超预算自动停止 | ✅ IBudgetController 原子层 |
| Mock 模式 CI | --mock 端到端测试通过 | 🟡 需实现 demo |

### 3.3 Phase B 愿景：Agent Marketplace

```
┌─ Agent Marketplace ─────────────────────────────────────────┐
│                                                              │
│  ┌─ Agent Package ────────────────────────────────────────┐ │
│  │  my_agent-0.2.0.hfpkg                                   │ │
│  │  ├── manifest.json         # 元数据 + 签名              │ │
│  │  ├── libMyAgent.so         # Plugin 二进制              │ │
│  │  ├── agents/*.agent.md     # DSL 工作流                 │ │
│  │  ├── skills/*/SKILL.md     # 技能定义                   │ │
│  │  ├── config/default.json   # 默认配置                   │ │
│  │  └── tests/test_*.cpp      # 安装时测试                 │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                              │
│  • 签名验证 (ED25519 / secp256k1)                            │
│  • 沙箱隔离 (cgroups + seccomp + Firecracker)                │
│  • 版本管理 (SemVer + 兼容性矩阵)                             │
│  • 依赖解析 (Agent A requires Agent B >= 1.0)                │
│  • 声誉系统 (Layer 4.5 集成)                                  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 3.4 Phase C 愿景：自进化平台

当 HydraForge 足够成熟时，Agent 可以：

1. **自我生成 DSL 工作流** — LLM 生成 .agent.md 响应新模式
2. **自我修改 SKILL.md** — Agent 根据反馈优化自己的技能
3. **自我组合** — Agent 动态发现并组合其他 Agent 完成新任务
4. **自举** — 用 HydraForge 构建 HydraForge（最终自举验证）

```
LLM Agent → 生成 .agent.md → DSLEngine 验证 → 热加载 → 执行 → 反馈 → 优化
     ↑                                                               │
     └──────────────────── 自进化循环 ─────────────────────────────────┘
```

---

## 四、应用层编排模式演进

### 4.1 当前：静态编排

```yaml
agents: [chat, loop, provider, session, budget]
orchestration: event_driven
entry: chat
```

Agent 列表在启动时确定，运行期不变。

### 4.2 近期：动态编排

```yaml
orchestration: dynamic
discovery:
  - directory: ./plugins/
  - marketplace: https://agents.hydraforge.io
  
routing:
  # 路由 Agent 根据用户意图动态选择 Agent 组合
  router_agent: intent_router
  select_by: user_intent_embedding
```

Agent 在运行时可加载/卸载，路由 Agent 决定调用组合。

### 4.3 远期：自适应编排

```yaml
orchestration: adaptive
evolution:
  # Agent 可以根据执行效果自我调整
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

## 五、关键技术缺口

从当前状态到 SOTA 应用层，需要填补的技术缺口：

| 缺口 | 当前状态 | 目标 | 估时 |
|------|---------|------|------|
| **SKILL.md Interpreter** | examples/skill_porting/ 有格式, 无运行时 | OS 内置 SkillInterpreter | 2-3 Sprint |
| **Agent Descriptor** | 无 (Plugin 只注册工具) | `pdk_register_agent()` + Discovery | 1 Sprint |
| **.agent.md 循环原语** | goto 有限支持 | while/for/break DSL 内建 | 2 Sprint |
| **Agent 热加载** | PluginLoader 有 load/unload | 运行时 swap Plugin | 1 Sprint |
| **SKILL → DSL 编译** | 无 | SKILL.md → .agent.md 自动翻译 | 3 Sprint |
| **Agent Sandbox** | 进程级 (ADR-0021 §3.3) | OS-level (seccomp/cgroups) | 2 Sprint |
| **Network Transport** | 进程内 (ADR-0051 §不变量) | MCP / OpenAI API (ADR-0050) | 3 Sprint |
| **Agent Observability** | audit events (ADR-0031) | OpenTelemetry integration | 1 Sprint |

---

## 六、设计原则总结

从 SOTA 趋势推导的 7 条设计原则：

| # | 原则 | 含义 | 来源 |
|---|------|------|------|
| 1 | **Agent 即一等公民** | Agent 是 API 核心, 不是附件 | OpenAI/Google 共识 |
| 2 | **多形态不锁死** | 同一 Agent 可 Skill/DSL/C++ 实现 | MCP 多语言 |
| 3 | **声明式优于命令式** | YAML/.agent.md 描述 > 代码编排 | K8s/LangGraph |
| 4 | **可观测是默认** | 每个 Agent 调用自动 trace | LangSmith/OTEL |
| 5 | **预算是硬约束** | Agent 不可绕过预算控制 | OpenAI CostConfig |
| 6 | **本地优先** | 离线可用, 联网增强 | pi-mono/tau 共识 |
| 7 | **组合优于继承** | Agent 通过 ToolRegistry/IInteractionBus 组合 | Unix pipe/OSGi |

---

## 七、与现有架构文档的关系

| 文档 | 关系 |
|------|------|
| `docs/specs/architecture.md` v2.2 | **互补** — 八层规范是"深度分层"，本文档是"应用视角" |
| `docs/guides/multi-domain-agent-architecture.md` | **演进** — 本文档将 Cognitive/Domain Worker 模式提升为 Agent-as-Plugin 范式 |
| `docs/adr/adr-0021-pdk-design.md` | **扩展** — PDK 从"工具脚手架"扩展为"Agent 脚手架" |
| `docs/adr/adr-0051-*.md` | **落地** — Spike 发现的 awkward pattern 是本文档的实践反馈 |
| `docs/archive/architecture/agent-as-plugin-architecture.md` | **基础** — 本文档建立在 Plugin 架构文档之上 |

---

## 八、下一步

1. **对齐讨论** — 团队确认核心命题："万物皆 Agent, Agent 皆 Plugin"
2. **Phase A 实施** — 构建 `examples/pdk_chat_demo` 验证范式
3. **ADR-0052 提案** — `AgentDescriptor` + `pdk_register_agent` 形式化
4. **SKILL.md Runtime** — 立项 OS SkillInterpreter
5. **SOTA 持续跟踪** — 每季度更新本文档 §二 趋势分析
