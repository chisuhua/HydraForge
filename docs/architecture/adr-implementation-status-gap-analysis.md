# ADR 实施状态差距分析

**生成日期**: 2026-07-30
**最后更新**: 2026-07-30 — 同步 2026-07-23~07-30 的 3 个 EventBus 链变更 (BusEvent/glob subscribe/CausalClock) + pkgm-temporal-agent 完整 ship
**分析范围**: 59 个 ADR（46 主 + 1 plugin + 12 skill 子项） vs 代码库实施状态
**数据源**: `docs/adr/` 目录、AGENTS.md Recent Changes、代码库架构扫描、code-review-graph、git log 2026-07-23~07-30

---

## 一、总体状态概览

| 状态 | 计数 | 占比 |
|------|:---:|:----:|
| ✅ Approved — 已批准且实施完成 | 31 | 52.5% |
| 🟡 Partial — 已批准但实施不完整 | 7 | 11.9% |
| 🔍 Proposed — 提议阶段，未批准 | 7 | 11.9% |
| ❌ Not Implemented — 未实施（含已归档） | 12 | 20.3% |
| ⛔ Superseded — 被替代 | 1 | 1.7% |
| 📦 Archived (已实施后归档) — 见脚注 | 1 | 1.7% |
| **总计** | **59** | **100%** |

> ① `docs/archive/adr/` 中 12 个已归档 ADR（0010-0018 全部未实施，0030 V1 被替代，0036 两文件未实施）已计入 `❌ Not Implemented`。② ADR-0032（CostCollector）已实施后归档，单列为 `📦 Archived`。③ ADR-0002（EventBus）未实施但文件仍在 `docs/adr/` 主目录，已计入 `❌ Not Implemented`。④ ADR-0037 于 2026-07-27 从 🔍 Proposed 提升为 🟡 Partial（CausalClock + emit auto-tick 已 ship）。

---

## 二、实施差距详细分析

### 2.1 🟡 Partial — 契约达成但实施不完整（7 个 ADR）

#### ADR-0007: 上下文压缩机制
- **ADR 状态**: 🟡 Partial
- **现状**: 快照机制已实现，但无 LLM 驱动的内容压缩
- **缺失**: LLM 摘要/压缩管道
- **影响**: 长对话场景下上下文窗口可能溢出
- **建议**: Phase 6 规划 LLM 压缩器，优先级 P2

#### ADR-0019: IInteractionBus 接口与 MVP
- **ADR 状态**: 🟡 Partial
- **现状**: IInteractionBus + InMemoryBus MPMC 已 ship (2026-06-24)。2026-07-26~27 新增 **BusEvent 公开契约** (Change A) + **subscribe_glob 通配符订阅** (Change B)。subscribe_glob 支持 `event_type::k*` 等通配符模式，通过 glob match 双路径分发 (O(1) 精确路径 + O(n) 通配符路径)。
- **缺失**: 正式的 `subscribe_topic(topic_pattern, callback)` 未实施 — 但 subscribe_glob 已部分覆盖该需求
- **影响**: 低 — subscribe_glob 已满足通配符订阅需求。topic-based 订阅在 ADR-0046 §2.1 中定义但当前范式下价值有限
- **建议**: 关闭 subscribe_topic gap，将 subscribe_glob 视为该需求的已实施版本

#### ADR-0030 V2: Phase 2 异步运行时
- **ADR 状态**: 🟡 Partial
- **已实施**: DomainWorkerPool ✅ / InMemoryBus MPMC ✅ / stream_to_bus bridge ✅ / Context fork/merge ✅ / Taskflow 集成 ✅
- **缺失**: FleetOrchestrator（16 路并行 LLM 调度）
- **Oracle 决议**: DEFER — 0 examples 使用并行 LLM，~1 周节省
- **影响**: 低（当前无多路 LLM 并发场景）
- **建议**: 维持 DEFER，在出现真实需求时重新评估

