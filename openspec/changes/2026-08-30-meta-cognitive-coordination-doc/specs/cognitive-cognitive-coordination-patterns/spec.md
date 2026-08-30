# Cognitive-Cognitive 协调模式目录 Specification

## Purpose

> **本 spec 为 ADR/已 ship 原语的**目录化命名**，不新增 contract 类**。5 模式全部基于:
> - `IAgentComposition` 4 模式（ADR-0060 ✅ Approved, `iagent_composition.h`）
> - 3 PDK Loop class（ADR-0021 ✅ Approved, Sprint 20 ship）
> - `IEvaluator` V2（ADR-0083 ✅ Approved, `ievaluator.h`）
> - `GEPALoop` V1（T19 ship 2026-08-27, `gepa_loop.h`）
>
> **本 spec 适用**: HydraForge 编排架构 doc §十八 + 决策树 §四 + 关联文档 §十七 的内容契约
>
> **强制标注**: stream-pipeline 标 🔴 V2 占位（`iagent_composition.h:67` 直接 `throw std::logic_error`）; debate-round 标 🟡 组合配方（3 已 ship 原语组合, 非单一 contract）

## ADDED Requirements

### Requirement: sync-delegate 模式可用性

`sync-delegate` 模式 MUST 通过 `IAgentComposition::delegate()` 实现, MUST 返回 `TaskHandle` (而非 `AgentResult<std::string>`), MUST 支持 priority 参数（默认 "normal"）。

#### Scenario: 模式名出现在编排 doc §十八

- **WHEN** 静态检查 `grep -c "\*\*sync-delegate\*\*" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 至少 2 行匹配（§十八目录表 + §四决策树分支）

#### Scenario: 原语锚点正确

- **WHEN** 静态检查 `grep -A1 "sync-delegate" docs/architecture/agent-orchestration-architecture-2026-08.md | grep "iagent_composition.h:59"`
- **THEN** 1 行匹配（含 `delegate()` 文件:行号锚点）

#### Scenario: 应用代号覆盖

- **WHEN** 静态检查 §十八 17 类应用映射表 `grep "A3\|A6\|B1\|B5" docs/architecture/agent-orchestration-architecture-2026-08.md | grep "sync-delegate"`
- **THEN** ≥3 行匹配（A3 + A6 + B1 + B5 中至少 3 个 sync-delegate 关联）

#### Scenario: 落地状态 ✅ Shipped

- **WHEN** 静态检查 `grep "sync-delegate.*✅ Shipped" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 1 行匹配

### Requirement: fan-out 模式可用性

`fan-out` 模式 MUST 通过 `IAgentComposition::call_async()` 并发 + `ForkJoinLoop::run(branches, ctx, token)` 实现, MUST 返回 `std::future<AgentResult<std::string>>`, MUST 支持并行聚合。

#### Scenario: 模式名 + 锚点正确

- **WHEN** 静态检查 `grep -c "\*\*fan-out\*\*" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 至少 2 行匹配

- **WHEN** 静态检查 `grep "call_async.*iagent_composition.h:53\|fork_join_loop.h:138" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 锚点行正确

#### Scenario: 应用代号覆盖

- **WHEN** 静态检查 `grep "fan-out.*A4\|fan-out.*B2\|fan-out.*C1" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** A4 / B2 / C1 中至少 3 个 fan-out 关联行

#### Scenario: ForkJoinLoop 集成

- **WHEN** 静态检查 `grep -A2 "fan-out.*ForkJoinLoop\|ForkJoinLoop.*fan-out" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** fan-out 与 ForkJoinLoop 关联说明存在

### Requirement: hierarchical-plan 模式可用性

`hierarchical-plan` 模式 MUST 通过 `PlanExecuteLoop::plan_phase()` + 嵌套 `delegate()` 实现, MUST 支持 plan→execute→verify 3 阶段。

#### Scenario: 模式名 + 3 阶段锚点

