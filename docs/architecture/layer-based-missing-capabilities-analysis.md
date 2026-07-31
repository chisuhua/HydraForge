# HydraForge/PDK 缺失能力分层分析（v1.2 五层模型）

**生成日期**: 2026-07-31
**最后验证**: 2026-07-31（数据修正版 v1.2.1，验证命令见附录 B）
**作者**: Architecture Working Group
**关联文档**:
- `docs/architecture/adr-implementation-status-gap-analysis.md`（2026-07-30）— ADR 实施状态基线
- `docs/specs/architecture.md`（2026-07-31 晋升, 原 v1.2）— 五层架构规范
- `docs/architecture/agent-evolution-pipeline.md` — Agent 进化路径
- `docs/research/pi-agent-vs-pdk-chat-demo-analyze.md`（2026-07-31 修订版）— pi-agent 借鉴路径

**分析范围**: HydraForge 框架层（L0+L1）+ PDK 契约与生态层（L2+L3）+ Agent 应用服务示例层（pdk_chat_demo / L4）三层视域下的缺失能力。
**数据源**: 截至 2026-07-31 的 **106/106 ctest** 通过版本（2026-07-31 实测 `cd build && ctest`；初稿"93/93"已修正）；59 个 ADR 状态（`tools/doc_metrics.py --adr` 实测：40 Approved / 6 Partial / 11 Proposed / 1 NI / 1 Superseded）；10 个 PDK 插件代码扫描；事件总线 emit 调用点全量 grep（2026-07-31 复核 28 处，X1 节已修正）。

---

## 一、问题陈述

HydraForge 已形成"五层抽象 + AgenticOS 范式 + 跨四态进化"的架构骨架，但当前状态存在显著的**契约-实施差距**：

| 维度 | 数据 |
|------|------|
| ADR 总数 | 59 个（46 主 + 1 plugin + 12 skill 子项） |
| ✅ Approved 且实施（gap-analysis 口径） | 31（52.5%） |
| 🟡 Partial（已批但实施不完整） | 7（11.9%） |
| 🔍 Proposed（提议但未批，部分已自发实施） | 7（11.9%） |
| ❌ Not Implemented | 12（20.3%） |
| Phase 6 架构评审通过但**零代码** | 12（ADR-0052~0065 区间共 14 个，其中 0055/0060 已 ship） |

> **口径说明**（2026-07-31 实测，`tools/doc_metrics.py --adr`）：ADR 文件头 status=✅ Approved 的总数为 **40**（主 33 + plugin 1 + skill 子项 6，含 ⚠ 无代码项）；上表"31"为 gap-analysis 的"Approved **且实施完成**"口径。两套口径均合法，引用时必须标注。

**核心矛盾**: 文档/契约层"应有尽有"（五层 + Phase 6 13 个 ADR），但实施层**关键拦截、动态发现、压缩、跨进程协议等关键能力缺位**，导致：
1. PDK 插件的表达力受限于"工具 + 简单参数"，无法实现 slash 命令、订阅钩子、编排协议
2. pdk_chat_demo 必须 hardcode 在 main.cpp 的逻辑越来越多，违背 Agent-as-Plugin 哲学
3. 与 pi-agent 等成熟生态对比，缺失整套事件钩子、拦截机制、动态 provider 体系

本文档按 **v1.2 五层模型**（L0~L4）逐层识别缺失能力，给出优先级、依赖关系和实施路径建议。

---

## 二、五层模型与能力归属

> 完整架构见 `docs/specs/architecture.md` §2.1，下表为各层职责 + 当前归属模块：

| 层 | 名称 | 职责 | 当前归属模块（代码位置） | 关键 ADR |
|----|------|------|------------------------|---------|
| **L0** | Runtime Core | DSL 解析、调度、执行、原子类型 | `src/core/`（DSLEngine, NodeExecutor, ContextEngine, MarkdownParser）+ `src/common/llm/llm_types.h` | ADR-0003, 0008, 0019 |
| **L1** | OS Services | 跨 Plugin 共享基础设施 | `include/agenticdsl/contract/`（IInteractionBus, IToolRegistry）+ `src/common/tools/registry.cpp` + `src/modules/{budget,scheduler,trace,context}/` + `src/modules/cognitive/` + SkillInterpreter | ADR-0019, 0020, 0022, 0031, 0033, 0041 |
| **L2** | Plugin Tools | 原子能力（不编排） | `pdk/{shell_tools,fs_tools,provider_agent,budget_agent,session_agent,g3_knowledge_base,llama_engine,model_router}/` | ADR-0004, 0034, 0035, 0040 |
| **L3** | PDK Contract | 抽象接口 + 注册宏 + 元数据 | `include/agenticdsl/{pdk,policy,plugin,types}/`（DECLARE_TOOL/DEFINE_AGENT 宏, PluginInfo, AgentDescriptor, IExecutionPolicy, IApprovalHandler, IModelRouter, ITemporalClient） | ADR-0021, 0023, 0031, 0041, 0043 |
| **L4** | Application Services | 编排 L2 工具 + 暴露外部 API | `pdk/{loop_agent,temporal_agent,g1_coding_assistant}/` + `examples/pdk_chat_demo/` + `examples/pkm_temporal_demo/` | ADR-0051, 0053, 0054, 0060, 0067 |

**层间依赖硬性约束**（v1.2 §2.4 规则 R1~R5）:
- R1: L4 只能通过 L3 访问 L2/L1（禁止 `#include` L2 内部头文件）
- R2: L3 不依赖 L4（接口契约层不感知具体 Agent 实现）
- R3: L2 不调其他 L2（原子工具自包含，不编排）
- R4: L3 接口可在 L2 或 L4 中**实现**（`ITemporalClient`: L4 Temporal Agent 实现）
- R5: L3→L1 访问是单向的（`ToolResult` 是 L1 类型，被 L3 引用）

---

## 三、横向贯通：跨层共性缺失能力（事件/钩子/契约）

在进入分层分析之前，先识别**横跨多层的共性缺口**——这些是 Plugin 表达力的"基础设施缺口"，每层都需要但无法单独在某层解决。

### X1. 事件发射契约缺位（**最关键横向缺口**）
**现状**: `IInteractionBus` 订阅侧（`event_handler.cpp::Impl`）已订阅 12 个主题。2026-07-31 全量复核（`grep -rn "emit(" src examples/pdk_chat_demo`）：**生产代码实际约 28 处 emit 调用点**（修正初稿"仅 8 处"的低估），分布于 `chat_session` (5) / `main` (2) / `tool_coordinator` (5) / `cognitive_worker` (2) / `domain_worker_pool` (4) / `node_executor` (4) / `compliance_decorator` (4) / `approval_callbacks` (1) / `skill_interpreter` (1)。更严重的是，emit 已**扩散出订阅清单之外**——`tool.audit.{invoked,completed,denied}`、`compliance.log`、`cognitive.task.*`、`domain.task.*`、`dsl.call.*`、`execution.failed` 等主题无任何订阅方文档化。无契约约束的自发 emit 扩散比初稿评估更严峻：

