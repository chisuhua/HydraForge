# Agent-as-Plugin 架构 v1.0

**日期**: 2026-07-16
**状态**: 🟡 Proposed (架构讨论)
**作者**: Architecture Working Group
**关联**: ADR-0019 ~ ADR-0051, `docs/specs/architecture.md` v2.2

---

## 一、核心命题

> **万物皆 Agent，Agent 皆 Plugin。**

HydraForge = **AgenticOS**（智能体操作系统）。

- AgenticOS **不提供任何业务逻辑**——只有基础设施。
- **每一个 Agent 都是一个 Plugin**（.so/.dll），链接进入 OS。
- Agent 的**内部实现**对 OS 完全透明，可以是：
  - **SKILL.md**（声明式技能工作流）
  - **AgenticDSL .agent.md**（DSL 图编排）
  - **C++ 代码**（基于 AgenticDSL 图的扩展，用于复杂或性能敏感场景）
- **应用 = Agent 组合**。不存在"应用代码"——只有 Agent 的编排配置。

```
┌─────────────────── 应用 (Application) ───────────────────────┐
│                                                                │
│  应用 = Agent 的组合 + 配置 (config.json / YAML / CLI)         │
│                                                                │
│  ┌─ Chat Agent ──┐  orchestrates  ┌─ Loop Agent ──┐          │
│  │  (编排者)      │ ──────────────→│  (思考循环)     │          │
│  └───────────────┘                └────────────────┘          │
│       │                               │                       │
│       │                               ├──→ Provider Agent     │
│       │                               ├──→ Session Agent      │
│       │                               ├──→ Budget Agent       │
│       │                               └──→ Code Agent         │
│       │                                                       │
│       └──→ FS Agent, Shell Agent, Browser Agent, ...          │
│                                                                │
├───────────────── AgenticOS (HydraForge 基础设施) ────────────┤
│                                                                │
│  DSLEngine │ ToolRegistry │ ILLMProvider │ IInteractionBus    │
│  IBudgetController │ IExecutionPolicy │ ToolCoordinator       │
│  ApprovalHandler │ TopoScheduler │ UserSession types          │
│  ILLMProviderFactory │ EventBus │ CostTrackingDecorator       │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### 1.1 与 pi-mono / tau / OpenAI Codex 的根本区别

| 范式 | Agent 形态 | 内部实现 | 可扩展性 |
|------|-----------|---------|---------|
| **pi-mono** | Library (Python) | 硬编码 AgentLoop | 代码级 fork |
| **tau** | Library (Python) | AgentHarness 类 | 子模块扩展 |
| **OpenAI Codex** | CLI / SDK | 内置 Agent | SDK 插件 |
| **HydraForge** | **PDK Plugin (.so)** | **SKILL.md / DSL / C++ 三选一** | **Plugin 热加载** |

---

## 二、架构总览

### 2.1 四层抽象（取代八层规范）

AgenticOS 在架构层面简化为**四层**（对应 `docs/specs/architecture.md` v2.2 的八层，但面向 Plugin 范式重新表达）：

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 3: Application Mesh (应用网)                           │
│  Agent 组合 + 配置 + CLI/TUI/Web UI                          │
│  没有"应用代码"——只有 Agent 编排声明                          │
├─────────────────────────────────────────────────────────────┤
│  Layer 2: Agent Plugin Layer (Agent 插件层)                   │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  每个 Agent = 一个 PDK Plugin (.so)                     │  │
│  │  ├─ SKILL.md      声明式 (LLM 编排, 热更新)              │  │
│  │  ├─ .agent.md     DSL 图 (结构化, 可审计)                │  │
│  │  └─ C++ code      原生 (高性能, 复杂状态)                │  │
│  │                                                         │  │
│  │  统一契约: pdk_plugin_info + pdk_register_tools          │  │
│  │             + pdk_register_agent (新增)                  │  │
│  └────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  Layer 1: AgenticOS Services (OS 服务层)                      │
│  ToolRegistry │ IInteractionBus │ IBudgetController           │
│  ILLMProvider+Factory │ IExecutionPolicy │ ApprovalHandler    │
│  TopoScheduler │ UserSession/TaskSession/SubtaskSession       │
│  ToolCoordinator │ CostTrackingDecorator │ PluginLoader       │
├─────────────────────────────────────────────────────────────┤
│  Layer 0: Runtime Core (运行时核心)                            │
│  DSLEngine │ NodeExecutor │ MarkdownParser │ ContextEngine    │
│  BudgetController(原子) │ LlamaAdapter │ TraceExporter        │
│  ILLMProvider (v2 contract) │ IToolRegistry (9 虚函数)       │
└─────────────────────────────────────────────────────────────┘
```

