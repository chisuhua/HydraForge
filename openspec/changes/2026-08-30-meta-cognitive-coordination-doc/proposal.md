# Cognitive-Cognitive 协调模式目录（§十八 OpenSpec change）

> **Oracle 判定**: 🟢 Go (2026-08-30, sessions ses_faf7caad0ffeOiLqgZMofEuPpA + ses_faf7e5317ffe2ha7qD6m09TF5x + ses_faf7b0a43ffeHenVBGL1Y9O3cu + ses_faf77e870ffeqfqNoyZrwpqD4O) — Path 1 Conditional-Go 3 强制条件全部满足(stream-pipeline 标 V2 占位 / debate-round 标组合配方 / example mock-mode 可跑),与 ADR-0085 §决策 5 Meta-Agent V1 defer 一致;纯文档沉淀零代码改动,docs/README.md adr-0061-08 stale 已 commit `259b9d1` 修复
>
> **状态**: 🔍 Proposed (2026-08-30, Oracle 综合评审后续 Step 1)
> **关联文档**: `docs/architecture/agent-orchestration-architecture-2026-08.md` (v1.4 含 §十八)
> **关联 ADR**:
> - ADR-0060 (Agent Composition, ✅ Approved) — `IAgentComposition` 4 模式 call/call_async/delegate/stream
> - ADR-0021 (PDK Design, ✅ Approved) — 3 Loop class + LoopDispatcher
> - ADR-0085 (Cross-Cutting Pattern PDK, ✅ Approved) — §决策 5 V1 不实施 Meta-Agent 自管理
> - ADR-0082 (Agent First-Class Registry, ✅ Approved) — IAgent V1 骨架 ship, AgentWorker 推迟 Sprint 24+
> **Oracle session**: `ses_faf7caad0ffeOiLqgZMofEuPpA` (Path 1) + `ses_faf7e5317ffe2ha7qD6m09TF5x` (Path 2) + `ses_faf7b0a43ffeHenVBGL1Y9O3cu` (Path 3) + `ses_faf77e870ffeqfqNoyZrwpqD4O` (Path 4)
> **最后更新**: 2026-08-30

## Why

### 不变量链 + 缺口定位

**已有能力 (4 Oracle 共识, ✅ Shipped)**:
- `IAgentComposition` 4 模式接口（ADR-0060, `include/agenticdsl/contract/iagent_composition.h`）— call/call_async/delegate 三个 ✅ Shipped, stream V2 占位
- 3 PDK Loop class（ADR-0021）— `ReactLoop` / `PlanExecuteLoop` / `ForkJoinLoop`
- `IAgentRegistry` + `IAgent` 骨架（ADR-0082 V1, AgentWorker 推迟 Sprint 24+）
- `IEvaluator` V2（ADR-0083 ✅ Shipped 2026-08-26）— 含 BehavioralEquivalence + Composite
- `GEPALoop` V1（ADR-0061-09 文件状态滞后, 实际 T19 已 ship 2026-08-27）— reflect_and_commit
- `MCTSWorkflowSearch` V1（T20 ship 2026-08-28）— 5 轴模板 + UCB1
- `IInteractionBus` + 27+ 主题（ADR-0068）— 全部 cognitive task 可观测

**真实缺口（Oracle Path 1 评审结论）**:
- `agent-orchestration-architecture-2026-08.md` v1.2 全文中, `IAgentComposition` 4 模式**散落 3 处**无命名目录:
  - §2.5 Agent Composition 1 行简述
  - §13.2 "IAgentComposition" 矩阵 1 行 ("ReactLoop → 委派给 ForkJoinLoop")
  - §十六.2 编排层角色分工 间接涉及
- **没有任何一处把"认知 agent 协调认知 agent"作为独立视角命名成模式** → 用户对"Meta Cognitive Agent"的需求本质上是**认知缺口**而非**能力缺口**（4 Oracle 全部共识）
- ADR-0085 §决策 5 已正式决议"V1 不实施 Meta-Agent 自管理", 文档化消化需求是合规路径

**审计依据**:
- Oracle Path 1 评审结论: "Conditional-Go, 0.5 sprint, 3 强制标注条件"
- Oracle Path 2 评审结论: "No-Go 直接扩 IAgent (2.3); 与 §七 P1 + ADR-0085 §决策 5 不冲突, 仅做文档即合规"
- Oracle Path 3 评审结论: "推荐 3.2 MCTS Axis6, 文档化当前原语命名更优先"
- Oracle Path 4 评审结论: "No-Go 完整平台 (4.3); 推荐 4.2 = 路径 1 + 信用分配 ADR 草稿"

**前置依赖**（全部已满足）:
- ✅ `include/agenticdsl/contract/iagent_composition.h` 4 模式 ship (ADR-0060)
- ✅ `include/agenticdsl/pdk/agent_loops/{react_loop,plan_execute_loop,fork_join_loop}.h` ship (Sprint 20)
- ✅ `include/agenticdsl/cognitive/{cognitive_worker,gepa_loop,mcts_workflow_search}.h` ship (T19/T20)
- ✅ `include/agenticdsl/contract/ievaluator.h` V2 ship (ADR-0083)
- ✅ 文档 v1.3 (ADR-0050 Phase 6 战略对齐 + ADR-0077 descoped 标注) 已 ship
- ✅ AgentWorker (ADR-0082 V2) 推迟 Sprint 24+ 既定, 不阻塞本 change

## What Changes

### 1. 文档沉淀（§十八新增, 已在 v1.4 ship, 本 change 锁定状态）

