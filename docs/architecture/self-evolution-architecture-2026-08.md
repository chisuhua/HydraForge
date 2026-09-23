# 自进化与协同进化架构定义（2026-08）

**生成日期**: 2026-08-26（**v1.5 升档 2026-09-23** — Pre-Wave3 4-Gate + Wave 3 Phase 1 ship 状态对齐 + 状态 🔍 → ✅ Approved；详见 §十 升档说明）
**最后验证**: 2026-09-23（v1.5，**11 项自进化基础设施 ✅ ship** + **C2 genome-registry ✅ ship 2026-09-19** + **C3 h-d-m-transition-guard v1.0 ✅ ship 2026-09-20** + **C4 harness-rsi-pilot ✅ ship + GO 2026-09-21** + **Pre-Wave3 4-Gate 全部 ✅ ship 2026-09-22** + **Wave 3 Phase 1 finetune-base-model pilot ✅ ship 2026-09-23** + **ADR-0078 ✅ Approved**；验证命令见 §九）
**作者**: Architecture Working Group  
**状态**: ✅ **Approved (v1.5, 2026-09-23 升档)** — 自进化方向架构 **Source of Truth**（与 [`./harness-architecture-2026-09.md`](./harness-architecture-2026-09.md) + [`./rsi-architecture-2026-09.md`](./rsi-architecture-2026-09.md) 共同构成 Self-Evolution / Harness / RSI 三方架构一致基线，详见 §十 升档说明）

> **定位 (升档后 v1.5)**: 自 2026-09-23 起，本文档作为 HydraForge 自进化方向的 **架构契约层 Source of Truth**（与 `harness-architecture-2026-09.md` + `rsi-architecture-2026-09.md` 配套）。9 段闭环、4 项支撑平面、3 阶段路线（§六 S0-S4）构成的自进化架构已经过 5 阶段 ship 实证：Wave 1 修 chat demo + Wave 2 自进化骨架 + Wave 2.5 Pilot + Pre-Wave3 4-Gate + Wave 3 Phase 1。任何会改变变异权限、事件 schema、训练管线或 serving 行为的决定，仍必须通过 ADR 或 ADR amendment（本文不替代 ADR 决策权）。
>
> **核心边界 (升档后 v1.5)**: HydraForge 当前定义的是**“受治理的单编排器自进化闭环”**，不是已经实现的多智能体协同进化平台。Agent-Agent 对等协同、在线权重更新、多教师池和 Meta Co-Evolution 均属于 S4 阶段研究路径，需独立 spike + promotion criteria 后才能立项。Wave 3 Phase 1 ✅ ship = D1+D3+D7 最小版启用（ADR-0078 翻牌激活），但 D4 (LoRA/QLoRA 训练管线) + D5 (评估框架) + D6 (AgenticMind 回流) + D7 完整推理 serving 仍属 Wave 3 Phase 2 范畴（待冷却期满 2026-09-24T05:33Z 后立项）。
>
> **关联文档**:
> - 研究输入：[`../research/agent-distillation-sota-2026-08.md`](../research/agent-distillation-sota-2026-08.md)
> - 能力与任务地图：[`capability-application-map-2026-08.md`](capability-application-map-2026-08.md) §八
> - 运行时工程管线：[`agent-evolution-pipeline.md`](agent-evolution-pipeline.md)
> - 变异治理：[`../adr/adr-0084-mutation-governance-contract.md`](../adr/adr-0084-mutation-governance-contract.md)
> - 评估信号：[`../adr/adr-0083-evaluator-reward-contract.md`](../adr/adr-0083-evaluator-reward-contract.md)
> - 轨迹格式：[`../adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md`](../adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md)
>
> 本文中的“已具备”表示已有项目契约或可复用实现，不表示自进化闭环已经端到端启用。

---

## 一、问题定义与术语

### 1.1 自进化不是单一算法

自进化是一个受治理的闭环：系统从运行经验中提取证据，形成评估信号，生成一个或多个候选改进，经过安全和行为门禁后，才允许候选进入下一版本。它可以使用反向传播、强化学习、进化搜索、元学习或人工审核，但这些是**更新器**，不是架构本身。

协同进化是自进化的扩展：至少两个变化中的主体共同改变适应压力。本文区分三种运行时形态：

| 形态 | 变化主体 | 典型用途 | HydraForge 状态 |
|---|---|---|---|
| 单编排器自进化 | Agent 的 prompt、skill、DSL 或训练资产 | 失败反思、行为改进、蒸馏 | 当前主路径，部分契约已具备 |
| Agent-Agent 协同进化 | 对手、搭档、评审器或种群 | 自我对弈、协作协议、角色/种群进化 | 未批准，需独立架构决议 |
| Agent-Environment / Meta | 任务环境、奖励机制或进化策略 | 课程生成、世界模型、更新器选择 | 研究方向，不作为当前 serving 前提 |

### 1.2 轨迹的正确定位

轨迹是序列决策和过程反馈的证据载体，但不是所有进化机制的强制输入：

- 对反思、代码修复、强化学习和对抗交互，轨迹是核心输入；
- 对种群适应度、任务难度分布和资源曲线，聚合指标可能已经足够；
- 在 HydraForge 中，Trajectory IR 是独立的序列化视图，不升级或污染运行时 `ParsedGraph`；
- 轨迹可以是私有学习材料，也可以在明确授权后成为共享进化信号。默认不因“可观测”而自动共享。

### 1.3 评估信号与信用分配

`IEvaluator/RewardSignal` 负责回答“候选表现如何”；信用分配负责回答“表现变化应归因于哪个主体、哪个变异、哪个环境变化”。二者不能混为同一个接口：

- 评估信号可以是成功率、行为回归 Verdict、成本、延迟、错误恢复率或人工反馈；
- **信用分配契约 (ADR-0086 v1.0+v1.1 ✅ Approved 2026-09-20)**：`agenticdsl::evolution` 命名空间下 `AttributionRecord` + `VersionPairDiff` + `ConfounderRecord` 5 态混杂分层（含 v1.1 `ConfounderKind::HarnessChange`）+ `AttributionVerdict` 4 态判定（Attributed / Confounded / Insufficient / NotAttempted）+ `kMinBaselineSamples=5` Hotelling T² 经验值基线门控 + `judge_data_freshness(data, current, registry)` 数据时效性判定算法 stub（fast-path 排除 self-match, C3 walk_ancestors 接口由 C3 实装补全）+ `GenomeVersion` struct 单一所有权避免 ODR；**默认 fail-closed**（Insufficient 优于误归因）。**实施载体**：`include/agenticdsl/types/attribution_record.h` + `include/agenticdsl/types/attribution_version_pair_diff.h` + `src/evolution/{attribution_record,version_pair_diff}.cpp` + `agenticdsl_evolution` 静态库 + `tests/test_credit_assignment.cpp` 12 cases / 40 assertions PASS（commit `5819f55` ship，merge commit `886def1`，2026-09-20）；
- 在信用分配契约未覆盖新混杂维度时，系统不得把相对胜负、单次成功或环境变简单认定为自身能力提升（HarnessChange confounder 是首要捕获目标 — genome 变更与 harness 字符串不一致时归因无效）。

---

## 二、架构原则