**层级职责边界**：

| 层 | 职责 | 变化频率 | 谁拥有 |
|----|------|---------|--------|
| L3: App Mesh | 应用编排、用户体验、部署 | 高（per-app） | 应用开发者 |
| L2: Plugin | Agent 领域逻辑 | 中（per-agent） | Agent 开发者 |
| L1: OS Services | 跨 Agent 共享服务 | 低（per-release） | HydraForge 团队 |
| L0: Runtime | 纯计算引擎 | 极低（ABI稳定5年） | HydraForge 团队 |

### 2.2 与八层规范的映射

| 本架构层 | 对应 specs/architecture.md v2.2 |
|---------|-------------------------------|
| L3: App Mesh | Layer 6 (应用) + Layer 5 (交互) + Layer 4.5 (社会) |
| L2: Plugin | Layer 4 (认知) + Layer 3 (推理) + Layer 2 (执行) + Layer 2.5 (标准库) |
| L1: OS Services | Layer 0 runtime services + Layer 1 storage |
| L0: Runtime Core | Layer 0 agentic-dsl-runtime C++ 核心 |

**关键区别**：八层规范按**执行深度**分层（编译/执行/状态/推理/认知），本架构按**变化频率和所有权**分层（Runtime/Services/Agents/Apps）。两种视角互补而非替代。

---

## 三、Agent Plugin 统一形态

### 3.1 核心原则

| 原则 | 含义 | 推导 |
|------|------|------|
| **P1: Agent 即 Plugin** | 每个 Agent 独立编译为 .so | 链接方式统一，OS 无感知内部实现 |
| **P2: 内部形态透明** | OS 不区分 SKILL/DSL/C++ | 同一契约，三种实现路径 |
| **P3: 契约唯一** | Plugin Info + Register Tools + Register Agent | 三种形态共享同一注册入口 |
| **P4: 自包含** | Plugin 包含所有资源（.md + 配置 + C++） | 插件可独立分发、加载 |
| **P5: 可组合** | Plugin 之间通过 ToolRegistry + IInteractionBus 交互 | 不直接 include 其他 plugin 的内部头文件 |

### 3.2 Plugin 目录结构规范

```
pdk/{agent_name}/
├── CMakeLists.txt                 # 编译为 .so
├── plugin.json                    # 插件元数据 (可选, pdk_plugin_info 优先)
├── include/
│   └── {agent_name}.h             # 公开接口 (仅契约类型)
├── src/
│   ├── pdk_entry.cpp              # 必需: pdk_plugin_info + pdk_register_tools
│   │                               # 可选: pdk_register_agent
│   └── .../*.cpp                  # 内部实现
├── agents/                        # 可选: .agent.md DSL 定义
│   ├── react.agent.md
│   └── plan_execute.agent.md
├── skills/                        # 可选: SKILL.md 声明式技能
│   ├── core/SKILL.md
│   └── extensions/SKILL.md
├── config/                        # 可选: 默认配置
│   └── default.json
└── tests/
    └── test_{agent_name}.cpp
```

### 3.3 统一注册入口

每个 Plugin **必须**导出两个 C 符号，**可选**导出第三个：

```cpp
// 必需 (ADR-0022 §1)
extern "C" const hydraforge::PluginInfo pdk_plugin_info;

// 必需 (ADR-0021 §3.1 + ADR-0051 §Decision 3)
// 注册工具到 IToolRegistry
extern "C" void pdk_register_tools(hydraforge::IToolRegistry& registry);

// 可选 (新增 — Agent Plugin 注册)
// 如果此 Plugin 定义了一个 Agent（不只是工具集合），注册 Agent 描述
extern "C" void pdk_register_agent(hydraforge::AgentDescriptor& desc);
```