| 主题 | EventHandler 订阅 | 实际 emit |
|------|:-:|:-:|
| `user.input` | ✅ | ✅ `chat_session.cpp:223` |
| `loop.done` | ✅ | ✅ `chat_session.cpp:306` |
| `loop.error` | ✅ | ✅ `chat_session.cpp:367` |
| `budget.checked` | ✅ | ✅ `chat_session.cpp:326` |
| `session.persist_request` | ✅ | ✅ `chat_session.cpp:349` |
| `app.shutdown` | ❌ | ✅ `main.cpp:55, 429` |
| `tool.coordinator.cycle_detected` | ❌ | ✅ `tool_coordinator.cpp:101` |
| `policy.approval.requested` | ❌ | ✅ `approval_callbacks.cpp:57` |
| **`loop.turn.start/end`** | ✅ | ❌ **零 emit** |
| **`tool.execution.start/update/end`** | ✅ | ❌ **零 emit** |
| **`llm.request/response`** | ✅ | ❌ **零 emit** |
| **`loop.decision`** | ✅ | ❌ **零 emit** |
| **`session.persisted`** | ✅ | ❌ **零 emit** |
| **`context.compact.{before,after}`** | ❌ | ❌ **零 emit** |
| **`temporal.*`** (5 种) | ❌ | ❌ 见 Temporal Agent 自实现 |

> **注**（2026-07-31 复核）：上述"零 emit"主题（`loop.turn.*` / `llm.*` / `loop.decision` / `tool.execution.*`）仅出现在 `examples/pdk_chat_demo/tests/test_e2e_mock.cpp` 的 mock 中——**测试在模拟生产代码不存在的行为**，测试与实现脱节。
>
> **另注**：初稿 emit 计数仅覆盖 pdk_chat_demo + 2 个核心文件，遗漏了 `src/modules/` 与 `src/common/` 的 ~20 处 emit；本表同步补充 `tool.audit.*` / `compliance.log` / `cognitive.task.*` / `domain.task.*` / `dsl.call.*` / `execution.failed` 等"有 emit 无订阅文档化"的反向缺口。

**根因**: 各模块"自愿调用"emit，无统一契约。**急需 ADR：Event Emission Contract**，规定每个主题的强制发射点（按层 + 按生命周期阶段）。

**影响**: pdk_chat_demo 借鉴 pi-agent 的所有事件钩子路径（§三、四、六）均依赖此契约。

### X2. 拦截/修改钩子机制完全缺失
**现状**: pi-agent 的 `beforeToolCall` / `afterToolCall` / `transformContext` / `session_before_*` 系列钩子在 HydraForge **无对应载体**：

| 钩子类型 | pi-agent | HydraForge 对应 |
|---------|---------|---------------|
| 工具调用拦截 + block | ✅ `beforeToolCall` | ⚠️ `ApprovalHandler` 仅可拒绝，**不可修改** |
| 工具结果修改 | ✅ `afterToolCall` | ❌ 无 |
| LLM 调用前消息注入/裁剪 | ✅ `transformContext()` | ❌ 无 |
| 会话生命周期拦截 | ✅ `session_before_switch/fork/compact` | ❌ 无 |
| 输入拦截 | ✅ `input` | ❌ 无 |

**根因**: `ToolCoordinator`（ADR-0031）只有"layer check → approval → call → audit"线性流，**未设计 hook 注入点**。

**影响**: PDK plugin 无法拦截工具调用，限制扩展能力；pdk_chat_demo 的 /tree /compact /fork 等 slash 命令无钩子机制承载。

### X3. PDK Plugin 命令/快捷键注册 API 完全缺失
**现状**: 源码核查 `grep -rn "registerCommand|registerShortcut" include/agenticdsl/pdk/` 返回**空**。

**根因**: PDK 仅有 `DECLARE_TOOL` 宏注册工具，未设计命令/快捷键抽象。

**影响**: `/tree`、`/compact`、`/fork`、`/clone` 等斜杠命令无法插件化，必须 hardcode 在 main.cpp，违背 §1 P5 "契约唯一" 原则。

### X4. pdk_chat_demo 隐藏缺陷：loop_agent bypass
**现状**（源码核查发现）: `chat_session.cpp:233` 有短路逻辑
```cpp
bool use_direct_llm = (llm != nullptr);  // line 233
if (use_direct_llm) {
    // 直连 llm->generate()（拼 prompt + 阻塞调用）
    // ← 实际 demo 流程走这里，跳过 loop_agent plugin！
} else {
    // 仅 llm == nullptr 时走 loop/run（test_loop_agent_plugin.cpp 测试场景）
    impl_->registry->call_tool("loop/run", loop_args);
}
```

**根因**: 当初实现时为"快速 mock"加了 `use_direct_llm` 短路分支；后来 Loop Agent 真实实现 ship 了，**短路分支未清理**。

**影响范围**:
- §五 Streaming / §七 Compaction / §八 工具并行 / §六 Steering 借鉴路径全部依赖 loop_agent 真正生效
- 设计文档（`DESIGN.md:530`）声明"Loop Agent 触发 via call_tool 'loop/run'"，与代码不符
- DESIGN.md 第六章"6 个 Agent Plugin"架构承诺被违反

**建议**: 列为 P0#0（最高优先级），先于其他借鉴工作修复。

---

## 四、L0 Runtime Core 缺失能力

L0 负责 DSL 解析、调度、执行、原子类型。当前核心已稳定（实施率 100% per ADR gap analysis），但存在 4 项与借鉴路径相关的缺口。

### L0-1. SessionManager 核心类缺失（核心存储）
**位置**: 应新建 `src/core/session_manager.{h,cpp}`
**关联 ADR**: ADR-0033（已批 Session Hierarchy）
**现状**:
- ✅ UserSession/TaskSession/SubtaskSession 三层**执行作用域**已 ship (Sprint 15 C5)
- ❌ **存储格式仍是线性 JSON**（`chat_session.cpp::save_to_disk` 单文件原子写）
- ❌ 缺 JSONL 树状持久化、open/fork/branch/compact API
**借鉴来源**: pi-agent `SessionManager` 类（`buildContextEntries` 从叶子到根遍历）
**影响**: §三 借鉴路径 P0.1 的核心，阻塞整个会话树能力
**估时**: 1.5 Sprint（存储层 + 树遍历 + 旧 JSON 迁移工具）

### L0-2. NodeExecutor 工具并行执行器未整合
**位置**: `src/modules/executor/node_executor.h` `execute_tool_call()`
**关联 ADR**: ADR-0020（已批）
**现状**:
- ✅ `DomainWorkerPool` 已 ship (Sprint 3, N 个 `std::jthread`)
- ❌ `execute_tool_call()` 仍取 `tool_calls[0]` 串行执行
- ❌ 工具结果顺序归并逻辑缺
**借鉴来源**: pi-agent 默认并行 + 按工具 `executionMode` 覆盖
**影响**: §八 借鉴路径 P2.1 的真正落点；多 LLM function call 场景性能差
**估时**: 1.5 Sprint（NodeExecutor 改造 + DomainWorkerPool 集成 + 顺序归并）

### L0-3. ContextCompactor 抽象缺失
**位置**: 应新建 `src/core/context_compactor.{h,cpp}`
**关联 ADR**: ADR-0007（🟡 Partial，"快照有,无 LLM 压缩"）
**现状**:
- ✅ 快照机制已 ship
- ❌ 无 LLM 摘要调用接口
- ❌ 无 `context.compact.{before,after}` 事件定义
- ❌ 阈值检测钩子缺（`compact_threshold_tokens` 在 `SessionConfig` 声明但无触发）
**借鉴来源**: pi-agent `compaction`（threshold/overflow + 摘要 + 完整历史保留）
**影响**: 长对话场景下上下文窗口可能溢出
**估时**: 1 Sprint（接口 + 阈值检测 + LLM 摘要调用）

