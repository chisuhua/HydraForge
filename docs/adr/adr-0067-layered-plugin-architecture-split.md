# ADR-0067: L2/L3/L4 分层插件架构拆分

## 状态

✅ Approved (追溯性正式化, 2026-07-23) — 决策源自 [`docs/specs/architecture.md`](../specs/architecture.md) §2.3 (2026-07-22), 核心决策 (A13-A16) 与依赖规则 (R1-R5) 已在代码中落地 (`include/agenticdsl/pdk/` 头文件 + `IToolRegistry` 接口), 本 ADR 为正式化的决策记录, 非新提案。

## 领域

Agent-as-Plugin 架构 / 插件分层 / PDK 结构细化

## 关联

- [ADR-0019 — IInteractionBus MVP](./adr-0019-iinteraction-bus-mvp.md) — L1 事件基础设施
- [ADR-0020 — Thread Model Isolation](./adr-0020-thread-model-isolation.md) — L1 Worker 隔离模型
- [ADR-0021 — PDK Design](./adr-0021-pdk-design.md) — PDK 定位与原则 (P1-P6, 本 ADR 细化但不改变)
- [ADR-0022 — Plugin Loading](./adr-0022-plugin-loading.md) — L1 PluginLoader
- [ADR-0031 — Execution Policy](./adr-0031-execution-policy.md) — L3 IExecutionPolicy 契约
- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md) — L4 manifest
- [ADR-0053 — AgentDescriptor](./adr-0053-agent-descriptor-interface.md) — L4 Agent 元数据
- [`docs/specs/architecture.md`](../specs/architecture.md) — 架构主文档 (决策源头)
- [`docs/architecture/agent-evolution-pipeline.md`](../architecture/agent-evolution-pipeline.md) — 四阶段管线 (L4→L2→L1 映射)

## 背景

### 问题

[架构文档 v1.1](../specs/architecture.md) (前置版本) 的 L2 "Agent Plugin Layer" 将两类完全不同职责的组件混装在同一个抽象层：

| 类型 | v1.1 归类 | 职责 | 实例 |
|------|----------|------|------|
| **原子工具** | L2 Plugin | 提供单一能力, 不调用其他工具 | `shell/exec`, `fs/read`, `provider/resolve` |
| **编排 Agent** | L2 Plugin | 编排多个工具, 暴露外部 API | `loop/run`, `coding_assistant/review`, `temporal/start_workflow` |

这导致了三个具体问题:

1. **分类模糊**: `pdk_chat_demo/DESIGN.md` 中 "6 个 Agent Plugin" 的表述不清晰——有些是工具提供者, 有些是编排者, 开发者和文档都无法区分
2. **抽象归属不清**: Temporal Agent 的 `ITemporalClient` 抽象应该放在哪一层？如果放在 L2, 它又是一个"编排者调用的接口"而非"原子工具"
3. **调用链无法用层级解释**: ADR-0051 Spike 中的 G1→G3 调用链 (`call_tool("knowledge_base/query")`) —— G1 (L4 编排 Agent) 调用 G3 (L2 原子工具提供者), 但 v1.1 将它们归类为同一层

### 目标

将 L2 拆分为职责清晰的三个层级:
- **L4**: 编排 + 暴露外部 API
- **L3**: 抽象接口 + 注册机制
- **L2**: 原子工具 (不编排)

## 决策

### 决策 1 — L2/L3/L4 三层拆分 (对应 A13)

