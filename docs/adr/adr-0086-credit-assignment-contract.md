# ADR-0086: 信用分配契约 (Credit Assignment Contract)

**日期**: 2026-08-31
**状态**: 🔍 **Proposed** (评审通过后 flip to ✅ Approved；按 self-evolution §七 #6 建议形式: spike + ADR, V1 不强制实施)
**父主题**: Phase 6 Agent 自进化方向 (self-evolution §一.1.3 + §四 4.2 + §七 #6)
**前置 ADR**:
- ADR-0083 (✅ Approved + V2 Shipped) — IEvaluator/RewardSignal 评估契约 ("表现如何")
- ADR-0084 (✅ Approved + V1 Shipped) — MutationGovernance ("是否允许提交")
- ADR-0080 (✅ Approved, v1.1/v1.2 amendments) — AppendOnlyEventLog (证据层)
- ADR-0061-02 (✅ Approved + T14 Shipped) — 行为回归 ("等价性判定")
- ADR-0079 (✅ Approved, v1.1) — Session 4-Scope (版本固定基础)
- ADR-0061-06 v1.1 (✅ Approved + T15 Shipped) — TrajectoryIR (轨迹序列化视图)

**关联文档**:
- `docs/architecture/self-evolution-architecture-2026-08.md` §一.1.3 + §四 4.2 + §五 + §六 S4 + §七 #6 — 信用分配的定位、边界与禁止行为
- `docs/architecture/agent-orchestration-architecture-2026-08.md` v1.5 §十八 §18.10.1 + §十七 — cognitive_domain chain 的信用分配触发条件
- `openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` — Phase 1 启动前置 blocker = 本 ADR 立项

**最后更新**: 2026-08-31

## 状态

🔍 **Proposed** (2026-08-31, 评审通过后 flip to ✅ Approved；按 self-evolution §七 #6 建议形式: spike + ADR, V1 不强制实施)

**前置文档**:
- `docs/architecture/self-evolution-architecture-2026-08.md` §一.1.3 + §四 4.2 + §五 + §六 S4 + §七 #6
- ADR-0083 (IEvaluator/RewardSignal ✅ Approved + V2 Shipped) — 评估层边界参照
- ADR-0084 (MutationGovernance ✅ Approved + V1 Shipped) — 治理集成参照
- `openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` — Phase 1 启动前置 blocker = 本 ADR 立项

---

## 背景

### 缺口定位

`self-evolution-architecture-2026-08.md` §一.1.3 明确划界:

> `IEvaluator/RewardSignal` 负责回答"候选表现如何"；信用分配负责回答"**表现变化应归因于哪个主体、哪个变异、哪个环境变化**"。二者不能混为同一个接口。
> - 信用分配需要**反事实、差分、版本对照或因果实验**等机制，目前不是 HydraForge 已批准的通用能力；
> - 在信用分配缺失时，系统**不得**把相对胜负、单次成功或环境变简单认定为自身能力提升。

§四 4.2 列出信用分配的 4 个必备机制:

1. **版本前后对照与固定基线**
2. **环境、对手、任务难度变化的分层记录**
3. **反事实或差分评估**
4. **多目标结果的可解释聚合**，而不是无来源的加权总分

§五 明确禁止行为:

> 在信用分配未定义前，Agent-Agent 与 Agent-Environment 的"能力提升"只能作为**相关性观察**，不得作为自动变异的充分条件。

§六 S4 协同进化的 promotion criteria 首项即信用分配；§七 #6 建议以 "1+2 sprint spike + ADR" 形式立项（原建议文件名 `adr-0085-credit-assignment-contract.md` 已过期 — 0085 已被横切 Pattern PDK 占用，本 ADR 用 0086）。

### 当前已 ship 基础设施 (本 ADR 复用, 不重建)

| 基础设施 | 回答的问题 | 状态 |
|---------|-----------|------|
| IEvaluator/RewardSignal (ADR-0083 V2) | "这次执行好不好" (quality + scalar + confidence) | ✅ ship |
| BehavioralRegressionGate (T14) | "两个版本是否等价" (Hotelling T²) | ✅ ship |
| MutationGovernor (ADR-0084 L1) | "是否允许提交变异" (propose→evaluator→回归门→commit) | ✅ ship |
| mutation.* 4 主题 (ADR-0068 v1.2.1) | "变异生命周期" (proposed/committed/reverted/denied) | ✅ ship |
| Session 4-Scope (ADR-0079) | "版本固定" (Conversation/Attempt/Step/Execution) | ✅ ship |
| TrajectoryIR (ADR-0061-06 v1.1) | "轨迹序列化" (训练/评估独立视图) | ✅ ship |
| AppendOnlyEventLog (ADR-0080) | "证据存储" (append-only + causal_time) | ✅ ship |

**缺口**: 上述组件各自回答了自进化闭环的**单点问题**，但**没有任何一个组件回答"这次提升/下降归因于谁"**。例如:
- GEPALoop reflect_and_commit 成功提交了新 prompt → eval score +0.08。**这 +0.08 归因于新 prompt，还是任务恰好变简单，还是 evaluator 噪声？** — 当前无法回答。
- MCTSWorkflowSearch 发现新 chain eval 0.71 vs 旧 chain 0.65。**+0.06 归因于 chain 结构，还是 specialists 各自贡献，还是 environmental drift？** — 当前无法回答。

这正是 §五 禁止行为试图防止的误判场景。

## 决策

### 决策 1 — 信用分配与评估信号的接口边界 (核心)

**信用分配不是 IEvaluator 的扩展**，而是**独立的归因层**，消费 IEvaluator 输出 + EventLog 证据 + 版本信息，产出归因报告:

```cpp
namespace agenticdsl {

// 评估层 (ADR-0083 已有, 不修改): "表现如何"
struct RewardSignal { Quality quality; double scalar; double confidence; };

// 归因层 (本 ADR 新增): "表现变化归因于谁"
struct AttributionRecord {
  std::string attribution_id;              // 唯一 ID
  std::string subject_version;             // 被归因主体版本 (e.g. "prompt_v3", "chain_reflect_search_compile")
  std::string parent_version;              // 对照基线版本 (e.g. "prompt_v2", "chain_reflect_compile")
  double eval_delta;                       // 评估差分 (child_eval - parent_eval, 绝对值)
  AttributionMethod method;                // 归因方法 (决策 2)
  AttributionVerdict verdict;              // 归因判定 (决策 3)
  double confidence;                       // 归因置信度 [0,1]
  std::vector<ConfounderRecord> confounders;  // 混杂因素记录 (决策 4)
  std::string evidence_refs;               // EventLog causal_time 引用 (ADR-0080)
  std::uint64_t timestamp_ms;
};

enum class AttributionMethod {
  VersionPairDiff,      // 版本对差分 (V1 唯一实装)
  Counterfactual,       // 反事实 (V2, deferred)
  CausalExperiment,     // 因果实验 (V2, deferred)
  ObservationalOnly,    // 仅相关性观察 (兜底, 禁止用于自动变异)
};

enum class AttributionVerdict {
  Attributed,           // 归因成立: eval_delta 在混杂控制下仍显著 (可支撑自动变异决策)
  Confounded,           // 归因存疑: 存在未控制混杂 (任务难度/环境/对手变化)
  Insufficient,         // 证据不足: 样本量/基线不稳 (只能作为相关性观察)
  NotAttempted,         // 未归因: ObservationalOnly 方法 (默认安全态)
};

struct ConfounderRecord {
  enum class Kind { TaskDifficulty, Environment, Opponent, EvaluatorDrift, ResourceChange };
  Kind kind;
  std::string description;               // e.g. "任务集从 hard_set 切换到 easy_set"
  double estimated_impact;               // 对 eval_delta 的估计影响 (可选, -1 = 未估计)
  bool controlled;                       // 是否已被控制 (分层记录是否完整)
};

} // namespace agenticdsl
```

**接口边界 (与 ADR-0083 零重叠)**:
- IEvaluator 输入 ExecutionTrace, 输出 RewardSignal — **单点评估**
- 本 ADR 输入 (child_version, parent_version, evidence_refs), 输出 AttributionRecord — **双版本归因**
- 本 ADR **不调用 LLM, 不执行 specialist, 不修改 IEvaluator** — 纯归因判定逻辑

### 决策 2 — V1 唯一归因方法: 版本对差分 (VersionPairDiff)

V1 仅实装**版本对差分**，它是 4 个必备机制 (§四 4.2) 中唯一可在当前基础设施上确定性实现的方法:

```cpp
// V1 归因算法 (确定性, 无 LLM):
AttributionRecord attribute_version_pair(
    const VersionSnapshot& child,      // 当前版本 (含 eval score + evidence)
    const VersionSnapshot& parent,     // 基线版本 (固定, ADR-0079 Session 4-Scope)
    const std::vector<ConfounderRecord>& confounders) {

  AttributionRecord rec;
  rec.method = AttributionMethod::VersionPairDiff;
  rec.eval_delta = child.eval_score - parent.eval_score;

  // 1. 混杂检查: 存在未控制混杂 → Confounded
  for (const auto& c : confounders) {
    if (!c.controlled) {
      rec.verdict = AttributionVerdict::Confounded;
      rec.confidence = 0.0;
      return rec;
    }
  }

  // 2. 基线稳定性检查: parent 样本不足 → Insufficient
  if (parent.sample_count < kMinBaselineSamples /* 默认 5 */) {
    rec.verdict = AttributionVerdict::Insufficient;
    rec.confidence = 0.0;
    return rec;
  }

  // 3. 差分显著性: |eval_delta| 超过 evaluator 噪声带 → Attributed
  //    (噪声带 = parent.eval_stddev * 2, 复用 T14 Hotelling T² 的方差估计)
  if (std::abs(rec.eval_delta) > 2.0 * parent.eval_stddev) {
    rec.verdict = AttributionVerdict::Attributed;
    rec.confidence = std::min(1.0, std::abs(rec.eval_delta) / (4.0 * parent.eval_stddev));
  } else {
    rec.verdict = AttributionVerdict::Insufficient;  // 差异在噪声带内, 只能相关性观察
    rec.confidence = 0.0;
  }
  return rec;
}
```

**反事实 / 因果实验 (V2 deferred)**: 需独立的重放基础设施 (replay harness) + 任务难度分层数据集, 当前无 ship 基础, 不在 V1 范围。

### 决策 3 — 归因判定与自动变异的关系 (治理绑定)

| AttributionVerdict | 是否可作为 MutationGovernor commit 依据 | 含义 |
|--------------------|----------------------------------------|------|
| `Attributed` | ✅ 可以 (仍需走 ADR-0084 完整 gate: evaluator → 回归门 → governor) | 归因成立 |
| `Confounded` | ❌ 禁止 | 存在未控制混杂, 只能相关性观察 |
| `Insufficient` | ❌ 禁止 | 证据不足, 只能相关性观察 |
| `NotAttempted` | ❌ 禁止 (默认安全态) | 未归因, fail-closed |

**治理集成 (不改 ADR-0084)**: MutationGovernor.commit() 的前置条件从 "evaluation_refs 非空" 增强为 "evaluation_refs 非空 **且** attribution_verdict == Attributed (当 attribution 已提供时)"。V1 阶段 attribution 为**可选输入** — 未提供时维持 ADR-0084 v1.0 行为 (单主体自进化 S1-S2 阶段不强制); **S4 协同进化阶段强制要求 attribution == Attributed**。

### 决策 4 — 混杂因素分层记录 (复用现有事件层)

混杂因素通过**复用现有事件主题**分层记录, 不新增基础设施:

| Confounder Kind | 记录来源 (已 ship) | 事件主题 |
|-----------------|-------------------|---------|
| TaskDifficulty | Session 4-Scope (ADR-0079) + TrajectoryIR | `session.*` |
| Environment | EnvBackend (ADR-0075) | `env.*` |
| Opponent | (Agent-Agent 场景, S4 未启动) | — (V2) |
| EvaluatorDrift | IEvaluator 版本固定 (ADR-0083) + mutation.* | `mutation.*` |
| ResourceChange | IBudgetController + DomainWorkerPool | `domain.task.*` |

`controlled = true` 的判据: 该混杂的来源事件层**在归因窗口内有完整记录** (append-only, 可回放)。未记录 → `controlled = false` → 归因判定 Confounded。

### 决策 5 — 事件主题注册 (ADR-0068 Appendix A v1.9)

本 ADR 新增 2 个主题 (随 V1 ship 同步提交 Appendix A v1.9 amendment; v1.8 已被 axis6.* 占用):
- `attribution.recorded` — 归因记录写入 (payload: attribution_id, subject_version, parent_version, verdict, confidence)
- `attribution.confounded` — 混杂检测告警 (payload: attribution_id, confounder_kind, description)

owner=attribution 模块 (新增 `src/common/attribution/` 或 `src/modules/attribution/`, Phase 1 决定)。

### 决策 6 — 与 Axis6 / 自进化应用的对接

| 应用 | 信用分配的角色 | 对接点 |
|------|--------------|--------|
| **B7 自进化 (GEPA)** | 每次 reflect_and_commit 的 eval_delta 归因 | GEPALoop commit 前可选调用 attribute_version_pair (V1 可选) |
| **Axis6 chain 搜索** | 新 chain vs 旧 chain 的 eval_delta 归因 (chain 级, 而非单版本) | MCTSWorkflowSearch commit_chain 前调用 (Phase 1, 前置 = 本 ADR ship) |
| **B6 蒸馏环境** | 教师→学生行为克隆的提升归因 | DistillationRecord 评估时调用 (V2) |
| **S4 协同进化** | 多主体提升的归因边界 | **强制**: 无 Attributed 判定的提升声明无效 (§五 禁止行为) |

### 决策 7 — 默认安全态与禁止行为强化

**默认 `NotAttempted` (fail-closed)**: 未提供归因时, 任何"能力提升"声明只能作为相关性观察写入审计, 不得触发自动变异。这与 §五 禁止行为完全一致 — 本 ADR 是该禁止行为的**可执行化**。

**明确禁止**:
- ❌ 用单次 eval 提升直接宣称能力提升 (必须版本对差分 + 混杂控制)
- ❌ 用相对胜负 (A 比 B 好) 宣称 A 自身提升 (必须 A_new 比 A_old 好 + 混杂控制)
- ❌ 在 Confounded / Insufficient 状态下 commit 变异 (治理绑定, 决策 3)

## 不变量

- **不变量 1**: IEvaluator/RewardSignal 零修改 (ADR-0083 边界, 评估层与归因层分离)
- **不变量 2**: MutationGovernor 接口零修改 (ADR-0084 边界, attribution 为可选前置输入)
- **不变量 3**: 归因方法 V1 仅 VersionPairDiff (确定性, 无 LLM 调用, 无 specialist 执行)
- **不变量 4**: 混杂因素记录复用现有事件层 (session.* / env.* / mutation.* / domain.task.*), 不新增基础设施
- **不变量 5**: 默认 `NotAttempted` fail-closed — Confounded/Insufficient/NotAttempted 三态禁止作为自动变异依据
- **不变量 6**: 5 contract 头文件零修改 (`include/agenticdsl/contract/`), attribution 类型放 `include/agenticdsl/types/`
- **不变量 7**: ADR-0068 Appendix A v1.9 amendment 随 V1 ship 同步 (2 个 attribution.* 主题注册)

## 实施

### 阶段 0 — Spike (1 sprint, self-evolution §七 #6 建议形式)

1. **`include/agenticdsl/types/attribution_record.h`** (新): AttributionRecord / AttributionMethod / AttributionVerdict / ConfounderRecord (决策 1)
2. **`include/agenticdsl/attribution/version_pair_diff.h`** (新): `attribute_version_pair()` 确定性算法 (决策 2)
3. **`tests/test_attribution_version_pair.cpp`** (新建, ≥6 cases):
   - eval_delta 超过噪声带 → Attributed
   - eval_delta 在噪声带内 → Insufficient
   - 未控制混杂 → Confounded
   - 基线样本不足 → Insufficient
   - 默认 NotAttempted fail-closed
   - confounder controlled=true 且 eval_delta 显著 → Attributed
4. **ADR-0068 Appendix A v1.9 amendment**: 注册 `attribution.recorded` + `attribution.confounded` (决策 5)
5. **docs/README.md ADR 表新增 ADR-0086 行** + orchestration doc §十七 更新 (本 ADR 从"待立项"翻转为 Proposed)

### 阶段 1 (后续 Sprint, 待 Axis6 Phase 0 ship + GEPA 集成需求明确)

6. **GEPALoop 集成**: reflect_and_commit 前可选调用 attribute_version_pair (B7 应用)
7. **MCTSWorkflowSearch 集成**: commit_chain 前调用 chain 级归因 (Axis6 Phase 1 解锁 — 本 ADR 是其前置 blocker)
8. **MutationGovernor 前置增强**: attribution_verdict == Attributed 校验 (S4 强制, S1-S2 可选)

### 阶段 2 (V2, 研究方向)

9. **反事实归因**: replay harness + 任务难度分层数据集
10. **因果实验**: A/B 对照 + 多主体归因 (S4 协同进化完整支持)

### 工作量

| 阶段 | 工作量 | 估时 |
|------|--------|------|
| 阶段 0 (类型 + 算法 + 6 cases + v1.9 主题) | spike + ADR | 1 sprint |
| 阶段 1 (GEPA/Axis6 集成) | 2 集成点 | 1 sprint |
| 阶段 2 (V2 反事实/因果) | 研究 | 2+ sprint (独立评审) |

## 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| **R1 与 IEvaluator 职责混淆** | 归因层被误用为评估器 | 决策 1 接口边界 + 不变量 1 (IEvaluator 零修改) + 类型分离 (types/attribution_record.h vs contract/ievaluator.h) |
| **R2 归因噪声带估计不准** | eval_stddev 估计偏差导致误判 | 复用 T14 Hotelling T² 方差估计 (已 ship), 不做独立估计 |
| **R3 混杂记录不完整** | controlled=true 但实际未记录 | 决策 4 controlled 判据 = 事件层有完整 append-only 记录, 可回放验证 |
| **R4 归因被当作充分条件滥用** | Attributed 判定被绕过治理直接 commit | 决策 3 治理绑定: Attributed 仍需走 ADR-0084 完整 gate, 归因只是前置之一 |
| **R5 V1 方法过简** | VersionPairDiff 无法覆盖复杂归因 | 决策 2 显式声明 V1 边界 + 决策 7 默认安全态兜底 (NotAttempted) |
| **R6 Axis6 Phase 1 依赖延迟** | 本 ADR 未 ship 导致 Axis6 Phase 1 烂尾 | 决策 6 对接表显式声明依赖方向 + 本 ADR 是 Axis6 Phase 1 的唯一前置 blocker |
| **R7 事件主题幻影** | attribution.* 未注册即 emit | 不变量 7: Appendix A v1.9 同步 ship |

## 关联变更

- `docs/README.md` ADR 表新增 ADR-0086 行
- `docs/architecture/agent-orchestration-architecture-2026-08.md` §十七 "adr-0086 待立项" → 更新为 "🔍 Proposed (本 ADR)"
- `docs/architecture/self-evolution-architecture-2026-08.md` §七 #6 标注"已立项 (adr-0086, 取代过期文件名 adr-0085-)"
- `openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` Phase 1 blocker 状态更新
- `docs/adr/adr-0068-event-emission-contract.md` Appendix A v1.9 amendment (2 个 attribution.* 主题)

## 参考

- self-evolution-architecture-2026-08.md §一.1.3 (评估信号 vs 信用分配划界) + §四 4.2 (4 必备机制) + §五 (禁止行为) + §七 #6 (立项建议)
- ADR-0083 (IEvaluator/RewardSignal — 评估层, 边界参照)
- ADR-0084 (MutationGovernance — 治理门, 集成参照)
- ADR-0061-02 (行为回归 Hotelling T² — 方差估计复用)
- ADR-0079 (Session 4-Scope — 版本固定基础)
- AFlow (arXiv:2410.10762) — 工作流搜索的评估噪声问题
- 因果推断文献 (Pearl, do-calculus) — V2 反事实归因理论基础 (仅参考, V1 不依赖)