### L0-4. MarkdownParser YAML 格式支持缺位
**位置**: `src/modules/parser/markdown_parser.{h,cpp}`
**关联 ADR**: —
**现状**:
- ✅ Validator (`DslValidator`) 支持 Markdown bold (`**key**: value`) 格式
- ❌ 当前 `lib/loop/*.agent.md` 实际使用 **YAML frontmatter** 格式
- ❌ `main.cpp:321` 检测到 YAML 格式时**直接跳过校验**（带 stderr 警告）
**影响**: DSL Schema 校验（pdk_chat_demo T2）失效，9 个测试 fixture 仍用 Markdown bold，与生产代码不一致
**估时**: 0.5 Sprint（YAML frontmatter 解析器 + 双格式兼容）

---

## 五、L1 OS Services 缺失能力

L1 提供跨 Plugin 共享基础设施。是**横向贯通缺口 X1/X2 的主要落点**。

### L1-1. 事件发射契约 ADR（横向 X1 的承载）
**位置**: `include/agenticdsl/contract/iinteraction_bus.h` + 各 emitter 调用点
**关联 ADR**: ADR-0019（🟡 Partial）+ 新增 Event Emission Contract ADR
**现状**:
- ✅ IInteractionBus + InMemoryBus MPMC + BusEvent 公开契约 + subscribe_glob 通配符订阅 + CausalClock 全部 ship (2026-07-26~27)
- ❌ **缺统一发射契约**——8 个缺失主题的 emit 未在任何模块实现
- ❌ 各模块"自愿调用"，无强制约束
**建议**: 新增 ADR-0068 "Event Emission Contract"：
- 规定每个主题的强制发射层（L0/L1/L2/L3/L4）+ 生命周期阶段
- 规定 payload 字段（JSON Schema 约束）
- 规定 backward compat 政策（新增主题 vs 修改 payload）
- 引入 EventBuilder helper 统一构造 BusEvent
**估时**: 1 Sprint（ADR + 8 个 emit 补齐 + EventBuilder helper + 测试）

### L1-2. ToolCoordinator Hook 注入点（横向 X2 的承载）
**位置**: `src/common/tools/tool_coordinator.{h,cpp}`
**关联 ADR**: ADR-0031（🟡 Partial，4 项 defer 至 C6）
**现状**:
- ✅ Layer check → ApprovalHandler → audit invoked → call_tool → audit completed 线性流
- ❌ 无 `before_tool_call` 钩子（PDK plugin 无法拦截）
- ❌ 无 `after_tool_call` 钩子（无法修改结果）
- ❌ 无 `transformContext` 钩子（LLM 调用前注入/裁剪消息）
**借鉴来源**: pi-agent `tool_call` 事件可 block，`tool_result` 事件可修改
**建议**: ToolCoordinator 改为 middleware 链式架构（类似 koa.js）：
```
ToolCoordinator:
  pre_hooks[] → layer_check → ApprovalHandler → call_tool
                → post_hooks[] → audit_completed → return
```
PDK plugin 通过 `IToolRegistry::register_tool_pre_hook(name, fn)` 注册
**估时**: 1 Sprint（middleware 架构 + 注册 API + 测试）

### L1-3. Session 生命周期事件总线契约（横向 X2 的承载）
**位置**: `include/agenticdsl/contract/iinteraction_bus.h` + `src/core/types/session.h`
**关联 ADR**: ADR-0033（已批 Session Hierarchy）
**现状**:
- ✅ 三层 Session 模型 ship
- ❌ 无 `session_before_switch` / `session_before_fork` / `session_before_compact` 事件
- ❌ 无 `session.persisted` 事件（虽在 EventHandler 订阅，但无 emit）
**借鉴来源**: pi-agent 会话管理事件链
**影响**: pi-agent 借鉴路径 P0.3 的 session 层能力
**估时**: 0.5 Sprint（事件定义 + SessionManager 集成）

### L1-4. ILLMProvider 中断/切换模型能力深化
**位置**: `include/agenticdsl/contract/i_llm_provider_decorator.h` + `src/common/llm/`
**关联 ADR**: ADR-0001 + ADR-0042（🔍 Proposed，C16 增量已 ship）
**现状**:
- ✅ `stop_token`（C++20 jthread 取消）支持完全中断
- ✅ `IGenerationStream` 流式接口已 ship
- ✅ Decorator 链（CostTracking/Compliance/RateLimit）已 ship
- ❌ **缺中途切换模型**（pi-agent `/model` Ctrl+L）
- ❌ **缺 thinking_level 选择**（依赖 provider 支持，框架层缺抽象）
- ❌ **缺异步 I/O 与 Agent 执行的双生产者协调原语**（stdin/steering 队列需要）
**影响**: §九 CLI 丰富化、§六 消息队列借鉴路径
**估时**: 1 Sprint（mid-stream switch 抽象 + thinking_level 抽象 + 双生产者协调原语）

### L1-5. LLMProviderFactory 运行时注册扩展
**位置**: `src/common/llm/llm_provider_factory.{h,cpp}`
**关联 ADR**: ADR-0005（已批 LLM 后端配置工厂）
**现状**:
- ✅ 构造时配置支持
- ❌ 无 `register_dynamic()` 方法支持运行时新增 provider
**借鉴来源**: pi-agent `pi.registerProvider()`
**影响**: §十 Provider 动态发现借鉴路径的前置依赖
**估时**: 0.5 Sprint（register_dynamic + 工厂扩展）

### L1-6. EventBus BusEvent 序列化协议缺位（横向）
**位置**: `include/agenticdsl/contract/bus_event.h` + `InMemoryBus` 序列化层
**关联 ADR**: ADR-0046（🔍 Proposed，PDK 插件间通信协议，实施率 ~35%）
**现状**:
- ✅ BusEvent 公开契约已 ship（Change A, 2026-07-26）
- ✅ subscribe_glob 通配符订阅已 ship（Change B, 2026-07-27）
- ❌ 跨进程序列化协议（JSON / MessagePack / Cap'n Proto）未定义
- ❌ 跨插件消息格式、版本协商无规范
**影响**: SKILL.md 子进程 ↔ 主进程通信当前用 ad-hoc pipe（skill_child_main 4 host function），无统一序列化
**估时**: 1 Sprint（序列化协议 + 版本协商 + 跨进程测试）

### L1-7. OpenTelemetry Exporter 零代码
**位置**: 应新建 `src/modules/trace/opentelemetry_exporter.{h,cpp}`
**关联 ADR**: ADR-0063（✅ Approved 但**零代码**）
**现状**:
- ✅ TraceRecord data-only struct ship
- ❌ 无 OTel 集成代码
**影响**: §十一 "可观测性" SOTA 黄金标准缺位
**估时**: 1.5 Sprint（OTLP HTTP exporter + W3C traceparent 兼容）

---

## 六、L2 Plugin Tools 缺失能力

L2 提供原子能力（不编排）。当前 8 个 PDK plugin 中，**仅 5 个真正实施了核心工具集**，3 个为 PoC/空壳状态。

### L2-1. provider_agent 缺动态发现工具
**位置**: `pdk/provider_agent/src/pdk_entry.cpp`
**关联 ADR**: —
**现状**:
- ✅ 4 个工具已注册：`provider/register`、`provider/resolve`、`provider/list`、`provider/health`
- ❌ 缺 `provider/refresh`（从 API 拉取最新模型目录）
- ❌ 缺 `provider/register_dynamic`（运行时注册新 provider）
- ❌ 缺 `provider/switch`（运行时切换默认模型）
**借鉴来源**: pi-agent `pi.registerProvider()` + 自动模型目录
**影响**: §十 Provider 动态发现借鉴路径
**估时**: 1 Sprint（refresh + register_dynamic + switch）

