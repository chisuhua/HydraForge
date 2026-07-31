# Agent-as-Plugin 架构 v1.2

**日期**: 2026-07-22
**状态**: ✅ Approved (四层服务接口架构确立，含 Temporal Agent PoC 对齐)
**作者**: Architecture Working Group
**关联**: ADR-0019 ~ ADR-0051, `docs/specs/architecture.md` v2.2, `docs/architecture/agent-evolution-pipeline.md`, `docs/architecture/application-layer-sota-positioning-v2.md`, `openspec/changes/pkgm-temporal-agent/`

**前置文档**: `docs/archive/architecture/agent-as-plugin-architecture.md` v1.0 → v1.1 (2026-07-16)

**v1.2 变更**: §2 四层架构重构 (L2 拆分, 新 L3 PDK 接口契约层, L4 Agent 应用服务层), §2.4 层间依赖规则, §9.3 新增 Temporal Agent, 与 `examples/pkm_temporal_demo/DESIGN.md` PoC 对齐

---

## 一、核心命题

> **万物皆 Agent，Agent 皆 Plugin。**

HydraForge = **AgenticOS**（智能体操作系统）。

- AgenticOS **不提供任何业务逻辑**——只有基础设施。
- **每一个 Agent 都是一个 Plugin**（.so/.dll/.wasm），链接进入 OS。
- Agent 的**内部实现**对 OS 透明，且构成一个**可进化的连续谱**：

```
                    非结构化        结构化        高性能        可移植
                       │              │              │              │
SKILL.md ──(固化)──→ AgenticDSL ──(性能化)──→ C++ ──(可移植化)──→ Wasm
 (.md)               (.agent.md)   (native)     (native)       (.wasm)
  │                      │              │              │              │
 解释执行              编译图         原生代码       二进制沙箱
 隔离环境              可审计         可调试         跨平台
 快速迭代              可预算控制     复杂状态机     边缘部署
 复用技能生态          可热更新       系统级交互     强安全
```

- **Agent 可以选择只支持 SKILL.md 或只支持 AgenticDSL**；不需要强制同时支持三种形态。
- 如果 Agent 设计目标支持 **SKILL.md**，必须运行在**隔离环境**（解释器沙箱、进程隔离或 WebAssembly 沙箱）。
- 如果支持**固化**（把 SKILL.md 转写成 AgenticDSL），可进一步把 `.agent.md` 编译为 **Wasm 二进制**（边缘部署、跨平台分发、强隔离）。
- 也可以**一开始**就用 AgenticDSL 或 C++ 实现。
- **应用 = Agent 组合**。不存在"应用代码"——只有 Agent 的编排配置。

### 1.1 与 SOTA 定位差异

| 框架 | Agent 形态 | 内部实现 | 可扩展性 | 进化路径 |
|------|-----------|---------|---------|---------|
| **LangGraph** | Graph 节点 | Python lambda | 代码级 fork | 无 |
| **CrewAI** | Crew Agent | Python class | 子模块扩展 | 无 |
| **OpenAI Codex** | CLI / SDK | 内置 Agent | SDK 插件 | 无 |
| **MCP** | Server | 任意语言 | 协议级 | 无内部形态 |
| **OSGi** | Bundle | Java class | 动态 bundle | 无 AI 专用语义 |
| **HydraForge** | **PDK Plugin (.so/.wasm)** | **SKILL.md / DSL / C++ / Wasm 四态进化** | **Plugin 热加载** | **SKILL→DSL→C++→Wasm** |

**SOTA 关键证据**：
- **Manifest-first 是 SOTA 黄金标准**（Zylos Research 2026, MCP 2026-07-28 RC, OSGi, ATD, VS Code）
- **Process isolation** 是生产级插件的默认要求（MCP, VS Code, AOS, Agent libOS）
- **Capability-based security** 被 ATD、Agent libOS、Kaman MCP 共同采用
- **Schema-first contracts** 是跨生态互操作的基础（MCP, ATD, SW4RM）

---

## 二、架构总览

### 2.1 五层抽象 (v1.2)