**新增 `AgentDescriptor`**（OS 用于发现 Agent 能力）：

```cpp
// include/agenticdsl/pdk/agent_descriptor.h
namespace hydraforge {

enum class AgentForm {
    Skill,      // 内部用 SKILL.md 实现
    DSL,        // 内部用 .agent.md DSL 图实现
    Cpp,        // 内部用 C++ 代码实现
    Hybrid      // 混合 (e.g., DSL 图 + C++ 扩展节点)
};

struct AgentDescriptor {
    std::string id;                 // 唯一标识 "chat.loop.react"
    std::string display_name;       // 显示名 "ReactLoop Agent"
    AgentForm form;                 // 内部实现形态
    std::string entry_tool;         // 入口工具名 "loop/run"
    std::vector<std::string> provided_tools;   // 提供的工具列表
    std::vector<std::string> requires_agents;  // 依赖的其他 Agent
    nlohmann::json default_config;  // 默认配置
};

} // namespace hydraforge
```

---

## 四、三种内部实现形态

### 4.1 形态选择决策树

```
你的 Agent 需要 LLM 推理吗？
├─ NO → 你的逻辑可以用 DAG 图表达吗？
│       ├─ YES → 能用 DSL 原语表达吗？
│       │        ├─ YES → Form::DSL (.agent.md)
│       │        └─ NO  → Form::Cpp (C++ 扩展节点)
│       └─ NO → Form::Cpp (纯 C++ 状态机)
│
└─ YES → 你的推理策略经常变化吗？
         ├─ YES → Form::Skill (SKILL.md, 热更新)
         │        └─ 但 LLM 编排需要结构化？
         │           └─ Form::DSL (.agent.md + LLM 节点)
         └─ NO → Form::Cpp (C++ 硬编码循环, 性能优先)
```

### 4.2 Form::Skill — 声明式技能工作流

**本质**：一个 SKILL.md 文件描述 Agent 的行为模式，由 OS 的 SKILL Interpreter 执行。

**适用场景**：
- 需要 LLM 驱动的意图理解和动态编排
- 策略经常变化（无需重新编译）
- 领域知识丰富、流程灵活

**示例：Coding Review Agent**

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
// pdk_entry.cpp
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    .name = "code_review_agent",
    .version = "0.1.0",
    .abi_version = 2
};

extern "C" void pdk_register_tools(hydraforge::IToolRegistry& registry) {
    // 注册一个 "skill_runner" 工具 → 加载并执行 SKILL.md
    registry.register_tool_function(
        "code_review/run",
        ToolMetadata{ToolCategory::Execute, ...},
        [](const auto& args) -> nlohmann::json {
            auto skill_path = "skills/review/SKILL.md";
            // 委托给 OS 的 SkillInterpreter
            return skill_interpreter::run(skill_path, args);
        }
    );
}

