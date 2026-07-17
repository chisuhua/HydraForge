# Agent-as-Plugin 架构综合研究摘要

**日期**: 2026-07-16
**状态**: 🟡 Proposed (架构讨论中)
**作者**: Architecture Working Group
**来源**: SOTA 调研 + 接口模式调研 + HydraForge 项目现状分析

---

## 一、研究目标

梳理 Agent-as-Plugin 架构的核心命题、形态进化、接口契约、组合模式，并明确 HydraForge 项目需要新增的架构组件与决策。

---

## 二、核心命题

> **万物皆 Agent，Agent 皆 Plugin。**

HydraForge = AgenticOS（智能体操作系统）。

- AgenticOS 只提供基础设施，不包含业务逻辑。
- 每个 Agent 是 PDK Plugin（.so / .dll / .wasm）。
- Agent 内部实现可以是四种形态之一：
  - **SKILL.md**：非结构化描述，解释执行，隔离环境
  - **AgenticDSL (.agent.md)**：结构化图，编译执行，可审计
  - **C++**：原生代码，高性能，复杂状态机
  - **Wasm**：固化产物，跨平台，强隔离
- 四种形态构成**进化路径**：Skill → DSL → C++ → Wasm。
- 应用 = Agent 组合，不存在"应用代码"。

---

## 三、架构分层（四层抽象）

| 层 | 名称 | 职责 | 变化频率 | 谁拥有 |
|---|------|------|---------|--------|
| L3 | Application Mesh | Agent 组合 + 配置 + UI | 高 | 应用开发者 |
| L2 | Agent Plugin Layer | Agent 领域逻辑 | 中 | Agent 开发者 |
| L1 | AgenticOS Services | 跨 Agent 共享服务 | 低 | HydraForge 团队 |
| L0 | Runtime Core | 纯计算引擎 | 极低 | HydraForge 团队 |

---

## 四、Agent 形态进化

```
SKILL.md ──(固化)──→ .agent.md ──(性能化)──→ C++ ──(可移植化)──→ Wasm
   │                       │                     │                   │
 阶段 1                  阶段 2               阶段 3             阶段 4
 Prototype            Production           Native             Portable
 非结构化              结构化               高性能             跨平台
 隔离执行              可审计               可调试             强隔离
 快速迭代              可热更新             系统级操作         边缘部署
```

### 关键术语

- **Solidification（固化）**：SKILL.md → .agent.md，行为等价但结构化和可审计。
- **Performance Engineering（性能化）**：.agent.md → C++，保持 DSL 编排但替换热点节点。
- **Portability（可移植化）**：DSL/C++ → Wasm，用于跨平台和不可信环境。

### 形态选择决策树

```
需要 LLM 推理？
├─ NO → 能用 DAG 表达？
│       ├─ YES → 能用 DSL 原语表达？
│       │        ├─ YES → DSL → 可选 Wasm
│       │        └─ NO → C++ → 可选 Wasm
│       └─ NO → C++ → 可选 Wasm
└─ YES → 推理策略经常变化？
         ├─ YES → Skill（隔离）→ 可固化到 DSL → 可选 Wasm
         └─ NO → DSL 或 C++
```

---

## 五、核心原则

| # | 原则 | 说明 |
|---|------|------|
| P1 | Agent 即 Plugin | 每个 Agent 独立编译为 .so / .wasm |
| P2 | 内部形态可进化 | Skill → DSL → C++ → Wasm 是优化路径 |
| P3 | SKILL 必须隔离 | 解释执行不可信技能，必须沙箱化 |
| P4 | 固化产物可编译 | 固化后的 .agent.md 可编译为 Wasm |
| P5 | 契约唯一 | 所有形态共享 `pdk_plugin_info` + `pdk_register_tools` + `pdk_register_agent` |
| P6 | 自包含 | Plugin 包含所有资源（.md + 配置 + C++ + Wasm） |
| P7 | 可组合 | Agent 间通过 ToolRegistry + IInteractionBus 交互 |
| P8 | Manifest-first | 每个 Plugin 自带机器可读 `pdk_manifest.json` |
| P9 | Capability-based discovery | 按 input/output schema 和能力发现 Agent |
| P10 | Lifecycle-aware | 支持 install/init/activate/deactivate/uninstall |

---

## 六、新增 OS 服务

| 服务 | 职责 | 优先级 | 状态 |
|------|------|--------|------|
| `SkillInterpreter` | 隔离解释 SKILL.md | P1 | 待实现 |
| `AgenticDSLCompiler` | .agent.md → ParsedGraph | P0 | 部分实现 |
| `WasmRuntime` | 加载执行 .wasm Agent | P2 | 待实现 |
| `AgentEvolutionEngine` | 管理 Skill → DSL → C++ → Wasm 转换 | P3 | 待实现 |
| `CapabilityRegistry` | 按能力发现 Agent | P1 | 待实现 |
| `ManifestRegistry` | 按 manifest 索引 Plugin | P1 | 待实现 |
| `AgentOrchestrator` | 声明式应用编排 + Reconciler | P2 | 待实现 |

