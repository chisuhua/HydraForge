# ADR-0083: 评估/奖励信号契约 (IEvaluator & RewardSignal)

**日期**: 2026-08-24
**父主题**: Phase 6 Agent 自进化方向

## 状态

✅ Approved (ship 2026-08-26) — 契约代码已 ship: IEvaluator + RewardSignal + ExecutionTrace + TaskSuccessEvaluator V1 + CognitiveWorker/DomainWorkerPool setter 注入 + evaluation.result 事件发射

> **Ship 证据 (2026-08-26)**:
> - 契约头文件: `include/agenticdsl/contract/ievaluator.h` + `include/agenticdsl/types/reward_signal.h`
>   + `include/agenticdsl/types/execution_trace.h` + `include/agenticdsl/contract/evaluation_events.h`
> - Worker 集成: `cognitive_worker.{h,cpp}` / `domain_worker_pool.{h,cpp}` 新增 `set_evaluator()` 可选注入
>   (构造签名不变, 默认 nullptr 不评估不发射事件)
> - `tests/test_evaluator.cpp` 12 cases / 31 assertions PASS (≥ 4 cases 判定满足)
> - 全量 ctest 186/186 PASS 零回归
> - OpenSpec change `2026-08-26-ship-ievaluator-reward-contract` archived
>
> **历史状态说明 (2026-08-26 自审修正, ship 前)**:
> 本 ADR 文档结构与 5 项决策点经 Oracle Pre-Review 通过 (session `ses_fcba5e477ffeG9wEBHVhU64J0o`)，
> 但契约代码一度未 ship — 经 Oracle 深度审查 session `ses_fc3090b49ffe7yJwXhx1MoNz5N` 识别头部/§状态 自相矛盾，
> 已统一为 🔍 Proposed + 代码 ship 待办；2026-08-26 代码 ship 完成后翻转为 ✅ Approved。
>
> **V2 ship 证据 (2026-08-27, OpenSpec change `evaluator-v2-composite`)**:
> - 新增 `BehavioralEquivalenceEvaluator` (V2, T14 集成): `include/agenticdsl/cognitive/behavioral_equivalence_evaluator.h`
>   + `src/modules/cognitive/behavioral_equivalence_evaluator.cpp` — compare(a,b) 复用
>   `agenticdsl::compute_fingerprint` + `hotelling_t2_test` (行为指纹 + Hotelling T²)，
>   Pass/Inconclusive → 0, Fail → 按 reward scalar 比较返回 +1/-1；evaluate(trace) V1 占位 Acceptable(0.5)
> - 新增 `CompositeEvaluator` (V2, 多评估器聚合): `include/agenticdsl/cognitive/composite_evaluator.h`
>   + `src/modules/cognitive/composite_evaluator.cpp` — scalar 归一化权重加权平均 + quality 众数(平局取高)
>   + confidence min + compare 加权求和 ±0.1 阈值
> - IEvaluator 接口**零修改** (V2 仅子类实现)；V1 TaskSuccessEvaluator + 12 cases 零回归
> - `tests/test_evaluator.cpp` 新增 8 cases / 18 assertions PASS (BehavioralEquivalence 3 + Composite 3 + 集成 2)
>   — 满足 ≥ 6 新增 cases 判定
> - 全量 ctest 动态基线 0 回归

**前置文档**:
- `docs/architecture/capability-application-map-2026-08.md` §八 Oracle 评审
- Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o` (Pre-Review)
- Oracle session `ses_fc3090b49ffe7yJwXhx1MoNz5N` (深度审查, 2026-08-26)
- ADR-0061 Agent Evolution Pipeline (12 子项)
- ADR-0061-02 Behavioral Regression (已 ship, T14)
- ADR-0074 Prompt Evidence Gate (依赖本 ADR)
- ADR-0078 Fine-tune Pipeline (依赖本 ADR)

---

## 背景

**Oracle 评审关键发现**（2026-08-24, session `ses_fcba5e477ffeG9wEBHVhU64J0o`）：

> 22 项已 ship 能力中**没有任何一项能回答"这次执行好不好"**。
> GEPA/AFlow/fine-tune/行为克隆全部依赖评估信号。
> 这是**未识别的架构层缺口**——9 项缺失子能力中没有任何一项覆盖它。

**与现有 ADR 的关系**:

| ADR | 是否依赖本 ADR |
|---|---|
| ADR-0061-02 (行为回归, 已 ship T14) | 否（回归是"等价性"判定, 非"质量"评估）|
| ADR-0061-09 GEPA 反思循环 (Proposed) | **是** (需要 failure-aware 评估) |
| ADR-0061-08 AFlow MCTS (Proposed) | **是** (需要 comparative 评估) |
| ADR-0061-07 PASTE 推测 (Proposed) | **是** (需要 partial-result 评估) |
| ADR-0074 Prompt Evidence Gate (Proposed) | **是** (prompt 质量评估) |
| ADR-0078 Fine-tune Pipeline (Proposed) | **是** (RLHF/DPO reward signal) |
| ADR-0061-03 SkillCompiler (Proposed) | **是** (skill 改进信号) |

**结论**: 本 ADR 是**至少 6 个下游 ADR 的硬前置**——无 IEvaluator 契约则自进化方向无法启动。

---

## 决策

### 决策 1 — 双层契约: `IEvaluator` + `RewardSignal`

```cpp
// 抽象评估器: 单次执行的标量评估
class IEvaluator {
 public:
  virtual ~IEvaluator() = default;
  