extern "C" void pdk_register_agent(hydraforge::AgentDescriptor& desc) {
    desc.id = "code.review";
    desc.form = AgentForm::Skill;
    desc.entry_tool = "code_review/run";
    desc.provided_tools = {"code_review/run", "code_review/suggest"};
}
```

**优势**：
- 修改 Agent 行为只需编辑 SKILL.md，无需重新编译
- 对非 C++ 开发者友好
- LLM 可以直接理解和修改 SKILL.md

**限制**：
- 依赖 OS 提供 SkillInterpreter（L1 服务）
- 性能受限于 SKILL.md → DSL → C++ 解释链路
- 不适合高频调用场景

### 4.3 Form::DSL — AgenticDSL 图编排

**本质**：用 `.agent.md` 定义 DAG 工作流，OS 用 DSLEngine 执行。

**适用场景**：
- 流程结构化、可审计
- 需要 fork/join/condition 等 DAG 原语
- 需要预算控制和预算继承
- 中等复杂度，需要 LLM 参与部分决策

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
# ReactLoop Agent — Think-Decide-Act-Observe 循环

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

```cpp
// pdk_entry.cpp
extern "C" void pdk_register_tools(hydraforge::IToolRegistry& registry) {
    registry.register_tool_function(
        "loop/run",
        ToolMetadata{ToolCategory::Execute, ApprovalPolicy::agent_only},
        [](const auto& args) -> nlohmann::json {
            auto loop_type = args.at("loop_type");
            auto agent_file = loop_type == "react"
                ? "agents/react.agent.md"
                : "agents/plan_execute.agent.md";
            
            // 创建子 DSLEngine 执行 .agent.md 图
            auto sub_engine = DSLEngine::from_markdown_file(agent_file);
            auto ctx = LayeredContext::from_json(args);
            auto result = sub_engine->run(ctx);
            return result.to_json();
        }
    );
}
```

**优势**：
- 流程可视化（DAG 图可以直接渲染）
- 预算控制内建（继承/严格/自适应）
- 可审计（每个节点有 Trace 记录）
- LLM 可以生成/修改 .agent.md

**限制**：
- 循环 (goto/while) 支持尚不成熟（Sprint 21 路线图中）
- 复杂状态管理需要 C++ 扩展

### 4.4 Form::Cpp — C++ 原生实现

**本质**：直接用 C++ 实现 Agent 逻辑，使用 DEFINE_AGENT 宏和 PDK API。

**适用场景**：
- 性能关键路径（微秒级延迟）
- 复杂状态机（多线程、持久化、加密）
- 安全敏感（凭据管理、密钥操作）
- 与底层系统交互（文件系统、网络、进程）

**示例：Provider Agent**

```
pdk/provider_agent/
├── CMakeLists.txt
├── include/provider_agent.h     # ProviderInfo 结构体
└── src/
    ├── pdk_entry.cpp            # DECLARE_TOOL + 注册
    ├── provider_resolve.cpp     # resolve 逻辑
    └── credential_store.cpp     # 凭据管理 (C++ 原生)
```

```cpp
// pdk_entry.cpp
#include <hydraforge/pdk.h>

// 使用 PDK DEFINE_AGENT 宏 (ADR-0021 §3.2)
DEFINE_AGENT(ProviderManager, AgentLoopType::React)

// 工具注册 (不使用 DECLARE_TOOL — ADR-0051 §Decision 3)
extern "C" void pdk_register_tools(hydraforge::IToolRegistry& registry) {
    registry.register_tool_function(
        "provider/resolve",
        ToolMetadata{
            .category = ToolCategory::ReadOnly,
            .approval = ApprovalPolicy::auto_approve,
            .allowed_layers = {LayerProfile::Workflow}
        },
        [](const auto& args) -> nlohmann::json {
            // C++ 原生: 安全读取 env var, 解析凭据
            auto provider_id = args.at("provider_id");
            auto model_id = args.at("model_id");
            auto cfg = credential_store::resolve(provider_id, model_id);
            return cfg.to_json();
        }
    );
}