#### 1.1 §十八 Cognitive-Cognitive 协调模式目录（5 模式）

| 模式 | 性质 | 原语锚点 | 应用代号 | 落地状态 |
|------|------|----------|----------|----------|
| **sync-delegate** | ✅ 原语 | `IAgentComposition::delegate()` (`iagent_composition.h:59` → `TaskHandle`) | A3 / A6 / B1 / B5 | ✅ Shipped |
| **fan-out** | ✅ 原语 | `IAgentComposition::call_async()` + `ForkJoinLoop::run(branches, ctx, token)` | A4 / B2 / C1 | ✅ Shipped |
| **hierarchical-plan** | ✅ 原语 | `PlanExecuteLoop::plan_phase()` + `delegate()` 嵌套 | A3 / B2 | ✅ Shipped |
| **debate-round** | 🟡 组合配方 | `call_async()` + `IEvaluator` + `GEPALoop.reflect_and_commit()` 三者组合 | B7 / C2 | 🟡 配方 |
| **stream-pipeline** | 🔴 V2 占位 | `IAgentComposition::stream()` (`iagent_composition.h:67` 直接 `throw std::logic_error`) | B4 / C1 | 🔴 V2 docs-only |

#### 1.2 决策树新增分支（§四）

```
├─ 需要认知 agent 互相协调？ ──是──▶ 选 §十八 cognitive-cognitive 协调模式
                                 （sync-delegate / fan-out / hierarchical-plan /
                                  debate-round 配方 / stream-pipeline V2 占位）
```

#### 1.3 5 模式演示文档（§18.3-18.7）

每模式 1 段 C++ 演示代码, 注明状态:
- ✅ sync-delegate / fan-out / hierarchical-plan — 演示代码基于 ship 原语
- 🟡 debate-round — 演示代码标"3 组件组合, 各自 ship"
- 🔴 stream-pipeline — 演示代码标"伪代码, V2 实装前降级为 call_async + 轮询"

#### 1.4 不变量（§18.8 适用 5 模式全部）

- 不新增 contract 类
- per-worker 隔离不破坏（遵守 ADR-0020/ADR-0030 V2）
- fail-safe 默认
- 可观测性（5 模式全部经 IInteractionBus emit `agent.task.*`）

#### 1.5 17 类应用映射（§18.9）

每 cap-map §三 应用映射 1 个推荐模式 + 备选, 完整覆盖 A1-C4 全部 17 类。

#### 1.6 升级触发条件（§18.10）

启动 ADR-0085 V2 MetaAgent 评审 + 路径 3.2 MCTS Axis6 立项的 4 个硬条件:
- `IAgentComposition::stream()` 由 Phase 2 实装
- ≥2 个真实 `IAgent` 实现类
- S4 promotion criteria 全部满足（含 ADR-0086 信用分配 ship）
- 用户需求被识别为"运行时动态选择协调模式"

### 2. 文档不更新项（明确 out of scope）

- **不动 IAgent 接口**（Oracle Path 2 No-Go 2.3）
- **不新建 CognitiveOrchestrator 类**（Oracle Path 3 No-Go 3.3, 命名冲突 `icognitive_orchestrator.h` 已存在）
- **不实施 MetaAgent 自管理**（ADR-0085 §决策 5 已决议 V1 不实施）
- **不动 gRPC data plane**（ADR-0077 Wave 4 descoped docs-only, Oracle Path 4 No-Go 4.3）
- **不动 Phase 6 服务化轨道**（ADR-0050 Candidate B 🔒 冻结）

### 3. ADR 状态

- ADR-0060 / 0021 / 0085 / 0082 / 0083 / 0084 / 0080 / 0061-13 / 0061-09 / 0061-08 / 0068 — **全部已 Approved 或 Shipped, 本 change 不修改任何 ADR**
- ADR-0086 credit-assignment-contract.md — **不在本 change 范围内**（属 Step 2, 后续 OpenSpec change）

## 不变量

- **零代码改动**: 仅文档修改 + spec delta（编排 doc §十八新增 + §四决策树 + §十七关联文档 + §九验证命令）
- **零 ADR 修改**: 不触动任何 Approved/Proposed ADR 状态
- **零 OpenSpec active change 阻塞**: 不与 from-roadmap-phase-6c-* 冲突（Step 1 文档工作 vs Phase 6c 实施评估不同维度）
- **零 ctest 影响**: 不触碰 include/src/tests/examples 任何代码

## 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| §十八内容随时间漂移（5 模式落地状态变化未及时更新） | 中 | §九验证命令 #18-#22 强制每 Sprint 收官 grep 校验；Update-Trigger 注明 ADR-0060 stream Phase 2 实装 / AgentWorker ship / IEvaluator V3 时必更新 |
| 用户对"Meta Cognitive Agent"需求实际是路径 3 运行时决策而非静态选型 | 低 | §18.10 升级触发条件明示 + 评审 Oracle Path 3 建议"先 MCTS Axis6, 再路径 3.3 重定位" |
| §十八 debate-round / stream-pipeline 被读者误读为可用能力 | 中 | §18 顶部 3 个强制标注 + §18.5/§18.6/§18.7 段头明示"🟡 配方" / "🔴 V2 占位" + §九验证命令 #19/#20 grep 强制标注 |
| 文档示例代码（§18.3-18.7）腐化（API 演进后失同步） | 低 | 所有锚点含文件:行号（`iagent_composition.h:59`, `fork_join_loop.h:138` 等），Sprint 收官验证命令 #18 一并检测锚点有效性 |
