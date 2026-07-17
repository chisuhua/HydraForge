# Agent-as-Plugin 架构 v1.1

**日期**: 2026-07-16
**状态**: 🟡 Proposed (架构讨论中，含 SOTA 调研与进化路径更新)
**作者**: Architecture Working Group
**关联**: ADR-0019 ~ ADR-0051, `docs/specs/architecture.md` v2.2, `docs/architecture/agent-evolution-pipeline.md`, `docs/architecture/application-layer-sota-positioning.md`

**前置文档**: `docs/architecture/agent-as-plugin-architecture.md` v1.0

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

### 2.1 四层抽象

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 3: Application Mesh (应用网)                           │
│  Agent 组合 + 配置 + CLI/TUI/Web UI                          │
│  没有"应用代码"——只有 Agent 编排声明                          │
├─────────────────────────────────────────────────────────────┤
│  Layer 2: Agent Plugin Layer (Agent 插件层)                   │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  每个 Agent = 一个 PDK Plugin (.so 或 .wasm)            │  │
│  │                                                         │  │
│  │  ┌─ SKILL.md 实现 ──────────────────────────────────┐  │  │
│  │  │ 非结构化描述，解释执行，隔离环境                    │  │  │
│  │  │ 适合探索、快速迭代、利用现有技能生态                │  │  │
│  │  └───────────────────────────────────────────────────┘  │  │
│  │                                                         │  │
│  │  ┌─ AgenticDSL (.agent.md) 实现 ──────────────────────┐  │  │
│  │  │ 结构化图，编译为 DAG，可审计，可预算控制            │  │  │
│  │  │ 适合生产、可验证、可热更新                          │  │  │
│  │  └───────────────────────────────────────────────────┘  │  │
│  │                                                         │  │
│  │  ┌─ C++ / Wasm 实现 ───────────────────────────────────┐  │  │
│  │  │ 原生性能 / 可移植安全沙箱                           │  │  │
│  │  │ 适合性能/安全关键、复杂状态机                       │  │  │
│  │  └───────────────────────────────────────────────────┘  │  │
│  │                                                         │  │
│  │  统一契约: pdk_plugin_info + pdk_register_tools          │  │
│  │             + pdk_register_agent + pdk_manifest          │  │
│  └────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  Layer 1: AgenticOS Services (OS 服务层)                      │
│  ToolRegistry │ IInteractionBus │ IBudgetController           │
│  ILLMProvider+Factory │ IExecutionPolicy │ ApprovalHandler    │
│  TopoScheduler │ UserSession/TaskSession/SubtaskSession       │
│  ToolCoordinator │ CostTrackingDecorator │ PluginLoader       │
│  SkillInterpreter │ AgenticDSLCompiler │ WasmRuntime        │
│  ManifestRegistry │ CapabilityRegistry                        │
├─────────────────────────────────────────────────────────────┤
│  Layer 0: Runtime Core (运行时核心)                            │
│  DSLEngine │ NodeExecutor │ MarkdownParser │ ContextEngine    │
│  BudgetController(原子) │ LlamaAdapter │ TraceExporter        │
│  ILLMProvider (v2 contract) │ IToolRegistry (9 虚函数)       │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 与八层规范的映射

| 本架构层 | 对应 specs/architecture.md v2.2 |
|---------|-------------------------------|
| L3: App Mesh | Layer 6 (应用) + Layer 5 (交互) + Layer 4.5 (社会) |
| L2: Plugin | Layer 4 (认知) + Layer 3 (推理) + Layer 2 (执行) + Layer 2.5 (标准库) |
| L1: OS Services | Layer 0 runtime services + Layer 1 storage + 新增编译/解释服务 |
| L0: Runtime Core | Layer 0 agentic-dsl-runtime C++ 核心 |

### 2.3 与多领域智能体架构的演进关系

`docs/guides/multi-domain-agent-architecture.md` 定义了：
- **Cognitive Worker**（编排者）
- **Domain Workers**（执行者，通过 ToolRegistry 提供工具）

本架构 v1.1 将其升级为：**Cognitive Worker 和 Domain Worker 都是 Agent Plugin**。区别只在于角色：
- **Cognitive Agent** = 编排型 Agent（如 Chat Agent、Loop Agent）
- **Domain Agent** = 能力型 Agent（如 Provider Agent、FS Agent、Code Agent）

两者都遵循同一 Plugin 契约，都可以通过 SKILL/DSL/C++/Wasm 实现。

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

### 6.3 Agent 对 Agent 的协议

Agent 之间**不直接通信**。所有交互通过 OS 基础设施：

```
Agent A ──call_tool──→ IToolRegistry ──dispatch──→ Agent B (注册的工具)
Agent A ──emit──→ IInteractionBus ──subscribe──→ Agent B (监听者)
Agent A ──run──→ DSLEngine ──execute .agent.md──→ 内含 Agent C 的工具调用
Agent A ──run──→ SkillInterpreter ──execute SKILL.md──→ 受限调用 Agent C
Agent A ──invoke──→ WasmRuntime ──execute .wasm──→ 受限调用 Agent C
```

---

## 七、Agent 组合模型

### 7.1 四种组合模式

#### 模式 A：工具调用链（最简单）

