# RSI 三算子（Data/Harness/Model）与 HydraForge 实施路径架构 (2026-09)

**生成日期**: 2026-09-23（**v1.0 升档** — 由 research 类调研 → architecture 类 SoT；详见文末 §十 升档说明）
**最后验证**: 2026-09-23（v1.0，**Harness-RSI 全部 ✅ SHIPPED + GO 2026-09-21** + **Pre-Wave3 4-Gate 全部 ✅ ship 2026-09-22** + **Wave 3 Phase 1 finetune-base-model pilot ✅ ship 2026-09-23** + **ADR-0078 ✅ Approved**；完整 ship 实证矩阵见 §十.2）
**作者**: Architecture Working Group
**状态**: ✅ **Approved (v1.0, 2026-09-23 升档)** — **RSI 维度 Source of Truth**（与 [`./self-evolution-architecture-2026-08.md`](./self-evolution-architecture-2026-08.md) v1.5 + [`./harness-architecture-2026-09.md`](./harness-architecture-2026-09.md) v1.0 共同构成 Self-Evolution / Harness / RSI 三方架构一致基线）

> **三方配套关系**:
> - **自进化架构** ([`./self-evolution-architecture-2026-08.md`](./self-evolution-architecture-2026-08.md) v1.5) — 整体 9 段闭环 + 4 支撑平面 + 3 阶段路线
> - **Harness 架构** ([`./harness-architecture-2026-09.md`](./harness-architecture-2026-09.md) v1.0) — Harness 作为自进化核心变异对象, 数据模型 + 装配 + 变更能力 + 守门 + 持久化的纵深内容
> - **RSI 架构** (本文档) — Harness/Data/Model 三算子叠加策略 + 5 阶段 ship 实施路径 + 2 阶段未 ship 候选立项
>
> 三方无冲突, 互补. 任何变更任一方必须同步另两方的边界段.

> **核心边界 (v1.0 升档后)**:
> HydraForge 当前定义的是 **"受治理的单编排器 RSI 路径"**——即在受 ADR-0084 MutationGovernance + ADR-0086 CreditAssignment + ADR-0088 TransitionGuard 治理的单一编排器内, 顺序执行 Harness-RSI 与 Model-RSI 算子, 不实现多智能体协同 RSI. Agent-Agent 对等协同演化 + 在线权重热切换 + 多教师池 + Meta Co-Evolution 均属未来独立 spike, 不作为 v1.0 Source of Truth. Wave 3 Phase 1 ✅ ship (D1+D3+D7 最小版启动) = Harness-RSI 与 Model-RSI 接口已对齐. Wave 3 Phase 2 (D4+D5+D6+D7 完整) 待冷却期满 2026-09-24T05:33Z 后立项.

> **升档路径记录 (✅ evidence)**: 原位于 `docs/research/rsi-three-operators-hydraforge-mapping-2026-09.md`（2026-09-18 调研版本 + 2026-09-21 校准版本）→ 本次 (2026-09-23) 通过 `git mv` 迁移到 `docs/architecture/rsi-architecture-2026-09.md` + 状态 research → architecture 升档 + 加 §十 升档说明 + 同步 5 阶段 ship 实证矩阵 + 同步 Wave 3 Phase 1 ship 状态 + 状态行 ✅ Approved (v1.0) + 配套三方 SoT 基线 (self-evolution v1.5 + harness v1.0 + rsi v1.0).

---

## 执行摘要

HydraForge **已经在实施 RSI**——10 个 ship 评估/治理/审计/会话/沙箱 ADR 已构成完整基础设施，C0+C1 chat demo 端到端 2026-09-16 已 ship，**C2 genome-registry 2026-09-19 已 ship**（12 tests / 266 assertions, archived `2026-09-19-2026-09-16-genome-registry`），**C3 h-d-m-transition-guard v1.0 + C4 harness-rsi-pilot 已于 2026-09-20/21 ship**（ADR-0088 ✅ Approved；test_transition_guard 13 cases / 47 assertions + test_harness_rsi_pilot 9 cases / 43 assertions）。**遗留关键缺口（2026-09-21 Oracle 审查确认）**：`IGenomeRegistry` 无生产接线——`apply_harness_mutation` 仅改内存、`GEPALoop::reflect_and_commit` 仅发审计事件，闭环第 7 环"版本提交"仍断（接线 change `genome-wiring-harness-rsi-gepa` 待立项）。

**核心评级**：⭐⭐⭐⭐（基础设施已就位，缺 Genome 装配 + H→D→M 守门 + Harness-RSI Pilot 验证）

---

## 一、三篇文章对位 + 项目落地点

### 1.1 三篇文章核心概念 ↔ HydraForge 对应

| 来源 | 核心概念 | HydraForge 对应 | 状态 |
|------|----------|-----------------|------:|
| **MetaRSI-v1** | **Genome**（Harness 可版本化资产） | `2026-09-16-genome-registry` (C2) | ✅ Shipped 2026-09-19 (12 tests / 266 assertions) |
| **MetaRSI-v1** | **Data-RSI**（轨迹→训练数据） | ADR-0061-06 v1.1 Trajectory IR + ADR-0080 D10 Distillation | ✅ Shipped |
| **MetaRSI-v1** | **Harness-RSI**（改 prompt/tools/workflow） | `2026-09-16-harness-rsi-pilot` (C4) + ADR-0084 Mutation Governance | ✅ C4 shipped 2026-09-21 (9 cases / 43 assertions) + ADR ✅ (Genome 持久化接线待 follow-up) |
| **MetaRSI-v1** | **Model-RSI**（训练权重） | ADR-0078 Fine-tune 🔍 Proposed + ADR-0084 V1 显式禁止 L4 | 🔍 待立项 (Wave 3, gated by C4 Go) |
| **MetaRSI-v1** | **H→D→M 交通规则** | `2026-09-16-h-d-m-transition-guard` (C3) + ADR-0086 Credit Assignment | ✅ C3 shipped 2026-09-20 (ADR-0088 ✅ Approved) + ADR-0086 ✅ Approved v1.1 |
| **字节 Seed Aspire** | 目标形成（"改什么"） | ADR-0086 Credit Assignment（归因层）+ ADR-0083 IEvaluator（评估层） | ✅ Credit ✅ Approved v1.1 (2026-09-20) |
| **字节 Seed S³Gym** | 经验整合（历史→能力） | ADR-0061-04 SLM Routing First + ADR-0083 V2 + ADR-0080 D10 | ✅ Shipped |
| **字节 Seed HarnessDev** | 系统进化（改 Harness） | C4 Harness-RSI Pilot + ADR-0084 + ADR-0081 Pre-Step Hook | ✅ C4 shipped 2026-09-21 + ADR ✅ |
| **DSH 三权分立** | Cordis 能力 / Loop 推进 / Session 事实 | SkillInterpreter / DSLEngine+3 Loop / SessionManager JSONL | ✅ Shipped |
| **DSH 投影语义** | messages 是事实的投影 | ChatSession `build_context_entries(leaf)` 叶到根投影 | ✅ Shipped |
| **omp 单一权威会话** | 一切皆 Entity Delta | SessionManager JSONL 树 + append-only + EventLog 双通道 | ✅ Shipped |
| **omp 沙箱只执行不决定** | 宿主/Sandbox/Sub-agent 三层 | DSLEngine / SkillInterpreter / CognitiveWorker | ✅ Shipped |
| **omp Prompt Cache 前缀稳定** | 5 工具加速 2.4× | **⚠️ 空白**——LayeredContext 无强约束 | ❌ 缺口 |

### 1.2 项目已 ship 的自进化基础设施矩阵（10 项）

