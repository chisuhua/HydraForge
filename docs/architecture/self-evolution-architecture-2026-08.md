# 自进化与协同进化架构定义（2026-08）

**生成日期**: 2026-08-26  
**最后验证**: 2026-08-26（v1.2，ADR-0083 ✅ Approved 代码已 ship + ADR-0084 ✅ Approved V1 代码已 ship (G11 ✅ Closed) + IDistillationWriter 代码 ship 待办标注，验证命令见 §九）  
**作者**: Architecture Working Group  
**状态**: 🔍 Proposed

> **定位**: 本文是自进化方向的架构工作文档和 ADR-0084、T19/T20/T22 的证据输入，不是已批准的运行时契约。任何会改变变异权限、事件 schema、训练管线或 serving 行为的决定，必须提升为 ADR 或 ADR amendment。
>
> **核心边界**: HydraForge 当前定义的是“受治理的单编排器自进化闭环”，不是已经实现的多智能体协同进化平台。Agent-Agent 对等协同、在线权重更新、多教师池和 Meta Co-Evolution 均属于后续研究方向。
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
- 信用分配需要反事实、差分、版本对照或因果实验等机制，目前不是 HydraForge 已批准的通用能力；
- 在信用分配缺失时，系统不得把相对胜负、单次成功或环境变简单认定为自身能力提升。

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

`IEvaluator/RewardSignal` 是当前已批准的评估契约，覆盖质量、成功/失败和 retryable 判断等基础场景。信用分配仍是缺口，应在引入多主体协同前单独定义：

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
| 轨迹视图 | ADR-0061-06 独立 Trajectory IR | T15 尚需工程实现 |
| 评估信号 | ADR-0083 IEvaluator/RewardSignal (✅ Approved, 代码 ship 2026-08-26) | ✅ IEvaluator 已 ship (tests/test_evaluator.cpp 12 cases / 31 assertions); 多主体信用分配未定义 |
| 变异对象 | ADR-0074 Prompt Evidence、ADR-0061-03 SkillCompiler、DSL 资产 | L2/L3 候选生成和版本化尚未完整实现 |
| 变异治理 | ADR-0084 ✅ Approved + V1 gate-and-audit 代码 ship (G11 ✅ Closed 2026-08-26, commit `a2b2d52`); ApprovalPolicy/ExecutionPolicy 可复用 | ✅ MutationGovernor 已 ship (13 cases / 139 assertions); 自动提交经 gate-and-audit 门禁后允许 |
| 稳定性门 | ADR-0061-02 行为回归、历史版本、SLM routing | 防共谋、多样性、语义对齐指标未定义 |
| 运行资源 | IBudgetController、DomainWorkerPool、stop_token、SLM 路由 | 进化任务调度策略未形成独立契约 |
| 蒸馏输出 | ADR-0061-13 DistillationRecord/IDistillationWriter (✅ Approved, 代码 ship 待办 2026-08-26 自审) | IDistillationWriter 类代码不存在 (grep 0 命中); 训练管线与模型回流依赖外部 AgenticMind |
| 环境/对手共进化 | EnvBackend、Agent Composition 契约骨架 | 尚无成熟 Agent-Agent 或世界模型运行时 |

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
3. **Trajectory IR 工程实现**（ADR-0061-06 v1.1 ✅）：序列化视图、敏感字段和版本兼容 — **T15 启动 Sprint 25**；
4. **IDistillationWriter 代码 ship**（ADR-0061-13 ✅）：`include/agenticdsl/contract/idistillation_writer.h` + `distillation_record.h` + 3 文件分离实现 — **2026-08-26 自审识别代码不存在，待 OpenSpec task 排期**；
5. 进化事件与 `EvolutionAttempt` schema：引用关系、幂等性和审计查询；
6. 信用分配契约：单主体与多主体评估的归因边界（建议预估 `adr-0085-credit-assignment-contract.md`，1+2 sprint spike + ADR）；
7. 稳定性与反共谋评估：历史锚点、多样性、独立对手/环境和停止条件；
8. 在线教师蒸馏研究协议：教师准入、门控吸收、异步调度、成本和回滚；
9. Agent-Agent/Agent-Environment 协同进化：只有在 S4 promotion criteria 满足后再单独立项。

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

# IDistillationWriter 代码 ship 状态
grep -r "class IDistillationWriter\|struct IDistillationWriter" include/agenticdsl/contract/ src/ 2>/dev/null
# 预期 (2026-08-26): 0 命中 — 代码 ship 待办

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
```

---

**维护规则**: 当 ADR-0084、T15、T19、T21、T22 或任一协同进化 spike 状态变化时，更新本文 §五/§六/§七，并同步能力地图 §八；研究结论变化只更新研究文档，不得直接改变本文的批准状态。