```
ChatAgent
  └─ call_tool("loop/run", {prompt, tools: [...]})
       └─ LoopAgent 内部:
            ├─ call_tool("fs/read", ...)   ← FS Agent 的工具
            ├─ call_tool("shell/exec", ...) ← Shell Agent 的工具
            └─ call_tool("provider/resolve", ...) ← Provider Agent 的工具
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

### 8.1 Chat 应用 (CLI)

```yaml
agents:
  - id: chat.orchestrator
    plugin: libChatAgent.so
    form: cpp
  
  - id: chat.loop
    plugin: libLoopAgent.so
    form: dsl
  
  - id: infra.provider
    plugin: libProviderAgent.so
    form: cpp
  
  - id: infra.session
    plugin: libSessionAgent.so
    form: hybrid
  
  - id: tool.fs
    plugin: libFSTools.so
    form: cpp
  
  - id: tool.code_review
    plugin: libCodeReviewAgent.wasm
    form: wasm
  
  - id: tool.documentation
    plugin: libDocAgent.so
    form: skill
    requires_isolation: true

orchestration:
  model: event_driven
  entry_agent: chat.orchestrator
  event_flow:
    user_input → chat.orchestrator
    chat.orchestrator → chat.loop (via loop/run)
    chat.loop → tool.fs, tool.shell, tool.code_review (via call_tool)
    chat.loop → infra.provider (via provider/resolve)
```

---

## 九、现有设施的 Plugin 化路径

### 9.1 新增 OS 服务（支持进化路径）

| 服务 | 职责 | 状态 | 优先级 |
|------|------|------|--------|
| `SkillInterpreter` | 隔离解释 SKILL.md | 待实现 | P1 |
| `AgenticDSLCompiler` | .agent.md → ParsedGraph | 部分实现（DSLEngine） | P0 |
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

### 9.3 当前 PDK 插件盘点与建议新增

| 现有插件 | 形态 | 工具 | 状态 |
|------|:----:|-----------|------|
| `pdk/llama_engine/` | C++ | inference/* (12 工具) | ✅ ADR-0035/0040/0044 |
| `pdk/model_router/` | C++ | model_router/* | ✅ ADR-0034 |
| `pdk/g1_coding_assistant/` | C++ | coding_assistant/review | ✅ ADR-0051 |
| `pdk/g3_knowledge_base/` | C++ | knowledge_base/query | ✅ ADR-0051 |

| 建议新增 | 推荐形态 | 理由 |
|-------|:--------:|------|
| **Loop Agent** | DSL (首选) + C++ (备选) | ADR-0021 已有 3 种 LoopType；.agent.md 可热更新 |
| **Provider Agent** | C++ | 凭据管理需安全内存；扩展 model_router |
| **Session Agent** | Hybrid | LLM 压缩用 DSL，持久化用 C++ |
| **Budget Agent** | C++ | 预算策略 + 跨 session 聚合 |
| **Code Agent** | Skill | 代码审查/生成策略经常变化 |
| **Browser Agent** | C++ | 需要 Playwright/Puppeteer 集成 |
| **Search Agent** | Skill | 搜索策略灵活，依赖外部 API |

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

## 十一、关键决策记录

| 编号 | 决策 | 状态 | 关联 |
|------|------|------|------|
| **A1** | Agent = Plugin (.so / .wasm) | ✅ 确定 | ADR-0021/0022 |
| **A2** | 四形态进化路径（Skill/DSL/C++/Wasm） | ✅ 确定 | 新决策 |
| **A3** | SKILL.md 必须隔离 | ✅ 确定 | 安全原则 + SOTA |
| **A4** | DSL 可固化，可编译为 Wasm | ✅ 确定 | 新决策 |
| **A5** | `pdk_register_agent` + `pdk_manifest` 新增 | ✅ 确定 | 扩展 ADR-0022 |
| **A6** | OS 不解析 Agent 内部形态 | ✅ 确定 | P2 原则 |
| **A7** | Agent 间不直接通信 | ✅ 确定 | ADR-0019/0031 |
| **A8** | SkillInterpreter / WasmRuntime 在 OS 层 | ✅ 确定 | 新决策 |
| **A9** | Manifest-first 注册 | ✅ 确定 | SOTA 共识 |
| **A10** | Capability-based discovery | ✅ 确定 | FIPA DF + OSGi |
| **A11** | Lifecycle-aware 插件管理 | ✅ 确定 | OSGi + VS Code + MCP |
| **A12** | Schema-first tool contracts | ✅ 确定 | MCP + ATD |

---

## 十二、相关文档

| 文档 | 路径 |
|------|------|
| Agent 进化管线 | `docs/architecture/agent-evolution-pipeline.md` |
| 应用层 SOTA 定位 | `docs/architecture/application-layer-sota-positioning.md` |
| Agent-as-Plugin v1.0 (基线) | `docs/architecture/agent-as-plugin-architecture.md` |
| AgenticOS 八层架构 (v2.2) | `docs/specs/architecture.md` |
| 多领域智能体架构 | `docs/guides/multi-domain-agent-architecture.md` |
| PDK 设计 | `docs/adr/adr-0021-pdk-design.md` |
| Plugin 加载 | `docs/adr/adr-0022-plugin-loading.md` |
| PDK 工具命名 | `docs/adr/adr-0043-pdk-tool-naming-convention.md` |
| PDK Composition Spike | `docs/adr/adr-0051-phase6-pdk-composition-spike.md` |
| Session 层次 | `docs/adr/adr-0033-session-hierarchy.md` |
| ToolCoordinator | `docs/adr/adr-0031-execution-policy.md` |
| PDK Chat Demo 设计 | `docs/superpowers/specs/2026-07-16-pdk-chat-demo-design.md` |