v1.1 的四层架构存在一个关键问题：L2 "Agent Plugin Layer" 把两种完全不同职责的组件混在一起——
**原子工具提供者** (shell_tools, fs_tools) 和 **编排型 Agent** (loop_agent, g1_coding_assistant)。
v1.2 将其拆分为 L2 (Plugin 工具层) + L3 (PDK 接口契约层) + L4 (Agent 应用服务层)，
原有 L3 (App Mesh) 移至 L4 的子概念。

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 4: Agent 应用服务层 (Application Services)             │
│                                                              │
│  编排 L2 工具 + 组合 L3 接口 → 暴露为外部可消费的 Agent 服务  │
│  职责: 编排多个工具完成复杂任务, 对外暴露 API 面               │
│  依赖: L3 + L2 + L1 (通过 L3 间接访问 L2/L1)                  │
│                                                              │
│  ┌─ Temporal Agent (pdk/temporal_agent/) ─────────────┐     │
│  │ 编排 shell/exec (Temporal CLI) + 暴露 HTTP API     │     │
│  │ 实现 ITemporalClient 契约 (L3), 供 PKGM-Web 调用    │     │
│  └────────────────────────────────────────────────────┘     │
│  ┌─ Loop Agent (pdk/loop_agent/) ─────────────────────┐     │
│  │ think→decide→tool_call→observe 循环                │     │
│  └────────────────────────────────────────────────────┘     │
│  ┌─ G1 Coding Assistant (pdk/g1_coding_assistant/) ──┐     │
│  │ 调用 G3 Knowledge Base → 综合 → review comment     │     │
│  └────────────────────────────────────────────────────┘     │
│                                                              │
│  形态: .agent.md (DSL 编排) / C++ (复杂状态机)                │
├─────────────────────────────────────────────────────────────┤
│  Layer 3: PDK 接口契约层 (PDK Contract Layer)                 │
│                                                              │
│  L4 与 L2/L1 之间的唯一桥梁——L4 只能通过 L3 访问下层          │
│  包含: 抽象接口 + 注册宏 + 元数据规范                         │
│                                                              │
│  接口契约:                                                    │
│  ├─ IToolRegistry (9 pure virtual)     ← 工具注册/调用       │
│  ├─ ITemporalClient (5 pure virtual)   ← Temporal 抽象接口   │
│  ├─ IModelRouter (2 pure virtual)      ← 模型路由            │
│  ├─ IExecutionPolicy (5 pure virtual)  ← 执行策略            │
│  ├─ IApprovalHandler (1 pure virtual)  ← 审批处理            │
│  └─ AgentDescriptor (ADR-0053)         ← Agent 元数据         │
│                                                              │
│  注册机制:                                                    │
│  ├─ DECLARE_TOOL 宏                   ← 工具注册脚手架       │
│  ├─ DEFINE_AGENT 宏                   ← Agent 循环脚手架     │
│  ├─ PluginInfo / pdk_plugin_info      ← 插件 POD 元数据      │
│  └─ pdk_manifest.json                 ← 机器可读 manifest    │
│                                                              │
│  位置: include/agenticdsl/{contract,pdk,policy,plugin}/       │
├─────────────────────────────────────────────────────────────┤
│  Layer 2: Plugin 工具层 (Plugin Tools)                        │
│                                                              │
│  提供原子能力——不编排, 被 L4 通过 L3 调用                     │
│  每个工具 = 单一无副作用的操作 (或副作用显式声明)              │
│                                                              │
│  ┌─ shell_tools: shell/exec, shell/which, shell/env ─────┐  │
│  │ fork+exec+pipe, 危险命令黑名单                         │  │
│  └────────────────────────────────────────────────────────┘  │
│  ┌─ fs_tools: fs/read, fs/write, fs/list, fs/exists ────┐  │
│  └────────────────────────────────────────────────────────┘  │
│  ┌─ provider_agent: provider/register, resolve, list ────┐  │
│  │ 凭据管理 + LLM provider 路由                           │  │
│  └────────────────────────────────────────────────────────┘  │
│  ┌─ G3 Knowledge Base: knowledge_base/query ─────────────┐  │
│  │ 文档检索 + MockLLM 生成 + session store               │  │
│  └────────────────────────────────────────────────────────┘  │
│  ┌─ llama_engine: inference/* (12 工具) ─────────────────┐  │
│  └─ model_router: model_router/* (3 策略 .so) ───────────┘  │
│                                                              │
│  形态: C++ (原生性能) / Wasm (安全沙箱)                       │
├─────────────────────────────────────────────────────────────┤
│  Layer 1: AgenticOS Services (OS 服务层)                      │
│                                                              │
│  运行时基础设施——所有上层 (L2/L3/L4) 共享                     │
│                                                              │
│  ToolRegistry │ IInteractionBus │ IBudgetController           │
│  ILLMProvider+Factory │ PluginLoader │ SkillInterpreter      │
│  TopoScheduler │ UserSession/TaskSession/SubtaskSession       │
│  ToolCoordinator │ CostTrackingDecorator │ WasmRuntime       │
│  ManifestRegistry │ CapabilityRegistry │ TraceExporter        │
├─────────────────────────────────────────────────────────────┤
│  Layer 0: Runtime Core (运行时核心)                            │
│  DSLEngine │ NodeExecutor │ MarkdownParser │ ContextEngine    │
│  BudgetController(原子) │ LlamaAdapter                       │
│  ILLMProvider (v2 contract) │ IToolRegistry (9 虚函数)       │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 层级职责对比

| 层 | 职责 | 编排能力 | 暴露给外部 | 实现形态 | 示例 |
|----|------|:---:|:---:|---------|------|
| **L4** Agent 应用服务 | 编排 L2 工具, 暴露外部 API | ✅ 是 | ✅ HTTP/CLI | DSL / C++ | Temporal Agent, Loop Agent, G1 |
| **L3** PDK 接口契约 | 抽象接口 + 注册机制 + 元数据 | ❌ 否 | ❌ 否 | C++ 头文件 | IToolRegistry, ITemporalClient |
| **L2** Plugin 工具 | 原子能力提供 (不编排) | ❌ 否 | ❌ 否 | C++ / Wasm | shell_tools, fs_tools, provider_agent |
| **L1** OS Services | 运行时基础设施 | ❌ 否 | ❌ 否 | C++ 核心 | PluginLoader, IInteractionBus |
| **L0** Runtime Core | DSL 解析/调度/执行 | — | ❌ 否 | C++ 核心 | DSLEngine, NodeExecutor |

### 2.3 关键设计决策: 为什么拆 L2? (v1.1 → v1.2)

v1.1 的 L2 "Agent Plugin Layer" 把两类完全不同职责的组件揉在一起:

| 类型 | v1.1 归类 | 职责 | 实例 |
|------|----------|------|------|
| **原子工具** | L2 Plugin | 提供单一能力, 不调其他工具 | `shell/exec`, `fs/read`, `provider/resolve` |
| **编排 Agent** | L2 Plugin | 编排多个工具, 暴露外部接口 | `loop/run`, `coding_assistant/review`, `temporal/start_workflow` |

这导致:
- `pdk_chat_demo/DESIGN.md` 中 "6 个 Agent Plugin" 的模糊分类——有些是工具提供者, 有些是编排者
- Temporal Agent 的 `ITemporalClient` 抽象应该放在 L2 还是独立层不清晰
- ADR-0051 Spike 的 G1→G3 调用链 (`call_tool("knowledge_base/query")`) 无法用 v1.1 层级解释

v1.2 的核心决策: **编排是应用逻辑, 不是插件能力**。能"编排多个工具"和"被编排"
的组件应分层:

```
L4 编排 Agent ──call_tool()──→ L3 IToolRegistry ──dispatch──→ L2 原子工具
  (Temporal Agent)              (PDK 接口契约)                  (shell_tools)
```

### 2.4 层间依赖规则 (硬性约束)

| 规则 | 含义 | 反例 (禁止) |
|------|------|-----------|
| **R1**: L4 只通过 L3 访问 L2/L1 | L4 不能直接 `#include` L2 插件的内部头文件 | ❌ `#include "shell_tools/src/internal.h"` |
| **R2**: L3 不依赖 L4 | 接口契约层不感知具体 Agent 实现 | ❌ `IToolRegistry` 引入 Temporal class |
| **R3**: L2 不调其他 L2 | 原子工具自包含, 不编排其他工具 | ❌ `fs/read` 内部调 `shell/exec` |
| **R4**: L3 接口可在 L2 或 L4 中**实现** | 契约层只管定义, 实现者可以在任意上层 | ✅ `ITemporalClient`: L4 Temporal Agent 实现 |
| **R5**: L3→L1 访问是单向的 | L3 引用 L1 类型 (如 `ToolResult`), L1 不引用 L3 | ✅ `IToolRegistry.h` `#include "core/types/tool_result.h"` |

### 2.5 Temporal Agent 的层归属验证

以 Temporal Agent 的完整依赖链验证四层模型:

```
PKGM-Web 浏览器
    │ POST /api/agent/run {workflow_type, args}
    ▼
L4: Temporal Agent (pdk/temporal_agent/lib.so)          ← 编排 + 暴露外部 API
    │
    ├── 使用 L3 PDK 接口:
    │   ├── register_tool_function("temporal/start_workflow", meta, handler)  ← L3
    │   ├── call_tool("temporal/poll", args)                                   ← L3
    │   └── ITemporalClient::start_workflow_blocking()                         ← L3 契约
    │                                                                           │
    ├── 调用 L2 Plugin 工具 (通过 L3):                                         │
    │   └── call_tool("shell/exec", {command: "temporal workflow execute..."}) ← L2
    │                                                                           │
    └── 使用 L1 OS 服务:                                                        │
        ├── IInteractionBus::emit("temporal.workflow.start", ...)             ← L1
        └── PluginLoader::load_so("libTemporalAgent.so")                      ← L1
```

**验证通过**: 每一层只依赖其下方的层, L4 通过 L3 访问 L2, 无越级依赖。

### 2.6 与八层规范的映射 (更新)

| 本架构层 | 对应 specs/architecture.md v2.2 |
|---------|-------------------------------|
| L4: Agent 应用服务 | Layer 6 (应用) — Agent 编排 + 外部 API 暴露 |
| L3: PDK 接口契约 | (新增层, 原 specs 无直接对应 — v1.2 核心贡献) |
| L2: Plugin 工具 | Layer 2 (执行) + Layer 2.5 (标准库) — 原子能力 |
| L1: OS Services | Layer 0 runtime services + Layer 1 storage + 编译/解释服务 |
| L0: Runtime Core | Layer 0 agentic-dsl-runtime C++ 核心 |

### 2.7 与多领域智能体架构的演进关系

`docs/guides/multi-domain-agent-architecture.md` 定义了:
- **Cognitive Worker**（编排者）→ **L4 Agent 应用服务**
- **Domain Workers**（执行者，提供工具）→ **L2 Plugin 工具**

v1.2 将其精确映射为:

| 原角色 | v1.2 层 | 说明 |
|--------|---------|------|
| **Cognitive Agent** (编排者) | L4 Agent 应用服务 | Chat Agent, Loop Agent, Temporal Agent |
| **Domain Agent** (能力者) | L2 Plugin 工具 | FS Agent, Shell Agent, Provider Agent |

两者通过 **L3 PDK 接口契约** (`IToolRegistry::call_tool()`) 交互,
遵循 **R1** 依赖规则。

---

## 三、Agent Plugin 统一形态

### 3.1 核心原则

| 原则 | 含义 | 推导 |
|------|------|------|
| **P1: Agent 即 Plugin** | 每个 Agent 独立编译为 .so / .wasm | 链接方式统一，OS 无感知内部实现 |
| **P2: 内部形态可进化** | SKILL → DSL → C++ → Wasm 是单向/双向优化路径 | 允许 Agent 从原型演化为生产系统 |
| **P3: SKILL 必须隔离** | 解释执行不可信技能，必须沙箱化 | 安全原则（SOTA 共识） |
| **P4: 固化产物可编译** | 固化后的 .agent.md 可编译为 Wasm | 跨平台、强隔离、边缘部署 |
| **P5: 契约唯一** | Plugin Info + Manifest + Register Tools + Register Agent | 多种形态共享同一注册入口 |
| **P6: 自包含** | Plugin 包含所有资源（.md + 配置 + C++ + Wasm） | 插件可独立分发、加载 |
| **P7: 可组合** | Plugin 之间通过 ToolRegistry + IInteractionBus 交互 | 不直接 include 其他 plugin 的内部头文件 |
| **P8: Manifest-first** | 每个 Plugin 自带机器可读 manifest | SOTA 黄金标准（MCP, Zylos, OSGi, ATD） |
| **P9: Capability-based discovery** | 按 input/output schema 和能力发现 Agent | FIPA DF + OSGi Service Registry 映射 |
| **P10: Lifecycle-aware** | 支持 install/init/activate/deactivate/uninstall | OSGi, VS Code, MCP 共同模式 |

### 3.2 Plugin 目录结构规范

```
pdk/{agent_name}/
├── CMakeLists.txt                 # 编译为 .so 或 .wasm
├── pdk_manifest.json              # 机器可读插件元数据 (manifest-first)
│                                   # 来源：SOTA 共识 (MCP, Zylos, OSGi, ATD)
├── include/
│   └── {agent_name}.h             # 公开接口（仅契约类型）
├── src/
│   ├── pdk_entry.cpp              # 必需: pdk_plugin_info + pdk_register_tools
│   │                               # 可选: pdk_register_agent + pdk_manifest
│   └── .../*.cpp                  # 内部实现
├── agents/                        # 可选: .agent.md DSL 定义
│   ├── react.agent.md
│   └── plan_execute.agent.md
├── skills/                        # 可选: SKILL.md 声明式技能（隔离执行）
│   ├── core/SKILL.md
│   └── extensions/SKILL.md
├── wasm/                          # 可选: 固化后的 .wasm 产物
│   └── agent.wasm
├── config/                        # 可选: 默认配置
│   └── default.json
└── tests/
    └── test_{agent_name}.cpp
```

### 3.3 统一注册入口

每个 Plugin **必须**导出 C 符号，**可选**导出更多：

```cpp
// 必需 (ADR-0022 §1)
extern "C" const hydraforge::PluginInfo pdk_plugin_info;

// 必需 (ADR-0021 §3.1 + ADR-0051 §Decision 3)
// 注册工具到 IToolRegistry
extern "C" void pdk_register_tools(hydraforge::IToolRegistry& registry);

// 可选：如果此 Plugin 定义了一个 Agent，注册 Agent 描述
extern "C" void pdk_register_agent(hydraforge::AgentDescriptor& desc);

// 建议新增：机器可读 manifest（SOTA 标准）
extern "C" const char* pdk_manifest();
```

**pdk_manifest.json 推荐字段**（SOTA 标准，参考 MCP + Zylos + ATD）：

| 字段族 | 字段 | 说明 |
|--------|------|------|
| Identity | `id`, `name`, `version` | 唯一标识与版本 |
| ABI | `abi_version`, `min_host_version`, `max_host_version` | 兼容性约束（OSGi 风格） |
| Interface | `interface_versions` | 支持哪些 IAgent 版本（COM 风格） |
| Form | `implementation_forms` | `skill`, `dsl`, `cpp`, `wasm` |
| Capabilities | `capabilities`, `provided_tools`, `entry_tool` | 能力黄页（FIPA DF 映射） |
| Schema | `input_schema`, `output_schema` | JSON Schema 2020-12 |
| Trust | `publisher`, `signature`, `trust_level` | 来源与信任 |
| Resources | `timeout_ms`, `max_concurrent` | 资源约束 |
| Activation | `activation_events` | 懒加载触发条件（VS Code 模式） |
| Safety | `requires_isolation`, `side_effects` | 安全声明 |

### 3.4 新增 `AgentDescriptor`

```cpp
namespace hydraforge {

enum class AgentForm {
    Skill,      // 内部用 SKILL.md 实现（解释执行，隔离）
    DSL,        // 内部用 .agent.md DSL 图实现（编译）
    Cpp,        // 内部用 C++ 代码实现
    Wasm,       // 固化后的 WebAssembly 二进制
    Hybrid      // 混合形态
};

struct AgentDescriptor {
    std::string id;                 // 唯一标识 "chat.loop.react"
    std::string display_name;       // 显示名
    std::vector<AgentForm> forms;   // 此 Agent 支持的形态（可进化）
    std::string entry_tool;         // 入口工具名
    std::vector<std::string> provided_tools;
    std::vector<std::string> requires_agents;  // 依赖的其他 Agent
    nlohmann::json default_config;  // 默认配置
    bool requires_isolation;        // 是否需要隔离环境（SKILL 必须为 true）
    std::vector<std::string> interface_versions;  // 支持的 IAgent 版本
};

} // namespace hydraforge
```

---

## 四、四种内部实现形态与进化路径

### 4.1 形态选择决策树

```
你的 Agent 需要 LLM 推理吗？
├─ NO → 你的逻辑可以用 DAG 图表达吗？
│       ├─ YES → 能用 DSL 原语表达吗？
│       │        ├─ YES → Form::DSL (.agent.md)
│       │        │        └─ 需要性能/边缘部署？
│       │        │           └─ YES → Form::Wasm (DSL→Wasm)
│       │        └─ NO  → Form::Cpp (C++ 扩展节点)
│       │                 └─ 需要跨平台/隔离？
│       │                    └─ YES → Form::Wasm (C++→Wasm)
│       └─ NO → Form::Cpp (纯 C++ 状态机)
│              └─ 需要跨平台/隔离？
│                 └─ YES → Form::Wasm (C++→Wasm)
│
└─ YES → 你的推理策略经常变化吗？
         ├─ YES → Form::Skill (SKILL.md, 隔离执行，热更新)
         │        └─ 需要固化/审计吗？
         │           ├─ YES → 固化成 Form::DSL (.agent.md)
         │           │        └─ 需要性能/边缘部署？
         │           │           └─ YES → Form::Wasm (DSL→Wasm)
         │           └─ NO → 保持 Skill
         └─ NO → Form::DSL 或 C++
```

### 4.2 Form::Skill — 声明式技能工作流

**本质**：一个 SKILL.md 文件描述 Agent 的行为模式，由 OS 的 `SkillInterpreter` 在隔离环境中执行。

**关键约束**：
- **必须隔离**：解释执行不可信代码，必须运行在沙箱中（进程隔离、Wasm 解释器或受限脚本环境）。
- **不可直接调用 OS 内部服务**：只能通过 `IToolRegistry` / `IInteractionBus` 间接交互。
- **资源受限**：必须声明 `timeout_ms` / `max_concurrent` / `side_effects`。

**示例：Code Review Agent**

```
pdk/code_review_agent/
├── CMakeLists.txt
├── src/pdk_entry.cpp          # 注册 skill_runner 入口工具
├── skills/
│   ├── review/SKILL.md        # 核心审查逻辑
│   └── suggest/SKILL.md       # 改进建议逻辑
└── config/default.json
```

```cpp
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    .name = "code_review_agent",
    .version = "0.1.0",
    .abi_version = 2
};

extern "C" void pdk_register_tools(hydraforge::IToolRegistry& registry) {
    registry.register_tool_function(
        "code_review/run",
        ToolMetadata{ToolCategory::Execute, ...},
        [](const auto& args) -> nlohmann::json {
            auto skill_path = "skills/review/SKILL.md";
            // 委托给 OS 的 SkillInterpreter（隔离执行）
            return skill_interpreter::run(skill_path, args);
        }
    );
}

extern "C" void pdk_register_agent(hydraforge::AgentDescriptor& desc) {
    desc.id = "code.review";
    desc.forms = {AgentForm::Skill};
    desc.requires_isolation = true;  // SKILL 必须隔离
    desc.entry_tool = "code_review/run";
    desc.provided_tools = {"code_review/run", "code_review/suggest"};
}
```

**优势**：
- 修改 Agent 行为只需编辑 SKILL.md，无需重新编译
- 利用现有技能生态（OpenCode Skills, Claude Code Skills, etc.）
- LLM 可以直接理解和修改 SKILL.md

**限制**：
- 需要隔离运行时，启动开销大
- 性能受限于解释器
- 非结构化，难以形式化验证

### 4.3 Form::DSL — AgenticDSL 图编排

**本质**：用 `.agent.md` 定义 DAG 工作流，OS 用 `DSLEngine` 编译并执行。

**固化路径**：
- SKILL.md 可通过 LLM / 规则引擎转写为 .agent.md（结构固化）
- .agent.md 可被编译为中间图（`ParsedGraph`）
- 编译产物可以序列化、版本化、diff

**示例：ReactLoop Agent**

```
pdk/loop_agent/
├── CMakeLists.txt
├── src/pdk_entry.cpp
├── agents/
│   ├── react.agent.md         # think → decide → tool_call → observe 循环
│   ├── plan_execute.agent.md  # plan → execute → verify
│   └── fork_join.agent.md     # fork N branches → join
└── config/default.json
```

```markdown
# agents/react.agent.md
## metadata
- version: 0.1
- loop_type: react
- max_steps: 50
- budget_inheritance: strict

## nodes
### think
- type: generate
- prompt: "{{system_prompt}}\nConversation:\n{{history}}\nUser: {{user_input}}"
- tools: {{active_tools}}
- output: llm_response

### decide
- type: condition
- condition: "{{llm_response.tool_calls.length}} > 0"
- true: tool_call
- false: respond

### tool_call
- type: tool
- name: "{{llm_response.tool_calls[0].name}}"
- args: "{{llm_response.tool_calls[0].args}}"
- output: tool_result
- goto: observe

### observe
- type: assign
- history: "{{history}}\nTool: {{tool_result}}"
- goto: think

### respond
- type: end
- output: "{{llm_response.content}}"
```

**优势**：
- 流程可视化、可审计
- 预算控制内建（继承/严格/自适应）
- 可形式化验证部分属性
- LLM 可以生成/修改 .agent.md

**限制**：
- 循环 (goto/while) 支持需要继续增强
- 复杂状态管理需要 C++ 扩展

### 4.4 Form::Cpp — C++ 原生实现

**本质**：直接用 C++ 实现 Agent 逻辑，使用 `DEFINE_AGENT` 宏和 PDK API。

**适用场景**：性能关键、安全敏感、复杂状态机。

**优势**：
- 完全控制性能和资源
- 可以做系统级操作（env var / file / network）
- 与 PDK API 深度集成
- 编译时类型安全

### 4.5 Form::Wasm — 固化产物

**本质**：把 `.agent.md` 或 C++ 实现编译为 WebAssembly 二进制，运行在 OS 提供的 `WasmRuntime` 中。

**两种 Wasm 来源**：
1. **DSL → Wasm**：`.agent.md` → DSL 编译器 → Wasm 字节码（保留图结构，解释执行）
2. **C++ → Wasm**：C++ 实现用 Emscripten/wasi-sdk 编译为 Wasm（接近原生性能）

**Wasm 的优势**：
- 强沙箱（内存安全、capability-based 安全）
- 跨平台分发
- 边缘设备部署
- 启动快于 .so 加载（某些场景）

**关键设计**：OS 的 `WasmRuntime` 向 Wasm 模块暴露 capability-limited 的 host functions：
- `host_call_tool(name, args_json)` → 调用 `IToolRegistry`
- `host_emit_event(topic, payload)` → 调用 `IInteractionBus`
- `host_consume_budget(amount)` → 调用 `IBudgetController`
- **不暴露**：文件系统、网络、环境变量（除非显式 capability）

### 4.6 形态对比矩阵（含进化路径）

| 维度 | Skill | DSL | C++ | Wasm | Hybrid |
|------|:-----:|:---:|:---:|:----:|:------:|
| **热更新** | ✅ | ✅ | ❌ | ❌ | ⚠️ 部分 |
| **LLM 可修改** | ✅ | ✅ | ❌ | ❌ | ⚠️ 部分 |
| **性能** | 低 | 中 | 高 | 中高 | 混合 |
| **类型安全** | 弱 | 中 | 强 | 强 | 混合 |
| **可审计** | 弱 | ✅ | 需手加 | 强 | 混合 |
| **预算控制** | ❌ | ✅ | 需手动 | ✅ | 混合 |
| **开发门槛** | 低 | 中 | 高 | 高 | 高 |
| **隔离性** | 必须强 | 中 | 弱 | 强 | 混合 |
| **边缘部署** | 差 | 中 | 差 | ✅ | 混合 |
| **可固化来源** | → DSL | → C++ / Wasm | → Wasm | — | — |
| **适合场景** | 探索、生态复用 | 结构化生产 | 性能/安全 | 跨平台/边缘 | 复杂 Agent |

---

## 五、Agent 进化管线

详见 `docs/architecture/agent-evolution-pipeline.md`。核心流程：

```
1. Prototype (Skill)
   用 SKILL.md 快速验证业务逻辑
   │
   ▼ 固化 (Solidification)
2. Production DSL (.agent.md)
   通过 LLM 或规则将 SKILL.md 转写为结构化 DSL
   通过 DSL 验证器检查正确性
   │
   ▼ 性能化 (Performance Engineering)
3. Native C++
   将 DSL 热点路径用 C++ 节点替换
   保留 DSL 外层编排
   │
   ▼ 可移植化 (Portability)
4. WebAssembly (.wasm)
   将 DSL 或 C++ 编译为 Wasm
   分发到边缘/不可信环境
```

**关键概念**：
- **Solidification（固化）**：从非结构化的 SKILL.md 提取结构化的 .agent.md，行为等价但可审计、可优化。
- **Performance Engineering（性能化）**：不改变语义，将 DSL 节点替换为 C++ 实现。
- **Portability（可移植化）**：将可重入的 Agent 编译为 Wasm，保持契约不变。

---

## 六、Agent 契约

### 6.1 Agent 对 OS 的需求

| 需求 | OS 服务 | 现有 ADR |
|------|---------|---------|
| 工具注册 | `IToolRegistry::register_tool_function()` | ADR-0004 V2 |
| 工具调用 | `IToolRegistry::call_tool()` | ADR-0023 |
| LLM 推理 | `ILLMProvider::generate()` | ADR-0001/0042 |
| 事件推送 | `IInteractionBus::emit()` | ADR-0019 |
| 事件订阅 | `IInteractionBus::subscribe()` | ADR-0019 |
| 预算查询 | `IBudgetController::try_consume_*()` | ADR-0033 |
| 执行策略 | `IExecutionPolicy::requires_approval()` | ADR-0031 |
| DSL 执行 | `DSLEngine::from_markdown()` + `run()` | — |
| Skill 执行 | `SkillInterpreter::run()` | 新增 |
| Wasm 执行 | `WasmRuntime::instantiate()` | 新增 |
| 插件加载 | `PluginLoader::load_so()` / `load_wasm()` | ADR-0022 |
| Session 作用域 | `UserSession/TaskSession/SubtaskSession` | ADR-0033 |

### 6.2 Agent 对 OS 的承诺

| 承诺 | 验证方式 |
|------|---------|
| 导出 `pdk_plugin_info` | PluginLoader dlsym 检查 |
| 导出 `pdk_register_tools` | PluginLoader dlsym 检查 |
| 提供 `pdk_manifest()` | 运行时 JSON 校验 |
| ABI 版本兼容 | `pdk_plugin_info.abi_version` >= OS 要求 |
| 工具命名遵循 slash-only | ADR-0043 |
| 返回值遵循 ToolResult 信封 | ADR-0023 |
| 异常不穿透 Plugin 边界 | ADR-0021 SafeExec 封装 |
| SKILL 形态必须声明隔离 | `AgentDescriptor.requires_isolation = true` |
| Wasm 形态必须 capability 受限 | WasmRuntime host function 白名单 |
| 线程安全声明 | metadata 中标注 single-threaded / thread-safe |

### 6.3 Agent 对 Agent 的协议 (v1.2 更新)

Agent 之间**不直接通信**。所有交互通过 L3 PDK 接口契约层转发到 L1 OS 基础设施：

```
L4 Agent A ──call_tool()──→ L3 IToolRegistry ──dispatch──→ L2 Plugin B (注册的工具)
L4 Agent A ──emit()───────→ L1 IInteractionBus ──subscribe──→ L4 Agent B (监听者)
L4 Agent A ──run()────────→ L0 DSLEngine ──execute .agent.md──→ 内含 L2 Plugin C 的工具调用
L4 Agent A ──run()────────→ L1 SkillInterpreter ──execute SKILL.md──→ 受限调用 L2 Plugin C
L4 Agent A ──invoke()─────→ L1 WasmRuntime ──execute .wasm──→ 受限调用 L2 Plugin C
```

层级视角下的交互路径:

```
┌──────────────┐    L3 IToolRegistry     ┌──────────────┐
│ L4 Temporal  │──── call_tool() ───────→│ L2 shell_tools│
│ Agent        │←─── return json ────────│              │
└──────────────┘                         └──────────────┘
       │ L4                                  │ L2
       │ 编排 L2 工具                         │ 原子执行
       │ 暴露外部 API                         │ 不调其他工具
```

---

## 七、Agent 组合模型 (v1.2 更新)

### 7.1 五种组合模式

#### 模式 A：工具调用链（最简单，L4 → L3 → L2）

```
ChatAgent (L4)
  └─ call_tool("loop/run", {prompt, tools: [...]})    ← L3 IToolRegistry
       └─ LoopAgent 内部 (L4):
            ├─ call_tool("fs/read", ...)               ← L2 fs_tools
            ├─ call_tool("shell/exec", ...)            ← L2 shell_tools
            └─ call_tool("provider/resolve", ...)       ← L2 provider_agent
```

#### 模式 A2：Temporal Agent 作为 L4 编排 L2 (v1.2 新增)

```
PKGM-Web 浏览器
  └─ POST /api/agent/run → L4 Temporal Agent
       └─ call_tool("temporal/start_workflow", {...})  ← L3 ITemporalClient 契约
            ├─ call_tool("shell/exec",                 ← L2 shell_tools
            │     {command: "temporal workflow execute --output json"})
            │
            ├─ call_tool("temporal/poll", {...})       ← 轮询 L3 ITemporalClient
            │     └─ call_tool("shell/exec",
            │           {command: "temporal workflow describe --output json"})
            │
            └─ IInteractionBus::emit(                   ← L1
                  "temporal.workflow.complete", {...})
```

#### 模式 B：DSL 子图嵌入（声明式）

多 Agent 的固定流程编排，如 Plan→Execute→Review 循环。

#### 模式 C：事件驱动协作（最灵活）

```
ChatAgent ──── emit("chat.user.input", {...}) ────→ IInteractionBus
                                                         │
                          ┌──────────────────────────────┤
                          ▼                              ▼
                  LoopAgent 订阅                  BudgetAgent 订阅
                  "chat.user.input"               "chat.user.input"
                          │                              │
                  emit("loop.start", {...})        检查预算是否充足
                          │                              │
                          ▼                              ▼
                  执行 think→act→observe         emit("budget.check.ok")
                          │
                  emit("loop.done", {response})
                          │
                          ▼
                  ChatAgent 订阅 "loop.done"
                  输出响应给用户
```

#### 模式 D：Wasm 模块组合（新增，跨平台/强隔离）

```
ChatAgent (.so)
  └─ call_tool("loop/run", {prompt})
       └─ LoopAgent (.agent.md)
            ├─ call_tool("fs/read", ...)   → FS Agent (.so)
            ├─ call_tool("shell/exec", ...) → Shell Agent (.so)
            ├─ call_tool("provider/resolve", ...) → Provider Agent (.so)
            └─ invoke_wasm("review/grammar.wasm", ...) → Review Agent (.wasm)
```

### 7.2 组合模式的层级关系

```
模式 C (事件驱动)           ← 最灵活，最松耦合
    └─ 包含 模式 B (DSL 子图)   ← 结构化流程
         └─ 包含 模式 A (工具调用链)  ← 最简单的请求-响应
              └─ 可调用 模式 D (Wasm Agent)  ← 强隔离/跨平台
```

---

## 八、典型应用组装

### 8.1 Chat 应用 (CLI) — 含层标注

```yaml
agents:
  - id: chat.orchestrator    # L4
    plugin: libChatAgent.so
    form: cpp
  
  - id: chat.loop            # L4
    plugin: libLoopAgent.so
    form: dsl
  
  - id: pkm.temporal         # L4 (v1.2 新增)
    plugin: libTemporalAgent.so
    form: cpp
  
  - id: infra.provider       # L2
    plugin: libProviderAgent.so
    form: cpp
  
  - id: infra.session        # L2
    plugin: libSessionAgent.so
    form: hybrid
  
  - id: tool.fs              # L2
    plugin: libFSTools.so
    form: cpp
  
  - id: tool.shell           # L2
    plugin: libShellTools.so
    form: cpp
  
  - id: tool.code_review     # L4 (编排 L2)
    plugin: libCodeReviewAgent.wasm
    form: wasm

orchestration:
  model: event_driven
  entry_agent: chat.orchestrator
  event_flow:
    user_input → chat.orchestrator
    chat.orchestrator → chat.loop (via L3 IToolRegistry::call_tool)
    chat.loop → tool.fs, tool.shell, pkm.temporal (via L3)
    chat.loop → infra.provider (via L3 provider/resolve)
```

---

## 九、现有设施的 Plugin 化路径

### 9.1 新增 OS 服务（支持进化路径）

| 服务 | 职责 | 状态 | 优先级 |
|------|------|------|--------|
| `SkillInterpreter` | 隔离解释 SKILL.md | 🟡 V1 done ([ADR-0066](../adr/adr-0066-skill-interpreter-arch.md)); V2: `host_read_context` + `derive_capability()` + `SkillCapability` 动态注入 | P1 |
| `AgenticDSLCompiler` | .agent.md → ParsedGraph | ✅ (DSLEngine) | P0 |
| `WasmRuntime` | 加载并执行 .wasm Agent | 待实现 | P2 |
| `AgentEvolutionEngine` | 管理 SKILL → DSL → C++ → Wasm 的转换 | 待实现 | P3 |
| `CapabilityRegistry` | 按能力发现 Agent（FIPA DF 角色） | 待实现 | P1 |
| `ManifestRegistry` | 按 manifest 索引已加载 Plugin | 待实现 | P1 |

### 9.2 保留在 OS 的组件

| 组件 | 保留理由 |
|------|----------|
| `DSLEngine` | 纯计算引擎，所有 Plugin 依赖它 |
| `IToolRegistry` | Agent 发现机制的基础 |
| `IInteractionBus` | 消息总线，Plugin 化产生循环依赖 |
| `IBudgetController` (原子) | 安全闸门 |
| `IExecutionPolicy` | 安全决策，失败必须 fail-stop |
| `ApprovalHandler` | 安全边界 |
| `TopoScheduler` | DAG 调度器 |
| `PluginLoader` | 加载其他 Plugin 的基础 |
| `SkillInterpreter` | 需要隔离运行时，不能 Plugin 化 |
| `WasmRuntime` | 需要 OS 级 host function 控制 |

### 9.3 当前 PDK 插件盘点与建议新增 (v1.2 更新)

**L2 Plugin 工具层 (原子能力)**:

| 插件 | 形态 | 工具 | 状态 |
|------|:----:|-----------|------|
| `pdk/llama_engine/` | C++ | inference/* (12 工具) | ✅ ADR-0035/0040/0044 |
| `pdk/model_router/` | C++ | model_router/* | ✅ ADR-0034 |
| `pdk/shell_tools/` | C++ | shell/exec, shell/which, shell/env | ✅ |
| `pdk/fs_tools/` | C++ | fs/read, fs/write, fs/list, fs/exists | ✅ |
| `pdk/provider_agent/` | C++ | provider/register, resolve, list, health | ✅ |
| `pdk/budget_agent/` | C++ | budget/query, set_limit, alerts | ✅ |
| `pdk/session_agent/` | C++ | session/history, compact, branch, persist | ✅ |
| `pdk/g3_knowledge_base/` | C++ | knowledge_base/query | ✅ ADR-0051 |

**L4 Agent 应用服务层 (编排 + 外部 API)**:

| 插件 | 形态 | 工具 | 状态 |
|------|:----:|-----------|------|
| `pdk/loop_agent/` | DSL | loop/run, loop/plan_execute, loop/fork_join | ✅ |
| `pdk/g1_coding_assistant/` | C++ | coding_assistant/review | ✅ ADR-0051 |
| `pdk/temporal_agent/` | C++ | temporal/start_workflow, start_async, poll, signal, query | 🟡 PoC 规划中 |

| 建议新增 | 推荐层 | 推荐形态 | 理由 |
|-------|:------:|:--------:|------|
| **Temporal Agent** | L4 | C++ | 编排 L2 shell_tools 调 Temporal CLI; 暴露 HTTP API 给 PKGM-Web; 实现 L3 ITemporalClient 契约 |
| **Browser Agent** | L2 | C++ | 需要 Playwright/Puppeteer 集成 |
| **Search Agent** | L2 | Skill | 搜索策略灵活，依赖外部 API |
| **Code Agent** | L4 | Skill | 代码审查/生成策略经常变化，编排 L2 工具 |

> **层归属判定规则**: 如果插件内部调用了其他 L2 工具 (= 编排行为), 则归属 L4; 如果仅提供单一原子操作, 则归属 L2。

---

## 十、SOTA 定位与设计原则

### 10.1 第一性原则

| 原则 | 推导 | 来源 |
|------|------|------|
| **关注点分离** | OS 只管基础设施，Agent 只管领域逻辑 | 微内核架构 (Mach/MINIX) |
| **Manifest-first** | 每个插件自带机器可读描述 | MCP, Zylos, OSGi, ATD |
| **Capability-based discovery** | 按能力而非名字发现 Agent | FIPA DF, OSGi Service Registry |
| **多形态透明** | 调用者不关心 Agent 内部实现 | COM IUnknown / ROS 2 black box |
| **可组合性** | Agent 间通过标准协议组合 | Unix pipe / FIPA ACC |
| **安全纵深** | 每层验证，不信任 Plugin 声明 | Layer Profile (ADR-0004/0031) |
| **可观测性** | 每个 Agent 调用有 trace | ADR-0031 audit events / OpenTelemetry |
| **进程隔离** | 解释执行不可信代码必须沙箱 | MCP, AOS, Agent libOS, VS Code |
| **Schema-first contracts** | 输入输出必须有机器可读 schema | MCP, ATD, SW4RM |
| **Lifecycle-aware** | install/init/activate/deactivate/uninstall | OSGi, VS Code, MCP |

### 10.2 与 SOTA 框架的定位

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

**HydraForge 的独特价值**：
1. **Agent 四形态透明 + 进化路径** — 唯一支持 SKILL→DSL→C++→Wasm 的框架
2. **编译时安全 + 运行时热更新** — C++ 插件提供性能，SKILL.md 提供灵活性
3. **DSL 工作流可审计** — .agent.md 是声明式的、可视化的、LLM 可理解的
4. **预算控制内建** — 每个 Agent 执行消耗预算，防止无限递归
5. **本地优先 + 边缘 Wasm** — llama.cpp 后端，无需云端 API

### 10.3 与 SOTA 的差距与优先级

#### 🔴 P0：必须补齐

| 缺口 | SOTA 参考 | 建议方案 |
|------|----------|---------|
| **Agent 插件无 manifest** | Zylos, MCP, ATD | 添加 `pdk_manifest()` + `pdk_manifest.json` |
| **缺 capability-based discovery** | FIPA DF, OSGi Service Registry | `CapabilityRegistry` + `AgentDescriptor` |
| **缺 Skill 隔离运行时** | MCP, Agent libOS, AOS | `SkillInterpreter` + 沙箱 |
| **缺 Wasm 运行时** | 边缘部署需求 | `WasmRuntime` + capability-limited host functions |
| **缺跨进程/跨网络协议** | A2A, MCP | Phase 6 服务化引入 `RemoteAgentAdapter` |

#### 🟠 P1：强烈建议

| 缺口 | SOTA 参考 | 建议方案 |
|------|----------|---------|
| **缺 hot-reload** | OSGi bundle update | 文件 watcher + reload hook |
| **缺 lazy-load / activation events** | VS Code `activationEvents` | `pdk_activation_events()` |
| **缺 semver 版本约束** | OSGi version ranges | `PluginInfo` 扩展 |
| **缺工具 input/output schema 强制校验** | MCP, ATD | `ToolMetadata` 增 schema 字段 |
| **缺 distributed Bus** | Pipecat `PgmqBus` | Redis/PostgreSQL Bus backend |
| **缺 OpenTelemetry trace** | MCP `traceparent` | `OpenTelemetryExporter` |

#### 🟡 P2：可选增强

| 缺口 | SOTA 参考 | 建议方案 |
|------|----------|---------|
| **缺 conformance test suite** | MCP SEP-2484 | `pdk-conformance-test-kit` |
| **缺多语言 PDK** | Rust/Python bindings | 独立仓库 `hydraforge-pdk-*` |
| **缺 Agent Marketplace** | VS Code Marketplace | Phase B 实现 |

---

## 十一、关键决策记录 (v1.2 更新)

| 编号 | 决策 | 状态 | 关联 |
|------|------|------|------|
| **A1** | Agent = Plugin (.so / .wasm) | ✅ 确定 | ADR-0021/0022 |
| **A2** | 四形态进化路径（Skill/DSL/C++/Wasm） | ✅ 确定 | 新决策 |
| **A3** | SKILL.md 必须隔离 | ✅ 确定 | 安全原则 + SOTA |
| **A4** | DSL 可固化，可编译为 Wasm | ✅ 确定 | 新决策 |
| **A5** | `pdk_register_agent` + `pdk_manifest` 新增 | ✅ 确定 | 扩展 ADR-0022 |
| **A6** | OS 不解析 Agent 内部形态 | ✅ 确定 | P2 原则 |
| **A7** | Agent 间不直接通信 | ✅ 确定 | ADR-0019/0031 |
| **A8** | SkillInterpreter / WasmRuntime 在 L1 OS 层 | ✅ 确定 | 新决策 |
| **A9** | Manifest-first 注册 | ✅ 确定 | SOTA 共识 |
| **A10** | Capability-based discovery | ✅ 确定 | FIPA DF + OSGi |
| **A11** | Lifecycle-aware 插件管理 | ✅ 确定 | OSGi + VS Code + MCP |
| **A12** | Schema-first tool contracts | ✅ 确定 | MCP + ATD |
| **A13** | **L2/L3/L4 三层拆分** (v1.2) | ✅ 确定 | 编排 vs 提供 职责分离 |
| **A14** | **L3 PDK 接口契约层独立** (v1.2) | ✅ 确定 | IToolRegistry + ITemporalClient 归属 |
| **A15** | **Temporal Agent 归属 L4** (v1.2) | ✅ 确定 | 与 PKGM-Web PoC 对齐 |
| **A16** | **L4 只通过 L3 访问 L2/L1** (v1.2) | ✅ 确定 | 依赖规则 R1 |

> **A13-A16** 为 v1.2 新增决策, 源于 `openspec/changes/pkgm-temporal-agent/` 审查 +
> `examples/pkm_temporal_demo/DESIGN.md` PoC 设计过程中发现的架构模糊性。

---

## 十二、相关文档

| 文档 | 路径 |
|------|------|
| Agent 进化管线 | `docs/architecture/agent-evolution-pipeline.md` |
| 应用层 SOTA 定位 (v2, 当前有效) | `docs/architecture/application-layer-sota-positioning-v2.md` |
| 应用层 SOTA 定位 (v1, 已归档) | `docs/archive/architecture/application-layer-sota-positioning.md` |
| Agent-as-Plugin v1.0 (基线) | `docs/archive/architecture/agent-as-plugin-architecture.md` |
| AgenticOS 八层架构 (v2.2) | `docs/specs/architecture.md` |
| 多领域智能体架构 | `docs/guides/multi-domain-agent-architecture.md` |
| PKM Temporal Agent PoC 设计 | `examples/pkm_temporal_demo/DESIGN.md` |
| PDK Chat Demo 设计 | `examples/pdk_chat_demo/DESIGN.md` |
| PDK 设计 | `docs/adr/adr-0021-pdk-design.md` |
| Plugin 加载 | `docs/adr/adr-0022-plugin-loading.md` |
| PDK 工具命名 | `docs/adr/adr-0043-pdk-tool-naming-convention.md` |
| PDK Composition Spike | `docs/adr/adr-0051-phase6-pdk-composition-spike.md` |
| Session 层次 | `docs/adr/adr-0033-session-hierarchy.md` |
| ToolCoordinator | `docs/adr/adr-0031-execution-policy.md` |
| PDK Chat Demo 设计 | `docs/superpowers/specs/2026-07-16-pdk-chat-demo-design.md` |