### L2-2. session_agent 缺 SessionManager 持久化能力
**位置**: `pdk/session_agent/`
**关联 ADR**: ADR-0033（已批）
**现状**:
- ✅ `session/history`、`session/compact`、`session/branch`、`session/persist` 工具已声明
- ❌ 实际实现：**仅复用 chat_session.cpp 的内联实现**，未真正独立为通用 plugin
- ❌ 缺 SessionManager L0 集成（L0-1）
**影响**: pdk_chat_demo 的 chat_session 应基于此 plugin 而非内联
**估时**: 1 Sprint（与 L0-1 SessionManager 协同实施）

### L2-3. budget_agent 工具能力不足
**位置**: `pdk/budget_agent/src/`
**关联 ADR**: ADR-0031（🟡 Partial，4 项 defer 至 C6 含成本预算集成）
**现状**:
- ✅ `budget/query`、`budget/set_limit`、`budget/alerts` 已注册
- ❌ **成本预算未完全闭环**（ADR-0031 §决策 8 defer 至 C6）
- ❌ 工具调用无超时保护（同 ADR-0031 defer）
- ❌ 审批历史未持久化（同 ADR-0031 defer）
**影响**: 当前 demo 中预算告警后**直接终止 chat**，无法降级或续费
**估时**: 1 Sprint（成本闭环 + 超时 + 审批历史持久化）

### L2-4. fs_tools / shell_tools 缺 input/output schema
**位置**: `pdk/fs_tools/`、`pdk/shell_tools/`
**关联 ADR**: ADR-0043（已批 PDK 工具命名约定）、ADR-0058（✅ Approved 但**零代码**）
**现状**:
- ✅ 工具已注册（fs/read, fs/write, fs/list, fs/exists; shell/exec, shell/which, shell/env）
- ❌ 无 JSON Schema 2020-12 强约束（input_schema/output_schema）
- ❌ ADR-0058 Schema 强制校验**整体零代码**
**影响**: §十.3 P1 "缺工具 input/output schema 强制校验" SOTA 差距
**估时**: 0.5 Sprint（JSON Schema 字段 + ToolRegistry 校验 hook）

### L2-5. 缺 WasmRuntime 跨平台原子工具
**位置**: 应新建 `src/common/wasm/wasm_runtime.{h,cpp}` 或 L1 OS Services
**关联 ADR**: ADR-0056（✅ Approved 但**零代码**）、ADR-0065（Python PDK，**零代码**）
**现状**:
- ❌ **WasmRuntime 完全缺位**
- ❌ 无 capability-limited host function 实现
- ❌ 无 wasi-sdk 集成（ADR-0061-05 ✅ Approved 但**无代码**）
**借鉴来源**: WASI 规范 + wamr/wasmtime 集成
**影响**: §十.3 P0 SOTA 差距 "缺 Wasm 运行时"
**估时**: 2 Sprint（wamr 集成 + host function 白名单 + tests）

### L2-6. 缺 Browser Agent / Search Agent
**位置**: 应新建 `pdk/browser_agent/`、`pdk/search_agent/`
**关联 ADR**: —
**现状**:
- ❌ **完全缺位**
- 规划建议：Browser (L2 C++, Playwright 集成), Search (L2 Skill, 依赖外部 API)
**影响**: §九 章节"建议新增"插件列表中明确提及但未实施
**估时**: 2 Sprint（Browser, 需 Playwright/Chromium）+ 1 Sprint（Search）

### L2-7. Temporal Agent 实施状态
**位置**: `pdk/temporal_agent/`
**关联 ADR**: ADR-0051（Phase 6 Composition Spike）
**现状**:
- ✅ Phase 1+2 已 ship (2026-07-28, 41/41 tasks, 10/10 ctest)
- 🟡 仅 Phase 1（InMemoryTemporalBackend），Phase 2 gRPC defer
- ✅ 5 个工具注册（`temporal/start_workflow`、`start_async`、`poll`、`signal`、`query`）
- ✅ `ITemporalClient` L3 契约 ship
- ✅ Temporal Agent 归属 L4（v1.2 决策 A15）
**注**: 这是 L4 编排 Agent（非 L2 原子工具），实际在第八节分析

### L2 实施率总结

| Plugin | 状态 | 工具数 | 形态 | 备注 |
|--------|------|:---:|------|------|
| `llama_engine` | ✅ 完整 | 12 | C++ | ADR-0035/0040/0044 |
| `model_router` | ✅ 完整 | 4 | C++ | ADR-0034 |
| `shell_tools` | 🟡 部分 | 3 | C++ | 缺 schema 校验（L2-4） |
| `fs_tools` | 🟡 部分 | 4 | C++ | 缺 schema 校验（L2-4） |
| `provider_agent` | 🟡 部分 | 4 | C++ | 缺动态工具（L2-1） |
| `budget_agent` | 🟡 部分 | 3 | C++ | 缺成本闭环（L2-3） |
| `session_agent` | 🟡 PoC | 4 | C++ | 复用 chat_session（L2-2） |
| `g3_knowledge_base` | ✅ 完整 | 1 | C++ | ADR-0051 |
| `temporal_agent` | ✅ Phase 1+2 | 5 | C++ | L4 编排（见第八节） |
| `loop_agent` | 🟡 部分 | 2 | C++/DSL | **bypass bug**（见第八节） |
| `g1_coding_assistant` | 🟡 部分 | 1 | C++ | ADR-0051, 编排 G3 |
| `browser_agent` (建议新增) | ❌ 缺位 | — | C++ | L2-6 |
| `search_agent` (建议新增) | ❌ 缺位 | — | Skill | L2-6 |

---

## 七、L3 PDK Contract 缺失能力

L3 是 PDK 接口契约层。当前已具备基础（IToolRegistry 9 虚函数、ITemporalClient 5 虚函数、IModelRouter、IExecutionPolicy、IApprovalHandler），但仍有 6 项关键缺口。

### L3-1. Plugin Manifest 扩展能力不足
**位置**: `include/agenticdsl/plugin/plugin_info.{h,cpp}` + `pdk_manifest.json`
**关联 ADR**: ADR-0052（✅ Approved 但**零代码**）、ADR-0054（✅ Approved 但**零代码**）
**现状**:
- ✅ PluginInfo V2 已 ship（ADR-0041，abi_version/name/version/capabilities/dependencies）
- 🟡 `pdk_manifest.json` 在 loop_agent / provider_agent 已有，但**未标准化强制字段**
- ❌ `pdk_manifest()` C 符号导出**零插件实施**
- ❌ 缺 Capability Discovery 索引（ManifestRegistry / CapabilityRegistry 均为**待实现**）
**借鉴来源**: SOTA 黄金标准 MCP + Zylos + ATD
**影响**: §十.3 P0 SOTA 差距 "Agent 插件无 manifest"
**估时**: 1 Sprint（pdk_manifest() 强制 + CapabilityRegistry + 测试）

### L3-2. Plugin Lifecycle 管理缺位
**位置**: 应新建 `include/agenticdsl/plugin/plugin_lifecycle.{h,cpp}`
**关联 ADR**: ADR-0057（✅ Approved 但**零代码**）
**现状**:
- ✅ PluginLoader V2 已 ship（ADR-0041, `pdk_plugin_init` / `pdk_plugin_fini` 钩子）
- ❌ **缺 Lifecycle 管理器**：install / init / activate / deactivate / uninstall 状态机
- ❌ 无 lazy-load 触发器（VS Code `activationEvents` 风格）
- ❌ 无 hot-reload（OSGi bundle update 风格）
- ❌ 无 semver 版本约束
**借鉴来源**: OSGi + VS Code + MCP 共同模式
**影响**: §十.3 P1 SOTA 差距（hot-reload / lazy-load / semver）
**估时**: 2 Sprint（Lifecycle 状态机 + activation_events + hot-reload）