#### ADR-0031: IExecutionPolicy 执行策略与审批
- **ADR 状态**: 🟡 Partial
- **已实施**: C3 P1-P2 ✅ (IExecutionPolicy 5-method / 3 种策略 / PolicyFactory / ApprovalHandler / ModeSwitchDialog), C4 P3-P4 ✅ (ToolCoordinator / LayerProfile / Audit Log) — ship 日期见 AGENTS.md
- **缺失**: 4 项 defer 至 C6 — 超时机制接 IBudgetController、std::async 异步执行、成本预算集成、审批历史持久化
- **影响**: 中等 — 工具调用无超时保护，成本预算未完全闭环
- **建议**: Phase 6 重新评估 C6 优先级

#### ADR-0066: SkillInterpreter 模块架构
- **ADR 状态**: 🟡 Partial
- **已实施**: V1 — SkillInterpreter PIMPL (posix_spawn + seccomp + pipe IPC, 2026-07-22), `skill_child_main` 4 host function
- **缺失**: 未达到 ADR-0066 定义的完整架构（多 skill 协同、hot-reload、性能基准）
- **影响**: 低 — V1 已满足当前需要
- **建议**: Phase 6 规划 V2 增强

#### ADR-0037: 跨 Worker 事件因果序
- **ADR 状态**: 🟡 Partial (2026-07-27 从 🔍 Proposed 提升)
- **已实施**: CausalClock 类 (2026-07-27) — 单调递增 64 位时钟，`emit()` 时自动 tick + attach 到 BusEvent.causal_time。InMemoryBus 集成 emit auto-tick。3 个 Change 链 (A)BusEvent → (B)subscribe_glob → (C)CausalClock 全部 ship。
- **缺失**: 完整因果序系统 — 向量时钟、跨 worker 版本向量合并、分布式因果关系检测
- **影响**: 低 — CausalClock 基础已满足单进程 emit 顺序追踪需求
- **建议**: 维持单进程 CausalClock 范式，分布式的向量时钟在 Phase 6 出现跨进程需求时再评估

### 2.2 🔍 Proposed — 未批准但有自发性实施

#### ADR-0042: ILLMProvider 演进路径
- **ADR 状态**: 🔍 Proposed (主文档硬性 banner)
- **已实施**: C16 增量决议已记录 — Decorator 链 / Dual Consumer / available_models pure virtual / PluginLoader V2 / LlamaAdapter deprecated
- **问题**: ADR 整体未批准，但 5 项决策已落地为代码
- **建议**: 或批准 ADR-0042 承认现有事实，或将增量决议升级为正式 ADR

#### ADR-0045: 编排 PDK Plugin 规范
- **ADR 状态**: 🔍 Proposed
- **现状**: 实施率 ~20% — PDK 插件多数仅含基础入口骨架。`provider_agent`（390 行，4 个工具注册）和 `g1_coding_assistant`/`g3_knowledge_base`（各 200+ 行）有部分逻辑，但编排 DSL schema、插件间协调协议、执行审计均缺失
- **缺失**: 编排 DSL schema、插件间协调协议、执行审计
- **建议**: Phase 6 定义编排 MVP 范围后推进

#### ADR-0046: PDK 插件间通信协议
- **ADR 状态**: 🔍 Proposed
- **现状**: 实施率 ~35% — InMemoryBus 基础存在 + BusEvent 公开契约 (2026-07-26) + subscribe_glob (2026-07-27) 部分覆盖了协议需求
- **缺失**: 跨插件消息格式、序列化协议、版本协商
- **建议**: 与 ADR-0045 编排插件同步推进

#### 其余 Proposed ADR
- **ADR-0038** (动态配置): DELAY — 被 ADR-0041 PluginLoader lifecycle 覆盖
- **ADR-0039** (性能元数据): DELAY — JSON 工具未实现
- **ADR-0061-07~12** (P2 子项): v2 candidate — 均为实验性方向

### 2.3 ✅ Approved — 但代码对齐度需确认

以下 ADR 标记为 Approved，但存在代码-契约差距。Phase 6 系列（0052-0064）的"Approved"为架构评审批准，非实施完成批准，属正常的前期定义阶段。