- **WHEN** 静态检查 `grep -c "\*\*hierarchical-plan\*\*" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 至少 2 行匹配

- **WHEN** 静态检查 `grep "plan_phase\|plan_execute_loop.h:107" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** PlanExecuteLoop 锚点正确

#### Scenario: 应用映射 A3 + B2

- **WHEN** 静态检查 §十八 17 类映射 `grep "hierarchical-plan.*A3\|hierarchical-plan.*B2" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

### Requirement: debate-round 模式强制标注为组合配方

`debate-round` 模式 MUST 标注为 🟡 组合配方, MUST NOT 被呈现为单一 contract 或单一接口。组合由 3 个已 ship 原语构成: `call_async()` + `IEvaluator::evaluate()` + `GEPALoop::reflect_and_commit()`。

#### Scenario: 强制 🟡 标注

- **WHEN** 静态检查 `grep "debate-round.*🟡\|debate-round.*组合配方" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: 3 组件组合显式

- **WHEN** 静态检查 §十八 §18.6 段 `grep "call_async.*IEvaluator.*GEPALoop\|GEPALoop.reflect" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 3 组件组合明确写出

#### Scenario: 不得存在 IDebateRound 误用

- **WHEN** 静态检查 `grep "IDebateRound\|class DebateRound" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 0 命中（避免误导读者认为存在该 contract）

#### Scenario: 应用映射 B7 + C2

- **WHEN** 静态检查 `grep "debate-round.*B7\|debate-round.*C2" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配（B7 自进化 GEPA + C2 自进化 agent）

### Requirement: stream-pipeline 模式强制标注为 V2 占位

`stream-pipeline` 模式 MUST 标注为 🔴 V2 占位, MUST 显式引用 `iagent_composition.h:67` 代码 `throw std::logic_error("Phase 2 - stream not yet implemented")`, MUST NOT 被呈现为可用能力。

#### Scenario: 强制 🔴 标注

- **WHEN** 静态检查 `grep "stream-pipeline.*🔴\|stream-pipeline.*V2 占位" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: iagent_composition.h:67 代码引用

- **WHEN** 静态检查 `grep "iagent_composition.h:67\|throw std::logic_error" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 至少 1 行匹配 stream 上下文

#### Scenario: 演示代码标"伪代码"

- **WHEN** 静态检查 §十八 §18.7 段 `grep "伪代码\|V2 实装前降级" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: 应用映射 B4 + C1

- **WHEN** 静态检查 `grep "stream-pipeline.*B4\|stream-pipeline.*C1" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配（B4 Streaming Agent + C1 跨主机联邦）

### Requirement: §四决策树分支完整性

`agent-orchestration-architecture-2026-08.md` §四 编排决策树 MUST 包含新分支"需要认知 agent 互相协调？", MUST 引用 §十八 模式目录。

#### Scenario: 决策树分支存在

- **WHEN** 静态检查 `grep "需要认知 agent 互相协调" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 1 行匹配

#### Scenario: 引用 §十八

- **WHEN** 静态检查 `grep "§十八" docs/architecture/agent-orchestration-architecture-2026-08.md | wc -l`
- **THEN** ≥2 行（决策树分支 + 关联文档交叉引用）

#### Scenario: 5 模式全部出现在决策树分支

- **WHEN** 静态检查 §四决策树 `grep -E "sync-delegate|fan-out|hierarchical-plan|debate-round|stream-pipeline" docs/architecture/agent-orchestration-architecture-2026-08.md | head`
- **THEN** 5 个模式名全部出现至少 1 次

### Requirement: 升级触发条件文档化

§十八 MUST 包含 4 个升级触发条件, 用于启动 ADR-0085 V2 MetaAgent 评审 + 路径 3.2 MCTS Axis6 立项, 4 条件全部"且"语义。

#### Scenario: 4 触发条件列出

- **WHEN** 静态检查 §十八 §18.10 段 `grep "升级触发条件\|触发.*条件" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 1 行匹配（§18.10 标题）

#### Scenario: 触发条件 1 — stream Phase 2 实装

- **WHEN** 静态检查 `grep "stream.*Phase 2\|IAgentComposition::stream" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: 触发条件 2 — ≥2 真实 IAgent 实现类