### L3-3. 缺 Plugin 命令/快捷键注册宏（横向 X3）
**位置**: 应新增 `include/agenticdsl/pdk/command_macros.h`
**关联 ADR**: —
**现状**:
- ❌ 源码核查 `grep -rn "registerCommand|registerShortcut" include/agenticdsl/pdk/` 返回空
- ❌ DECLARE_COMMAND / DECLARE_SHORTCUT 宏不存在
**借鉴来源**: pi-agent `pi.registerCommand(name, handler)` + `pi.registerShortcut(key, handler)`
**影响**: 阻断所有 slash 命令的可插拔化
**估时**: 0.5 Sprint（DECLARE_COMMAND 宏 + CommandRegistry + 注册流程）

### L3-4. PDK Plugin 事件订阅 API 缺位
**位置**: 应新增 `include/agenticdsl/pdk/event_subscription_macros.h`
**关联 ADR**: ADR-0046（🔍 Proposed，~35% 实施率）
**现状**:
- ✅ IInteractionBus::subscribe() 在基础设施层可用
- ❌ PDK 缺乏官方订阅 API 包装宏（`pi.on()` 等价物）
- ❌ `DECLARE_EVENT_SUBSCRIBER(topic, handler)` 宏不存在
**影响**: §四 借鉴路径 P0.3 的 plugin 端承载
**估时**: 0.5 Sprint（订阅宏 + 通配符支持）

### L3-5. ToolMetadata 增 Schema 字段
**位置**: `include/agenticdsl/policy/execution_policy.h`（ToolMetadata 定义）
**关联 ADR**: ADR-0058（✅ Approved 但**零代码**）、ADR-0004 V2
**现状**:
- ✅ ToolMetadata V2 已 ship（name/description/category/min_layer/allowed_layers/approval）
- ❌ **缺 input_schema / output_schema JSON Schema 2020-12 字段**
- ❌ ToolRegistry 注册时**无 schema 校验**
**影响**: §十.3 P1 SOTA 差距
**估时**: 0.5 Sprint（schema 字段 + JSON Schema validator 集成）

### L3-6. PDK 安装包管理基础设施缺位
**位置**: 应新建 `scripts/pdk-{install,uninstall,search}.sh` + 包注册表索引
**关联 ADR**: —
**现状**:
- ✅ `scripts/sync-pdk.sh` 已存在（仅 monorepo 同步）
- ❌ 无 `pdk install <name>` / `pdk uninstall` CLI
- ❌ 无包注册表/索引/版本约束解析
**借鉴来源**: OSGi bundle repository + VS Code Marketplace
**影响**: §十一 "安装包管理" 借鉴路径的承载
**估时**: 2 Sprint（CLI + 包注册表 schema + monorepo→standalone 流程）

---

## 八、L4 Application Services 缺失能力（含 pdk_chat_demo）

L4 是 Agent 应用服务层。当前 4 个 L4 plugin 中，**3 个有 loop_agent bypass 等问题**。

### L4-1. 🔴 loop_agent 短路 bug（最高优先级）
**位置**: `examples/pdk_chat_demo/chat_session.cpp:233-274`
**关联 ADR**: ADR-0051（Phase 6 Composition Spike）
**现状**:
- 设计文档（`DESIGN.md:530`）声明"Loop Agent 触发 via call_tool 'loop/run'"
- 实际代码：`use_direct_llm = (llm != nullptr)` 短路，**正常 demo 流程绕过 loop_agent**
- `loop/run` 工具仅在测试（test_loop_agent_plugin.cpp）和 llm == nullptr 时触发
- L4 Plugin `pdk/loop_agent/` 实际是 **dead code**
**影响**:
- §五 Streaming 借鉴：stream 永远无法触达用户
- §七 Compaction 借鉴：必须经 loop_agent 才能 hook
- §八 工具并行借鉴：必须经 loop_agent 走 NodeExecutor
- §六 Steering 借鉴：必须经 loop_agent 才能注入到 turn 中断点
- 整个 pdk_chat_demo 与 DESIGN.md §八"Chat 应用"层标注不一致

**修复方案**:
```cpp
// 删除 use_direct_llm 分支，统一调用 loop/run
nlohmann::json loop_result = impl_->registry->call_tool("loop/run", loop_args);
// loop_agent plugin 内部根据 tls_parent_provider 决定 mock fallback vs 真实执行
```

**估时**: 0.5 Sprint（删除分支 + loop_agent 内部 mock fallback 兜底 + 测试）

### L4-2. pdk_chat_demo 异步 I/O 改造
**位置**: `examples/pdk_chat_demo/main.cpp:388` + `chat_session.cpp::chat()`
**关联 ADR**: —
**现状**:
- ❌ `while(getline)` 同步循环，Agent 运行时无法输入
- ❌ 无 `steering_queue_` / `follow_up_queue_`
**借鉴来源**: pi-agent `agent.steer()` + `agent.followUp()`
**影响**: §六 消息队列/Steering 借鉴路径
**依赖**: L1-4（中断/切换模型原语）
**估时**: 1 Sprint（双线程 + 队列 + 事件集成）

### L4-3. pdk_chat_demo 流式渲染 + slash 命令 TUI
**位置**: `examples/pdk_chat_demo/event_handler.cpp`
**关联 ADR**: —
**现状**:
- 🟡 EventHandler 已订阅 12 个主题（X1），但流式渲染逻辑无
- ❌ 无 slash 命令解析（`/tree`、`/compact`、`/fork`、`/clone`）
- ❌ 无 `--system-prompt` / `--append-system-prompt` 命令行
**依赖**: L1-1（事件发射契约补齐）+ L3-3（命令注册宏）
**估时**: 1 Sprint（流式渲染 + slash 解析 + CLI 扩展）

### L4-4. pdk_chat_demo CLI 解析层重写
**位置**: `examples/pdk_chat_demo/main.cpp:76-83`
**关联 ADR**: —
**现状**:
- 🟡 手撸 args 循环（仅支持 `--mock` 和 `--session <id>`）
- ❌ 缺 `-p` print 模式 / `--mode json|rpc` / `-c` 续最近 / `-r` 选择 / `--provider` / `--offline` 等
**借鉴来源**: pi-agent CLI flag 设计
**依赖**: L3-2（CLI flag 声明）
**估时**: 3 天（cxxopts/argparse 引入）

### L4-5. pdk_chat_demo /compact 命令 + 阈值检测
**位置**: ChatSession + EventHandler
**关联 ADR**: ADR-0007（🟡 Partial）
**现状**:
- 🟡 `compact_threshold_tokens` 在 `SessionConfig` 已声明
- ❌ 每轮结束后**无 token 计数与阈值触发**
- ❌ 无 LLM 摘要调用
**依赖**: L0-3（ContextCompactor 抽象）
**估时**: 0.5 Sprint（与 L0-3 协同）

### L4-6. pdk_chat_demo 会话树 TUI（/tree /fork /clone）
**位置**: ChatSession + EventHandler
**关联 ADR**: ADR-0033（已批 Session Hierarchy）
**现状**:
- ❌ 无树导航 TUI
- ❌ 无 `/fork` / `/clone` 命令
- ❌ 无 `--fork <id>` / `--name` CLI flag
**依赖**: L0-1（SessionManager）+ L1-3（session 生命周期事件）
**估时**: 1 Sprint（与 L0-1 协同）