| ADR | 差距 |
|-----|------|
| **ADR-0052~0064** (Phase 6 系列) | 架构评审 Approved 但多数**无代码实现** — 属正常的前期定义阶段，待 Phase 6 实施 |
| **ADR-0056** (Wasm 运行时) | 零代码 — 待 wamr 集成 |
| **ADR-0057** (Agent 生命周期) | 设计已确认但生命周期管理器未实现 |
| **ADR-0058** (Schema 校验) | 零代码 — ToolMetadata V2 已有关联但 schema 校验独立 |
| **ADR-0059** (跨进程协议) | 零代码 — 与 ADR-0060 6 种协作模式对齐但未实施 |
| **ADR-0062** (Marketplace) | 设计已确认但包格式/分发未实现 |
| **ADR-0063** (OpenTelemetry) | 零代码 — 分布式追踪未集成 |
| **ADR-0064** (Conformance Suite) | 零代码 — PDK 兼容性测试套件未建立 |
| **ADR-0065** (Python PDK) | 零代码 — Wasm 多语言支持未启动 |

### 2.4 已归档/未实施 ADR

#### ADR-0002: EventBus 有界队列
- **ADR 状态**: ❌ Not Implemented（文件在 `docs/adr/` 主目录，未归档）
- **现状**: V2 版设计文档已锁定，但 EventBus / DispatchMode 全系统从未实施
- **取代**: Phase 1 由 IInteractionBus + InMemoryBus（ADR-0019）承担事件通信职责
- **影响**: 无 — 功能已被 ADR-0019 覆盖

#### 已归档 ADR（编号 0010-0018）
所有 9 个 Phase 0 认知/记忆 ADR 于 2026-06-09 整批归档，全部未实施。这些 ADR 描述了知识图谱、向量记忆、IPER 推理等认知增强功能，因 Phase 0 MVP 范围缩小而被推迟。

---

## 三、代码-架构对齐量化

### 3.1 核心模块达成率

| 模块 | ADR 对应 | 实施率 | 备注 |
|------|---------|:---:|------|
| DSLEngine 核心 | 0019, 0033, 0008 | **100%** | PIMPL-lite 解耦完成，仅 1 types 例外 |
| 抽象接口层 | 0019 §1.4 | **100%** | 7 接口全部实现 |
| 认知编排 | 0020 | **95%** | FleetOrchestrator DEFER |
| PDK 系统 | 0021, 0034 | **100%** | 3 Agent 循环 + ToolRegistry V2 |
| Skill 隔离 | 0055, 0066 | **80%** | V1 done, V2 deferred |
| LLM Providers | 0001, 0005, 0042 | **100%** | 3 Decorators + CloudLLMAdapter |
| 异步运行时 | 0030 V2 | **85%** | Fleet DEFER |
| 策略/审批 | 0031 | **85%** | 4 项 defer 至 C6 |
| 插件加载 | 0022, 0041 | **100%** | PluginLoader V2 shipped |
| 调度/执行 | 0033, 0019 | **100%** | TopoScheduler + NodeExecutor |
| **EventBus 基础设施** | 0019, 0002, 0037, 0046 | **90%** | 2026-07-26~27: BusEvent 公开契约 + subscribe_glob + CausalClock 全部 ship |
| **Temporal Agent** | — | **100%** | 2026-07-28: pkgm-temporal-agent Phase 1+2 完整 ship (41/41 tasks, 10/10 ctest)

### 3.2 测试覆盖

| 指标 | 数值 |
|------|:---:|
| 测试文件数 | 100+ (`.cpp` 测试文件) |
| 最新 ctest | 93/93 PASS (2026-07-30) |
| main 分支状态 | 93/93 零回归，3 个 EventBus 变更 + pkgm-temporal-agent 全部 ship

---

## 四、关键发现与建议

### 🔴 高优先级

1. **ADR-0042 状态不匹配**: 5 项 C16 增量决议已落地为代码，但 ADR 状态仍为 🔍 Proposed。建议正式批准或创建独立 ADR 记录。

2. **PDK 插件 (8 个) 实施进度不透明**: `fs_tools` (203 行), `shell_tools`, `provider_agent` (390 行, 4 工具已注册), `loop_agent` (208 行), `session_agent`, `budget_agent`, `g1_coding_assistant` (242 行), `g3_knowledge_base` (345 行) 均有一定代码量，但多数缺少实际编排/业务逻辑。`provider_agent` 已注册 4 个工具，`g1_coding_assistant` 和 `g3_knowledge_base` 有状态管理和查询逻辑。需要明确每个插件的完成度评估标准和实施计划。

