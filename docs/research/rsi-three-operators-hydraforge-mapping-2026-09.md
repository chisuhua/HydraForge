# RSI 三算子与 HydraForge 实施路径分析

> **调研日期**: 2026-09-18
> **调研方式**: 6 项并行代码调研 + 10 个自进化 ADR 交叉对照 + 3 个 RSI 占位 change 深度分析
> **关联文档**:
> - 三篇 RSI 文章（外部参考）: MetaRSI-v1（清华/北大/斯坦福）/ 字节 Seed Self-Developing Agents 三篇（Aspire/S³Gym/HarnessDev）/ DeepSeek Harness 三权分立
> - 项目占位 change: `openspec/changes/2026-09-16-genome-registry/` / `2026-09-16-h-d-m-transition-guard/` / `2026-09-16-harness-rsi-pilot/`
> - 关联 ADR: ADR-0083/0084/0086/0087/0080/0061/0079/0075/0037/0050
> - 路线图: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md`
> - 自进化架构顶层: `docs/architecture/self-evolution-architecture-2026-08.md`

---

## 执行摘要

HydraForge **已经在实施 RSI**——10 个 ship 评估/治理/审计/会话/沙箱 ADR 已构成完整基础设施，C0+C1 chat demo 端到端 2026-09-16 已 ship。`2026-09-16-genome-registry` (C2) / `h-d-m-transition-guard` (C3) / `harness-rsi-pilot` (C4) 三个 PLACEHOLDER change 直接对应 MetaRSI-v1 三个算子（Data-RSI / Harness-RSI / Model-RSI）和"禁止 H→M 直跳"关键禁令。Sprint 35-36 是 RSI 工程的"启动周"。

**核心评级**：⭐⭐⭐⭐（基础设施已就位，缺 Genome 装配 + H→D→M 守门 + Harness-RSI Pilot 验证）

---

## 一、三篇文章对位 + 项目落地点

### 1.1 三篇文章核心概念 ↔ HydraForge 对应

| 来源 | 核心概念 | HydraForge 对应 | 状态 |
|------|----------|-----------------|------:|
| **MetaRSI-v1** | **Genome**（Harness 可版本化资产） | `2026-09-16-genome-registry` (C2) | 📝 占位 |
| **MetaRSI-v1** | **Data-RSI**（轨迹→训练数据） | ADR-0061-06 v1.1 Trajectory IR + ADR-0080 D10 Distillation | ✅ Shipped |
| **MetaRSI-v1** | **Harness-RSI**（改 prompt/tools/workflow） | `2026-09-16-harness-rsi-pilot` (C4) + ADR-0084 Mutation Governance | 📝 C4 + ADR ✅ |
| **MetaRSI-v1** | **Model-RSI**（训练权重） | ADR-0078 Fine-tune 🔍 Proposed + ADR-0084 V1 显式禁止 L4 | 🔍 待立项 |
| **MetaRSI-v1** | **H→D→M 交通规则** | `2026-09-16-h-d-m-transition-guard` (C3) + ADR-0086 Credit Assignment 🔍 | 📝 C3 + 🔍 ADR |
| **字节 Seed Aspire** | 目标形成（"改什么"） | ADR-0086 Credit Assignment（归因层）+ ADR-0083 IEvaluator（评估层） | 🟡 Credit 🔍 |
| **字节 Seed S³Gym** | 经验整合（历史→能力） | ADR-0061-04 SLM Routing First + ADR-0083 V2 + ADR-0080 D10 | ✅ Shipped |
| **字节 Seed HarnessDev** | 系统进化（改 Harness） | C4 Harness-RSI Pilot + ADR-0084 + ADR-0081 Pre-Step Hook | 🟡 C4 + ADR ✅ |
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
| **Genome Registry**（C2 占位） | 标识"当前 Harness 版本号"（last_harness_change_version） |
| **IEvaluator 评估结果** | 4 条件矩阵的"回归门 PASS"判定 |
| **Mutation Governance** | 变异授权白名单（source_id）+ 模式×等级矩阵 |
| **Credit Assignment**（ADR-0086 🔍） | 归因结果（attributed/insufficient/confounded），决定是否升级 |
| **BudgetController** | "预算充足"判定 |

C3 TransitionGuard 的 4 条件矩阵天然落在既有契约栈上（per Oracle M2 评审取消新建 3 算子接口框架，复用既有 ADR-0083/0084/0086）。

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
- IGenomeRegistry 5 方法：`load(name@version)` / `commit(genome)` / `fork(parent, mutations)` / `list_versions(name)` / `diff(v1, v2)`

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

**4 条件 evaluate_readiness 矩阵**：
1. 归因 Attributed（复用 ADR-0086）
2. 回归门 PASS（复用 ADR-0083 IEvaluator）
3. 预算充足（复用 ADR-0019 ExecutionBudget）
4. 无未控制混杂（复用 ADR-0086 ConfounderRecord）

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
2. **ADR-0086 Credit Assignment**（🔍 待 ship）——归因层与评估层划界（VersionPairDiff V1 归因方法 + ConfounderRecord 混杂分层记录）
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
| 三个算子（Data/Harness/Model-RSI） | 3 个 OpenSpec change + 10 个 ship ADR | 直接对应 |
| "Genome" 概念 | Genome Registry（C2 占位） | 概念契合 |
| 三层递归调度内核 | H→D→M Transition Guard（C3）+ Mutation Governance + IEvaluator 门禁链 | 拆分到既有契约栈（YAGNI） |
| 验证逻辑与生成分离 | `evaluate_readiness` 4 条件 + MutationGovernor gate-and-audit | 完全契合 |
| 强制 H→D→M 禁令 | C3 TransitionGuard.can_transition() | 待实施 |
| 动态调度优于静态流程 | CancellationRegistry + 双队列 + 3 Loop | 已有 |

### 5.2 项目 vs 字节 Seed 三篇论文

| 论文洞察 | HydraForge 对应 | 差距 |
|---------|----------------|------|
| Aspire: AI 难以决定"改什么" | IEvaluator + Credit Assignment（待 ship）+ Mutation 白名单 | Credit Assignment 🔍 Proposed，🔧 优先 ship |
| S³Gym: 经验不可靠 | D10 蒸馏 + SLM Routing First + Trajectory IR | 已 ship，验证流 OK |
| HarnessDev: 自我修改 ≠ 自我改进 | Mutation Governance + H→D→M 守门（C3）+ session fork revert | 治理完整，**H→D→M 守门待 ship** |

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