  // 输入: 一次完整执行的结果 (含 trajectory)
  // 输出: 标量 reward [-1.0, 1.0] 或 Verdict 三值
  // 约束: side-effect-free, thread-safe (无状态或仅 readonly 状态)
  virtual RewardSignal evaluate(const ExecutionTrace& trace) = 0;
  
  // 输入: 两个轨迹, 输出比较 (A vs B 谁更好)
  //   - 返回 +1 表示 A 更好, -1 表示 B 更好, 0 表示平局
  virtual int compare(const ExecutionTrace& a, const ExecutionTrace& b) = 0;
};

// 奖励信号值类型 (V1: 三态, V2: 连续)
struct RewardSignal {
  enum class Quality { Excellent, Acceptable, Poor } quality;
  double scalar;     // [-1.0, 1.0], invalid range throws std::out_of_range
  double confidence; // [0.0, 1.0], default 1.0, 用于 RLHF/DPO 梯度加权
  
  // 工厂方法
  static RewardSignal excellent(double confidence = 1.0);
  static RewardSignal acceptable(double confidence = 0.5);
  static RewardSignal poor(double confidence = 1.0);
};

// 评估输入结构
struct ExecutionTrace {
  std::string trace_id;     // 关联的 trace 标识
  ToolResult final_result;  // 任务的最终结果
  nlohmann::json metadata; // 可选：额外元数据
};
```

### 决策 2 — 三种内置评估器

| 评估器 | 输入 | 输出 | 适用 | 范围 |
|---|---|---|---|---|
| `TaskSuccessEvaluator` | ToolResult.ok + error_code | RewardSignal | 大多数任务（默认）| **V1 本 change** |
| `BehavioralEquivalenceEvaluator` | 两个 BehaviorFingerprint | Verdict (Pass/Fail/Inconclusive) | 演化前后对比 | **V2 follow-up** |
| `CompositeEvaluator` | 多个 `IEvaluator*` 加权聚合 | RewardSignal 加权和 | 复杂场景（GEPA/AFlow）| **V2 follow-up** |

### 决策 3 — 与 ToolResult / ErrorCode 的关系

- `IEvaluator::evaluate()` **不修改** `ToolResult`（评估是 side-effect-free）
- `RewardSignal.quality` 与 `ErrorCode` 正交: 一个任务可能 `ok=true` 但 quality=Poor（成功但低效）
- V2: `ErrorCode::Retryable` 与 `RewardSignal.quality::Poor` 联合判定是否触发 retry

### 决策 4 — 集成点（Setter 注入，不修改构造签名）

| 集成对象 | 集成方式 | 约束 |
|---|---|---|
| `CognitiveWorker` | `set_evaluator(std::shared_ptr<IEvaluator>)` setter，运行时可选注入 | **不修改构造签名** `(unique_ptr<DSLEngine>, shared_ptr<IInteractionBus>)` |
| `DomainWorkerPool` | `set_evaluator(std::shared_ptr<IEvaluator>)` setter，运行时可选注入 | **不修改构造签名** `(num_threads)` / `(num_threads, bus)` |
| `IAgentHookRegistry` (ADR-0081) | post-step hook 调用 `evaluate()` 写 metric | V2 follow-up |
| `EventLog` | 新增 `evaluation.result` 主题（**不在 ADR-0068 registry 中**） | 本 change 独立引入 |
| `CostTrackingDecorator` (P16) | 评估代价计入 budget | V2 follow-up |

**集成时机**: evaluator 为 nullptr 时不调用 evaluate()，不发射 evaluation.result 事件（幂等，无崩溃）。

### 决策 5 — V1 简化 (避免 V0 重蹈 ADR-0057 覆辙)

V1 仅实现 `TaskSuccessEvaluator`（基于 ToolResult.ok 的 3 行实现）:

```cpp
class TaskSuccessEvaluator : public IEvaluator {
  RewardSignal evaluate(const ExecutionTrace& trace) override {
    if (trace.final_result.ok) return RewardSignal::excellent();
    return RewardSignal::poor();
  }
  int compare(const ExecutionTrace& a, const ExecutionTrace& b) override {
    return (a.final_result.ok ? 1 : 0) - (b.final_result.ok ? 1 : 0);
  }
};
```

`BehavioralEquivalenceEvaluator` 与 `CompositeEvaluator` 明确为 **V2 follow-up**，不在本 change 范围。

### 决策 6 — evaluation.result 事件 Schema

```json
{
  "evaluation_id": "eval_<uuid>",
  "schema_version": "1.0",
  "evaluator_type": "TaskSuccessEvaluator",
  "trace_ref": "<trace_id>",
  "quality": "Excellent|Acceptable|Poor",
  "scalar": 1.0,
  "confidence": 1.0,
  "evaluation_refs": []
}
```

- `evaluation_id`: 不透明唯一标识，格式 `eval_<uuid>`，跨系统全局唯一
- `schema_version`: 初始为 "1.0"，事件格式版本
- `evaluation.result` **不在** ADR-0068 Canonical Topic Registry 中注册（本 change 独立引入）
- 事件发射时机: `cognitive.task.completed` / `domain.task.completed` 之后

### 决策 7 — ExecutionTrace 与 TraceRecord 的边界

| 类型 | 职责 | 构造时机 |
|---|---|---|
| `TraceRecord` | 单节点执行追踪（per-node telemetry，ADR-0019 §1.4） | 节点执行时 |
| `ExecutionTrace` | 任务级评估输入（evaluation input） | 任务完成时 |

- `ExecutionTrace.final_result` 来自 `ToolResult`（任务的最终结果）
- `ExecutionTrace.trace_id` 关联对应任务的 trace
- `TraceRecord` 是细粒度节点追踪，EvaluationSignal 是粗粒度评估
- 两者**不互相包含**，通过 trace_id 关联

---

## 不变量

1. `IEvaluator` 接口纯虚函数, 必须 override 全部
2. `RewardSignal.scalar` 必须在 `[-1.0, 1.0]` 区间（违反抛 `std::out_of_range`）
3. 评估器无状态或仅 readonly 状态（线程安全，V1 TaskSuccessEvaluator 无状态）
4. 评估器**不修改**输入 trace（避免 double-evaluation 副作用）
5. `evaluation_id` 格式为 `eval_<uuid>`，每次 evaluate() 调用生成新 uuid
6. evaluator 为 nullptr 时不发射 evaluation.result 事件（幂等）

---

## 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| V1 `TaskSuccessEvaluator` 过于简单, GEPA 无法区分"成功但低效" | GEPA 反思效果差 | V2 增加 latency/tokens/cost 维度评估 |
| `CompositeEvaluator` 加权策略难定义 | 复杂场景无标准做法 | V2 推迟, 提供 heuristic 实现 |
| 评估器 vs 回归门职责重叠 (与 ADR-0061-02) | API 混淆 | 本 ADR 评估"质量", T14 评估"等价性", 文档明确区分 |

---

## 实施

- **文件**:
  - `include/agenticdsl/contract/ievaluator.h` (L1 契约层)
  - `include/agenticdsl/types/reward_signal.h` (值类型)
  - `include/agenticdsl/types/execution_trace.h` (评估输入)
  - `src/modules/cognitive/task_success_evaluator.cpp` (V1 实现)
  - `src/modules/cognitive/cognitive_worker.cpp` (setter + evaluate 调用)
  - `src/modules/cognitive/domain_worker_pool.cpp` (setter + evaluate 调用)
  - `tests/test_evaluator.cpp` (≥ 4 cases)
- **估时**: 1.5 sprint
- **优先级**: P0 (Oracle 评审: "本周最高杠杆"之一)

---

## 关联变更

- `docs/architecture/capability-application-map-2026-08.md` §八 新增 G10
- 解锁后续: A2 (IEvaluator) → T15 (Trajectory IR 集成评估) → T19 (GEPA MVP) → T21 (Prompt Evidence Gate) → T22 (Fine-tune)
- 与 `tests/test_evaluator.cpp` 联合 ship gate 验证

---

## 参考

- Oracle 评审: session `ses_fcba5e477ffeG9wEBHVhU64J0o`
- AgentAssay: arXiv:2603.02601 (Token-efficient verdict)
- RLHF reward modeling: Christiano et al. 2017
- Process Reward Model (PRM): UCB 2023
- ADR-0061-02 (行为回归已 ship, T14) - 同族但不同职责
- ADR-0057 (Agent 生命周期) - amendment 教训: V1 需避免"零实施无需新设计"误判