### 🟡 中优先级

3. **ADR-0031 4 项 defer**: 工具调用无超时保护的缺口在 Phase 6 应重新评估优先级。

4. **ADR-0007 LLM 压缩**: 长期对话场景下可能成为瓶颈。

5. **ADR-0019 subscribe_topic**: subscribe_glob 已 ship (2026-07-27)，覆盖了通配符订阅需求。建议正式关闭此 gap，除非未来出现明确的 topic-based 路由需求。

### 🟢 低优先级

6. **Phase 6 ADR (0052-0064)**: 13 个 ADR 架构评审 Approved 但多数无代码 — 属前期定义，按 Phase 6 节奏推进即可。

7. **已归档 ADR (0010-0018)**: 认知增强功能推迟，Phase 6 可能重新评估但非当前焦点。

8. **ADR-0037 CausalClock**: 基础 ship 完成，分布式向量时钟留待跨进程需求出现时评估。

---

## 五、演进路径建议

```
Phase 5 (当前) ──→ Phase 6
  ✅ 核心已稳定          🔴 ADR-0042 状态对齐
  🟡 7 Partial 跟踪      🟡 ADR-0031 C6 收尾
  🔍 7 Proposed 评估     🔍 ADR-0045/0046 编排协议
  ✅ EventBus 链已 ship   🟢 Phase 6 ADR 代码实施
  ✅ Temporal Agent ship  🟢 ADR-0066 V2 增强
                           🟢 ADR-0037 分布式向量时钟 (跨进程时)
```

---

## 附录：完整 ADR 状态清单

### ✅ Approved（31 个）

| ADR | 标题 | 批准日期 | 备注 |
|-----|------|---------|------|
| 0001 | ILLMProvider 流式接口 | 2026-05-28 | C16 增补 Decorator/Dual Consumer |
| 0003 | DSLEngine 线程安全 | 2026-05-12 | 代码已落地 |
| 0004 | ToolRegistry 安全模型 V2 | 2026-07-02 | C6 ship |
| 0005 | LLM 后端配置工厂 | 2026-05-12 | C16 增补修订 |
| 0008 | 结构化 Context | 2026-06-12 | Sprint 20 桥接期 ship |
| 0009 | DSL 标准库规划 | 2026-05-12 | |
| 0020 | 多智能体线程模型 | 2026-06-24 | Sprint 5 ship |
| 0021 | PDK 设计 | 2026-06-24 | Sprint 20 增量 |
| 0022 | 插件加载机制 | 2026-06-24 | Sprint 5 ship |
| 0023 | ToolResult 标准化 | 2026-06-24 | Sprint 5 ship |
| 0033 | Session Hierarchy | 2026-07-02 | C5 ship |
| 0034 | IModelRouter 模型路由 | 2026-07-02 | C7 ship |
| 0035 | 推理引擎 Plugin 规范 | 2026-07-10 | C14 ship |
| 0040 | 推理引擎构建策略 | 2026-07-10 | C14 ship |
| 0041 | PluginLoader 生命周期 | 2026-07-10 | C16 ship |
| 0043 | PDK 工具命名约定 | 2026-07-10 | C13/C14 D3 |
| 0044 | 推理引擎安全模型 | 2026-07-10 | C14 ship |
| 0050 | Phase 6 战略评估 | 2026-07-23 | 转向 PDK 生产化 |
| 0051 | Phase 6 Composition Spike | 2026-07-15 | W3 ship gate |
| 0052 | Agent Plugin Manifest | 2026-07-16 | 架构评审确认 |
| 0053 | AgentDescriptor 接口 | 2026-07-16 | 架构评审确认 |
| 0054 | Capability Discovery | 2026-07-16 | 架构评审确认 |
| 0055 | SKILL.md 执行隔离 | 2026-07-22 | 实施完成 |
| 0056 | Wasm Agent 运行时 | 2026-07-16 | 无代码 |
| 0057 | Agent 生命周期 | 2026-07-16 | 无代码 |
| 0058 | Schema 强制校验 | 2026-07-16 | 无代码 |
| 0059 | 跨进程协议 | 2026-07-16 | 无代码 |
| 0060 | Agent 组合协议 | 2026-07-16 | 6 种协作模式 |
| 0061 | Agent 进化与固化 | 2026-07-16 | 父 ADR |
| 0062 | Agent Marketplace | 2026-07-16 | 无代码 |
| 0063 | OpenTelemetry 追踪 | 2026-07-16 | 无代码 |
| 0064 | Conformance Test Suite | 2026-07-16 | 无代码 |
| 0067 | 分层插件架构拆分 | 2026-07-23 | 追溯性正式化 |