### L4-7. pdk_chat_demo /export HTML + 分享
**位置**: ChatSession
**关联 ADR**: —
**现状**:
- ❌ 无 HTML 导出
- ❌ 无 HF 分享集成
**借鉴来源**: pi-agent `/export` HTML + `pi-share-hf`
**依赖**: PDK 层提供 /export 工具
**估时**: 1 Sprint

### L4-8. g1_coding_assistant 编排能力不完整
**位置**: `pdk/g1_coding_assistant/`
**关联 ADR**: ADR-0051
**现状**:
- ✅ `coding_assistant/review` 工具已注册
- ❌ 编排 G3 Knowledge Base 的完整流程未实现
- ❌ 无 review comment 状态机
**影响**: ADR-0051 Phase 6 Composition Spike 演示承诺未完全兑现
**估时**: 1 Sprint（完成 review flow 编排）

### L4 实施率总结

| Agent | 状态 | 工具数 | 形态 | 备注 |
|-------|------|:---:|------|------|
| `loop_agent` | 🔴 有 bug | 2 | C++/DSL | **bypass bug**（L4-1） |
| `temporal_agent` | ✅ Phase 1+2 | 5 | C++ | 41/41 tasks done |
| `g1_coding_assistant` | 🟡 部分 | 1 | C++ | L4-8 |
| `pdk_chat_demo` | 🔴 有 bug | (编排示例) | C++ | **loop_agent bypass**（L4-1）+ 多项缺失 |
| `pkm_temporal_demo` | ✅ 完整 | (示例) | C++ | 与 temporal_agent 配套 |

---

## 九、Phase 6 ADR 系列代码化映射

ADR-0052~0065 是 Phase 6 架构评审通过的 14 个 ADR，其中 12 个**零代码**（0055 SKILL.md 隔离与 0060 组合协议已 ship）。下表映射到本文档识别的缺失能力：

| ADR | 标题 | 文档化内容 | 对应本文档缺口 | 估时 |
|-----|------|-----------|---------------|------|
| **ADR-0052** | Agent Plugin Manifest | manifest-first 规范 | **L3-1** | 1 Sprint |
| **ADR-0053** | AgentDescriptor 接口 | Agent 元数据抽象 | **L3-1** (与 ADR-0052 协同) | — |
| **ADR-0054** | Capability Discovery | 能力发现索引 | **L3-1** | — |
| **ADR-0055** | SKILL.md 执行隔离 | SkillInterpreter V1 | ✅ **已 ship**（2026-07-22） | — |
| **ADR-0056** | Wasm Agent 运行时 | wamr 集成 | **L2-5** | 2 Sprint |
| **ADR-0057** | Agent 生命周期 | Lifecycle 状态机 | **L3-2** | 2 Sprint |
| **ADR-0058** | Schema 强制校验 | JSON Schema 集成 | **L2-4** + **L3-5** | 0.5+0.5 Sprint |
| **ADR-0059** | 跨进程协议 | RemoteAgentAdapter | **L1-6** | 1 Sprint |
| **ADR-0060** | Agent 组合协议 | 6 种协作模式 | 已 ship（v1.2 §2.4 R1~R5） | — |
| **ADR-0061** | Agent 进化与固化 | 4 阶段管线 | `agent-evolution-pipeline.md` 已文档化，**AgentEvolutionEngine 待实现** | 3 Sprint |
| **ADR-0062** | Agent Marketplace | 包分发 | **L3-6** | 2 Sprint |
| **ADR-0063** | OpenTelemetry 追踪 | OTel exporter | **L1-7** | 1.5 Sprint |
| **ADR-0064** | Conformance Test Suite | PDK 兼容性测试 | 新增 `tests/conformance/` | 1 Sprint |
| **ADR-0065** | Python PDK | Wasm 多语言 | **L2-5** | 2 Sprint |
| **ADR-0066** | SkillInterpreter 架构 | V2 增强 | 🟡 V1 done, V2 deferred | 1 Sprint |
| **ADR-0067** | 分层插件架构拆分 | L2/L3/L4 三层拆分 | ✅ **已 ship**（2026-07-23 追溯性正式化） | — |

**Phase 6 总估时**: 12 个零代码 ADR 全部代码化约 **18 Sprint（~36 周）**，按本文档优先级矩阵分阶段执行。

---

## 十、优先级矩阵与执行依赖

### 10.1 按优先级排序的完整缺失能力清单

| 序 | 编号 | 缺失项 | 层 | 阻塞路径 | 估时 (Sprint) |
|---|------|--------|----|---------|---------------:|
| 1 | **L4-1** | 🔴 loop_agent 短路 bug | L4 | §五/§六/§七/§八 全部借鉴 | 0.5 |
| 2 | **X1** | 事件发射契约 ADR + 8 个 emit 补齐 | L1 横向 | §四/§七 全部借鉴 | 1.0 |
| 3 | **L3-3** | Plugin 命令/快捷键注册宏 | L3 | 所有 slash 命令 | 0.5 |
| 4 | **X2 / L1-2** | ToolCoordinator Hook 注入点 | L1 | PDK 拦截能力 | 1.0 |
| 5 | **L0-1** | SessionManager + JSONL 存储 | L0 | §三 会话树借鉴 | 1.5 |
| 6 | **L0-3** | ContextCompactor 抽象 | L0 | §七 Compaction | 1.0 |
| 7 | **L2-2** | session_agent SessionManager 集成 | L2 | ChatSession 通用化 | 1.0 |
| 8 | **L0-2** | NodeExecutor 工具并行 | L0 | §八 工具并行 | 1.5 |
| 9 | **L4-2** | ChatSession 异步 I/O（steering/follow-up） | L4 | §六 Steering | 1.0 |
| 10 | **L1-4** | 中断/切换模型抽象 | L1 | §九 /model 借鉴 | 1.0 |
| 11 | **L2-1** | provider/refresh + register_dynamic + switch | L2 | §十 动态发现 | 1.0 |
| 12 | **L1-5** | LLMProviderFactory 运行时注册 | L1 | L2-1 前置 | 0.5 |
| 13 | **L3-1** | Plugin Manifest 标准化 + CapabilityRegistry | L3 | ADR-0052/0053/0054 | 1.0 |
| 14 | **L1-3** | Session 生命周期事件 | L1 | pi-agent §四 session 借鉴 | 0.5 |
| 15 | **L4-3** | EventHandler 流式 + slash 渲染 | L4 | §五/§三 P0.2 | 1.0 |
| 16 | **L4-4** | CLI 解析（cxxopts） | L4 | §九 | 0.3 |
| 17 | **L2-4** | fs_tools/shell_tools schema 校验 | L2 | ADR-0058 | 0.5 |
| 18 | **L3-5** | ToolMetadata JSON Schema 字段 | L3 | ADR-0058 | 0.5 |
| 19 | **L3-4** | PDK 事件订阅 API | L3 | §四 hook 注册 | 0.5 |
| 20 | **L0-4** | MarkdownParser YAML 格式支持 | L0 | pdk_chat_demo T2 | 0.5 |
| 21 | **L2-3** | budget_agent 成本闭环 + 超时 | L2 | ADR-0031 C6 defer | 1.0 |
| 22 | **L4-5** | /compact 命令 + 阈值检测 | L4 | L0-3 协同 | 0.5 |
| 23 | **L4-6** | /tree /fork /clone TUI | L4 | L0-1 协同 | 1.0 |
| 24 | **L3-2** | Plugin Lifecycle 管理器 + hot-reload | L3 | ADR-0057 | 2.0 |
| 25 | **L1-6** | EventBus 序列化协议 | L1 | ADR-0046 | 1.0 |
| 26 | **L1-7** | OpenTelemetry Exporter | L1 | ADR-0063 | 1.5 |
| 27 | **L4-7** | /export HTML + HF 分享 | L4 | L4 阶段 | 1.0 |
| 28 | **L4-8** | g1_coding_assistant 完整 review flow | L4 | ADR-0051 | 1.0 |
| 29 | **L2-5** | WasmRuntime 集成（wamr + host functions） | L1/L2 | ADR-0056 | 2.0 |
| 30 | **L3-6** | PDK 安装包管理（pdk install CLI） | L3 | §十一 包管理 | 2.0 |
| 31 | **L2-6** | Browser Agent / Search Agent | L2 | §九 建议新增 | 3.0 |
| 32 | **ADR-0061** | AgentEvolutionEngine 实施 | L4 | 4 阶段管线 | 3.0 |
| 33 | **ADR-0064** | Conformance Test Suite | L3 | PDK 兼容性 | 1.0 |
| 34 | **ADR-0065** | Python PDK | L3 | Wasm 多语言 | 2.0 |
| 35 | **ADR-0066** | SkillInterpreter V2 增强 | L1 | 🟡 Partial 收尾 | 1.0 |