1. **治理先于变异**：候选生成与候选提交分离；未通过授权、评估和回归门禁的候选不得改变运行时资产。
2. **证据与决策分离**：EventLog、SessionManager 和 Trajectory IR 保存证据；IEvaluator 产生评估；Mutation Governance 决定是否允许提交。
3. **更新器可插拔**：反向传播是可微模型的主力更新器，但不可微、非平稳或多目标场景可以使用进化策略、强化学习、元学习或梯度协调方法。
4. **稳定性优先于短期得分**：必须保留历史锚点、版本对照和独立回归集，防止循环博弈、模式坍塌、共谋退化和灾难性遗忘。
5. **默认最小权限**：外部输入只能触发观测或候选生成，不能直接触发自修改、权重写入、热加载或权限升级。
6. **训练路径与 serving 路径隔离**：在线教师蒸馏、LoRA 更新和高成本评估属于训练/研究路径；默认 serving 仍经既有模型路由、预算和执行策略。
7. **不把研究假设写成实现承诺**：预测编码、世界模型、多教师蒸馏和 Agent-Agent 协同只有在完成独立 spike 与 promotion criteria 后，才能进入工程任务或 ADR。

---

## 三、统一自进化闭环

```text
运行观测
  → 事件/会话/轨迹抽取
  → 质量评估与奖励信号
  → 信用分配与变化归因
  → 候选改进生成
  → 安全、权限、资源和语义检查
  → 行为回归与独立锚点评估
  → 版本提交/发布或拒绝并回滚
  → 结果审计，进入下一轮观测
```

每一轮必须产生可审计的 `EvolutionAttempt` 记录，至少关联：

- `subject_version`：被改进主体的版本；
- `parent_version`：候选的父版本；
- `evidence_refs`：事件、会话或轨迹证据引用；
- `evaluation_refs`：评估器、数据集、基线和结果；
- `mutation_kind`：prompt、skill、DSL、策略配置或权重；
- `authorization`：执行模式、审批结果和授权主体；
- `decision`：accepted、rejected、rolled_back 或 read_only；
- `resource_cost`：模型调用、token、时间和并发资源；
- `rollback_ref`：可恢复的父版本或 session fork。

`EvolutionAttempt` 是架构概念，不代表当前已有同名 C++ 类型。具体 schema 需要由后续 ADR 定义，不能直接把研究文档中的字段当作稳定 API。

---

## 四、支撑平面

### 4.1 证据与轨迹平面

已有基础：EventLog、SessionManager、Session 4-scope、EventBuilder 和 D10 Distillation Capture 契约。近期应由 Trajectory IR 将运行时事件转换为独立训练/评估视图，避免训练格式反向耦合执行图。

约束：

- 默认 append-only、可追溯、可按权限过滤；
- prompt、response、工具参数和用户数据遵循敏感信息捕获策略；
- 轨迹抽取失败不能伪造成功评估；
- 轨迹共享必须由 capture mode、agent scope 和治理策略共同决定。

### 4.2 评估、奖励与信用分配平面

`IEvaluator/RewardSignal` 是当前已批准的评估契约，覆盖质量、成功/失败和 retryable 判断等基础场景。T19 GEPA MVP V1 已在此平面接通同步的失败轨迹反思、候选评估与 MutationGovernor 授权提交（2026-08-27）；T21 Prompt Evidence Gate V1（2026-08-28）进一步为 prompt 类变异对象提供 Go/Conditional/No-Go 质量门控（parse-valid 阈值 + IEvaluator V2 评估），使 B7 自进化的 prompt 修订具备客观吸收/拒绝标准；T20 AFlow MCTS V1（2026-08-28）在这一平面接通**搜索空间自动发现**：以 5 轴模板实例化工作流图为搜索状态，UCB1 选择/扩展/模拟/反向传播 + IEvaluator V2 加权奖励 + BehavioralRegressionGate 回归门 + MutationGovernor L1 workflow variants 授权，使自进化从"失败反思修订"扩展至"结构化搜索最优工作流"（C2）；信用分配仍是缺口，应在引入多主体协同前单独定义：

- 版本前后对照与固定基线；
- 环境、对手、任务难度变化的分层记录；
- 反事实或差分评估；
- 多目标结果的可解释聚合，而不是无来源的加权总分。

在信用分配未定义前，Agent-Agent 与 Agent-Environment 的“能力提升”只能作为相关性观察，不得作为自动变异的充分条件。

### 4.3 预测与非平稳性平面

预测编码可以实现为世界模型、对手策略预测、任务难度预测或一般的预测误差监控。它不是当前 HydraForge 的必需运行时组件。对于近期 GEPA/Prompt 反思，优先使用实际执行反馈和基线回归，不引入常驻世界模型。

若未来引入预测模型，必须额外回答：预测误差是否只用于诊断、是否进入 RewardSignal、如何防止错误预测驱动变异，以及预测模型本身如何评估和回滚。

### 4.4 稳定性、探索与语义对齐平面

协同进化至少需要以下保护机制：

| 机制 | 目的 | 当前状态 |
|---|---|---|
| 历史最佳/固定基线 | 防止相对指标虚假提升 | 行为回归可承载，策略尚需明确 |
| 多样性与反共谋检测 | 防止循环博弈、模式坍塌、双方放宽标准 | 未定义 |
| 安全探索边界 | 防止生成不可解任务或不可逆变异 | ApprovalPolicy/SafeExec 可部分承载 |
| 表征/语义锚点 | 防止多方 embedding 或概念漂移 | 未定义，当前单编排器不强制需要 |
| 资源感知调度 | 控制更新频率、并发和成本 | DomainWorkerPool、Budget、SLM routing 可部分承载 |
| 因果诊断 | 解释性能变化来源 | EventLog 可提供证据，归因算法未定义 |

这些能力不能通过“增加日志字段”自动获得；每一项都需要可验证的指标和失败处理策略。

### 4.5 更新与知识传递平面

更新器按对象选择：

- Prompt/Skill/DSL：候选生成、反思、搜索、规则变异和行为回归；
- 可微模型参数：反向传播、蒸馏、强化学习或 LoRA；
- 不可微策略、任务参数和调度超参数：进化策略、贝叶斯优化或离散搜索；
- 多目标场景：约束优化、分层目标或梯度冲突协调。

在线教师蒸馏建议采用“候选学生 → 独立验证 → 门控吸收 → 异步蒸馏”的训练期模式。教师不是无条件可信源，也不应作为默认 serving 常驻大模型。多教师池需要额外的策略选择、版本治理和模式覆盖评估，暂不纳入当前工程承诺。

---

## 五、HydraForge 映射与当前边界