> **注**: ADR-0032（CostCollector, 2026-06-30）已实施后归档至 `docs/archive/adr/`，不在此 Approved 列表中。详见 §二.2.4。

### ADR-0061 子项

| 子项 | 名称 | 状态 | 备注 |
|------|------|------|------|
| 0061-01 | SKILL.md 标准对齐 | ✅ Approved | P0 |
| 0061-02 | 行为回归套件 | ✅ Approved | P0, 无代码 |
| 0061-03 | SkillCompiler 实施 | ✅ Approved | P0, 无代码 |
| 0061-04 | SLM 路由优先 | ✅ Approved | P1 |
| 0061-05 | wasi-sdk 集成 | ✅ Approved | P1, 无代码 |
| 0061-06 | Trajectory IR 升级 | ✅ Approved | P1, 无代码 |
| 0061-07 | PASTE 推测执行 | 🔍 Proposed | P2 |
| 0061-08 | MCTS 工作流搜索 | 🔍 Proposed | P2 |
| 0061-09 | GEPA 反思循环 | 🔍 Proposed | P2 |
| 0061-10 | Config 结构检查 | 🔍 Proposed | P2 |
| 0061-11 | DSL→Wasm 编译器 | 🔍 Proposed | P2 |
| 0061-12 | WebLLM 集成 | 🔍 Proposed | P2 |

### 🟡 Partial（7 个）

| ADR | 标题 | 缺失项 |
|-----|------|--------|
| 0007 | 上下文压缩 | 无 LLM 压缩 |
| 0019 | IInteractionBus | subscribe_glob 已 ship，subscribe_topic 正式关闭 gap |
| 0030 V2 | 异步运行时 | FleetOrchestrator DEFER |
| 0031 | 执行策略 | 4 项 defer 至 C6 |
| 0037 | 因果序 | CausalClock 基础 ship，分布式向量时钟 defer |
| 0066 | SkillInterpreter 架构 | V1 done, V2 deferred |

### 🔍 Proposed（7 个）

| ADR | 标题 | 备注 |
|-----|------|------|
| 0038 | 动态配置接口 | BatchingQueue 延迟 |
| 0039 | 性能元数据契约 | JSON 工具未实现 |
| 0042 | ILLMProvider 演进路径 | C16 决议已记录 |
| 0045 | 编排 Plugin 规范 | 实施率 ~20% |
| 0046 | 插件间通信协议 | 实施率 ~35% (BusEvent + subscribe_glob 已覆盖部分需求) |
| 0061-07~12 | P2 子项 (6 个) | v2 candidate |

### ⛔ Superseded（1 个）

| ADR | 标题 | 被替代为 |
|-----|------|---------|
| 0006 | HarnessEngine 线程模型 | ADR-0020 |

### ❌ Not Implemented（12 个）

| ADR | 标题 | 位置 | 备注 |
|-----|------|------|------|
| 0002 | EventBus 有界队列 | `docs/adr/` | V2 设计锁定，功能由 ADR-0019 覆盖 |
| 0010-0018 | Phase 0 认知/记忆 (9 个) | `docs/archive/adr/` | 2026-06-09 整批归档 |
| 0030 V1 | 异步运行时 V1 | `docs/archive/adr/` | 被 V2 替代 |
| 0036 | 三层服务协议 / 混合内核 (2 文件) | `docs/archive/adr/` | 未实施 |