- **WHEN** 静态检查 `grep "≥2.*IAgent\|2 个真实.*IAgent\|SimpleAgent" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: 触发条件 3 — S4 promotion criteria

- **WHEN** 静态检查 `grep "S4.*promotion\|promotion.*criteria" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: 触发条件 4 — 运行时决策需求

- **WHEN** 静态检查 `grep "运行时动态\|运行时决策" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

## MODIFIED Requirements

### Requirement: 不新增 contract 类（继承 ADR-0019 §1.4）

本 change MUST NOT 新增任何 contract 头文件（`include/agenticdsl/contract/` 0 diff）, MUST NOT 触动 `include/agenticdsl/cognitive/` 现有 `icognitive_orchestrator.h` / `simple_orchestrator.h` 文件（避免命名冲突）。

#### Scenario: contract 头文件零修改

- **WHEN** 运行 `git diff HEAD --stat -- 'include/agenticdsl/contract/'`
- **THEN** 0 行变更

#### Scenario: cognitive/ 现有文件零修改

- **WHEN** 运行 `git diff HEAD --stat -- 'include/agenticdsl/cognitive/'`
- **THEN** 0 行变更

#### Scenario: icognitive_orchestrator.h 未触动

- **WHEN** 运行 `git diff HEAD -- include/agenticdsl/cognitive/icognitive_orchestrator.h`
- **THEN** 0 行变更（避免与 §十八新组件命名冲突）

### Requirement: per-worker 隔离不破坏（继承 ADR-0020 + ADR-0030 V2）

本 change MUST 遵守 ADR-0020 "Agent 状态不跨 worker 共享"决议 + ADR-0030 V2 §风险 "Context fork/merge 深拷贝解决共享可变 Context 风险"。

#### Scenario: 不得推荐 shared_ptr<LayeredContext> 共享

- **WHEN** 静态检查 §十八 §18.8 不变量段 `grep "shared_ptr.*LayeredContext\|共享.*LayeredContext" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 0 命中（仅可推荐"fork/merge + bus 事件"模式）

#### Scenario: ADR-0020 引用存在

- **WHEN** 静态检查 `grep "ADR-0020\|per-worker 隔离" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

## CROSS-REFERENCED Requirements

### Requirement: ADR-0085 §决策 5 引用一致性

本 change MUST 引用 ADR-0085 §决策 5 "V1 不实施 Meta-Agent 自管理", MUST 明确本目录 §十八 是 Meta-Agent 的"需求消化层"而非组件前置设计。

#### Scenario: ADR-0085 §决策 5 引用

- **WHEN** 静态检查 §十八顶部 `grep "ADR-0085.*决策 5\|V1 不实施 Meta-Agent" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: 不得推荐新建 MetaAgent

- **WHEN** 静态检查 `grep "新建.*MetaAgent\|CognitiveMetaAgent" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** 0 命中（避免违背 §决策 5）

### Requirement: Phase 6 战略对齐（继承 ADR-0050）

本 change MUST 标注 ADR-0050 Candidate B 服务化 🔒 冻结, MUST 标注 ADR-0077 Wave 4 descoped, MUST 不将自进化路径归类为服务化轨道。

#### Scenario: ADR-0050 🔒 冻结引用

- **WHEN** 静态检查 `grep "🔒 冻结" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: ADR-0077 Wave 4 descoped 引用

- **WHEN** 静态检查 `grep "Wave 4 descoped" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: 信用分配 ADR-0086 待立项引用

- **WHEN** 静态检查 `grep "adr-0086-credit" docs/architecture/agent-orchestration-architecture-2026-08.md`
- **THEN** ≥1 行匹配

#### Scenario: 不得使用过期文件名

- **WHEN** 静态检查 `grep "adr-0085-credit-assignment" docs/architecture/agent-orchestration-architecture-2026-08.md | grep -v "过期"`
- **THEN** 0 命中（仅允许在"取代过期文件名"元描述中出现）