### 10.2 执行依赖图

```
P0#0 (L4-1 loop_agent bypass 修复)
    │
    ├─→ P0#1 (X1 事件发射契约)
    │       │
    │       └─→ P1#1 (L4-3 流式渲染)
    │
    ├─→ P0#2 (L3-3 Plugin 命令注册)
    │       │
    │       └─→ P1#2 (L4-6 /tree /fork TUI)
    │
    ├─→ P0#3 (X2 / L1-2 ToolCoordinator Hook)
    │       │
    │       └─→ P1#3 (L3-4 PDK 事件订阅 API)
    │
    ├─→ P0#4 (L0-1 SessionManager)
    │       │
    │       ├─→ P1#4 (L1-3 Session 生命周期事件)
    │       │       │
    │       │       └─→ P1#5 (L4-6 /tree TUI)
    │       │
    │       └─→ P1#6 (L2-2 session_agent 集成)
    │
    ├─→ P0#5 (L0-3 ContextCompactor)
    │       │
    │       └─→ P1#7 (L4-5 /compact 命令)
    │
    ├─→ P0#6 (L1-5 LLMProviderFactory 运行时注册)
    │       │
    │       └─→ P1#8 (L2-1 provider 动态工具)
    │
    └─→ P0#7 (L0-2 NodeExecutor 工具并行)
            │
            └─→ P1#9 (L4-3 流式 + 顺序归并)

P2#1 (L3-1 Plugin Manifest)
P2#2 (L3-2 Lifecycle)
P2#3 (L2-5 WasmRuntime)
P2#4 (L1-7 OTel)
P2#5 (L3-6 pdk install)
P2#6 (L2-6 Browser/Search Agent)
P2#7 (ADR-0061 AgentEvolutionEngine)
```

### 10.3 关键路径（Critical Path）

按 v1.2 五层模型的最短"借鉴闭环"路径：

```
L4-1 fix (0.5S) → X1 事件契约 (1.0S) → X2 Hook 注入 (1.0S) → L0-1 SessionManager (1.5S) → L3-3 命令注册 (0.5S)
→ L4-6 /tree TUI (1.0S)
                                                        ↓
                                              L0-2 工具并行 (1.5S)
                                                        ↓
                                              L0-3 ContextCompactor (1.0S)
                                                        ↓
                                              L2-1 provider 动态 (1.0S)
                                                        ↓
                                              L4-3 流式 + slash (1.0S)
```

**Critical Path 估时**: ~9.5 Sprint（约 19 周，可部分并行缩短至 ~14 周）

### 10.4 总估时汇总

| 维度 | 估时 |
|------|-----:|
| P0#0 loop_agent bypass 修复 | 0.5 Sprint |
| 借鉴路径 P0 (X1, X2, L3-3, L0-1) | ~4 Sprint |
| 借鉴路径 P1 (L0-2, L0-3, L1-4, L2-1) | ~5 Sprint |
| 借鉴路径 P2 (L3-1, L3-2, L2-5, L1-7, L3-6) | ~10 Sprint |
| Phase 6 ADR 全部代码化（ADR-0064/0065/0066 + ADR-0061） | ~8 Sprint |
| L4 应用层应用集成（L4-2 ~ L4-8） | ~6 Sprint |
| **合计** | **~33 Sprint (~66 周)** |

**注**: 按团队并行能力（W1/W2/W3 三轨），可压缩至 **~22 Sprint (~44 周)**。

---

## 十一、关键洞察总结

### 11.1 三层最关键缺口

| 层 | 最关键缺口 | 影响范围 |
|----|----------|---------|
| **L1 OS Services** | 事件发射契约（X1）+ 拦截钩子（X2/L1-2） | 全部借鉴路径生效的前提 |
| **L3 PDK Contract** | Plugin 命令/快捷键注册（L3-3） | 所有 slash 命令可插拔化 |
| **L4 Application** | loop_agent bypass bug（L4-1） | 隐藏缺陷，必须 P0 修复 |

### 11.2 v1.2 五层模型的完整性验证

v1.2 五层模型在概念层面**完整且清晰**（L0~L4 职责分明 + R1~R5 硬性约束），但在**实施层面严重不均衡**：
- **L0/L1 实施率 ~85-95%**（核心引擎稳定，缺关键钩子）
- **L2 实施率 ~75%**（8 个 plugin 中 5 个完整，3 个有缺口）
- **L3 实施率 ~65%**（基础接口有，命令/订阅/manifest 等关键扩展点缺）
- **L4 实施率 ~40%**（示例层 bug 暴露 + 6 项 Phase 6 ADR 零代码）

### 11.3 与 SOTA 的核心差距（按 v1.2 §10.3 优先级重新校准）

| 等级 | 缺口 | 现状 | 建议 |
|------|------|------|------|
| 🔴 **P0 关键** | loop_agent bypass bug | 已 ship 但 bypass | 必须 P0#0 修复 |
| 🔴 **P0 关键** | 事件发射契约（X1） | 8 个主题无 emit | 1 Sprint 补齐 |
| 🔴 **P0 关键** | ToolCoordinator Hook（X2） | 无拦截机制 | 1 Sprint middleware 化 |
| 🔴 **P0 关键** | Plugin 命令注册（L3-3） | 完全缺位 | 0.5 Sprint DECLARE_COMMAND 宏 |
| 🟠 **P1 强烈建议** | SessionManager JSONL（L0-1） | 线性 JSON 限制 | 1.5 Sprint |
| 🟠 **P1 强烈建议** | ContextCompactor（L0-3） | ADR-0007 🟡 Partial | 1 Sprint |
| 🟠 **P1 强烈建议** | Plugin Manifest 标准化（L3-1） | ADR-0052~0054 零代码 | 1 Sprint |
| 🟠 **P1 强烈建议** | Plugin Lifecycle（L3-2） | ADR-0057 零代码 | 2 Sprint |
| 🟠 **P1 强烈建议** | ToolMetadata JSON Schema（L3-5） | ADR-0058 零代码 | 0.5 Sprint |
| 🟠 **P1 强烈建议** | LLMProviderFactory 运行时注册（L1-5） | provider_agent 缺动态工具 | 0.5+1 Sprint |
| 🟠 **P1 强烈建议** | NodeExecutor 工具并行（L0-2） | DomainWorkerPool 未整合 | 1.5 Sprint |
| 🟢 **P2 可选** | WasmRuntime（L2-5） | ADR-0056 零代码 | 2 Sprint |
| 🟢 **P2 可选** | OpenTelemetry Exporter（L1-7） | ADR-0063 零代码 | 1.5 Sprint |
| 🟢 **P2 可选** | Marketplace（L3-6） | ADR-0062 零代码 | 2 Sprint |
| 🟢 **P2 可选** | Browser/Search Agent（L2-6） | 完全缺位 | 3 Sprint |