---

## 七、接口契约建议

### Plugin 导出符号

```cpp
// 必需
extern "C" const hydraforge::PluginInfo pdk_plugin_info;
extern "C" void pdk_register_tools(hydraforge::IToolRegistry& registry);

// 可选（如果定义了 Agent）
extern "C" void pdk_register_agent(hydraforge::AgentDescriptor& desc);

// 建议新增
extern "C" const char* pdk_manifest();
```

### AgentDescriptor

```cpp
enum class AgentForm { Skill, DSL, Cpp, Wasm, Hybrid };

struct AgentDescriptor {
    std::string id;
    std::string display_name;
    std::vector<AgentForm> forms;      // 支持哪些形态
    std::string entry_tool;
    std::vector<std::string> provided_tools;
    std::vector<std::string> requires_agents;
    nlohmann::json default_config;
    bool requires_isolation;           // SKILL 必须为 true
    std::vector<std::string> interface_versions;
};
```

### pdk_manifest.json 关键字段

- Identity: `id`, `name`, `version`
- ABI: `abi_version`, `min_host_version`, `max_host_version`
- Interface: `interface_versions`
- Form: `implementation_forms`
- Capabilities: `capabilities`, `provided_tools`, `entry_tool`
- Schema: `input_schema`, `output_schema` (JSON Schema 2020-12)
- Trust: `publisher`, `signature`, `trust_level`
- Resources: `timeout_ms`, `max_concurrent`
- Activation: `activation_events`
- Safety: `requires_isolation`, `side_effects`

---

## 八、组合模式

| 模式 | 名称 | 特点 | 适用 |
|------|------|------|------|
| A | 工具调用链 | 最简单，请求-响应 | 叶子层工具调用 |
| B | DSL 子图嵌入 | 声明式，结构化 | 多 Agent 固定流程 |
| C | 事件驱动协作 | 最灵活，松耦合 | 顶层编排 |
| D | Wasm 模块组合 | 强隔离，跨平台 | 不可信/边缘 Agent |

---

## 九、与现有架构的关系

| 现有文档 | 关系 |
|---------|------|
| `docs/specs/architecture.md` v2.2 | 八层规范是执行深度分层，本架构是所有权/变化频率分层 |
| `docs/guides/multi-domain-agent-architecture.md` | Cognitive/Domain Worker 提升为 Agent Plugin |
| `docs/adr/adr-0021-pdk-design.md` | PDK 从工具脚手架扩展为 Agent 脚手架 |
| `docs/adr/adr-0051-phase6-pdk-composition-spike.md` | Spike 已验证 PDK 插件可组合 |
| `docs/architecture/agent-as-plugin-architecture-v1.1.md` | 本摘要的详细版 |
| `docs/architecture/agent-evolution-pipeline.md` | 进化管线的详细设计 |
| `docs/architecture/application-layer-sota-positioning-v2.md` | SOTA 定位分析 |

---

## 十、HydraForge 独特定位

> **HydraForge is the C++20-native, capability-controlled agent operating system where every application agent is a first-class PDK plugin (.so/.dll/.wasm), internally evolvable from SKILL.md to AgenticDSL to C++ to WebAssembly, connected through a contract-first policy layer, operating on a 5-layer structured context, and orchestrated via neuro-symbolic DSL with hard trust boundaries.**

**核心优势**：
1. C++20 native graph engine
2. 14-header contract/policy 层
3. 三层会话层级（ADR-0033）
4. 5-层结构化上下文（ADR-0008）
5. ToolMetadata V2 多维策略
6. 真 .so PDK Plugin
7. 94 PDK 测试（React/PlanExecute/ForkJoin）
8. 四形态进化路径（Skill→DSL→C++→Wasm）

**主要差距**：
- P0: 无 manifest 体系
- P0: 无 capability-based discovery
- P0: 无 Skill 隔离运行时
- P0: 无 Wasm 运行时
- P0: 无跨进程/跨网络协议
- P1: 无 hot-reload / lazy-load
- P1: 无 semver 版本约束
- P1: 无 tool schema 强制校验
- P1: 无 distributed bus / OpenTelemetry
- P2: 无 conformance test suite / 多语言 PDK

---

## 十一、相关文档

- `docs/research/agent-plugin-architecture-sota-2026.md` — SOTA 调研详细摘要
- `docs/research/agent-plugin-interface-patterns.md` — 接口与组合模式调研摘要
- `docs/research/adr-candidates-agent-as-plugin.md` — ADR 候选议题列表
- `docs/architecture/agent-as-plugin-architecture-v1.1.md` — 架构主文档 v1.1
- `docs/architecture/agent-evolution-pipeline.md` — 进化管线详细设计
- `docs/architecture/application-layer-sota-positioning-v2.md` — 应用层 SOTA 定位