extern "C" void pdk_register_agent(hydraforge::AgentDescriptor& desc) {
    desc.id = "infra.provider";
    desc.form = AgentForm::Cpp;
    desc.entry_tool = "provider/resolve";
    desc.provided_tools = {"provider/resolve", "provider/register", "provider/list"};
}
```

**优势**：
- 完全控制性能和资源
- 可以做系统级操作（env var / file / network）
- 与 PDK API (DECLARE_TOOL, DEFINE_AGENT, SafeExec) 深度集成
- 编译时类型安全

**限制**：
- 修改需重新编译
- 开发门槛较高
- 不具备 Agent 行为的可解释性

### 4.5 Form::Hybrid — 混合形态

**实际场景**：复杂 Agent 经常是 DSL 图 + C++ 扩展节点的组合。

```
pdk/session_agent/
├── CMakeLists.txt
├── src/
│   ├── pdk_entry.cpp         # C++ 工具注册
│   └── session_store.cpp     # C++ 持久化存储
├── agents/
│   └── compact.agent.md      # DSL 图: LLM 压缩历史
├── skills/
│   └── search/SKILL.md       # SKILL: 历史搜索
└── config/default.json
```

这个 Agent 同时暴露：
- `session/compact` → DSL 图执行（LLM 驱动压缩）
- `session/persist` → C++ 工具（JSONL 文件写入）
- `session/search` → SKILL.md 解释（LLM 搜索）

OS 层面看到的只是三个工具，不关心背后的实现差异。

### 4.6 形态对比矩阵

| 维度 | Skill | DSL | C++ | Hybrid |
|------|:-----:|:---:|:---:|:------:|
| **热更新** | ✅ 编辑 .md | ✅ 编辑 .agent.md | ❌ 重新编译 | ⚠️ 部分 |
| **LLM 可修改** | ✅ | ✅ | ❌ | ⚠️ 部分 |
| **性能** | 低 | 中 | 高 | 混合 |
| **类型安全** | 弱 | 中 | 强 | 混合 |
| **可审计** | 弱 | ✅ DAG trace | 需手加 | 混合 |
| **预算控制** | ❌ | ✅ | 需手动 | 混合 |
| **开发门槛** | 低 | 中 | 高 | 高 |
| **适合场景** | 策略多变 | 结构化流程 | 性能/安全 | 复杂 Agent |

---

## 五、Agent 契约

### 5.1 Agent 对 OS 的需求（OS 必须提供）

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
| 插件加载 | `PluginLoader::load_so()` | ADR-0022 |
| Session 作用域 | `UserSession/TaskSession/SubtaskSession` | ADR-0033 |

### 5.2 Agent 对 OS 的承诺（Agent 必须遵守）

| 承诺 | 验证方式 |
|------|---------|
| 导出 `pdk_plugin_info` | PluginLoader dlsym 检查 |
| 导出 `pdk_register_tools` | PluginLoader dlsym 检查 |
| ABI 版本兼容 | `pdk_plugin_info.abi_version` >= OS 要求 |
| 工具命名遵循 slash-only | ADR-0043 (`loop/run`, `provider/resolve`) |
| 返回值遵循 ToolResult 信封 | ADR-0023 (`{ok, data, meta}`) |
| 异常不穿透 Plugin 边界 | ADR-0021 SafeExec 封装 |
| 不直接修改 OS 内部状态 | Plugin 只通过 IToolRegistry/IInteractionBus |
| 线程安全声明 | metadata 中标注 single-threaded / thread-safe |

### 5.3 Agent 对 Agent 的协议

Agent 之间**不直接通信**。所有交互通过 OS 基础设施：

```
Agent A ──call_tool──→ IToolRegistry ──dispatch──→ Agent B (注册的工具)
Agent A ──emit──→ IInteractionBus ──subscribe──→ Agent B (监听者)
Agent A ──run──→ DSLEngine ──execute .agent.md──→ 内含 Agent C 的工具调用
```

**禁止的交互模式**：
- ❌ Agent A `#include "agent_b_internal.h"`
- ❌ Agent A 直接调用 Agent B 的 C++ 函数
- ❌ Agent A 修改 Agent B 的配置文件
- ❌ Agent A 通过 `dlopen` 加载 Agent B 的内部符号

**允许的交互模式**：
- ✅ 通过 `IToolRegistry::call_tool("agent_b/tool_name", args)` 调用
- ✅ 通过 `IInteractionBus::emit("agent_b.topic", event)` 发布事件
- ✅ 通过 `DSLEngine::run(.agent.md)` 执行 DSL 图，图中引用了 Agent B 的工具
- ✅ 通过共享 `LayeredContext` 传递数据（只读快照）

---

## 六、Agent 组合模型

### 6.1 三种组合模式

#### 模式 A：工具调用链（最简单）

```
ChatAgent
  └─ call_tool("loop/run", {prompt, tools: [...]})
       └─ LoopAgent 内部:
            ├─ call_tool("fs/read", ...)   ← FS Agent 的工具
            ├─ call_tool("shell/exec", ...) ← Shell Agent 的工具
            └─ call_tool("provider/resolve", ...) ← Provider Agent 的工具
```