**选择**: 将原 v1.1 的单一 L2 "Agent Plugin Layer" 拆分为:

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 4: Agent 应用服务层 (Application Services)             │
│  编排 L2 工具 + 组合 L3 接口 → 暴露为外部可消费的 Agent 服务  │
│  实例: Loop Agent, G1 Coding Assistant, Temporal Agent      │
│  形态: .agent.md (DSL 编排) / C++ (复杂状态机)                │
├─────────────────────────────────────────────────────────────┤
│  Layer 3: PDK 接口契约层 (PDK Contract Layer)                 │
│  L4 与 L2/L1 之间的唯一桥梁——L4 只能通过 L3 访问下层          │
│  包含: 抽象接口 (`IToolRegistry`, `ITemporalClient`)         │
│       + 注册宏 (`DECLARE_TOOL`, `DEFINE_AGENT`)               │
│       + 元数据规范 (`PluginInfo`, `pdk_manifest.json`)        │
│  位置: `include/agenticdsl/{contract,pdk,policy,plugin}/`     │
├─────────────────────────────────────────────────────────────┤
│  Layer 2: Plugin 工具层 (Plugin Tools)                        │
│  提供原子能力——不编排, 被 L4 通过 L3 调用                     │
│  实例: shell_tools, fs_tools, provider_agent, llama_engine   │
│  形态: C++ (原生性能) / Wasm (安全沙箱)                       │
└─────────────────────────────────────────────────────────────┘
```

**理由**:
- **编排是应用逻辑, 不是插件能力**。能"编排多个工具"和"被编排"的组件应分层
- **PDK 概念需要细化**: v1.1 中 PDK 被描述为一个统一的"工具脚手架", 但其中的 `IToolRegistry` (契约) 和 `shell_tools` (实现) 是不同语义层次的概念
- **Temporal Agent PoC 提供了 concrete 证据**: Temporal Agent 需要同时实现 `ITemporalClient` 契约 (L3) 和编排 `shell_tools` (L4 通过 L3 调用 L2), v1.1 的单一 L2 无法容纳这种多层交互

**后果**:
- 现有 PDK 插件需按"是否编排其他工具"重新判定层归属
- 新增插件时必须遵循 R1-R5 依赖规则

### 决策 2 — L3 PDK 接口契约层独立 (对应 A14)

**选择**: 将原来隐含在 PDK 概念中的"接口契约"提取为独立层 L3。

L3 包含两类实体:
1. **纯虚接口**: `IToolRegistry` (9 纯虚函数), `ITemporalClient` (5 纯虚函数), `IModelRouter` (2 纯虚函数), `IExecutionPolicy` (5 纯虚函数), `IApprovalHandler` (1 纯虚函数)
2. **注册机制**: `DECLARE_TOOL` 宏, `DEFINE_AGENT` 宏, `PluginInfo` POD, `pdk_manifest.json`

**理由**:
- L3 是 `include/agenticdsl/` 下所有头文件的语义锚点——它们共同组成 "Agent 契约 API"
- 从 ADR-0019 §1.4 的 PIMPL-lite 解耦工作 (`engine.h` 的 `modules/` include 从 7→1) 可以看出, 契约与实现分离是项目已验证的模式

**与 ADR-0021 的关系**:
- ADR-0021 描述 PDK 为统一的"开发者工具包"(P1-P6 原则)。本 ADR 将 PDK 概念细化为 L3 (契约) + L2 (工具实现), 但不改变 ADR-0021 的 P1-P6 原则
- `DECLARE_TOOL`/`DEFINE_AGENT` 宏属于 L3 (它们定义的是"Agent 向 OS 注册时遵循的契约格式")
- `shell_tools`/`fs_tools`/`provider_agent` 等具体工具实现属于 L2 (它们是"遵循 L3 契约的具体实现")
- `pdk/loop_agent/` 插件 (.so + .agent.md)、`pdk/g1_coding_assistant/`、`pdk/temporal_agent/` 等编排型 Agent 插件属于 L4 (它们通过 L3 契约编排 L2 工具)。注: `include/agenticdsl/pdk/agent_loops/` 下的 header-only 类 (PlanExecuteLoop 等) 属于 L3, 详见 §决策 后果表格

### 决策 3 — L4 只通过 L3 访问 L2/L1 (对应 A16 + 依赖规则 R1-R5)

**硬性约束**:

| 规则 | 含义 | 反例 (禁止) |
|------|------|-----------|
| **R1**: L4 只通过 L3 访问 L2/L1 | L4 不能直接 `#include` L2 插件的内部头文件 | ❌ `#include "shell_tools/src/internal.h"` |
| **R2**: L3 不依赖 L4 | 接口契约层不感知具体 Agent 实现 | ❌ `IToolRegistry` 引入 Temporal class |
| **R3**: L2 不调其他 L2 | 原子工具自包含, 不编排其他工具 | ❌ `fs/read` 内部调 `shell/exec` |
| **R4**: L3 接口可在 L2 或 L4 中**实现** | 契约层只管定义, 实现者可以在任意上层 | ✅ `ITemporalClient`: L4 Temporal Agent 实现 |
| **R5**: L3→L1 访问是单向的 | L3 引用 L1 类型 (如 `ToolResult`), L1 不引用 L3 | ✅ `IToolRegistry.h` `#include "core/types/tool_result.h"` |