### 11.4 核心结论

1. **HydraForge 框架层（L0+L1）当前已有强健基底**，但**事件契约 + 拦截钩子**这两项"通用扩展原语"是横向缺口，阻塞所有 Plugin 表达力提升。
2. **PDK 层（L2+L3）当前处于"工具丰富但元数据贫乏"状态**——8 个 plugin 有工具，但 manifest / 命令 / 订阅 / Lifecycle 等元数据能力几乎为零。
3. **L4 Application 层隐藏缺陷严重**——pdk_chat_demo 的 loop_agent bypass 是设计与代码脱节的典型案例，**必须 P0 修复**才能让借鉴路径生效。
4. **Phase 6 ADR 系列（13 个零代码）**已构成显著的实施债务，建议分 2-3 个 Sprint 集中治理（先 ADR-0058/0052/0057/0063 这 4 个与现有 plugin 直接相关的）。
5. **总实施成本约 33 Sprint (~66 周)**，按三轨并行可压缩至 22 Sprint (~44 周)，是 Phase 6 服务化的重要前置。

---

## 十二、建议执行节奏

### Wave 1（P0#0 + 借鉴路径 P0 关键 4 项，~3 Sprint）
1. L4-1 loop_agent bypass 修复（0.5）
2. X1 事件发射契约（1.0）
3. L3-3 Plugin 命令注册宏（0.5）
4. X2 / L1-2 ToolCoordinator Hook 注入（1.0）

### Wave 2（借鉴路径 P0+P1 主体，~8 Sprint）
5. L0-1 SessionManager（1.5）
6. L1-3 Session 生命周期事件（0.5）
7. L2-2 session_agent 集成（1.0）
8. L0-3 ContextCompactor（1.0）
9. L4-5 /compact 命令（0.5）
10. L0-2 NodeExecutor 工具并行（1.5）
11. L1-5 LLMProviderFactory 运行时注册（0.5）
12. L2-1 provider 动态工具（1.0）
13. L4-3 流式渲染（1.0）

### Wave 3（L4 应用集成 + 借鉴路径 P2，~6 Sprint）
14. L4-2 异步 I/O（1.0）
15. L4-4 CLI 重写（0.3）
16. L4-6 /tree /fork TUI（1.0）
17. L4-7 /export HTML（1.0）
18. L4-8 g1_coding_assistant 完整化（1.0）
19. L1-4 中断/切换模型抽象（1.0）
20. L0-4 MarkdownParser YAML（0.5）

### Wave 4（Phase 6 ADR 代码化 + 高级能力，~16 Sprint）
21. L3-1 Plugin Manifest 标准化（1.0）
22. L2-4 + L3-5 Schema 校验（0.5+0.5）
23. L3-4 PDK 事件订阅 API（0.5）
24. L1-6 EventBus 序列化协议（1.0）
25. L2-3 budget_agent 成本闭环（1.0）
26. L1-7 OpenTelemetry Exporter（1.5）
27. L3-2 Plugin Lifecycle + hot-reload（2.0）
28. L2-5 WasmRuntime（2.0）
29. L3-6 PDK 安装包管理（2.0）
30. L2-6 Browser/Search Agent（3.0）
31. ADR-0061 AgentEvolutionEngine（3.0）
32. ADR-0064 Conformance Test Suite（1.0）
33. ADR-0065 Python PDK（2.0）
34. ADR-0066 SkillInterpreter V2（1.0）

---

## 十三、附录：ADR 状态交叉引用

> **单一事实源原则**（2026-07-31 治理修正）：ADR 状态的唯一权威来源是 `adr-implementation-status-gap-analysis.md` + 各 ADR 文件头。本节**不再维护状态副本**（初稿的状态表已删除，避免双份事实源漂移），仅保留本文档独有的"缺口 → ADR"映射指针：

| 缺失能力 | 关联 ADR |
|---------|---------|
| **X1 / L1-1** 事件发射契约 | ADR-0019 + 拟新增 ADR-0068 |
| **X2 / L1-2** ToolCoordinator Hook | ADR-0031 + 拟新增 ADR-0069 |
| **X3 / L3-3** 命令/快捷键注册 | 拟新增 ADR-0070 |
| **L0-1 / L1-3 / L2-2** SessionManager | ADR-0033 |
| **L0-2** NodeExecutor 工具并行 | ADR-0030 V2（DomainWorkerPool 已 ship 未整合） |
| **L0-3 / L4-5** ContextCompactor | ADR-0007 |
| **L1-4** 中断/切换模型抽象 | ADR-0042 |
| **L1-5 / L2-1** Provider 动态注册 | ADR-0005 |
| **L1-6 / L3-4** EventBus 序列化 + PDK 订阅 | ADR-0046 |
| **L1-7** OpenTelemetry Exporter | ADR-0063 |
| **L2-3** budget_agent 成本闭环 | ADR-0031 §决策 8 defer 项 |
| **L2-4 / L3-5** Schema 校验 | ADR-0058 |
| **L2-5** WasmRuntime | ADR-0056 + ADR-0065（协同） |
| **L3-1** Manifest 标准化 | ADR-0052 + 0053 + 0054（三件套） |
| **L3-2** Plugin Lifecycle | ADR-0057 |
| **L3-6** PDK 包管理 | ADR-0062 |
| **L4-1 / L4-8** loop_agent / g1 编排 | ADR-0051 |
| **ADR-0066 V2** SkillInterpreter 增强 | ADR-0066 |

**未在 ADR 列表中但本文档识别的 4 项**：
1. **X2 / L1-2** ToolCoordinator Hook 注入点（需新增 ADR-0069）
2. **L3-3** Plugin 命令/快捷键注册宏（需新增 ADR-0070）
3. **L4-1** loop_agent bypass bug（按 Phase 6 plan+commit 模式修复——Phase 6 已决议不用 OpenSpec change 仪式，修复后补 ADR 追溯）
4. **L0-4** MarkdownParser YAML 格式支持（轻量级，建议快速修复不另开 ADR）

---

## 附录 B：数据验证命令

本文档所有计数类数据必须可用以下命令复现（治理要求：不可复现的数字不得写入）：

| 数据 | 验证命令 |
|------|---------|
| ctest 总数/通过数 | `cd build && ctest -N`（总数）→ `ctest`（通过数） |
| ADR 状态计数 | `python3 tools/doc_metrics.py --adr` |
| emit 调用点 | `grep -rn "emit(" src examples/pdk_chat_demo --include="*.cpp" \| grep -v test` |
| ADR 子节点数 | `ls docs/adr/skill/ docs/adr/plugin/` |

---

**文档状态**: ✅ 完成 v1.2.1（2026-07-31 数据修正版）
**下次更新触发**: Wave 1 完成后（建议 2026 Q4）
**维护者**: Architecture Working Group