| 自进化组件 | 可复用项目能力 | 当前缺口/限制 |
|---|---|---|
| 观测与审计 | EventLog、EventBuilder、IInteractionBus | 进化事件主题和 `EvolutionAttempt` schema 尚未批准 |
| 会话与证据 | SessionManager、Session 4-scope、D10 Capture | 采集启用、抽取和训练数据流水线未完全实现 |
| 轨迹视图 | ADR-0061-06 v1.1 独立 Trajectory IR | ✅ 已 ship (2026-08-27/29): `include/agenticdsl/ir/trajectory_ir.h` + `src/modules/ir/trajectory_ir_backend.cpp` + `src/core/parsed_graph_to_trajectory_ir.cpp` (T15, 9 cases / 55 assertions, ParsedGraph 零修改) |
| 评估信号 | ADR-0083 IEvaluator/RewardSignal (✅ Approved, 代码 ship 2026-08-26) | ✅ IEvaluator 已 ship (tests/test_evaluator.cpp 12 cases / 31 assertions); 多主体信用分配未定义 |
| **信用分配契约** | **ADR-0086 v1.0+v1.1 ✅ Approved (2026-09-20)** — `agenticdsl::evolution` namespace AttributionRecord/VersionPairDiff/ConfounderRecord 5 态混杂分层 + HarnessChange v1.1 + kMinBaselineSamples=5 + judge_data_freshness() 算法 stub + GenomeVersion 单一所有权 + 默认 fail-closed | ✅ **已 ship (merge commit `886def1`)**: test_credit_assignment **12 cases / 40 assertions PASS** + `agenticdsl_evolution` 静态库; G16 Closed; C3 h-d-m-transition-guard 现可 fill; Phase 6c MetaRSI-v1 hard prerequisite |
| **H→D→M Transition Guard v1.0** | **ADR-0088 v1.0 ✅ Approved (2026-09-20)** — `agenticdsl::evolution` namespace EvolutionState 5 态 (Idle/Harness/Data/Model/Done) + can_transition 编译期 5×5 矩阵 + evaluate_readiness 三条件门控 (Attributed + 回归门 + 预算, 累积报告非短路) + reset_to_idle() 显式 API + 复用 IEvaluator/IBudgetController/AttributionRecord; D1-D4 + D7-D9 部分 ship, D5/D6/D8 主题注册 deferred to Sprint 34+ follow-up | ✅ **已 ship (merge commit `7a15744`)**: test_transition_guard **13/13 cases / 47 assertions PASS** + `agenticdsl_evolution` 静态库扩展 transition_guard.cpp + openspec archive (8 specs); Oracle post-ship verdict **ALIGNMENT SCORE 62 / NEEDS_FIX** (2 critical + 4 major + 4 minor debt); C3 h-d-m-transition-guard v1.0 部分 ship, C4 harness-rsi-pilot unblocked; 2 Sprint 34+ follow-up 已正式登记: `2026-09-20-ig-genome-registry-walk-ancestors` (D5/D6/D9) + `2026-09-20-adr-0068-appendix-a-evolution-themes` (D8 主题注册); **Phase 6c MetaRSI-v1 C3 关键路径 ship** |
| 变异对象 | ADR-0074 Prompt Evidence、ADR-0061-03 SkillCompiler、DSL 资产 | L2/L3 候选生成和版本化尚未完整实现 |
| 变异治理 | ADR-0084 ✅ Approved + V1 gate-and-audit 代码 ship (G11 ✅ Closed 2026-08-26, commit `a2b2d52`); ApprovalPolicy/ExecutionPolicy 可复用 | ✅ MutationGovernor 已 ship (13 cases / 139 assertions); 自动提交经 gate-and-audit 门禁后允许 |
| 稳定性门 | ADR-0061-02 行为回归、历史版本、SLM routing | 防共谋、多样性、语义对齐指标未定义 |
| 运行资源 | IBudgetController、DomainWorkerPool、stop_token、SLM 路由 | 进化任务调度策略未形成独立契约 |
| 蒸馏输出 | ADR-0061-13 DistillationRecord/IDistillationWriter (✅ Approved, 代码 ✅ 已 ship 2026-08-29) | ✅ `include/agenticdsl/contract/idistillation_writer.h` + `src/modules/distillation/file_writer.{h,cpp}` + `trajectory_bridge.{h,cpp}` (commits `11d3515` + `9a781f8`, capture-mode-and-distillation-writer-v1 archived, 21 cases PASS); 训练管线与模型回流仍依赖外部 AgenticMind |
| 环境/对手共进化 | EnvBackend、Agent Composition 契约骨架 | 尚无成熟 Agent-Agent 或世界模型运行时 |
| **Genome 版本提交/发布 (闭环第 7 环)** | **ADR-0088 + C2 genome-registry + C4 harness-rsi-pilot** — `IGenomeRegistry` (C2 ship) + `apply_harness_mutation` Gate 3 persist-before-apply (G4 wiring) + `GEPALoop::reflect_and_commit` fork 持久化 | ✅ **已接线 (genome-wiring-harness-rsi-gepa, 2026-09-22)**: Gate 3 `fork(name, parent_version, final_spec)` 在内存 apply 之前持久化, 失败零状态变更; `undo_applied_mutation` 快照式回滚; `genome.committed`/`genome.persist_failed` 2 事件注册 (ADR-0068 Appendix A v2.3); GEPA persist-then-commit (version_id = name@N) |

### 当前允许的最小闭环

1. 读取已授权的事件/会话证据；
2. 由 IEvaluator 产生可解释评估；
3. 生成 prompt/skill/DSL 候选；
4. 在只读或显式审批模式下进行行为回归；
5. 输出候选、评估和拒绝原因，默认不自动提交。

### 当前禁止的自动行为

- 外部输入直接修改 prompt、skill、DSL 或权重；
- 未通过独立回归和授权的候选热加载；
- 默认 serving 路径常驻教师模型或在线改变权重；
- 以单次成功、相对胜负或预测误差直接触发提交；
- 在没有信用分配和对照基线时宣称多主体能力提升。

---

## 六、阶段路线

| 阶段 | 目标 | 允许的自动化 | Promotion criteria |
|---|---|---|---|
| S0 证据闭环 | Event/Session → Trajectory IR → IEvaluator → 回归报告 | 只读抽取和评估 | 证据可追溯、敏感数据策略有效、失败不丢失 |
| S1 反思候选 | GEPA/反思生成 prompt 或 skill 候选 | 只读生成，不提交变异 | 候选可重放、评估可复现、行为回归无退化 |
| S2 受治理变异 | ADR-0084 批准后允许显式授权提交 | Prompt/Skill/DSL 分级提交 | 审计、回滚、审批、版本固定和攻击面测试通过 |
| S3 训练期蒸馏 | 教师/学生异步蒸馏与输出数据管线 | 训练环境内门控吸收 | 独立验证优于父版本，成本预算满足，serving 隔离 |
| S4 协同进化研究 | Agent-Agent、Agent-Environment、Meta | 仅 spike 或沙箱 | 信用分配、稳定性、防共谋、语义对齐和资源模型完成独立评审 |

T19 GEPA 在 S1 阶段只能执行只读反思；S2 之前不得执行 `commit(PromptEdit)`。T22 Fine-tune 属于事件驱动的训练路径，不是当前 serving 的默认自修改机制。

---

## 七、需要继续形成的架构决议