**适用**：简单的请求-响应链条，如 Chat → Loop → Tool。

#### 模式 B：DSL 子图嵌入（声明式）

```
main.agent.md:
  nodes:
    plan:
      type: tool
      name: "planning/run"          ← PlanningAgent 的入口工具
      output: plan
    
    execute:
      type: tool
      name: "execution/run"          ← ExecutionAgent 的入口工具
      args:
        plan: "{{plan}}"
      output: result
    
    review:
      type: tool
      name: "review/run"             ← ReviewAgent 的入口工具
      args:
        result: "{{result}}"
      output: review
    
    condition:
      type: condition
      condition: "{{review.approved}}"
      true: end
      false: plan                    # 回到 plan 节点
```

**适用**：多 Agent 的固定流程编排，如 Plan→Execute→Review 循环。

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

**适用**：松耦合、异步、多 Agent 并行响应。

### 6.2 组合模式的层级关系

```
模式 C (事件驱动)           ← 最灵活，最松耦合
    └─ 包含 模式 B (DSL 子图)   ← 结构化流程
         └─ 包含 模式 A (工具调用链)  ← 最简单的请求-响应
```

一个应用通常**同时使用三种模式**：
- 顶层编排用**事件驱动**（ChatAgent 发布用户输入，等待 Agent 们响应）
- 中间层用**DSL 子图**（LoopAgent 的 think-decide-act-observe 循环）
- 叶子层用**工具调用链**（具体工具执行）

---

## 七、Agent 生命周期

### 7.1 Plugin 生命周期

```
load → init → register → run → unregister → unload
```

| 阶段 | OS 操作 | Plugin 操作 |
|------|---------|------------|
| **load** | `PluginLoader::load_so(name)` | — |
| **init** | 调用 `pdk_plugin_init(hooks)` (ADR-0041) | 初始化内部状态 |
| **register** | 调用 `pdk_register_tools(registry)` | 注册工具到 IToolRegistry |
| **register** | 调用 `pdk_register_agent(desc)` (可选) | 注册 Agent 描述 |
| **run** | Agent 的工具被其他 Agent/用户调用 | 处理请求，返回结果 |
| **unregister** | OS 调用 `pdk_plugin_fini()` (ADR-0041) | 清理内部状态 |
| **unload** | `PluginLoader::unload_so()` | — |

### 7.2 Agent 运行时状态机

```
           ┌──────────┐
     ┌────→│  idle    │←─────────────────────┐
     │     └────┬─────┘                      │
     │          │ chat() / call_tool()        │
     │          ▼                            │
     │     ┌──────────┐                      │
     │     │ running  │──────success─────────┘
     │     └────┬─────┘
     │          │ failure (retryable)
     │          ▼
     │     ┌──────────┐
     │     │ retrying │───max retries───→ error
     │     └────┬─────┘
     │          │ retry success
     └──────────┘
```

对应 ADR-0033：
- `idle` = 无活跃 TaskSession
- `running` = 有活跃 TaskSession
- `retrying` = TaskSession.failure_count > 0 但 < 3（KeepSession 策略）
- `error` = failure_count >= 3（NewSession 策略，fork 新 SubtaskSession）

---

## 八、典型应用组装

### 8.1 Chat 应用 (CLI)

```yaml
# application.yaml — Chat 应用编排配置
agents:
  - id: chat.orchestrator
    plugin: libChatAgent.so
    config:
      system_prompt: "You are a helpful coding assistant."
      max_turns: 100
  
  - id: chat.loop
    plugin: libLoopAgent.so
    config:
      default_type: react
      max_steps: 50
  
  - id: infra.provider
    plugin: libProviderAgent.so
    config:
      providers_file: providers.json
  
  - id: infra.session
    plugin: libSessionAgent.so
    config:
      persist_dir: ~/.hydraforge/sessions/
  
  - id: tool.fs
    plugin: libFSTools.so
  
  - id: tool.shell
    plugin: libShellTools.so
  
  - id: infra.budget
    plugin: libBudgetAgent.so
    config:
      daily_limit: 10.0  # USD
  
orchestration:
  model: event_driven          # 顶层编排模式
  entry_agent: chat.orchestrator
  event_flow:
    user_input → chat.orchestrator
    chat.orchestrator → chat.loop (via loop/run)
    chat.loop → tool.fs, tool.shell (via call_tool)
    chat.loop → infra.provider (via provider/resolve)
```