**关于 R4 与 ADR-0021 P3 (静态链接) 的协调**:
- R4 规定 L3 接口可由 L2/L4 实现——这意味着 L3 接口的实现者可以位于任意层
- ADR-0021 P3 规定 "PDK 静态链接到插件"——这指的是 PDK 头文件 (L3 宏定义) 在编译时融入插件代码
- **两者不冲突**: PDK 宏在 L4 插件 (如 Temporal Agent) 的编译时展开, 宏展开后的代码调用 `IToolRegistry` (L3 接口), 该接口的实现在 L1 (`src/common/tools/registry.cpp`)。R4 的"在 L4 中实现"指 L4 可以实现 L3 中定义的*新接口* (如 `ITemporalClient`), 而非重新实现 L3 中已有的接口 (如 `IToolRegistry`)
- 实现者通过 `DECLARE_TOOL`/`DEFINE_AGENT` 宏使用 PDK, 仍遵循 P3 的静态链接模式

## 后果

### 对现有代码的约束

| 代码位置 | 归属层 | 不受影响 (已满足 R1-R5) |
|----------|:------:|------------------------|
| `include/agenticdsl/pdk/` (宏 + 模板) | L3 | ✅ 仅引用 L1 契约类型 |
| `include/agenticdsl/contract/` (纯虚接口) | L3 | ✅ 仅引用 L1 类型 |
| `pdk/shell_tools/`, `pdk/fs_tools/` | L2 | ✅ 仅注册工具, 不编排 |
| `pdk/llama_engine/`, `pdk/model_router/` | L2 | ✅ 原子工具 |
| `pdk/loop_agent/` (agents/*.agent.md) | L4 | ✅ 通过 L3 call_tool 访问 L2 |
| `pdk/provider_agent/` | L2 | ✅ 原子工具 |
| `src/modules/cognitive/` (CognitiveWorker) | L4->L1 legacy | ⚠️ 直接调用 L1, 应在未来通过 L3 抽象化 |

### 对新增插件的规则

任何新插件在创建时必须:

1. 判定自身层归属: 编排其他工具 → L4; 仅提供原子操作 → L2; 定义新接口 → L3
2. 遵循对应层的依赖规则 R1-R5
3. 在 `pdk_manifest.json` 的 `layer` 字段 (或等价位置) 声明层归属

### 对 ADR-0021 的影响

- ADR-0021 的 P1-P6 原则 (PDK 独立仓库/可选依赖/静态链接/无领域逻辑/版本解耦/测试替身) **全部不变**
- PDK 概念从"统一工具包"细化为 L3 契约 + L2 工具实现
- 见 [ADR-0021 §更新记录 v1.2 对齐](./adr-0021-pdk-design.md) 的自然语言说明

## 替代方案

### 备选: 保持 v1.1 的单一 L2

**理由**: 简单, 减少分层概念复杂度。

**为何不采纳**: 
- L2 混装导致分类模糊 (工具 vs 编排), 已在实际设计 (Temporal Agent) 中产生摩擦
- PDK 的概念细化 (契约 vs 实现) 是长期趋势——当前已有 `IToolRegistry`/`ITemporalClient`/`IModelRouter` 等接口, 它们天然需要独立层级
- 拆分后 API 边界更清晰, 有利于 PDK 独立化 (ADR-0021 Dual-Repo 策略)

### 备选: 四个层级 (将 L3 拆为"接口层"和"宏层")

**理由**: 纯虚接口 (`IToolRegistry`) 和注册宏 (`DECLARE_TOOL`) 是不同粒度的概念。

**为何不采纳**:
- 它们在代码上共存于同一位置 (`include/agenticdsl/pdk/`)
- 宏是对接口的"脚手架", 拆分会增加层级而不增加清晰度
- 可在 L3 内通过子段落区分, 无需新层级

## 交叉引用

- 层间依赖规则 R1-R5 的表格见 [架构文档 §2.4](../specs/architecture.md#24-层间依赖规则-硬性约束)
- Temporal Agent 的层归属验证见 [架构文档 §2.5](../specs/architecture.md#25-temporal-agent-的层归属验证)
- PDK 目录结构规范见 [架构文档 §3.2](../specs/architecture.md#32-plugin-目录结构规范)
- 决策 A1-A16 全表见 [架构文档 §十一](../specs/architecture.md#十一关键决策记录-v12-更新)