1. ~~**ADR-0084**（2026-08-26 文件已创建 🔍 Proposed）~~ ✅ **已完成 (2026-08-26, G11 Closed)**：变异对象 L1-L4 分级 / 授权绑定复用 ADR-0004+ADR-0031 / 治理流程 propose→evaluator→回归门→commit / 审计复用 ADR-0080 + ADR-0068 amendment 注册 4 个 `mutation.*` 主题 / 失败回滚 / 攻击面 fail-closed — **ADR-0084 ✅ Approved + V1 gate-and-audit 代码 ship (commit `a2b2d52`, 13 cases / 139 assertions, ctest 187/187 PASS)**；
2. ~~**IEvaluator 代码 ship**（ADR-0083）~~ ✅ **已完成 (2026-08-26)**：`include/agenticdsl/contract/ievaluator.h` + `reward_signal.h` + TaskSuccessEvaluator V1 + CognitiveWorker/DomainWorkerPool setter 注入 + evaluation.result 事件发射，`tests/test_evaluator.cpp` 12 cases / 31 assertions PASS（V2 BehavioralEquivalence/Composite 评估器留 follow-up `ship-evaluator-v2-composite`）；
3. ~~**Trajectory IR 工程实现**（ADR-0061-06 v1.1 ✅）~~ ✅ **已完成 (2026-08-27/29, T15)**：序列化视图、敏感字段和版本兼容已 ship — `include/agenticdsl/ir/trajectory_ir.h` + `src/modules/ir/trajectory_ir_backend.cpp` + `src/core/parsed_graph_to_trajectory_ir.cpp`，9 cases / 55 assertions PASS，ParsedGraph 零修改；
4. ~~**IDistillationWriter 代码 ship**（ADR-0061-13 ✅）~~ ✅ **已完成 (2026-08-29)**：`include/agenticdsl/contract/idistillation_writer.h` + `distillation_record.h` + 3 文件分离实现 (`src/modules/distillation/`)，commits `11d3515` + `9a781f8`；
5. 进化事件与 `EvolutionAttempt` schema：引用关系、幂等性和审计查询；
6. ~~信用分配契约：单主体与多主体评估的归因边界（建议预估 `adr-0085-credit-assignment-contract.md`，1+2 sprint spike + ADR）~~ ✅ **已立项 + 已 ship (2026-08-31 立项 / 2026-09-20 ship v1.0+v1.1, merge commit `886def1`)**：[`../adr/adr-0086-credit-assignment-contract.md`](../adr/adr-0086-credit-assignment-contract.md) ✅ **Approved (v1.1, 2026-09-20)** — 文件名修正为 0086 (0085 已被横切 Pattern PDK 占用)；评估层 vs 归因层划界 + VersionPairDiff V1 + ConfounderRecord 5 态混杂分层 (v1.1 +`ConfounderKind::HarnessChange`) + kMinBaselineSamples=5 基线门控 + judge_data_freshness() 数据时效性算法 stub (v1.1) + GenomeVersion 单一所有权 (v1.1) + 默认 NotAttempted fail-closed；**G16 Closed**, **Phase 6c MetaRSI-v1 hard prerequisite, C3 h-d-m-transition-guard 现可 fill**；
6a. ~~**H→D→M Transition Guard 状态机** (Phase 6c MetaRSI-v1 C3 关键规则强制)~~ ✅ **已立项 + 已 ship v1.0 (2026-09-20 rdd-arch 立项 → rdd-planner improvement + planner-handoff v1.1 → rdd-builder P0 case 1 approve (auto-decision complex) → P2 实施 commit `0ffc637` → openspec archive commit `7a15744`)**：[`../adr/adr-0088-h-d-m-transition-guard.md`](../adr/adr-0088-h-d-m-transition-guard.md) ✅ **Approved (v1.0, 2026-09-20)** — D1 5 态状态机 + D2 EvolutionVerdict + D3 can_transition 编译期矩阵 + D4 reset_to_idle + D7 复用 IEvaluator/IBudgetController/AttributionRecord + D8 evolution.transition.denied + evolution.readiness.denied 主题常量 + D9 walk_ancestors 默认实现; test_transition_guard **13/13 cases / 47 assertions PASS**; **Phase 6c MetaRSI-v1 C3 v1.0 ship** (D1-D4 + D7 + D9 部分 ship);

  **Sprint 34+ follow-up ship 2026-09-20** (Oracle SHIP-with-fixes verdict resolved, 8 atomic commits `a40e9e1` + `9a7fb08` + `cd1e340` + `6e1f8a5` + `e4403c9` + `8d956f8` + `a2f868b` + `bcab2eb`):
  - ✅ D5 walk_ancestors IGenomeRegistry 接口扩展 (5 → 6 public methods, IGenomeRegistry 新增 walk_ancestors virtual method + LineageWalk struct + default impl 返回 NotImplemented, FilesystemGenomeRegistry override 实装 closest-first + self-inclusive + visited set + cross-name rejection)
  - ✅ D6 judge_data_freshness 完整实装 (5 cases: fast-path / cross-name Confounded / not-in-lineage / Harness-changed-after-data / in-lineage-no-change + Critical C2 walk-failure → Insufficient fail-closed; 9 test cases / 51 assertions GREEN via test_genome_walk_ancestors)
  - ✅ Critical C1 type unification (attribution_record.h stub 删除 + signature 改 ::agenticdsl::genome::IGenomeRegistry& + MockRegistry 2 处迁移 derive from genome::IGenomeRegistry + 5 纯虚 override stub)
  - ✅ Critical C3 GenomeError::NotImplemented 新增 (6 → 7 variants, append-at-end 保 ABI compat)
  - ⚠ D8 主题正式注册 (evolution.transition.denied / evolution.readiness.denied 进 ADR-0068 Appendix A) — Sprint 34+ follow-up **仍 pending**: `2026-09-20-adr-0068-appendix-a-evolution-themes` (已 registered, 未启动)

  **G17 Closed**, C4 harness-rsi-pilot unblocked (D5/D6 实装 + judge_data_freshness lineage tests 全 GREEN);