### 8.2 IDE 插件应用

```yaml
# application.yaml — VS Code Agent 扩展
agents:
  - id: ide.completion
    plugin: libCompletionAgent.so    # C++ 实现 (性能)
    form: cpp
    
  - id: ide.refactor
    plugin: libRefactorAgent.so      # DSL 实现 (可审计)
    form: dsl
    agents/refactor.agent.md: inline
    
  - id: ide.explain
    plugin: libExplainAgent.so       # SKILL 实现 (灵活)
    form: skill
    
  - id: ide.test_gen
    plugin: libTestGenAgent.so       # Hybrid
    form: hybrid

orchestration:
  model: tool_chain               # IDE 直接调用工具
  # IDE 根据当前动作选择调用哪个 Agent
```

### 8.3 自动化工作流应用

```yaml
# application.yaml — CI/CD Agent Pipeline
agents:
  - id: ci.analyze
    plugin: libAnalyzeAgent.so
  - id: ci.build
    plugin: libBuildAgent.so
  - id: ci.test
    plugin: libTestAgent.so
  - id: ci.deploy
    plugin: libDeployAgent.so

orchestration:
  model: dsl_subgraph
  workflow: |
    # pipeline.agent.md
    analyze → (success) → build
    build → (success) → test
    test → (success) → deploy
    (any) → (failure) → notify_error
```

---

## 九、现有设施的 Plugin 化路径

### 9.1 当前 PDK 插件盘点