| # | 组件 | ADR / 实现 | 状态 | 自进化作用 |
|---|------|-----------|:----:|----------|
| 1 | **评估信号** | ADR-0083 IEvaluator V1+V2 (20 cases/49 assertions) | ✅ | 回答"这次执行好不好"——所有进化决策的输入 |
| 2 | **变异治理** | ADR-0084 Mutation Governance V1 (13 cases/139 assertions) | ✅ | L1-L4 分级 + gate-and-audit + 4 个 `mutation.*` 事件 |
| 3 | **审计日志** | ADR-0080 AppendOnlyEventLog + D10 Distillation Capture | ✅ | 训练数据采集 + 审计 |
| 4 | **轨迹格式** | ADR-0061-06 v1.1 Trajectory IR 独立序列化视图 | ✅ | 蒸馏/评估数据格式，与 ParsedGraph 零耦合 |
| 5 | **会话模型** | ADR-0079 4-Scope 会话（Conversation/Attempt/Step/Execution） | ✅ | GEPA/MCTS 反思的轨迹基础 |
| 6 | **因果序** | ADR-0037 CausalClock + auto-tick | 🟡 | 多 worker 事件全序（分布式向量时钟 defer） |
| 7 | **沙箱验证** | ADR-0075 LocalBackend/DockerBackend | ✅ | 候选 workflow 隔离执行 |
| 8 | **性能基础** | ADR-0087 多 worker 3.3× 加速（Sprint 33+ Day 3.1） | ✅ | GEPA/MCTS/Compactor 多 worker 真并发 |
| 9 | **会话持久化** | SessionManager JSONL + lock-order fix (2026-09-15) | ✅ | fork/branch/compact 投影式装配 |
| 10 | **事件去抖** | IInteractionBus::wait_for_drain() (AGENTS.md 模式 #9) | ✅ | consumer 析构前排空 bus callback |

**核心洞察**：10 项基础设施已 ship，**评估信号 + 治理护栏 + 审计日志 + 轨迹格式 + 会话模型 + 沙箱**构成完整的"自进化前置条件栈"。剩下的 RSI 工作是**装配**（Genome Registry） + **守门**（H→D→M Guard） + **试水**（Harness-RSI Pilot）。

---

## 二、架构特点的"RSI 友好性"分析

### 2.1 六层 Harness 模型在项目中的对位

| Harness 层 | HydraForge 实现 | 对应 DSH/omp 概念 | RSI 友好性 |
|-----------|-----------------|-------------------|:---------:|
| **L1 动作空间** | IToolRegistry (ADR-0004 V2) + DECLARE_TOOL 宏 + DECLARE_COMMAND (ADR-0070) | omp 工具面 | ⭐⭐⭐⭐ |
| **L2 上下文装配** | LayeredContext 5 层 (ADR-0008) + ContextCompactor (ADR-0007) + SessionManager 叶到根投影 | omp 单一权威会话 | ⭐⭐⭐⭐ |
| **L3 反馈通道** | IInteractionBus + EventBuilder V2 (ADR-0068) + 18 个 canonical topic + TracingDecorator | DSH SessionEvent 流 | ⭐⭐⭐⭐⭐ |
| **L4 控制循环** | 3 种 Loop (React/PlanExecute/ForkJoin) + ChatSession 双队列 + cancellation chain 7 步 | DSH Agent Loop + Inbox | ⭐⭐⭐⭐⭐ |
| **L5 推理参数** | ILLMProvider Decorator 链 (ADR-0042) + IModelRouter (ADR-0034) + BudgetController | DSH Token 冻结 | ⭐⭐⭐ |
| **L6 信任边界** | ToolCoordinator (ADR-0031) + EnvBackend (ADR-0075) + 4 级权限分层 (ADR-0084) + Pre-Step Hook (ADR-0081) | omp 沙箱 + Codex 内核 | ⭐⭐⭐⭐⭐ |

**平均友好度 4.4/5**——L3/L4/L6 完美，L1/L2 良好，**L5 是唯一短板**（Prompt Cache 前缀稳定性无强约束）。

### 2.2 三种"算子"在项目中的天然落点

| MetaRSI-v1 算子 | 项目对应点 | RSI 操作粒度 |
|----------------|-----------|------------|
| **Harness-RSI** | ChatSession 的 `override_system_prompt/append_system_prompt` + ToolRegistry 运行时 register/unregister + SkillInterpreter capability 注入（allowed_tools/allowed_topics） | 即时生效，下个 turn 可见，无需训练 |
| **Data-RSI** | ADR-0080 D10 Distillation Capture 落盘 prompt/response + ADR-0061-06 v1.1 Trajectory IR 序列化 + SessionManager JSONL 轨迹 + IEvaluator 打标 | 离线产出 LoRA 训练数据 |
| **Model-RSI** | ADR-0078 Fine-tune（🔍 Proposed）— V1 显式禁止（ADR-0084 决策 1） | 一次性训练成本，长期受益 |

### 2.3 CognitiveWorker vs DomainWorkerPool 正交抽象

| 维度 | CognitiveWorker | DomainWorkerPool |
|------|-----------------|------------------|
| **设计目标** | per-agent LLM 推理编排（"思考"） | 多领域工具并行执行（"行动"） |
| **线程模型** | 单 `std::thread` | N 个 `std::jthread`（默认 4） |
| **消费者模式** | 单消费者，串行保因果序 | 多消费者，共享 FIFO 队列 |
| **任务结构** | `(task_id, prompt, parent_trace)` | `DomainTask { domain, tool_name, arguments, output_key, parent_trace }` |
| **处理器注册** | 不支持（内部委托 SimpleCognitiveOrchestrator） | 支持运行时 `register_domain_handler()` |
| **异常隔离** | 无（单线程顺序） | `try-catch` + `catch(...)` 确保 worker 不死 |
| **锁顺序约束** | 无 | `queue_mutex_` 先于 `handlers_mutex_`（CP.22） |
| **发射事件** | `agent.spawned`/`cognitive.task.*`/`agent.terminated`/`evaluation.result` | `domain.task.*` |

**核心洞察**：两者是**正交抽象**，CognitiveWorker 可委托 DomainWorkerPool 执行工具（通过注册 domain handler），形成"思考 + 行动"完整闭环。RSI 可独立修改 prompt（影响 CognitiveWorker）或工具集（影响 DomainWorkerPool handler 注册表），不破坏两个 worker 的抽象边界。

### 2.4 横切契约层（5 个）共同支撑六层 Harness

| Contract 层 | 位置 | 六层对应 | 真实消费者 |
|------------|------|---------|----------|
| **IInteractionBus + InMemoryBus** | `include/agenticdsl/contract/iinteraction_bus.h` | L3 反馈通道 | CognitiveWorker/DomainWorkerPool/3 Loop/EventLogWriter/SessionWriter |
| **EventBuilder V2** | `include/agenticdsl/contract/event_builder.h` | L3 反馈通道 | TracingDecorator + 8 处 operation-result 迁移点 |
| **ILogger** | `include/agenticdsl/contract/ilogger.h` | 横切可观测性 | ChatSession Meyers singleton + DefaultLoggerGuard RAII |
| **ITimerService** | `include/agenticdsl/contract/timer_service.h` | L4 控制循环 | WorkflowCallbackChannel + SkillInterpreter + ChatSession |
| **TracingDecorator** | `src/common/llm/tracing_decorator.cpp` | L5 推理参数 | ILLMProvider 链装饰（emit llm.request/response） |

**关键设计原则**：每个 contract 层解决一个正交关注点，通过接口注入实现跨模块复用，同时保持 PDK 插件隔离能力（ADR-0021 §3.5: PDK 头文件仅依赖 `agenticdsl/contract/*.h`）。

### 2.5 H→D→M 守卫的天然兼容性

| 项目基础设施 | 对应 MetaRSI-v1 角色 |
|------------|---------------------|
| **Genome Registry**（C2 ✅ Shipped 2026-09-19） | 标识"当前 Harness 版本号"（last_harness_change_version）+ walk_ancestors 谱系（✅ 2026-09-20 实装，见 C3 follow-up D5） |
| **IEvaluator 评估结果**（ADR-0083 V2 ✅） | 3+1 条件矩阵的"回归门 PASS"判定（见 §3.2 注：原 4 条件逻辑冗余，实际 3 独立条件 + 1 派生注释） |
| **Mutation Governance**（ADR-0084 V1 ✅） | 变异授权白名单（source_id）+ 模式×等级矩阵 |
| **Credit Assignment**（ADR-0086 🔍 Proposed → 🟡 amendment in flight） | 归因结果（attributed/insufficient/confounded），决定是否升级 |
| **BudgetController**（`IBudgetController` Sprint 11 C1） | "预算充足"判定（`ExecutionBudget` core type，**非 ADR-0019**——ADR-0019 是 IInteractionBus，引用修正见 §3.2） |

C3 TransitionGuard 的 3+1 条件矩阵天然落在既有契约栈上（per Oracle M2 评审取消新建 3 算子接口框架，复用既有 ADR-0083/0084/0086 v1.1）。

---

## 三、RSI 在 HydraForge 的实施路径（3 阶段）

### 3.1 当前阶段（Sprint 33+ 已 ship）：基础设施就位

```
✅ ADR-0083 IEvaluator          — 评估信号源
✅ ADR-0084 Mutation Governance  — 治理护栏（L4 显式禁止）
✅ ADR-0080 AppendOnlyEventLog   — 审计 + D10 蒸馏采集
✅ ADR-0061-06 v1.1 Trajectory IR — 训练数据格式
✅ ADR-0079 4-Scope Session       — 轨迹会话
✅ ADR-0075 EnvBackend            — 沙箱验证
✅ ADR-0087 多 worker 3.3×        — 性能税解除
✅ C0+C1 chat demo 端到端         — 真实事件流采集（2026-09-16 ship）
```

**当前可做**：
- 用 IEvaluator 评估 LLM 响应质量
- 用 Mutation Governance 拦截恶意/越权变异
- 用 EventLog D10 mode 落盘 prompt/response 供蒸馏
- 用 Trajectory IR 序列化轨迹用于 SFT 数据准备
- 用 EnvBackend 沙箱运行候选 workflow 验证

**当前不能做**：
- 自动修改 Harness（缺 Genome Registry → 无版本锚点）
- 自动重训模型（ADR-0078 🔍 Proposed + ADR-0084 V1 显式禁止 L4）
- 自进化闭环（缺 H→D→M 守门 + Credit Assignment 🔍）

### 3.2 短期阶段（Sprint 35-36）：Harness-RSI 试水

依赖链：`C2 (Genome Registry)` → `C3 (H→D→M Guard)` → `C4 (Harness-RSI Pilot)`

#### C2 — Genome Registry（1 周，P1，Sprint 35）

**目标**：把分散的 Harness 配置（`config.json` + `lib/loop/*.agent.md` + `ChatConfig` 隐式）打包为可版本化、可 fork、可 diff 的 Genome 对象

**关键决策**：
- D9 storage backend：filesystem（`~/.hydraforge/genomes/<name>/<version>/genome.yaml`）优先，Git-LFS 后续
- D10 signature scheme：HMAC（简单，V1 够用），ed25519 V2
- IGenomeRegistry **6 方法**：`load(name@version)` / `commit(genome)` / `fork(parent, mutations)` / `list_versions(name)` / `diff(v1, v2)` / **`walk_ancestors(name, from_version, to_version)`**（D5 于 2026-09-20 实装，5 → 6 public methods，default impl 返回 NotImplemented，FilesystemGenomeRegistry override 提供 closest-first + self-inclusive + visited set + cross-name rejection）

**RSI 价值**：
- Genome 版本号是 **C3 TransitionGuard 判断"过期数据"** 的基础
- fork/diff 是 Harness-RSI 候选生成与回滚的载体
- HMAC 签名保证 Genome 不可被恶意篡改

#### C3 — H→D→M Transition Guard（2-3 天，P1，Sprint 35）

**目标**：用轻量状态机强制执行 MetaRSI-v1 的关键规则

**核心规则**：
```cpp
EvolutionVerdict can_transition(
    State from, State to,
    const GenomeVersion& current_genome_version,
    const GenomeVersion& last_harness_change_version) {
  if (from == Harness && to == Model) {
    return {.can_proceed = false, .reason = "H→M forbidden, must run H→D→M"};
  }
  // ... 其他规则
}
```

**3+1 条件 evaluate_readiness 矩阵**（per Oracle `ses_f45b96c94ffevTy454aeDBK7U2` 评审修正：原"4 条件"逻辑冗余，条件 1 蕴含条件 4）：
1. **归因 Attributed**（复用 ADR-0086 v1.1）—— 蕴含"无未控制混杂"，因 ADR-0086 决策 2 算法 `未控制混杂 → verdict=Confounded`
2. **回归门 PASS**（复用 ADR-0083 IEvaluator + ADR-0061-02 T14 Hotelling T²）
3. **预算充足**（复用 `ExecutionBudget` core type + `IBudgetController` 接口——**修正原映射文档错误引用 ADR-0019**，ADR-0019 是 IInteractionBus MVP）

> 注：原"4 条件矩阵"第 4 条"无未控制混杂"不是独立门，其检查已内嵌于条件 1 的算法（ADR-0086 决策 2）。详见 `openspec/changes/2026-09-20-adr-0086-v1-1-harness-change-confounder/` 决策 4（HarnessChange kind）+ 决策 9（数据新鲜度判定）。

**Oracle M2 关键决策**：取消原计划新建 3 算子接口（IDataRSI/IHarnessRSI/IModelRSI），复用既有契约栈——避免平行架构（YAGNI）。

#### C4 — Harness-RSI Pilot（1-2 周，P2，Sprint 36）

**目标**：验证 Harness-RSI 价值，决定是否扩展 Model-RSI

**实施路径**：
1. IHarnessRSI 首个真实实现 → 直接调 `ChatConfig::override_*` 方法
2. 双门禁集成：走 `evaluate_readiness()` + `MutationGovernanceVerdict`
3. Mock 闭环：Genome 变异 → ApprovalPolicy 通过 → 重新加载 ChatSession → 1 turn 验证
4. 真实 LLM 1 turn 验证（D2 配置 deepseek）
5. ApprovalPolicy 拦截测试（low-trust / dangerous 工具 add 被拦截）

**Go/No-Go 决策**：
- **Go** → 立项 ADR-0078 Model-RSI Pilot（Wave 3）
- **No-Go** → 归档 Wave 2 skeleton，等待真实训练数据 / 评估基线就绪

### 3.3 中期阶段（Sprint 37+）：Model-RSI + 协同进化

**Model-RSI Pilot**（基于 C4 Go 决策）：
- ADR-0078 Fine-tune V1（V1 边界 = ADR-0084 L4 解除 + ADR-0078 Approved）
- 训练数据 = D10 Distillation 落盘的 prompt/response + IEvaluator 过滤 + Trajectory IR 序列化
- LoRA 微调 → 新模型加载到 ILLMProvider 链

**协同进化**（多智能体）：
- ADR-0086 Credit Assignment V1 ship 后，激活 S4 协同进化场景
- 当前 SkillInterpreter 单进程 fork+exec 隔离不支持嵌套，Hub-Spoke 协议缺失（ADR-0077 gRPC 🔍 Proposed 是远期依赖）

---

## 四、关键风险与缓解（与字节 Seed 三篇论文对照）

### 4.1 字节 Seed Aspire 风险：目标形成失败

**风险**：30 个实验单元中只有 1 个在隐藏评测集取得有效提升——AI 无法准确判断"该优化什么"

**HydraForge 缓解**：
1. **ADR-0083 IEvaluator** 多评估器组合（TaskSuccess + BehavioralEquivalence + Composite）——不依赖单一信号
2. **ADR-0086 Credit Assignment**（✅ 已 ship 2026-09-20 v1.1）——归因层与评估层划界（VersionPairDiff V1 归因方法 + ConfounderRecord 混杂分层记录 + HarnessChange kind + judge_data_freshness + GenomeVersion）
3. **ADR-0084 Mutation Governance L1-L4 分级**——只允许白名单 R 轨任务触发变异，禁止模型自行发起

### 4.2 字节 Seed S³Gym 风险：经验整合失败

**风险**：AI 对自己经验的判断与下一次任务表现相关性接近 0

**HydraForge 缓解**：
1. **ADR-0061-04 SLM Routing First**（V1 ship）——不依赖模型自我判断路由
2. **ADR-0080 D10 Distillation Capture**——把经验作为训练数据而非 in-context learning（避免 History ICL 的负迁移）
3. **ADR-0061-06 v1.1 Trajectory IR 独立序列化**——经验格式不耦合运行时图，可任意 replay
4. **IEvaluator 跨 Attempt/Conversation 复用**（ADR-0079 4-Scope）——评估信号可跨会话累积

### 4.3 字节 Seed HarnessDev 风险：自我修改 ≠ 自我改进

**风险**：64 次版本迭代只有 53.1% 修改在可见任务和隐藏任务上效果方向一致；"Agent 自我感觉最好的 9 个版本中只有 2 个是最优"

**HydraForge 缓解**（这正是 MetaRSI-v1 的核心禁令场景）：
1. **H→D→M 守卫（C3）**——禁止 Harness-RSI 后直接训练，必须重新生成与新 Harness 匹配的数据
2. **ADR-0084 Mutation Governance 模式×等级矩阵**——L2 变异必须 plan+agent 双审批，L3 必须 IApprovalHandler 人类复核
3. **ADR-0084 V1 L4 显式禁止**——V1 边界内不允许自动权重变异，需 ADR-0078 ship 后解锁
4. **ADR-0079 v1.1 session fork**——revert 实际恢复由调用方经 session fork 负责，非 governor 内部 storage
5. **ADR-0084 变异来源白名单**（不可变构造注入）——禁止外部输入/LLM 输出/Trajectory 抽取产物触发自修改
6. **ADR-0084 行为回归门**（ADR-0061-02 T14 已 ship）——任何 L1-L3 变异必须通过 6 case 等价性测试

### 4.4 项目特定的 3 个隐患

| 隐患 | 风险 | 缓解 |
|------|------|------|
| **L5 Prompt Cache 前缀稳定性无强约束** | ContextCompactor 压缩会击穿 cache 前缀，成本优化变放大（参考 omp 23→5 工具 86s→36s 数据） | 需新增"前缀冻结区"约束（ContextCompactor 压缩只动尾部，不动 cache 前缀） |
| **SessionNode 粒度仅 message 级** | 无 tool/call 与 tool/result 细粒度分离，DSH "未知状态"恢复不可达 | 评估是否将 tool/call 与 tool/result 拆为独立 SessionNode |
| **Oracle session `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` 评审取消的 3 算子接口** | 复用既有契约栈可能让 H→D→M 守门成为"胶水代码" | C3 设计时验证 TransitionGuard 与 ADR-0083/0084/0086 接口契合度，必要时申请回归接口 |

---

## 五、项目形态对照（vs 三篇文章 + DSH + omp）

### 5.1 项目 = MetaRSI-v1 的工程化映射

| MetaRSI-v1 抽象 | HydraForge 工程化映射 | 评估 |
|---------------|---------------------|:----:|
| 三个算子（Data/Harness/Model-RSI） | C2 + C3 + C4 均已 archived + 10 个 ship ADR | Data-RSI ✅ / Harness-RSI 🟡 (接线待 follow-up) / Model-RSI 🔍 |
| "Genome" 概念 | Genome Registry（C2 ✅ Shipped 2026-09-19）+ walk_ancestors（C3 amendment 扩展） | 概念契合 + 谱系可追溯 |
| 三层递归调度内核 | H→D→M Transition Guard（C3）+ Mutation Governance + IEvaluator 门禁链 | 拆分到既有契约栈（YAGNI） |
| 验证逻辑与生成分离 | `evaluate_readiness` **3+1 条件**（归因 + 回归门 + 预算；混杂检查内嵌）+ MutationGovernor gate-and-audit | 完全契合（Oracle 评审修正：原"4 条件"冗余） |
| 强制 H→D→M 禁令 | C3 TransitionGuard.can_transition() + ADR-0086 v1.1 HarnessChange kind | ✅ C3 v1.0 shipped 2026-09-20 (ADR-0088) + ADR-0086 ✅ Approved v1.1 |
| 动态调度优于静态流程 | CancellationRegistry + 双队列 + 3 Loop | 已有 |

### 5.2 项目 vs 字节 Seed 三篇论文

| 论文洞察 | HydraForge 对应 | 差距 |
|---------|----------------|------|
| Aspire: AI 难以决定"改什么" | IEvaluator + Credit Assignment（✅ 已 ship）+ Mutation 白名单 | Credit Assignment ✅ Approved v1.1，已落地 |
| S³Gym: 经验不可靠 | D10 蒸馏 + SLM Routing First + Trajectory IR | 已 ship，验证流 OK |
| HarnessDev: 自我修改 ≠ 自我改进 | Mutation Governance + H→D→M 守门（C3 ✅）+ session fork revert | 治理完整，**H→D→M 守门已 ship；Genome 持久化接线（闭环第 7 环）✅ 已接线 (genome-wiring-harness-rsi-gepa, 2026-09-22)** |

### 5.3 项目 vs DSH "三权分立"

| DSH 组件 | HydraForge 对应 | 一致性 |
|---------|----------------|:-----:|
| Cordis（能力管理） | SkillInterpreter + ToolRegistry + CognitiveWorker 注入点 | ✅ 完整 |
| Agent Loop（任务推进） | 3 Loop + DSLEngine + TopoScheduler | ✅ 完整 |
| Session（事实记录） | SessionManager JSONL + EventLog 双通道 + append-only | ✅ 完整 |
| 投影语义（messages 是事实的投影） | `build_context_entries(leaf)` 叶到根 | ✅ 完整 |
| Turn/Step 分解 | ADR-0079 4-Scope + ADR-0068 18 个 topic | ✅ 完整 |
| inbox（follow-up/steer/inject） | ChatSession 双队列 | ✅ 完整 |
| 工具管线（pre/guard/execute/post） | ToolCoordinator (ADR-0031) | ✅ 完整 |
| 单一权威会话 | SessionManager JSONL 树 + EventLog 双写 | 🟡 双通道（SessionWriter 13 topic 白名单 + EventLog 全量） |

### 5.4 项目 vs omp "7 大原则"

| omp 原则 | HydraForge 对应 | 一致性 |
|---------|----------------|:-----:|
| 单一物化会话 | SessionManager + ADR-0080 append-only + ADR-0079 4-Scope | ✅ |
| 沙箱只执行不决定 | SkillInterpreter (posix_spawn + seccomp + 4 host fn) + EnvBackend | ✅ |
| 模型怪癖是结构化知识 | IModelRouter (ADR-0034) + ILLMProvider Decorator 链 (ADR-0042) + IProviderFactory | ✅ |
| 工具面税（5 工具加速 2.4×） | **⚠️ 无强约束**——LayeredContext 工具定义未做"必备 5 + 长尾 CLI"分层 | ❌ 缺口 |
| 单向流原语渲染 | TUI EventHandler 单遍处理 | 🟡 |
| Rust 核心 + Python 扩展 | C++20 核心 + PDK Plugin (.so) 扩展 | ✅ 同构 |
| 模型怪癖统一持有 | IModelRouter + ProviderFactory | ✅ |

---

## 六、结论与建议

### 6.1 项目自进化能力评级

| 维度 | 评级 | 证据 |
|------|------|------|
| **基础设施成熟度** | ⭐⭐⭐⭐⭐ | 10 个 ship ADR（评估/治理/审计/轨迹/会话/沙箱/因果/性能/事件/log） |
| **架构 RSI 友好性** | ⭐⭐⭐⭐ | 六层 Harness 5/6 完整，L5 Prompt Cache 前缀待加固 |
| **多算子协调能力** | ⭐⭐⭐ | C2/C3/C4 三个 PLACEHOLDER change 已立项 + Oracle M2 评审明确路径 |
| **治理护栏** | ⭐⭐⭐⭐⭐ | ADR-0084 6 维度 + L1-L4 分级 + 白名单 + fail-closed |
| **协同进化能力** | ⭐⭐ | Skill 单进程 fork+exec，无 Hub-Spoke 协议，ADR-0077 gRPC 🔍 |
| **可验证性** | ⭐⭐⭐⭐ | IEvaluator V1+V2（20 cases/49 assertions）+ Mutation Governor 13 cases/139 assertions |

**总体评级**：⭐⭐⭐⭐——**RSI 基础设施已基本就位，缺 Genome Registry 装配 + H→D→M 守门 + Harness-RSI Pilot 验证**

### 6.2 优先行动（按 ROI 排序）

| 优先级 | 行动 | 理由 | 估时 |
|:------:|------|------|------|
| **P0** | **ADR-0086 Credit Assignment ship** | Aspire 风险（"AI 不知改什么"）需要归因层划界；C3 守门的 4 条件矩阵强依赖 | 1-2 sprint |
| **P0** | **C2 Genome Registry ship** | C3/C4 强依赖；Genome 版本号是"过期数据"判定的基础 | 1 周（Sprint 35） |
| **P0** | **C3 H→D→M Transition Guard ship** | MetaRSI-v1 关键禁令工程化；Oracle M2 已评审无阻塞 | 2-3 天（Sprint 35） |
| **P1** | **C4 Harness-RSI Pilot ship** | 验证 Harness-RSI 价值，决定 C5 Model-RSI 立项 | 1-2 周（Sprint 36） |
| **P1** | **Prompt Cache 前缀稳定性约束** | L5 短板 + omp 5-tool 2.4× 数据警示 | 1 sprint（建议独立 change） |
| **P2** | **ToolResult 粒度拆分为 tool/call + tool/result 独立 SessionNode** | DSH "未知状态"恢复 + 评估细粒度 | 1 sprint |
| **P2** | **ADR-0077 gRPC Data Plane ship** | Hub-Spoke 协同进化前置 | 2-3 sprint |

### 6.3 长期战略

1. **坚持"取消 3 算子接口"路线**（per Oracle M2）：复用 ADR-0083/0084/0086 既有契约栈，避免平行架构（YAGNI）
2. **坚持"Go/No-Go 决策门"**（per C4 pilot）：Harness-RSI 价值验证后再扩展 Model-RSI，避免盲投入
3. **坚持"治理先于能力"**：ADR-0084 V1 L4 显式禁止 + 白名单 + fail-closed 三重护栏是 RSI 安全的前置条件
4. **保持与三篇 RSI 文章对位**：定期对照 MetaRSI-v1 三算子 + 字节 Seed 三论文 + DSH/omp 4 范式，更新架构

### 6.4 一句话总结

> **HydraForge 的 RSI 不是"要不要做"的问题，而是"按什么顺序 ship"的问题**——10 项基础设施已就位，C2/C3/C4 三个 PLACEHOLDER change 直接对应 MetaRSI-v1 三算子，**Sprint 35-36 是 RSI 工程的"启动周"**，Oracle M2 评审的 YAGNI 原则（取消 3 算子接口框架，复用既有契约栈）是关键决策保证。建议优先 ship ADR-0086 Credit Assignment（P0）→ C2 Genome Registry（P0）→ C3 Transition Guard（P0）→ C4 Harness-RSI Pilot（P1）四步走，每步 ship 后用 IEvaluator + Mutation Governance + TSan/Regression Gate 三重验证。

---

## 附录 A：调研任务明细（追溯审计）

本报告由 6 项并行代码调研汇总产出，每项调研输出都已记录在 AGENTS.md 工作会话中：

| 调研任务 | Task ID | 覆盖子系统 |
|---------|---------|----------|
| CognitiveWorker 架构 | bg_40b3db1b | per-agent 隔离 + 单线程 + LLM 推理编排 |
| 横切契约层 | bg_a7bb10a2 | IInteractionBus + EventBuilder + ILogger + ITimerService + TracingDecorator |
| 3 种 Loop 协作 | bg_c950d46d | ReactLoop / PlanExecuteLoop / ForkJoinLoop + DECLARE_AGENT 宏 + stop_token 传播 |
| 自进化 ADR 矩阵 | bg_0a84ae20 | ADR-0083/0084/0086/0087/0080/0061/0079/0075/0037/0050 共 10 个 |
| SkillInterpreter 与多智能体 | bg_c685e741 | posix_spawn+execve+seccomp+IPC + fork/branch + Hub-Spoke 现状 |
| Session 与事实层 | bg_b7b7397e | SessionManager JSONL + SessionWriter + EventLogWriter + ADR-0080 D10 |

## 附录 B：关联 Oracle Sessions

- `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` — Oracle M2 评审（取消 3 算子接口框架，复用既有契约栈）
- `ses_fc640ea84ffe0f4dyYTa4aFjiL` — ADR-0084 战略评估（2026-08-26）
- `ses_fc41537bbffeC35NKqgvzn4m1c` — ADR-0084 Self-Review 预审
- `ses_fc3e070c0ffeIVgAhsgx2pNXFa` — ADR-0084 深度审查
- `ses_fc3090b49ffe7yJwXhx1MoNz5N` — ADR-0084 架构文档审计

## 十、v1.0 升档说明 (2026-09-23)

### 10.1 升档触发条件 (满足 3 项)

本节记录 v0.4 (research, 2026-09-18+21 校准) → v1.0 (architecture, 2026-09-23) 升档的**触发条件**、**已 ship 实证矩阵**、**配套治理文档**。升档前满足以下 3 项:

1. **Harness-RSI 第一个算子端到端 ship + GO** — C4 harness-rsi-pilot ✅ + Decision Record GO 5 判据全绿 (2026-09-21)
2. **Pre-Wave3 4-Gate 全部 ship** — G1 + G2 + G3 + G4 收口门禁全 ✅ (2026-09-22)
3. **Wave 3 Phase 1 Pilot 激活** — ADR-0078 ✅ Approved + merge `f0a5c4b` (2026-09-23), Model-RSI 最小版 ship

### 10.2 已 ship 5 阶段实证矩阵 (升档 evidence)

| 算子 | 阶段 | Change | Ship 日期 | 关键 commit + 实证 |
|------|------|--------|-----------|-------------------|
| **基础 10 项** (RSI 前置) | Wave 1 (2026-09-17) | C0/C1/P0/F1 + ADR-0083/0084/0074/0080 ship | 2026-08 + 09-17 | `f84dbb3` + `f4766be` + `016497e` + `23e8403` + `a96842e` (11 基础设施 ship) |
| **Data-RSI** (Trajectory IR + Distillation Writer) | 2026-08-27/29 | ADR-0061-06 v1.1 + ADR-0061-13 + ADR-0080 D10 | ship 已 ship | `T15` 9 cases / 55 assertions + 21 cases PASS; capture-mode-and-distillation-writer-v1 archived |
| **Harness-RSI** (Genome Registry + H→D 守门 + harness-rsi-pilot) | Wave 2 + 2.5 (2026-09-19-21) | C2 + C3 + C3 follow-up + C4 | 2026-09-21 | C2 12 cases / 266 assertions (commit `839590d`); C3 13 cases / 47 assertions (commit `0ffc637`); walk_ancestors 10 cases / 55 assertions (`231cd8d`); C4 9 cases / 43 assertions (`08aace2`) + **GO 决策** (`docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md`) |
| **Pre-Wave3 4-Gate 收口** | 2026-09-21-22 | G1 + G2 + G3 + G4 | 2026-09-22 | G1 merge `9709317` (4 cases / 22 assertions); G2 merge `dc12a17` (14 cases / 51 assertions); G3 merge `a196a09`; G4 merge `fb2769f` (22 cases / 111 assertions + gepa_phase2 21 cases / 46) |
| **Model-RSI (Phase 1 最小版)** | Wave 3 Phase 1 (2026-09-23) | W3.P1 finetune-base-model | 2026-09-23 | merge `f0a5c4b` + SHIP-with-fixes `232eb13` (18 files, +1057/-62 + 5 files, +66/-25; focused ctest 9/9 PASS); **ADR-0078 ✅ Approved**; 12 AC verification 全 ✅; 24h cooling-off 计时 2026-09-23T05:33Z |

**累计 ship 实证**: 33 atomic commits + 5 Oracle review sessions (bg_a818a6a1 + bg_9ade564d + bg_89293120 + bg_f55f307f6ffe + bg_6a8e4397 + bg_3c06ae5b + bg_687a5662 + bg_534a2541 + bg_e4eec567 + bg_7fe026cc 等).

### 10.3 升档边界 + 不在范围

**已具备 (✅ v1.0 Source of Truth 范围)**:

| 算子 | 子项 | 已 ship | 证据 |
|------|------|---------|------|
| **Data-RSI** | Trajectory IR + Distillation Writer + D10 Capture | ✅ | ADR-0061-06 v1.1 ship + ADR-0080 D10 ship + ADR-0061-13 ship (21 cases PASS) |
| **Data-RSI** | capture-mode 三态 (Online/Training/None) | ✅ | ADR-0080 v1.2 amendment (D10 + CaptureMode 三态 + Training fail-open 三重保护) |
| **Data-RSI** | AgenticMind 回流 (D6) | ⏳ Phase 2 | Wave 3 Phase 2 立项范畴 |
| **Harness-RSI** | apply_harness_mutation 6 字段签名 + dual-gate | ✅ | C4 ship + Decision Record GO |
| **Harness-RSI** | tools_remove 对称治理 | ✅ | G1 ship 2026-09-21 |
| **Harness-RSI** | Genome 真实经 IGenomeRegistry 持久化 | ✅ | G4 ship 2026-09-22 |
| **Harness-RSI** | load(genome@N) → 重建 ChatSession → 1 turn 端到端 | ❌ **V2 defer** | G4 out-of-scope 显式 deferred |
| **Harness-RSI** | workflow_patch L3 mutation | ❌ **V2 defer** | Oracle bg_1f291bc4 DEAL-BREAKER |
| **Model-RSI** | D1 4 维度评分 + D3 数据准备第 1 路 + D7 stub 注册 | ✅ | Wave 3 Phase 1 merge `f0a5c4b` + `232eb13` |
| **Model-RSI** | D4 LoRA/QLoRA 训练管线 | ⏳ Phase 2 | Wave 3 Phase 2 立项范畴 (2026-09-24 后) |
| **Model-RSI** | D5 评估框架 + D6 AgenticMind 回流 + D7 完整 serving | ⏳ Phase 2 | Wave 3 Phase 2 立项范畴 |
| **H→D→M 守门** (横向) | 5 态状态机 + can_transition 5×5 矩阵 + 4 条件门控 | ✅ | C3 ship + C3 follow-up ship |

**不在 v1.0 Source of Truth 范围 (out-of-scope)**:

| 项 | 原因 |
|----|------|
| **S4 阶段: Agent-Agent 协同进化** | MetaRSI-v1 进阶范式, 需独立 spike + promotion criteria (信用分配已具备 ✅); research 阶段 |
| **多教师蒸馏池** | S3 阶段 + Data-RSI V2 范畴, 当前仅支持单教师 (per `docs/architecture/self-evolution-architecture-2026-08.md` §四.5) |
| **Multi-Modal RSI** | L4 范畴, ADR-0084 显式禁止 |
| **Online Weight Hot-Swapping** | S4 阶段, 需独立治理框架; V1 serving 路径与训练路径隔离 (per §一核心边界) |
| **Meta Co-Evolution** | S4 阶段, 完全 research |
| **Agent-Environment / 任务环境演化** | 研究方向, 不作为当前 serving 前提 |

### 10.4 配套治理文档 (三方 SoT 基线)

升档后, 本文档是三方架构一致基线的 **RSI 维度**. 其他两份配套:

| 文档 | 维度 | 边界 |
|------|------|------|
| [`./self-evolution-architecture-2026-08.md`](./self-evolution-architecture-2026-08.md) v1.5 ✅ | **自进化** | 整体 9 段闭环 + 4 支撑平面 + 3 阶段路线 + S0-S4 promotion criteria |
| [`./harness-architecture-2026-09.md`](./harness-architecture-2026-09.md) v1.0 ✅ | **Harness** | Harness 作为自进化核心变异对象, 数据模型 + 装配 + 变更能力 + 5-tier 守门 + 持久化 |
| **本文档** (`./rsi-architecture-2026-09.md` v1.0 ✅) | **RSI** | Harness/Data/Model 三算子叠加策略 + 5 阶段 ship 实证 + 2 阶段未 ship 候选立项 |

**三方关系**:
- 自进化架构 = 整体闭环 + 5 段流水线定义
- RSI 架构 = 三算子拆分 (Data/Harness/Model), 本文
- Harness 架构 = RSI 中 Harness-RSI 的纵深内容 (数据模型 + 装配 + 变更能力 + 守门 + 持久化)

三者无冲突, 互补. 任何变更任一方必须同步另两方边界段.

### 10.5 v1.0 维护规则 (修订版)

**升档后, 维护触发**:
- 任何 Data-RSI / Harness-RSI / Model-RSI 阶段 ship 状态变化 → 更新本文 §十.2 已 ship 矩阵
- 任何"超出 v1.0 Source of Truth 范围"项目立项 → 先更新本文 §十.3 边界段 (S4 阶段立项必须经 Oracle dual-review)
- 任何配套文档 (self-evolution / harness) 变更 → 同步复核本文 §十.4 + §十.5
- 任何 Wave 3 Phase 2 (D4-D7) 立项 → 本文 v2.0 review (focus D4 训练管线 + D5 评估 + D6 回流)

**保留 v0.4 维护规则**: 三篇 RSI 文章对位分析更新 → 保留 §一 三篇对位 + §五 实施路径; 但 Status 升级后已不再"research 类调研", 而 "architecture 类契约层".

### 10.6 不与 ADR 冲突保证 (升档后)

**v1.0 与现行 ADR 状态一致, 无冲突**:
- ✅ ADR-0078 (Fine-tune) — 与本文 Model-RSI 段落一致 (Phase 1 ✅ ship, Phase 2 ⏳)
- ✅ ADR-0088 (H→D→M Transition Guard) — 与本文 H→D 守门段落一致
- ✅ ADR-0086 v1.0+v1.1 (Credit Assignment) — 与本文 §三 HarnessChange confounder 一致
- ✅ ADR-0084 (MutationGovernance) — 与本文 L1-L4 分级 + 6 字段 MutationContext 一致
- ✅ ADR-0083 (IEvaluator) — 与本文 §四 IEvaluator 评估信号一致
- ✅ ADR-0074 (Prompt Evidence Gate) — 与本文 Data-RSI 训练数据准备一致
- ✅ ADR-0080 AppendOnlyEventLog — 与本文审计 + D10 Capture 一致
- ✅ ADR-0068 Appendix A v2.3 — 与本文 §六 mutation.* + evolution.* + genome.* 主题一致

任何未来 ADR 与本文 v1.0 冲突, 必须先升档 / amend 本文档 (经 Oracle dual-agent review + 24h cooling-off) 后才能 ship.

### 10.7 验证命令 (v1.0)

```bash
# 文档存在 + 三方 SoT 配套
test -f docs/architecture/self-evolution-architecture-2026-08.md \
  && test -f docs/architecture/harness-architecture-2026-09.md \
  && test -f docs/architecture/rsi-architecture-2026-09.md \
  && echo "三方 SoT 配套 ✅"

# 5 阶段 ship 状态
for stage in \
  "openspec/changes/archive/2026-09-17-2026-09-16-loop-agent-tools" \
  "openspec/changes/archive/2026-09-19-2026-09-16-genome-registry" \
  "openspec/changes/archive/2026-09-20-2026-09-16-h-d-m-transition-guard" \
  "openspec/changes/archive/2026-09-21-2026-09-16-harness-rsi-pilot" \
  "openspec/changes/archive/wave-3-finetune-base-model-pilot-phase1-2026-09-23"; do
  test -d "$stage" && echo "$stage ✅" || echo "$stage ❌ MISSING"
done

# Pre-Wave3 4-Gate 状态
for g in harness-rsi-remove-governance evolution-verdict-reward-quality \
         sync-pdk-contract-header genome-wiring-harness-rsi-gepa; do
  test -d "openspec/changes/archive/${g}-2026-09-22" \
    && echo "$g ✅ archived" || echo "$g ❌ missing"
done

# ADR 状态字段
grep -m1 "✅.*Approved.*Wave 3 Phase 1" docs/adr/adr-0078-finetune-base-model.md
grep -m1 "✅.*Approved (v1.1)" docs/adr/adr-0086-credit-assignment-contract.md
grep -m1 "✅.*Approved (v1.0)" docs/adr/adr-0088-h-d-m-transition-guard.md

# 关键 ctest 验证 (main branch)
grep "^add_test" build/tests/CTestTestfile.cmake | wc -l
# 预期: ~211 (post Wave 3 Phase 1, 详见 active-status.md)
```

---

**维护规则 (v1.0)**: 三方架构中任意一方变更需复核另外两方 (cross-doc consistency check, 1-on-1 reasoning). Self-Review Checklist 强制 (per docs/architecture/adr-self-review-checklist.md). 当 ADR-0078 Phase 2 立项时, 本文档 v2.0 须重新 review (focus D4 LoRA + D5 评估 + D6 AgenticMind + D7 serving).

---

## 十一、承载例: pdk_chat_demo Reference Traceback (2026-09-23)

> **本节定位**: 把 RSI 三算子架构的**每个算子**(Data-RSI / Harness-RSI / Model-RSI) 标注到 `examples/pdk_chat_demo/` 内的具体 file/line/test, 让 abstract 的算子策略变得可触达. **本节** 与 [`./self-evolution-architecture-2026-08.md §十一`](./self-evolution-architecture-2026-08.md) + [`./harness-architecture-2026-09.md §十一`](./harness-architecture-2026-09.md) 三方 SoT 交叉引用, 形成 pdk_chat_demo 上完整 traceback.

### 11.1 Data-RSI (Trajectory IR + Distillation Capture) → pdk_chat_demo

| Data-RSI 组件 | pdk_chat_demo 落地 | 验证 |
|--------------|------------------|------|
| **Trajectory IR** (ADR-0061-06 v1.1, ship 2026-08-27/29) | `examples/pdk_chat_demo/event_handler.cpp::build_trajectory_from_session()` (草案, 需 L2 落地) | pdk-chat-demo-distill-source-survey-2026-08.md 已 ship 推荐 SessionWriter JSONL 作为过渡数据源 |
| **`IDistillationWriter` + `DistillationRecord`** (ADR-0061-13 ship 2026-08-29) | `examples/pdk_chat_demo/main.cpp` 启动期 capture_mode 设定 (CaptureMode::None 默认; CaptureMode::Training 启用 → SessionManager::open(capture_mode=Training) 自动 JSONL 化) | `tests/test_event_log_capture_mode_downgrade.cpp` (data plane 已 ship) + distill-source-survey |
| **ADR-0080 D10 Capture + v1.2 CaptureMode 三态** (✅ ship) | `examples/pdk_chat_demo/event_handler.cpp` 接收 `event_log.capture_mode_downgrade` 事件, 静默从 Training 降为 None (默认 fail-open 三重保护) | 已经 ship; pdk_chat_demo 受益 |
| **capture-mode-and-distillation-writer-v1 已 ship** (21 cases PASS) | pdk_chat_demo SessionManager 装载默认 None + 用户 `--capture-mode training` flag 后启用 (L2 新增) | tests/test_event_log_capture_mode_downgrade.cpp 6 cases |
| **D6 AgenticMind 回流** (Wave 3 Phase 2 ⏳) | pdk_chat_demo 暂无 → L2 + Phase 2 需 wire `external_data_flow.cpp` (草案) | (Phase 2 ⏳) |

### 11.2 Harness-RSI (Genome Registry + Apply + H→D 守门) → pdk_chat_demo

| Harness-RSI 组件 | pdk_chat_demo 落地 | 验证 |
|-----------------|------------------|------|
| **Genome CRD `spec.harness` + IGenomeRegistry** | pdk_chat_demo 通过 `LLMProviderFactory::register_dynamic` 间接注册 finetune provider; `Genome` 数据在 L2 启用后才在 pdk_chat_demo 实例中存在 | `tests/test_genome_registry` 13/13 cases (已 ship); pdk_chat_demo 主路径不直接持有 Genome |
| **`apply_harness_mutation` 6 字段签名** (C4 ship) | pdk_chat_demo 内部 mutation 经 `--model` + skill add/remove 命令; **完整 6 字段 wiring V2 缺** (L2 计划) | C4 test_harness_rsi_pilot 9/9 (mock path, 不经 pdk_chat_demo) |
| **H→D→M Transition Guard** (ADR-0088, C3 ship) | pdk_chat_demo 不显式触发 transition 状态机 (用户 turn 间不显式区分 H/D/M 阶段); V2 `pdk_chat_demo_evolution::EvolutionSession` 显式持有 | C3 test_transition_guard 13/13 (单元测, 不 wire 到 pdk_chat_demo) |
| **`tools_remove` 对称治理** (G1 ship 2026-09-21) | pdk_chat_demo `--capture-mode training` 启用后, observer 可记录 user remove tool 操作; 直接 mutation 仍经手动命令 | G1 test_harness_rsi_pilot 22/22 |
| **Genome 真实经 IGenomeRegistry 持久化 (G4)** | pdk_chat_demo V2 启用 → `examples/pdk_chat_demo_evolution/Genome::commit_demo_session()` 写 `~/.hydraforge/genomes/pdk_chat_demo/<name>/<version>/genome.yaml` | G4 test_harness_rsi_pilot 22/22 + test_genome_walk_ancestors 10/10 |
| **`load(genome@N) → 重建 ChatSession → 1 turn`** | **V2 缺** — pdk_chat_demo 主路径尚未启用 | (L2 计划) |

### 11.3 Model-RSI (Wave 3 算子) → pdk_chat_demo

| Model-RSI 组件 | pdk_chat_demo 落地 | 验证 |
|---------------|------------------|------|
| **Wave 3 Phase 1 D1 4 维度评分** (2026-09-23 ship) | LLM 候选基模选择决策 — pdk_chat_demo `--model` 命令可选 `agenticdsl-llama-3.1-70b-lora-v1` 名字 (registered stub) | `docs/research/wave-3-base-model-selection.md` ship |
| **Wave 3 Phase 1 D3 训练数据准备第 1 路** (ship) | `scripts/prepare_training_data.py` 迁移脚本加 `source` 字段 + 过滤; pdk_chat_demo SessionManager JSONL 既是数据源 | ADR-0078 D3 |
| **Wave 3 Phase 1 D7 serving provider stub** (ship) | `LLMProviderFactory::register_dynamic("agenticdsl-llama-3.1-70b-lora-v1", factory_fn)` 注册 → pdk_chat_demo V2 的 `model_command.cpp` 可选取 | tests/test_provider_factory 9 cases / test_llm_provider_factory_decorator |
| **Wave 3 Phase 2 D4 LoRA 训练管线** (⏳ 2026-09-24 后立项) | pdk_chat_demo V2: HF TRL/PEFT 引入, finetune pipeline 跑 chat_session.jsonl → LoRA adapter | (Phase 2 ⏳) |
| **Wave 3 Phase 2 D5 评估框架** (⏳) | BehavioralRegressionGate 接入 chat_session.cpp 的 mutation diff | (Phase 2 ⏳) |
| **Wave 3 Phase 2 D6 AgenticMind 回流** (⏳) | pdk_chat_demo 暂无外部 data flow; V2 需 wire external_data_flow | (Phase 2 ⏳) |
| **Wave 3 Phase 2 D7 完整推理 serving** (⏳) | `model_command.cpp` V2 接入真实 finetune provider (而非 stub) | (Phase 2 ⏳) |

### 11.4 H→D→M 守门 (横向) → pdk_chat_demo

| 守门器 | pdk_chat_demo 落地 | 验证 |
|--------|------------------|------|
| **`TransitionGuard::can_transition`** (C3 ship) | **V2 启用** (默认 pdk_chat_demo 不显式跟踪 H/D/M phase) | tests/test_transition_guard 13/13 |
| **`evaluate_readiness` 4 条件** (C3 ship) | **V2 启用** (Mutation Decision Engine) | tests/test_transition_guard (含 Case 7/8/9 judge_data_freshness lineage) |
| **`judge_data_freshness` fail-closed** (C3 follow-up ship) | **V2 启用** (Genome lineage 检查) | tests/test_genome_walk_ancestors 10/10 (含 Critical C2 walk-failure → Insufficient) |
| **`evolution.transition.denied` / `evolution.readiness.denied` 主题** (ADR-0068 Appendix A v2.2 ship) | pdk_chat_demo 主 demo 不发射; V2 `pdk_chat_demo_evolution/` 发射 | ✅ ship ADR-0068 Appendix A v2.2 |

### 11.5 5 阶段 ship (per §十.2) 在 pdk_chat_demo 的承载进度

| 阶段 | pdk_chat_demo 已 ship 部分 | 待 L2 + Phase 2 启用 |
|------|-------------------|-----------------------------|
| Wave 1 (C0/C1/P0/F1) | ✅ 全部 (chat demo 端到端修复) | — |
| Wave 2 (C2/C3/D8) | ⚠️ 部分 (C2 注册了 finetune-stub provider; C3/D8 治理未启用) | L2 启用完整 apply_harness_mutation |
| Wave 2.5 (C4) | ⚠️ mock path 测过; pdk_chat_demo 真实 wire 缺 (1 turn 装载回路) | L2 启用 `load(genome@N) → 重建 ChatSession → 1 turn` |
| Pre-Wave3 (G1/G2/G3/G4) | ⚠️ G2 eval_quality 字段可被 emit 进 pdk_chat_demo 事件流; G1/G3/G4 间接生效 (Registry 后端) | L2 启用 mutation → governance → registry 端到端 |
| Wave 3 Phase 1 (W3.P1) | ⚠️ D7 stub 注册 (provider 选取可用); D1 评分 + D3 脚本运行 (offline) | L2 接 D7 provider to ChatSession; Phase 2 接 D4+D5+D6 |

### 11.6 升级路径 (pdk_chat_demo 完整 RSI runtime target)

**当前承载进度** (2026-09-23):
- ✅ **6/9 段闭环** 在 pdk_chat_demo 已 ship (观测 / 抽取 / 奖励 / 审计 + LLM 切换 + 模型注册)
- ⏳ **3/9 段** 待 L2 + Phase 2: mutation → governance → persistence (端到端 wire)

**L1 (本文)** 完成 traceback 任务 (30-60 分钟).
**L2 (`pdk_chat_demo_evolution`)** 跑通 reference example (2-3 天), 详见 OpenSpec change `pdk-chat-demo-evolution-reference-example` (L2.1-L2.3 待立项).

### 11.7 Reference Example (L2) 关系

- **L1 (本文)** — traceback 只标注"每段 RSI 概念**已经**在 pdk_chat_demo 哪里落地" (✅ 已 ship) **或** "将在哪里落地" (⏳ Phase 2 / L2)
- **L2 (`pdk_chat_demo_evolution/`)** — 独立 example sub-project, 跑通 RSI 端到端 (data → harness mutation → model serving upgrade), 不修改 pdk_chat_demo 主 demo, 复用 chat_session.cpp + LoopAgent +5 个 PDK Agent (Chat/Loop/Provider/Session/Budget/FS/Shell) 实例化一个 evolution-aware 变体.

> **L1 + L2 协同价值**: L1 让 maintainer 知道每个架构概念的"已落地路径" (静态导航), L2 让 maintainer 跑 `examples/pdk_chat_demo_evolution/run_evolution_demo.sh` 看 RSI 实际效果 (动态验证). 两者互补, L2 必须先 follow L1 的 traceback 导航.

### 11.8 Governance Boundary (2026-09-23 升级)

> **本节定位**: 区分 **"Self-Dev 可船范围"** vs **"必须外部治理介入范围"**. 项目当前 Single-Dev 模式 = author = reviewer = approver, 必须明示这一边界避免任何"实现自改进"虚假声明 (per 用户 16 模块 R4 红线).

#### 11.8.1 Self-Dev 可船范围 (per R8.1-R8.3 + R9.1-R9.3 验证)

| 范围 | 验证命令 | 不可扩展 |
|------|----------|----------|
| **R1 (Harness-RSI) partial** | `--release-metrics` + `--ablation-mode=full` (R8) | 不能宣称 "达到了 R1" — 仅能说"R1 partial, mock 路径已 ship" |
| **Data-RSI Trajectory IR + Distillation Capture** | capture-mode=Training test (R5) | 无 |
| **H→D→M 守门** | test_transition_guard 13/13 (C3 ship) | 无 |
| **Wave 3 Phase 1 D1+D3+D7** | test_provider_factory + test_training_data_pipeline (R5) | 不能宣称"Model-RSI 已 ship" |
| **反作弊 3 类测试** | test_anti_cheat_search_solution / metric_tampering / sandbox_escape (R9) | 不能宣称"反作弊 100% 防御" — 仅能说"3 类已知失效模式已覆盖" |

#### 11.8.2 必须外部治理介入范围 (per R3 + R4 红线)

| 范围 | 必须介入者 | 缺失影响 | 立项窗口 |
|------|-----------|---------|---------|
| **R3 方法级进化** (修改自己的提炼规则/验证门阈值/搜索策略/迭代规则) | (a) 独立审计员 (b) 元指标定义共识 | ❌ 缺外部审计 → 任何"自我改进"声明属虚假 | Wave 3 Phase 2 之后独立 spike (~1 sprint) |
| **R4 指标级进化** (评价标准可优化 + 四权分离) | (a) 4 个独立治理主体 (b) 监管机构 (per 用户 16 模块 R4 + 用户红线) | ❌ Single-Dev 模式永远不达 R4 | 项目级治理范式变更 (不是技术问题) |
| **S4 Agent-Agent 协同进化** | (a) 对手/搭档种群训练 (b) 共基础设施仲裁 (d) 监管机构 | ❌ 任意"协同"声明属虚假 | 需独立 spike + promotion criteria |
| **在线权重热切换** | (a) 监管机构 + 备份机制 (b) 强回滚协议 | ⚠️ R3/R4 不通过前不允许立项 | 必须 R3 + R4 全部 ship 后 |
| **多教师蒸馏池** | (a) 教师准入策略 (b) 训练数据对比 (d) 监管机构 | ⚠️ 单教师已 ship (ADR-0074 D6); 多教师池必须独立 spike | S3 阶段后, 不在 Wave 3 Phase 2 scope |
| **外部训练数据源** (非 AgenticMind) | (a) 数据源评估 (b) PII 审计 (d) 监管机构 | ⚠️ ADR-0080 v1.2 CaptureMode 三态已 ship, 但需独立 spike | D6 立项窗口 |

#### 11.8.3 7 条 Self-Dev 治理原则

> 项目当前 Single-Dev 模式 = author = reviewer = approver. 必须强制执行以下 7 条原则避免权力真空:

1. **24h cooling-off 强制** (per `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §十.4 + AGENTS.md §模式 #4 + AGENTS.md "Reverse Indicator Rule")
2. **Oracle dual-agent 审查** (per pattern #5 + pattern #11) — Metis 查意图 + Oracle 查物理可行性
3. **4-file integrity 强制** (AGENTS.md Day 5 lesson) — archive 必须用 `git mv` 而非 `rm`
4. **双向指标 commit 字段** (per Reverse Indicator Rule) — `[Reverse Indicator]` 段必填
5. **5 阶段 ship 实证** (per §十.2) — 任何"实现自进化"必须有 Wave 1+ 实证
6. **3 份 SoT 双向引用** (per 三方 SoT 基线声明) — 任一变更必须同步另两方 + AGENTS.md
7. **24h cooling-off override 完整审计** (per Wave 3 Phase 1 cooling_off_override_audit 字段) — 用户 override 必须记录 override 时间/by/触发字段/违反治理/当前位置/剩余窗口

#### 11.8.4 4 条"必须外部治理"红线 (per 用户 16 模块 R4)

> **声称达到 R3 / R4 / L5 前, 4 条必须全部满足**:

1. **R3 元指标实证** — §12.1.1 + §12.4 双指标 + R8.1-R8.3 ship (当前 ⏳)
2. **R4 四权分离** — 4 个独立治理主体控制 (当前 ❌ Single-Dev 永远不达)
3. **S4 协同进化稳定性指标** — 多样性 + 反共谋检测 + 资源感知代理 (当前 ❌ 未 ship)
4. **外部审计 + 监管** — 独立审计员 + 监管机构 (当前 ❌ 项目级缺乏)

**现状**: 上述 4 条全部未满足, 因此项目当前**不应**声称 L5 / R4 / "实现自改进". 任何此类声明在 §十二 §12.1.1 + §12.2 + §12.4 + §12.5 任一未 ship 时**不予承认**.

#### 11.8.5 升级路径 (何时能解除)

| Red-line | 解除条件 | 估时 |
|----------|---------|------|
| **R3** | (1) 元指标定义 ship + (2) 至少 1 次完整代际链条实证 + (3) 反作弊 R9.1-R9.3 三类全 ship | 1-2 sprint (Wave 3 Phase 2 之后) |
| **R4** | (1) 治理范式从 Solo Dev 转向多主体 (2) 4 权分别有独立审计员 + (3) 监管机构承认 | 项目级变革, 非 sprint 估时 |
| **S4** | (1) 信用分配覆盖 Agent-Agent + (2) 多样性 + 反共谋指标 ship + (3) Agent-Environment 协议 ship | 多 sprint, 需独立 spike |
| **外部审计 + 监管** | (1) 独立审计员聘 + (2) 监管机构承认 + (3) 行业基准对齐 | 项目级外部依赖 |

#### 11.8.6 跨文档一致性

- **§十二 Verification Matrix** (本文档) — 与 §11.8 Governance Boundary 同步, R3 + R4 + S4 三块未 ship 状态明示
- **L2 spec §R8 + §R9** — 反作弊 + 反向指标门是 Governance Boundary 实施的基础
- **AGENTS.md Reverse Indicator Rule** — commit 强制字段是 7 条 Self-Dev 治理原则的具体实施
- **`./self-evolution-architecture-2026-08.md` §十 + §十二** — 5 阶段 ship 实证 + 治理债务跟踪
- **`./harness-architecture-2026-09.md` §十二 §12.5** — 不可逆闸门无绕过路径是 Governance Boundary 在 H1-H6 红线的具体化

#### 11.8.7 维保规则

**v1.0 → v1.1 升级触发**:
- R3 立项时 → §11.8.2 R3 行更新
- 治理范式变更时 (Solo Dev → Multi-Dev) → §11.8.1 + §11.8.4 大规模重审
- L2 spec 升档时 (R8 + R9 ship) → §11.8.1 + §11.8.2 同步引用
- 任何"实现自改进"声称时 → 必须 cross-ref §11.8.2 + §12 + L2 spec 红线

## 附录 B：关联 Oracle Sessions

- `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` — Oracle M2 评审（取消 3 算子接口框架，复用既有契约栈）
- `ses_fc640ea84ffe0f4dyYTa4aFjiL` — ADR-0084 战略评估（2026-08-26）
- `ses_fc41537bbffeC35NKqgvzn4m1c` — ADR-0084 Self-Review 预审
- `ses_fc3e070c0ffeIVgAhsgx2pNXFa` — ADR-0084 深度审查
- `ses_fc3090b49ffe7yJwXhx1MoNz5N` — ADR-0084 架构文档审计
- `bg_a818a6a1` — C2 design review (1m 45s)
- `bg_9ade564d` — C2 post-impl SHIP verdict
- `bg_89293120` — C2 dual-agent review (Metis ship-with-fixes)
- `bg_dd35a52d` — C3 follow-up Oracle pre-impl
- `bg_7984922b` — C3 follow-up Metis pre-impl
- `bg_f6190442` — C3 walk-ancestors post-impl SHIP-with-fixes
- `bg_86a511e0` — C3 follow-up 2nd review APPROVE 95/100
- `bg_3672cb57` — C4 pre-impl Oracle
- `bg_1f291bc4` — C4 pre-impl Metis
- `bg_770d1308` — C4 2nd review
- `bg_3ef7280a` — C4 3rd review
- `bg_afa84d4d` — C4 4th review (独立审查)
- `bg_3c06ae5b` — Oracle 深度审查自进化版本管理 (Critical C1: 闭环第 7 环断裂)
- `bg_687a5662` — G4 dual-review Metis
- `bg_534a2541` — G4 dual-review Oracle
- `bg_e4eec567` — G4 post-impl SHIP-with-fixes
- `bg_8237a316` — G1 post-impl SHIP-with-fixes
- `bg_ebfe1c25` — G2 post-impl SHIP verdict
- `bg_ef5a0ca4` — G3 post-impl SHIP-with-fixes
- `bg_7fe026cc` — Wave 3 Phase 1 post-impl SHIP-with-fixes (49m)

---

## 十二、Verification Matrix (R1-R4 真 RSI 三判据 + R8/R9, 2026-09-23 升级)

> **来源**: per Cross-Doc Review 2026-09-23 + 用户提交 16 模块 R1-R4 + 反向指标门 + 反作弊三模式.
> **核心命题**: 任何**"实现了 RSI"声明**必须满足**真 RSI 三判据** (方法级证据 + 代际链条 + 元指标单调), 否则不予承认. 三判据与反向指标门 + 反作弊测试联动.

### 12.1 真 RSI 三判据 (per 用户提交 16 模块 R3 + R4)

> **任何 L5 或更高声明** 必须证明以下 3 条, 缺一不予承认:

#### 12.1.1 方法级证据 (Method-level evidence)

**判据**: 至少一次"改进机制自身被改进"的完整记录, 且改进后**元指标提升**.

**项目当前状态**: ⏳ **R3 完全 V2 defer** — 项目当前没有"修改自己的提炼规则/验证门阈值/搜索策略/迭代规则"的实现

**R3 立项窗口**: Wave 3 Phase 2 之后再 1 个 sprint (~1 周), 需要独立 OpenSpec change.

#### 12.1.2 代际链条 (Generational chain)

**判据**: 第 N 代改进者由 N-1 代产出, 链条可追溯无断裂.

**项目当前状态**: ❌ **未实证** (R3 未 ship, 无证据链)

**L2 实例化需求**: [L2 spec §R8.3 消融实验](../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) 部分实证代际链条 — 1 turn baseline → 1 mutation → 1 reload + reload 后 capability 是否保留.

#### 12.1.3 元指标单调 (Meta-metric monotonicity)

**判据**: 第 N 代在"改进自身的能力"上优于 N-1, 不只是任务分数更高.

**项目当前状态**: ❌ **未实证** (元指标未定义)

**元指标候选 (per 用户 16 模块 R3)**:
- 改进效率: 单位尝试时间产出的能力提升
- 收敛速度: 达到给定 improvement 的 gen 数
- 反射深度: 改进的改进次数 (recursion depth)

### 12.2 四权分离 (Four-powers separation, per 用户 R4 + 用户红线)

**判据**: 提出 / 执行 / 评价 / 批准 四权分立, 不可由同一主体控制.

**项目当前状态**:

| 权 | 实际控制 | 评估 |
|---|--------|------|
| 提出 (Propose) | Agent (MutationGate) | ✅ 已 ship (per ADR-0084) |
| 执行 (Execute) | DSLEngine + SkillInterpreter | ✅ 已 ship |
| 评价 (Evaluate) | IEvaluator + AttributionRecord | ✅ 已 ship (per ADR-0083 + ADR-0086) |
| 批准 (Approve) | **Single-Dev author** (Solo Dev 模式 = author = reviewer = approver) | ⚠️ **未达 R4 红线** — 项目当前是"治理委员 + 单一作者", 不是 4 个独立主体 |

**R4 红线影响**: 项目在当前 Single-Dev 模式下**不应声称达到 R4 / L5**. 任何"达到 L5"声明都是**虚假声明**, 必须先从 Solo Dev 转向多主体治理.

### 12.3 反作弊 (per L2 spec R9, 2026-09-23 升级)

> 任何"RSI 评估"必须显式覆盖 3 类已知失效模式. [L2 spec R9](../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) 是 R1-R4 端到端 + 反作弊的最小可信实例化.

| R9.1 搜现成答案 | R9.2 修改评判指标 | R9.3 串谋外部平台 |
|------------------|------------------|---------------------|
| **适用**: 元指标"提升"可能是评测时搜 baseline 答 | **适用**: Agent 修改 metric 让任务"完成" | **适用**: Sandbox 串外平台拿答案 |
| **项目当前状态**: ❌ 无防护 (per 用户 R9.1 红线) | **项目当前状态**: ⚠️ 部分 (judge_data_freshness fail-closed) + 但评估者本身改 metric 未防 | **项目当前状态**: ❌ 无沙箱级防护 |
| **L2 验证**: test_anti_cheat_search_solution (1 case PASS) | **L2 验证**: test_anti_cheat_metric_tampering (1 case PASS) | **L2 验证**: test_anti_cheat_sandbox_escape (1 case PASS) |
| **R3 立项需求**: 全 pipeline 防作弊三模式必须 ship 才能宣称 R3 元指标 | (同上) | (同上) |

### 12.4 反向指标门 (R8, 2026-09-23 升级)

**RSI 评估的反向指标 (per R8.1)**:

| 正向 (新涨) | 反向 (旧掉, drop_ratio ≤ 5%) |
|--------------|------------------------------|
| 元指标提升 % | 任务分数 drop ratio |
| 改进效率 ↑ | 收敛速度 ↓ |
| 反射深度 ↑ | 单位尝试成本 ↑ |

**drop_ratio > 5% block** (per R8.1 红线). 任何 R3 + R4 stage ship 必须双向报告.

### 12.5 验证命令

```bash
# R3 实证检查 (待 Wave 3 Phase 2 + R3 立项)
# 当前: 元指标未定义, R3 未 ship
test -f openspec/changes/rsi-implementation-search-meta-rules/spec.md 2>/dev/null \
  && echo "R3 立项进度 ✅" || echo "R3 未立项 ⏳"

# R4 实证检查 (Single-Dev 模式下不应声称 R4)
# 检查是否有人错误地标记 L5 / R4
grep -lr "✅.*R4\|L5 实证\|实现自改进" docs/adr/ 2>&1 | wc -l
# 预期: 0 (避免虚假声明)

# R8 + R9 L2 实例化检查
test -f openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md \
  && grep "R8.1\|R8.2\|R8.3\|R9.1\|R9.2\|R9.3" \
       openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md \
  && echo "L2 spec R8 + R9 完整 ✅"

# 3 份 SoT 文档 §十二 验证 (反向指标门一致性)
for f in docs/architecture/{self-evolution-architecture-2026-08,harness-architecture-2026-09,rsi-architecture-2026-09}.md; do
  grep -c "## 十二、" "$f"
done
# 预期: 3 行 (1 per file)

# 关键 9 段 / 5-tier / 4 算子 ship 实证矩阵 (per §十.2)
git log --oneline | grep -E "f0a5c4b|fb2769f|dc12a17|a196a09|9709317" | wc -l
# 预期: ≥ 5 (Pre-Wave3 4-Gate + Wave 3)
```

### 12.6 跨文档一致性

**L2 ↔ SoT 双向引用**:
- L2 spec §R8 + §R9 → 本文 §12.3 + §12.4
- L2 design §十.7 R8/R9 → 本文 §12.3
- L2 tasks T6 → 本文 §12.5 (T6 + ship 验证)

**SoT 三向交叉引用**:
- [`./self-evolution-architecture-2026-08.md` §十二](./self-evolution-architecture-2026-08.md) (9 段闭环反向指标)
- [`./harness-architecture-2026-09.md` §十二](./harness-architecture-2026-09.md) (5-tier gate 反向校验)
- 本文档 §十二 (R1-R4 真 RSI 三判据)

### 12.9 上下文驱动约束 (Context-Driven Constraint, 2026-09-23 升级)

> **来源**: 用户原话 "L2 只是提供了用户交互的设施, 具体还要用户提供一个具体上下文请求, 这个上下文请求创建的目标才能做 harness/自进化/rsi 的验证"
> **核心命题**: 任何 RSI 三算子 (Data/Harness/Model) 验证 + R1-R4 等级实证 **必须** 由 ContextRequest 触发, **不**是 L2 autonomous evaluator.

#### 12.9.1 R1-R4 + ContextRequest 类 (≥ 3 类实证)

| R 等级 | 必须 ContextRequest 类 | 实证 |
|--------|------------------------|------|
| **R1 Harness 级** | code + research + debug | harness-rsi 5-tier gate 跨类对比 |
| **R2 参数级** | code + research (ADCA-GRPO 适用) + debug (SEAL 适用) | data-rsi capture-mode + IDistillationWriter 跨类 |
| **R3 方法级** | code + research + debug (≥ 3 类) | 元指标 跨类对比 (R3 实证必须 ≥ 3 类) |
| **R4 指标级** | (per §12.2 四权分离 红线 — Single-Dev 永远不达) | n/a |

#### 12.9.2 真 RSI 三判据 + ContextRequest 集成

| 判据 | 跨 ContextRequest 类要求 | 当前状态 |
|------|----------------------|----------|
| **方法级证据** (§12.1.1) | ≥ 3 类 ContextRequest + 元指标定义 | ❌ R3 未 ship |
| **代际链条** (§12.1.2) | ≥ 3 类 ContextRequest 在 N-1 vs N 代际下基线对比 | ❌ 未实证 |
| **元指标单调** (§12.1.3) | ≥ 3 类 ContextRequest 元指标 N-1 vs N 对比 | ❌ 元指标未定义 |

> **重要**: 单 ContextRequest 类的元指标提升**不**构成 R3 实证, 这是用户原话 "上下文请求创建的目标才能做 harness/自进化/rsi 的验证" 的具体化 (per L2 spec R13.3).

#### 12.9.3 反向指标门 + ContextRequest

**R8 (反向指标门) 跨 ContextRequest 类要求** (per R13.3):

| 反向指标 (R8.1) | 跨类计算 |
|------------------|----------|
| 元指标提升 % | ≥ 3 类 ContextRequest 元指标 N-1 vs N |
| 收敛速度 | 同一 input 反复跑 N-1 vs N 收敛时间 |
| 单位任务成本 | 跨类 cost curve (新增 per H6) |
| 反射深度 | N 代改进者 vs N-1 代 (per R3) |

`drop_ratio > 5%` block (per R8.1 红线) — 跨 ≥ 3 类 ContextRequest 计算.

#### 12.9.4 反作弊 + ContextRequest (R9 集成)

| 反作弊模式 | ContextRequest 防护 |
|------------|--------------------|
| **R9.1 搜现成答案** | production ContextRequest `turn_input` **必须无 hint**; L2 test fixture 可显式含 hint |
| **R9.2 修改评判指标** | `mutation_metric_*` ContextRequest 自动拒绝 (L2 spec R13.4) |
| **R9.3 串谋外部平台** | sandbox `network_mode=none` + `turn_input` 含网络调用关键字自动拒绝 |

#### 12.9.5 跨文档一致性

- [L2 spec §R13](../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) (ContextRequest schema 主契约)
- [`./self-evolution-architecture-2026-08.md` §12.9](./self-evolution-architecture-2026-08.md) (9 段闭环 + ContextRequest)
- [`./harness-architecture-2026-09.md` §12.9](./harness-architecture-2026-09.md) (H1-H6 + ContextRequest)
- [AGENTS.md "Reverse Indicator Rule"](../../AGENTS.md) (commit `[Reverse Indicator]` 段含 `context_ids`)

#### 12.9.6 维保规则

**v1.0 → v1.1 升级触发**:
- R3 立项时 (Wave 3 Phase 2 之后) → 同步 §12.9.1 R3 行 + L2 加 ≥ 3 类 ContextRequest 实证
- 任何新 ContextRequest 类 (e.g., multimodal, live_data) → 同步 §12.9.1 矩阵
- 元指标定义时 → 同步 §12.9.2 三判据行 + §12.9.3 反向指标行

### 11.8.8 上下文驱动治理 (Context-Driven Governance, 2026-09-23 升级)

> **本节定位**: 补 §11.8 Governance Boundary 第 8 子节, 显式 ContextRequest 在 Self-Dev 可船 / 必须外部治理 / 永远不达 范围的具体作用.

#### 11.8.8.1 ContextRequest 在治理边界中的作用

| 范围 | ContextRequest 角色 | 实证要求 |
|------|---------------------|----------|
| **Self-Dev 可船范围** (§11.8.1) | 单 ContextRequest 类实证 + ≥ 3 类对比 | L2 demo 默认提供 `examples/pdk_chat_demo_evolution/fixtures/contexts/{code,research,debug}-class-context.jsonl` 3 reference file |
| **必须外部治理范围** (§11.8.2) | 跨 ContextRequest 类 + 跨治理主体的元指标对比 | 需 4 个独立治理主体承认 (per R4 红线) |
| **永远不达范围** (R4) | n/a (Single-Dev = 单一 context provider) | n/a |

#### 11.8.8.2 ContextRequest 治理原则 (4 条)

1. **ContextRequest 不可被系统自动生成** — 仅 L2 binary `--context-file <path.jsonl>` 接受, L2 **零 hardcode**
2. **ContextRequest 必须含 ≥ 3 类** — R3 元指标实证必备 (per L2 spec R13.3)
3. **ContextRequest `metadata.is_hidden=true` 必须显式承认** — 公开/隐藏集分离 (per E2 红线)
4. **ContextRequest 失败可追溯 + `context_id`** — L2 trace JSONL 必含 (per L2 spec R4 §meta)

#### 11.8.8.3 ContextRequest 与 7 条 Self-Dev 治理原则联动

| 治理原则 | ContextRequest 影响 |
|----------|---------------------|
| 1. 24h cooling-off 强制 | ContextRequest 跨类 ≥ 3 收集需 24h (R3 实证前置) |
| 2. Oracle dual-agent 审查 | ContextRequest schema 变更需 Oracle 审查 (R13 升档) |
| 3. 4-file integrity 强制 | `examples/pdk_chat_demo_evolution/fixtures/contexts/` 目录 4-file integrity (ContextRequest fixture) |
| 4. 双向指标 commit 字段 | `[Reverse Indicator]` 段含 `context_ids: <uuid list>` |
| 5. 5 阶段 ship 实证 | ContextRequest 类 ≥ 3 视为阶段 ship 实证 (per R3 红线) |
| 6. 3 份 SoT 双向引用 | ContextRequest 引用任一 SoT §12.9 必双向同步 |
| 7. 24h cooling-off override | ContextRequest 必须记录 override 时间/by/触发字段/违反治理/当前位置 |

#### 11.8.8.4 ContextRequest 与 4 条"必须外部治理"红线联动

| 红线 | ContextRequest 缺失导致 |
|------|--------------------------|
| R3 元指标实证 | ❌ 任何"实现自进化"声称无 ≥ 3 类 ContextRequest 时不承认 |
| R4 四权分离 | ❌ Single-Dev = 单一 context provider, 永远不达 |
| S4 协同进化稳定性 | ❌ 任何"协同"声明无 ≥ 3 类 ContextRequest 跨主体时不承认 |
| 外部审计 + 监管 | ❌ ContextRequest 验证 ≠ 外部审计 |

#### 11.8.8.5 跨文档一致性

- §十二 §12.9 (本 rsi doc) — R1-R4 + ContextRequest 矩阵
- §十二 §12.9 (self-evolution doc) — 9 段闭环 + ContextRequest
- §十二 §12.9 (harness doc) — H1-H6 + ContextRequest
- [L2 spec §R13](../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) (ContextRequest schema 主契约)

### 12.7 维保规则

**v1.0 → v1.1 升级触发**:
- R3 立项时 (Wave 3 Phase 2 之后独立 sprint) → 同步 §12.1.1 元指标定义 + L2 重新跑消融
- R4 升级至多主体治理时 (Single-Dev 模式变更) → 同步 §12.2 + 重新评估 R4 红线
- 任何 Anti-Cheat 失效模式新增时 → 同步 §12.3 + L2 加 test case

**禁止**:
- 任何 L4 / L5 / R4 声明不能基于本文 §十二状态
- 任何"实现自改进" claim 在 ⚠️ 状态未变时不予承认
- 任何反向指标 + 反作弊覆盖未 ship 不得合并 R3 / R4 commit