7. 稳定性与反共谋评估：历史锚点、多样性、独立对手/环境和停止条件；
8. 在线教师蒸馏研究协议：教师准入、门控吸收、异步调度、成本和回滚；
9. Agent-Agent/Agent-Environment 协同进化：只有在 S4 promotion criteria 满足后再单独立项。**前置依赖**: C3 H→D→M Transition Guard v1.0 ✅ ship (commit `7a15744` 2026-09-20, G17 Closed per §七 #6a) + 2 Sprint 34+ follow-up (walk_ancestors + 主题注册) + C4 harness-rsi-pilot 验证。S4 promotion criteria (per AGENTS.md notes): C2-C4 全部 ship 后 2 周稳定期。

---

## 八、结论

讨论中提出的轨迹、预测编码、信用分配、反向传播、混合更新、稳定性机制和在线教师蒸馏，构成了从理论到工程落地所需的完整技术视角，但它们的成熟度不同：

- 轨迹、评估、回归、审计和治理是近期工程主线；
- 信用分配、稳定性/反共谋和资源感知更新是协同进化启动前的架构缺口；
- 预测编码、多教师蒸馏和 Meta Co-Evolution 是研究组件，不应成为当前架构的硬依赖；
- 反向传播应被定义为可插拔更新器之一，不能被误写成整个自进化系统的唯一执行机制。

因此，HydraForge 的近期目标应表述为：

> **先构建可追溯、可评估、可回归、可授权和可回滚的单编排器自进化闭环，再用独立 spike 验证协同进化机制，最后决定是否提升为正式运行时能力。**

---

## 九、验证命令

```bash
# 文档存在且关键引用有效
ls docs/architecture/self-evolution-architecture-2026-08.md
ls docs/research/agent-distillation-sota-2026-08.md
ls docs/architecture/capability-application-map-2026-08.md
ls docs/adr/adr-0083-evaluator-reward-contract.md
ls docs/adr/adr-0084-mutation-governance-contract.md

# ADR-0083 状态字段（2026-08-26 代码 ship 后翻转）
grep -m1 "^✅ Approved" docs/adr/adr-0083-evaluator-reward-contract.md
# 预期: ✅ Approved (ship 2026-08-26) — 契约代码已 ship ...

# ADR-0084 状态字段（2026-08-26 评审通过后翻转）
grep -m1 "^✅ Approved" docs/adr/adr-0084-mutation-governance-contract.md
# 预期: ✅ Approved (评审通过 2026-08-26 — V1 gate-and-audit 代码 ship, commit `a2b2d52`)

# IEvaluator 代码 ship 状态（2026-08-26 已 ship）
grep -r "class IEvaluator\|struct IEvaluator" include/agenticdsl/contract/ src/ 2>/dev/null
# 预期 (2026-08-26 ship 后): ≥ 1 命中 (include/agenticdsl/contract/ievaluator.h)

# IDistillationWriter 代码 ship 状态（2026-08-29 已 ship）
grep -r "class IDistillationWriter\|struct IDistillationWriter" include/agenticdsl/contract/ src/ 2>/dev/null
# 预期 (2026-08-29 ship 后): ≥ 1 命中 (include/agenticdsl/contract/idistillation_writer.h)

# 关键引用一致性 — cap-map §二 G10/G11 Closed 与 self-evolution §五 + ADR 文档状态一致
grep -n "G10.*Closed.*代码 ship\|G11.*Closed.*ADR-0084" \
  docs/architecture/capability-application-map-2026-08.md
# 预期: ≥ 1 行

# 检查不应出现在已定义架构中的占位词
! grep -nE "TBD|TODO|待填写|PLACEHOLDER" \
  docs/architecture/self-evolution-architecture-2026-08.md

# 检查核心边界和阶段约束仍存在
grep -n "受治理的单编排器自进化闭环\|不执行.*commit(PromptEdit)\|信用分配\|在线教师蒸馏" \
  docs/architecture/self-evolution-architecture-2026-08.md \
  docs/research/agent-distillation-sota-2026-08.md \
  docs/architecture/capability-application-map-2026-08.md

# ---- 升档 v1.5 (2026-09-23) 新增验证 ----

# §10 升档说明段存在
grep -n "^## 十、v1.5 升档说明" docs/architecture/self-evolution-architecture-2026-08.md

# 三方架构文档配套 (SoT 基线)
test -f docs/architecture/harness-architecture-2026-09.md \
  && test -f docs/architecture/rsi-architecture-2026-09.md \
  && echo "Harness + RSI 配套 SoT 文档存在 ✅"

# Wave 3 Phase 1 ship 状态（2026-09-23 merge f0a5c4b 后）
git log --oneline -1 | grep -E "f0a5c4b" \
  && echo "Wave 3 Phase 1 merge f0a5c4b 在 HEAD 链上 ✅"

# ADR-0078 状态翻牌
grep -m1 "Approved.*Wave 3 Phase 1 Pilot 激活" docs/adr/adr-0078-finetune-base-model.md
```

---

## 十、v1.5 升档说明（2026-09-23）

### 10.1 升档触发条件

本节记录 v1.4 → v1.5 升档的**触发条件**、**已 ship 证据矩阵**、**遗留缺口**、**配套治理文档**。原 🔍 Proposed 状态维持 7 个月，自 Wave 2 (C2 genome-registry) + Wave 2.5 (C4 harness-rsi-pilot) + Pre-Wave3 4-Gate + Wave 3 Phase 1 (ADR-0078 Pilot 激活) 全部 ship 后，于 2026-09-23 升档。

### 10.2 已 ship 5 阶段实证（升档 evidence 矩阵）

| 阶段 | Change | Ship 日期 | 关键证据 |
|------|--------|-----------|----------|
| **Wave 1** (Sprint 34, 2026-09-17) | C0 + C1 + P0 + F1 | 2026-09-17 + 18 | C0 commits `f84dbb3` + `d21ac6f` + `f3fbb9d`; C1 commit `f4766be` (22 case PASS); F1 commit `a96842e` + `9dc3ac8` (5 case / 13 assertion). |
| **Wave 2** (Sprint 35, 2026-09-19-20) | C2 + C3 + walk-ancestors + D8 | 2026-09-19 + 20 | C2 commits `2e7af89`+`a320032`+`839590d`+`b6114c2`+`507eae3` (12 case / 266 assertion PASS); C3 commits `0ffc637`+`94f4ab4`+`421fa62`+`7a15744` (13 case / 47 assertion PASS); walk-ancestors 10 commits `a40e9e1`→`231cd8d` (10 case / 55 assertion PASS); D8 commits `c7187d0`+`f5bbec2`+`5424e91`. |
| **Wave 2.5** (Sprint 36, 2026-09-20-21) | C4 + GO | 2026-09-21 | C4 11 commits `f7f0fe3`→`08aace2` 跨 5 days (9 case / 43 assertion PASS); Decision Record GO `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` (5 判据全绿). |
| **Pre-Wave3 收口门禁** (2026-09-21-22) | G1+G2+G3+G4 | 2026-09-21 + 22 | G1 merge `9709317` (4 case / 22 assertion); G2 merge `dc12a17` (14 case / 51 assertion); G3 merge `a196a09` (4 dry-run test); G4 merge `fb2769f` (22 case / 111 assertion + gepa_phase2 21 case / 46). |
| **Wave 3 Phase 1 Pilot** (2026-09-23) | W3.P1 | 2026-09-23 | merge `f0a5c4b` + SHIP-with-fixes `232eb13` (18 files, +1057/-62 + 5 files, +66/-25; focused ctest 9/9 PASS); **ADR-0078 ✅ Approved Wave 3 Pilot 激活**; 24h cooling-off 计时 2026-09-23T05:33Z → 满点 2026-09-24T05:33Z. |

**累计 5 阶段 ship 实证**: 33 atomic commits + 5 Oracle review sessions (bg_a818a6a1 + bg_9ade564d + bg_89293120 + bg_f55f307f6ffe + bg_6a8e4397 + bg_3c06ae5b + bg_687a5662 + bg_534a2541 + bg_e4eec567 + bg_7fe026cc 等) + AGENTS.md 模式 #10 + #11 沉淀 (`19e0e8d`).

### 10.3 升档边界与不在范围

**已具备 (✅ Source of Truth 范围)**:
- S0 证据闭环: EventLog + Session 4-scope + Trajectory IR + IEvaluator + ADR-0068 Appendix A v2.3 evolution.transition.denied/readiness.denied 主题
- S1 反思候选: GEPALoop V1 + MCTSWorkflowSearch V1 + IEvaluator V2 + BehavioralRegressionGate
- S2 受治理变异: ADR-0084 MutationGovernance (L1-L4 分级 + gate-and-audit) + ApprovalPolicy + 4 个 `mutation.*` 事件
- S3 训练期蒸馏 (Phase 1 最小版): Wave 3 Phase 1 D1+D3+D7 ship, D4-D7 完整管线待 Phase 2
- 信用分配契约: ADR-0086 v1.0+v1.1 + AttributionRecord + VersionPairDiff + ConfounderRecord 5 态混杂分层 + HarnessChange confounder v1.1 + judge_data_freshness fail-closed
- H→D→M 守门: ADR-0088 + C3 TransitionGuard v1.0 + can_transition 5×5 编译期矩阵 + evaluate_readiness 三条件门控
- Genome 版本提交 / 发布 (闭环第 7 环): C2 IGenomeRegistry + C4 harness-rsi-pilot + G4 wiring (fork before apply, commit failure → RegistryRejected + 零状态变更) + GEPALoop persist-then-commit + `genome.committed`/`genome.persist_failed` 事件

**不在 v1.5 Source of Truth 范围**:
- S3 训练期蒸馏完整管线 (D4 LoRA/QLoRA 训练 + D5 评估框架 + D6 AgenticMind 回流 + D7 真实推理) — Wave 3 Phase 2 范畴，待冷却期满后立项
- S4 协同进化 (Agent-Agent / Agent-Environment / Meta Co-Evolution) — 需独立 spike + promotion criteria (信用分配已具备 ✅)，research 阶段
- 协同进化的稳定性指标（多样性 + 反共谋检测 + 语义锚点）—— 见 §四.4 表格中的未定义项

### 10.4 配套治理文档 (三方 SoT 基线)

升档后，本文档是三方架构一致基线的**自进化维度**。其他两份配套：

| 文档 | 维度 | 边界 |
|------|------|------|
| [`harness-architecture-2026-09.md`](./harness-architecture-2026-09.md) (NEW, 2026-09-23) | **Harness** — Agent 的完整配置 + 变更能力 + 持久化 + 守门整体 | Genome CRD `spec.harness` 字段 + apply_harness_mutation + MutationGovernance + IGenomeRegistry + H→D 守门 |
| [`rsi-architecture-2026-09.md`](./rsi-architecture-2026-09.md) (MOVED from `research/`, 2026-09-23) | **RSI** — Harness/Data/Model 三算子叠加策略 | MetaRSI-v1 + 字节 Seed 三篇 + DeepSeek Harness 三权分立 → HydraForge 实施路径 |

**三方关系**: 自进化架构 = 整体闭环 + 5 段流水线定义; RSI 架构 = 三算子拆分 (Data-RSI / Harness-RSI / Model-RSI); Harness 架构 = RSI 中 Harness-RSI 的纵深内容 (数据模型 + 装配 + 变更能力 + 守门 + 持久化). 三者无冲突, 互补. 任何变更任一方必须同步另两方边界段.

### 10.5 升档后维护规则 (修订版)

**v1.5 后, 维护触发**:
- 任何 ADR 状态翻转 → 更新本文 §五 (§十旧 §七) + §七 (新增)
- 任何 Pre-Wave3+Wave 3+ 阶段 ship 状态变化 → 更新本文 §十.2 已 ship 矩阵
- 任何"超出 v1.5 Source of Truth 范围"项目立项 → 先更新本文 §十.3 边界段
- 任何配套文档 (`harness-architecture` / `rsi-architecture`) 变更 → 同步复核本文 §十.4

**保留 v1.0 维护规则**: 研究结论变化只更新研究文档, 不得直接改变本文的批准状态.

### 10.6 不与 ADR 冲突保证

升档依据以下已 ship ADR (v1.5 与现行 ADR 状态一致, 无冲突):
- ✅ ADR-0083 (IEvaluator) - 与本文 §四.2 + §五不冲突
- ✅ ADR-0084 (MutationGovernance) - 与本文 §四.2 + §五 "变异治理" 行不冲突
- ✅ ADR-0086 v1.0+v1.1 (Credit Assignment) - 与本文 §一.3 + §五 "信用分配契约" 行不冲突
- ✅ ADR-0088 (H→D→M Transition Guard) - 与本文 §五 "H→D→M Transition Guard v1.0" 行不冲突
- ✅ ADR-0078 (Fine-tune) - 与本文 §六 "S3 训练期蒸馏" 阶段一致 (Phase 1 ✅ ship, Phase 2 ⏳)
- ✅ ADR-0068 Appendix A v2.3 - 与本文 §五 "evolution.*" 主题对齐

任何未来 ADR 决策若与本文 v1.5 冲突, 必须先升档 / amend 本文档 (经 Oracle dual-agent review + 24h cooling-off) 后才能 ship.

---

**维护规则**: 当 ADR-0084、T15、T19、T21、T22 或任一协同进化 spike 状态变化时，更新本文 §五/§六/§七；研究结论变化只更新研究文档，不得直接改变本文的批准状态。 (v1.5 升档后, 同步更新 §十 升档矩阵 + 边界段 + 配套文档指针)

---

## 十一、承载例: pdk_chat_demo Reference Traceback (2026-09-23)

> **本节定位**: 让抽象的自进化闭环架构变得**可触达**——为每段架构概念 (评估 / 信用 / 治理 / 守门 / Genome) 标记 `examples/pdk_chat_demo/` 内的具体 file/line/test case, 让 future maintainer 第一天就能 grep 到落地路径. 这不是"pdk_chat_demo 必须承载", 而是"目前它是项目内最完整的 reference implementation, 优先以它做承载例".

### 11.1 9 段闭环 → pdk_chat_demo 落地映射

| 闭环阶段 (§三) | 概念 | pdk_chat_demo 落地 | 验证测试 |
|----------------|------|------------------|----------|
| 1. **运行观测** | EventLog + InteractionBus | `examples/pdk_chat_demo/event_handler.{h,cpp}` 接收 BusEvent | `test_chat_session_events.cpp` |
| 2. **事件/会话/轨迹抽取** | SessionManager JSONL | `examples/pdk_chat_demo/main.cpp` 启动后 `SessionManager::open` 自动启用 | pdk-chat-demo-distill-source-survey-2026-08.md 已 ship |
| 3. **质量评估 + 奖励信号** | IEvaluator | `--model deepseek-chat` + RewardSignal 经 CognitiveWorker 注入 (Sprint 22 ship) | `test_chat_session_events.cpp` Case 4 |
| 4. **信用分配 + 变化归因** | `judge_data_freshness` (ADR-0086 v1.1) | **未启用** — 评估 cell 接受 reward signal, 但 credit assignment 路径未 wire 到 pdk_chat_demo (Phase 2) | — |
| 5. **候选改进生成** | `apply_harness_mutation` (C4 ship) | `--model <name>` (`model_command.cpp`) 是 v1 的 prompt delta 来源; C4 完整 mutation 路径**未启用**于 pdk_chat_demo | `test_chat_session_loop_result_ok.cpp` (Loop OK 路径) |
| 6. **安全/权限/资源/语义检查** | MutationGovernance policy | C4/G1 ship 后 `policy.semantic_locked_tools` 可在 pdk_chat_demo 启用 (V2) | — |
| 7. **行为回归 + 独立锚点** | `BehavioralRegressionGate` (Sprint 22) | 命令级 `tree_command.cpp` 已可看 session tree, 但 mutation → regress gate 联动**未 wire** | (待 L2 立项) |
| 8. **版本提交/发布** | `IGenomeRegistry::commit` (C2) + G4 wiring | **V2 缺口** — `load(genome@N) → 重建 ChatSession → 1 turn` 端到端未 wire (Decision Record §3 第 6 项 post-hoc closure gate) | G4 test_harness_rsi_pilot 22/22 (genome.committed 含 genome_version) 但未接入 chat session 实例 |
| 9. **结果审计 + 下一轮观测** | AppendOnlyEventLog (ADR-0080) | 默认开启, 全部 mutation.* / evolution.* 事件进 IInteractionBus 后到 JSONL | `test_chat_session_events.cpp` |

### 11.2 支撑平面 → pdk_chat_demo 落地映射

| 支撑平面 (§四) | pdk_chat_demo 落地 |
|--------------|------------------|
| §4.1 证据与轨迹平面 | `examples/pdk_chat_demo/main.cpp` 默认装载 SessionManager + EventBuilder (L2 ship) + IDistillationWriter (D10 capture mode 可启用, per distill-source-survey) |
| §4.2 评估/奖励/信用平面 | `examples/pdk_chat_demo/tests/test_budget_alert.cpp` (budget 评估) + `skills/` 内嵌 skill 可被 BehavioralRegressionGate 评测 |
| §4.4 稳定性/探索/语义对齐平面 | `domain_worker_pool.cpp` + `budget_agent/` 提供预算与并发; ApprovalPolicy 链 `--fork` Session 4-scope (Phase 2 仍待 V2 完整接入) |
| §4.5 更新与知识传递平面 | `LoopAgent` 提供 React/PlanExecute/ForkJoin 3 循环; **Wave 3 Phase 2 D4-D7** 接入 finetune-base-model provider (Phase 1 已注册 stub, Phase 2 wire `model_command.cpp` 选取) |

### 11.3 升级路径 (pdk_chat_demo → 完整 self-evolution runtime)

> 当前 pdk_chat_demo **已具备**: 9 段闭环的前 4 段 (观测/抽取/评估+奖励/G-Wire 缺) + 第 9 段 (审计); **缺失**: 第 5-8 段的 mutation → governance → regression → commit 端到端 wire.

**Phase 1 ✅ ship (Wave 3, 2026-09-23)**: D7 `LLMProviderFactory::register_dynamic("agenticdsl-llama-3.1-70b-lora-v1", factory_fn)` 注册 finetune stub. **pdk_chat_demo 实际选择该 provider 的入口** → `examples/pdk_chat_demo/commands/model_command.cpp` + `provider_agent/` 需要在 Phase 2 添加 "agenticdsl-llama-3.1-70b-lora-v1" 名字到可用列表.

**Phase 2 ⏳ (Wave 3 Phase 2, cooling-off 满后立项)**:
- D4 LoRA 训练管线接入 (agenticdsl-llama-3.1-70b-lora-v1 服务注入)
- D5 评估框架接入 BehavioralRegressionGate 真实比对 (而非 mock)
- D6 AgenticMind 回流 → IDistillationWriter → Training pipeline 端到端
- D7 serving provider `load(genome@N) → 重建 ChatSession → 1 turn` (per §十 / Decision Record §3 第 6 项)
- 三段 [评估 → mutation → persistence] 完整 wire 入口建议在 `examples/pdk_chat_demo_evolution/` (L2 立项范畴, 详见 §11.4)

### 11.4 承载例 vs Reference Example (L1 + L2 关系)

- **L1 (本文)** — traceback 只标注"每段架构概念**已经**在 pdk_chat_demo 哪里落地" (✅ 已 ship) **或** "将在哪里落地" (⏳ Phase 2)
- **L2 (`pdk_chat_demo_evolution`)** — 独立 example sub-project (≤ 1 周实施), 提供**可运行 harness-rsi + data-rsi + model-rsi reference example**, 不是主线 chat demo 的修改, 而是用 chat session 实例 + LoopAgent 包装一个 evolution-aware 变体. 详见 OpenSpec change `pdk-chat-demo-evolution-reference-example` (即将草案)

> **本文不替代 OpenSpec change**: §X.Y 是**追溯 traceback**, L2 是**改造 + 验证**. 两者互不冲突, 但 L2 立项需先用本文 §X.Y 做导航, 然后再实施.

---

## 十二、Verification Matrix (反向指标门 + 反作弊三模式, 2026-09-23 升级)

> **来源**: per Cross-Doc Review 2026-09-23 + 用户提交 16 模块评审 + L2 spec R8/R9.
> **核心命题**: 任何"自进化能力 ship"必须同时输出**正向 + 反向指标** (per R8.1) + 100% 失败可追溯 (per R8.2) + 消融对照数据 (per R8.3), 缺一不予 merge. 此外, 任何"进化机制"评估必须显式覆盖 3 类已知反作弊场景 (per R9).

### 12.1 反向指标门 (Reverse Indicator Gate) — 应用于 9 段闭环

| 闭环阶段 (§三) | 正向指标 (新涨) | 反向指标 (旧掉, drop_ratio ≤ 5%) | 验证命令 |
|----------------|------------------|----------------------------------|----------|
| 1. 运行观测 | EventBus emit 数 ↑ | 旧 trace 兼容性 break ↓ | `git diff include/agenticdsl/contract/iinteraction_bus.h` |
| 2. 事件抽取 | SessionJSONL 完整度 ↑ | 旧字段 schema 错误率 ↑ | `ctest -R test_session_writer` |
| 3. 质量评估 | IEvaluator Acceptable % ↑ | 旧 prompt delta 验证失败率 ↑ | per `tests/test_evaluator.cpp Case 4` |
| 4. 信用分配 | Confounder 捕获率 ↑ | 旧 `judge_data_freshness` 误判率 ↑ | per `tests/test_genome_walk_ancestors.cpp Case 7/8/9` |
| 5. 候选生成 | Mutation 提议通过率 ↑ | 旧 valid mutation 失败率 ↑ | per `tests/test_harness_rsi_pilot.cpp Case 1` |
| 6. 安全检查 | Gate 拦截率 ↑ | 旧 valid mutation 被误拦率 ↑ | per `tests/test_harness_rsi_pilot.cpp Case 2` |
| 7. 行为回归 | BehavioralRegressionGate PASS % ↑ | 旧 baseline regression false positive ↑ | per `tests/test_gepa_phase2.cpp` |
| 8. 版本提交 | `genome.committed` 成功率 ↑ | 旧 fork 失败 false negative ↑ | per `tests/test_harness_rsi_pilot.cpp Case 3c` |
| 9. 审计闭环 | Audit 覆盖率 ↑ | 旧 audit 不可还原 ↑ | per `tests/test_chat_session_events.cpp` |

**drop_ratio > 5% 自动 block** (R8.1 红线). 任何单阶段反向指标失败必须显式 ack in commit message (Single-Dev 模式 = author ack).

### 12.2 失败→约束可追溯 (Failure Traceability)

**规则**: 任何自进化失败 (test failure / runtime exception / gating denial) 必须能 trace 到具体 contract / hook / event 名称 + 该 contract 的 ship commit hash.

```
failure_event: <event-name>
rule_id: <contract-id>@<commit-sha>  # 例如 ADR-0086 v1.1@886def1
rule_shipped_commit: <commit-sha>
reproduce_in_new_task: <demo-command>  # 在全新任务上也能复现拦截
```

**Acceptance**: 任何 ship gate 在缺 failure_trace 时 block commit message.

#### L2 实例化
[L2 spec R8.2](../../../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) S19-S20:
- S19: 失败样本 trace 输出 4 字段
- S20: failure sample 0% 失配 (任何 failure 必须 trace)

### 12.3 消融实验 (Ablation Experiment)

**规则**: 任何 Harness 变更 (ChatConfig.override_*, apply_harness_mutation, Genome 切换) 必须提供 3 段对照数据:

1. **同任务, 不同 Harness**: baseline_response (旧 Harness) vs post_mutation_response (新 Harness) — eval_quality diff
2. **同 Harness, 不同任务**: baseline task (Golden #1) vs mutation task (Golden #2) — eval_quality 一致性
3. **失败样本 drop ratio**: 旧 failure_samples 在新 Harness 下的保留拦截率 (预期 ≥ 95%)

**Acceptance**: 任何 mutation gate 在 3 段消融数据缺一时 block. Reference: LangChain 排名 30+ → 前 5 对照 (行业基准).

#### L2 实例化
[L2 spec R8.3](../../../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) S21-S22:
- S21: `--ablation-mode=full` flag 跑 3 段对照
- S22: 输出 `ablation_report.json`

### 12.4 反作弊三模式 (Anti-Cheat, per R9)

> 项目当前 5-tier gate (G0/G1/G2/G2.5/G3) 不覆盖 R9 三类已知失效模式. 必须**显式**增强:

| 失效模式 | 来源 | 当前防御 | 需增强 |
|----------|------|---------|--------|
| **R9.1 搜现成答案** (Poolside / Terminal-Bench 2.0) | 评测时 Agent 搜到 baseline 速通指令并复述 | 无显式防护 | L2 test_anti_cheat_search_solution (hint input / non-hint input 两组, eval_quality diff > -10%) |
| **R9.2 修改评判指标** (复旦马兴军团队实测) | Agent 改变衡量指标以"完成"任务 | ⚠️ 部分 (judge_data_freshness fail-closed) — 但评估者本身仍可改 metric | L2 test_anti_cheat_metric_tampering + IEvaluator grep verify 无 write 接口 |
| **R9.3 串谋外部平台** (OpenAI ExploitGym) | Sandbox 模型串联零日漏洞与窃取凭证 → RCE 拿答案 | ❌ 无沙箱级防护 | L2 test_anti_cheat_sandbox_escape + docker backend 默认 network_mode=none |

#### L2 实例化
[L2 spec R9.1-R9.3](../../../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) S23-S27:
- S23: Poolside hint input diff ≤ -10%
- S24: Mutation gate 拒绝 evaluator schema write + emit `evaluation.tampering_attempt` 事件
- S25: IEvaluator 无 write 方法
- S26: Sandbox outbound 拦截
- S27: docker backend 默认 network_mode=none

### 12.5 跨文档一致性 (Cross-doc consistency)

**L2 ↔ SoT 双向引用**:
- L2 spec.md §R8 ↔ 本文 §12.1-12.3 (R8.1 退化测试 + R8.2 失败可追溯 + R8.3 消融实验)
- L2 spec.md §R9 ↔ 本文 §12.4 (R9.1-R9.3 反作弊三模式)
- L2 design.md §十.7 ↔ 本文 §12.4 (R8 + R9 设计交底)
- L2 tasks.md T6 ↔ 本文 §12.5 (R8 + R9 测试用例任务组)

**反向**: 任何 SoT 文档 §十二 更新必须同步引用 L2 spec R8/R9.

### 12.6 验证命令

```bash
# L2 spec R8 + R9 验证 (2026-09-23 后)
test -f openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md \
  && grep "R8.1\|R8.2\|R8.3\|R9.1\|R9.2\|R9.3" \
      openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md \
  && echo "L2 spec R8 + R9 完整 ✅"

# 3 份 SoT 文档 §十二 验证 (反向指标门一致性)
for f in docs/architecture/{self-evolution-architecture-2026-08,harness-architecture-2026-09,rsi-architecture-2026-09}.md; do
  grep -c "## 十二、" "$f"
done
# 预期: 3 行 (1 per file)

# drop_ratio 检查 (R8.1 红线)
# 待 L2 实施 + ship 后由 `./run_evolution_demo.sh --mock --release-metrics` 跑通后验证
# 当前 placeholder: 暂无 metrics.json 校验 (L2 merge 后生效)
```

### 12.7 维保规则

**v1.5 升档后**, 本 §十二 维持:
- 任何 ship gate 强制 R8.1 双向指标输出 (per AGENTS.md "Reverse Indicator Rule" 同步)
- 任何 L2 reference example 强制 R9 三类反作弊覆盖
- 任何 Phase 2+ 立项 (Wave 3 Phase 2 + Wave 4) 强制 R8/R9 引用

**L1 ↔ L2 闭环**:
- L1 (本文 + L2 链接) 静态导航
- L2 (pdk_chat_demo_evolution) 动态验证
- 两者 in sync, 任一变更必须同步另一方 §12 + §R8/R9

### 12.9 上下文驱动约束 (Context-Driven Constraint, 2026-09-23 升级)

> **来源**: 用户原话 "L2 只是提供了用户交互的设施, 具体还要用户提供一个具体上下文请求, 这个上下文请求创建的目标才能做 harness/自进化/rsi 的验证"
> **核心命题**: 任何"自进化能力 ship"必须由 **ContextRequest** 触发, 不是 L2 demo 自身自动跑. L2 是 reference example 入口, **不**是 autonomous evaluator.

#### 12.9.1 ContextRequest 5+3 字段契约

| 字段 | 必填 | 约束 |
|------|------|------|
| `context_id` | ✅ | UUID v4 唯一, **用户**定义 (L2 不自动生成) |
| `turn_input` | ✅ | 用户给 ChatSession 的输入 |
| `task_class` | ✅ | enum (code_gen \| research \| summary \| debug \| ...) |
| `expected_eval_quality` | ⚠️ | Acceptable/Poor/Excellent/null — 用户标注 baseline 期望 |
| `invocation_mode` | ✅ | mock / real_llm_deepseek / real_llm_custom |
| `metadata.is_hidden` | ⚠️ | 默认 false (公开集); true = 隐藏集 (per E2 红线) |
| `metadata.sensitivity` | ⚠️ | public / internal / confidential (per H2 凭证隔离) |
| `metadata.{domain,tags}` | ⚠️ | 用户自填, L2 echo 进 trace |

完整 schema 见 [L2 spec §R13](../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md).

#### 12.9.2 9 段闭环 + ContextRequest 触发矩阵

| 闭环阶段 (§三) | ContextRequest 字段触发 | 反向指标 (R8) |
|----------------|--------------------------|----------------|
| 1. 运行观测 | `metadata.domain` + `tags` | drop_ratio ≤ 5% (跨类) |
| 2. 事件抽取 | `context_id` 进 JSONL | 0% 失配 |
| 3. 质量评估 | `task_class` + `expected_eval_quality` | 跨 ≥ 3 类对比 |
| 4. 信用分配 | `context_id` + `turn_input` | 跨类 `attribution_verdict` 一致性 |
| 5. 候选生成 | `turn_input` + `task_class` | MutationGate 通过率 ≥ baseline - 5% |
| 6. 安全检查 | `metadata.sensitivity` (internal/confidential → L2 不写盘) | confidential 0% 泄漏 |
| 7. 行为回归 | `context_id` 前后对比 | baseline vs post-mutation eval_quality |
| 8. 版本提交 | `context_id` 进 `genome.committed` payload | commit 成功率 ≥ 99% |
| 9. 审计闭环 | `context_id` 进 audit log | 100% 可 trace |

#### 12.9.3 ≥ 3 类 ContextRequest 实证 (per 用户 R3 红线)

> **任何"实现自进化"声称必须由 ≥ 3 类 ContextRequest 实证**, 单类不构成 generalizable.

| ContextRequest 类 | L2 验证 |
|------------------|----------|
| **Code 类** (`task_class: code_gen`) | K8s YAML / Python test / SQL query — harness-rsi 5-tier gate |
| **Research 类** (`task_class: research`) | 文档摘要 / 文献对比 — data-rsi capture-mode=Training |
| **Debug 类** (`task_class: debug`) | 日志分析 / 错误诊断 — model-rsi provider 选择 |

L2 ship 时附 `examples/contexts/{code,research,debug}-class-context.jsonl` 3 个 reference ContextRequest file (per L2 spec R13.3 S32).

#### 12.9.4 与 R8 / R9 的关系

**R13 ↔ R8 (反向指标门)**:
- R8.1 跨 ContextRequest 类 (≥ 3) — 单类退化 ≠ generalizable
- R8.2 失败可追溯 + 必含 `context_id`
- R8.3 消融跨类 — 单类 ≠ generalizable

**R13 ↔ R9 (反作弊)**:
- R9.1 hint input 显式标注于 Agent (R13 test fixture); production ContextRequest **必须无 hint**
- R9.2 mutation_metric_* ContextRequest 自动拒绝
- R9.3 sandbox `network_mode=none` + `turn_input` 含网络关键字自动警告

#### 12.9.5 跨文档一致性

- [L2 spec §R13](../../openspec/changes/pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md) (主契约)
- [`./harness-architecture-2026-09.md` §12.9](./harness-architecture-2026-09.md) (H1-H6 红线 + ContextRequest)
- [`./rsi-architecture-2026-09.md` §12.9 + §11.8.8](./rsi-architecture-2026-09.md) (R3 + R4 红线 + ≥ 3 类实证)
- [AGENTS.md "Reverse Indicator Rule"](../../AGENTS.md) (commit 强制 `[Reverse Indicator]` 段含 `context_ids` 列表)

#### 12.9.6 维保规则

**v1.5 → v1.6 升级触发**:
- L2 ship 后, 本 §12.9 + §十二 §12.1-12.4 双向同步
- ContextRequest schema 变更必须升档 L2 spec R13
- 任何 new ContextRequest 类 (e.g., `multimodal`, `live_data`) 加入时同步 §12.9.3