| 插件 | 形态 | 提供的工具 | 状态 |
|------|:----:|-----------|------|
| `pdk/llama_engine/` | C++ | inference/engine/*, inference/model/* (12 工具) | ✅ ADR-0035/0040/0044 |
| `pdk/model_router/` | C++ | model_router/cost, /quality, /latency, /registry | ✅ ADR-0034 |
| `pdk/g1_coding_assistant/` | C++ | coding_assistant/review (Spike G1) | ✅ ADR-0051 |
| `pdk/g3_knowledge_base/` | C++ | knowledge_base/query (Spike G3) | ✅ ADR-0051 |

### 9.2 建议新增的 Agent Plugin

| Agent | 推荐形态 | 理由 |
|-------|:--------:|------|
| **Loop Agent** (React/PlanExecute/ForkJoin) | DSL (首选) + C++ (备选) | ADR-0021 已有 3 种 LoopType C++ 实现; .agent.md 版本可实现热更新 |
| **Provider Agent** | C++ | 凭据管理需安全内存操作; 扩展 model_router |
| **Session Agent** | Hybrid | LLM 压缩用 DSL, 持久化用 C++ |
| **Budget Agent** | C++ | 预算策略 + 跨 session 聚合 |
| **Code Agent** | Skill | 代码审查/生成策略经常变化 |
| **Browser Agent** | C++ | 需要 Playwright/Puppeteer 系统集成 |
| **Search Agent** | Skill | 搜索策略灵活, 依赖外部 API |

### 9.3 现有 OS 组件的边界（不可 Plugin 化）

| 组件 | 保留在 OS 的理由 |
|------|----------------|
| `DSLEngine` | 纯计算引擎, 所有 Plugin 依赖它 |
| `IToolRegistry` | Agent 发现机制的基础, 无法自举 |
| `IInteractionBus` | 消息总线, Plugin 化产生循环依赖 |
| `IBudgetController` (原子) | 安全闸门, 不可绕过 |
| `IExecutionPolicy` | 安全决策, 失败必须 fail-stop |
| `ApprovalHandler` | 安全边界, 不可降级 |
| `TopoScheduler` | DAG 调度器, 纯计算 |
| `PluginLoader` | 加载其他 Plugin 的基础, 无法自举 |

---

## 十、SOTA 定位与设计原则

### 10.1 第一性原则

| 原则 | 推导 | 来源 |
|------|------|------|
| **关注点分离** | OS 只管基础设施, Agent 只管领域逻辑 | 微内核架构 (Mach/MINIX) |
| **契约设计** | 接口 5 年稳定, 实现自由迭代 | OSGi bundle contract |
| **多形态透明** | 调用者不关心 Agent 是 SKILL/DSL/C++ | COM IUnknown 抽象 |
| **可组合性** | Agent 间通过标准协议组合, 不直接依赖 | Unix pipe 哲学 |
| **安全纵深** | 每层验证, 不信任 Plugin 声明 | Layer Profile (ADR-0004/0031) |
| **可观测性** | 每个 Agent 调用有 trace, 可审计 | ADR-0031 audit events |

### 10.2 与 SOTA 框架的定位

| 框架 | Agent 形态 | 内部实现 | 通信 | 可扩展 | HydraForge 优势 |
|------|-----------|---------|------|--------|----------------|
| **LangGraph** | Graph 节点 | Python lambda | 状态传递 | Python 包 | DSL 图可热更新; C++ 性能 |
| **CrewAI** | Crew Agent | Python class | 共享状态 | 自定义 Agent | 多形态; 编译时安全 |
| **AutoGen** | Agent | Python class | 消息传递 | 自定义 Agent | Plugin ABI 稳定; 可热加载 |
| **Google ADK** | Agent | Python | 事件通信 | 插件系统 | C++ 核心性能; DSL 可审计 |
| **OpenAI Agents SDK** | Agent | Python | Handoff | 自定义 Agent | 本地推理; 预算控制 |
| **MCP Server** | Server | 任意语言 | JSON-RPC | 任意 | Agent 内嵌 OS; 无网络开销 |

**HydraForge 的独特价值**：
1. **Agent 三形态透明** — 唯一支持 SKILL.md / DSL / C++ 可互换的框架
2. **编译时安全 + 运行时热更新** — C++ 插件提供性能, SKILL.md 提供灵活性
3. **DSL 工作流可审计** — .agent.md 是声明式的、可视化的、LLM 可理解的
4. **预算控制内建** — 每个 Agent 执行消耗预算, 防止无限递归
5. **本地优先** — llama.cpp 后端, 无需云端 API

---

## 十一、关键决策记录

| 编号 | 决策 | 状态 | 关联 |
|------|------|------|------|
| **A1** | Agent = Plugin (.so) | ✅ 确定 | ADR-0021/0022 |
| **A2** | 三种内部形态 (Skill/DSL/C++) | ✅ 确定 | 新决策 |
| **A3** | `pdk_register_agent` 新增 | ✅ 确定 | 扩展 ADR-0022 |
| **A4** | OS 不解析 Agent 内部形态 | ✅ 确定 | P2 原则 |
| **A5** | Agent 间不直接通信 | ✅ 确定 | ADR-0019/0031 |
| **A6** | SKILL.md 由 OS SkillInterpreter 执行 | 🟡 待实现 | 新增 |
| **A7** | .agent.md 由 DSLEngine 子图执行 | 🟡 部分实现 | — |
| **A8** | Agent 配置通过 Plugin config/ 目录 | ✅ 确定 | — |

---

## 十二、相关文档

| 文档 | 路径 |
|------|------|
| AgenticOS 八层架构 (v2.2) | `docs/specs/architecture.md` |
| 多领域智能体架构 | `docs/guides/multi-domain-agent-architecture.md` |
| PDK 设计 | `docs/adr/adr-0021-pdk-design.md` |
| Plugin 加载 | `docs/adr/adr-0022-plugin-loading.md` |
| PDK 工具命名 | `docs/adr/adr-0043-pdk-tool-naming-convention.md` |
| PDK Composition Spike | `docs/adr/adr-0051-phase6-pdk-composition-spike.md` |
| Session 层次 | `docs/adr/adr-0033-session-hierarchy.md` |
| ToolCoordinator | `docs/adr/adr-0031-execution-policy.md` |
| PDK Chat Demo 设计 | `docs/superpowers/specs/2026-07-16-pdk-chat-demo-design.md` |
