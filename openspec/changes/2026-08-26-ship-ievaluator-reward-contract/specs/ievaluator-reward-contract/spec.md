# ievaluator-reward-contract Specification

## ADDED Requirements

### Requirement: IEvaluator 是评估执行的抽象接口

The IEvaluator MUST provide a pure virtual interface for evaluating a single execution trace, returning a scalar reward signal in `[-1.0, 1.0]` or a three-valued Verdict.

#### Scenario: 评估 happy path

- **GIVEN** 一个 ExecutionTrace（包含 final_result.ok + trace_id）
- **AND** 一个 IEvaluator 子类（TaskSuccessEvaluator V1 实现）
- **WHEN** 调用 `evaluator->evaluate(trace)`
- **THEN** 返回 RewardSignal.quality ∈ {Excellent, Acceptable, Poor}
- **AND** RewardSignal.scalar ∈ [-1.0, 1.0]

#### Scenario: 比较两个轨迹

- **GIVEN** 两个 ExecutionTrace（a 与 b）
- **AND** 一个 IEvaluator 子类
- **WHEN** 调用 `evaluator->compare(a, b)`
- **THEN** 返回 +1（a 更好）, -1（b 更好）, 或 0（平局）

#### Scenario: 评估器不修改输入 trace

- **GIVEN** 一个 ExecutionTrace
- **WHEN** 调用 `evaluator->evaluate(trace)` 多次
- **THEN** trace 内容保持不变（评估 side-effect-free，per ADR-0083 §不变量 4）

#### Scenario: 评估器线程安全

- **GIVEN** 一个 IEvaluator 子类实例
- **WHEN** 多个线程并发调用 `evaluate()`
- **THEN** 无数据竞争（评估器无状态或仅 readonly 状态，per ADR-0083 §不变量 3）

### Requirement: V1 TaskSuccessEvaluator 基于 ToolResult.ok 简化实现

The TaskSuccessEvaluator MUST be the V1 default evaluator that maps `ToolResult.ok` to RewardSignal.quality.

#### Scenario: ok=true → Excellent

- **GIVEN** ExecutionTrace.final_result.ok = true
- **WHEN** 调用 `TaskSuccessEvaluator::evaluate(trace)`
- **THEN** 返回 `RewardSignal::excellent(1.0)`

#### Scenario: ok=false → Poor

- **GIVEN** ExecutionTrace.final_result.ok = false
- **WHEN** 调用 `TaskSuccessEvaluator::evaluate(trace)`
- **THEN** 返回 `RewardSignal::poor(1.0)`

### Requirement: Setter 注入而非构造注入

CognitiveWorker 和 DomainWorkerPool 的构造签名保持不变。IEvaluator 通过可选 setter 注入。

#### Scenario: setter 注入 evaluator

- **GIVEN** CognitiveWorker 已构造（evaluator=nullptr）
- **WHEN** 调用 `worker.set_evaluator(std::make_shared<TaskSuccessEvaluator>())`
- **THEN** 后续任务完成时调用 evaluate() 并发射 evaluation.result 事件

#### Scenario: null evaluator 不发射事件

- **GIVEN** CognitiveWorker 已构造（evaluator=nullptr）
- **WHEN** 任务完成
- **THEN** 不调用 evaluate()，不发射 evaluation.result 事件（无崩溃）

### Requirement: evaluation.result 事件 schema

evaluation.result 事件在 cognitive.task.completed / domain.task.completed 之后发射。

#### Scenario: evaluation.result 事件字段

- **GIVEN** CognitiveWorker 任务完成且已设置 evaluator
- **WHEN** 任务执行完成
- **THEN** 发射 `evaluation.result` 事件，payload 符合以下 schema：

```json
{
  "evaluation_id": "eval_<uuid>",
  "schema_version": "1.0",
  "evaluator_type": "TaskSuccessEvaluator",
  "trace_ref": "<trace_id>",
  "quality": "Excellent",
  "scalar": 1.0,
  "confidence": 1.0,
  "evaluation_refs": []
}
```

#### Scenario: evaluation_id 唯一性

- **GIVEN** 每次 evaluate() 调用
- **WHEN** 生成 evaluation_id
- **THEN** 格式为 `eval_<uuid>`，跨调用全局唯一

#### Scenario: scalar 范围验证

- **GIVEN** RewardSignal 构造或工厂方法调用
- **WHEN** scalar 值超出 [-1.0, 1.0] 区间
- **THEN** 抛 `std::out_of_range`

### Requirement: ExecutionTrace 与 TraceRecord 的边界定义

ExecutionTrace 用于评估输入，TraceRecord 用于执行追踪。

#### Scenario: ExecutionTrace 构造

- **GIVEN** 任务完成后的 ToolResult
- **WHEN** CognitiveWorker/DomainWorkerPool 需要评估时
- **THEN** 构造 ExecutionTrace { final_result: ToolResult, trace_id: string }

#### Scenario: ExecutionTrace 不包含 TraceRecord

- **GIVEN** ExecutionTrace
- **THEN** 仅含 final_result（ToolResult）+ trace_id（string）+ 可选 metadata
- **AND** 不直接包含 TraceRecord 数组（TraceRecord 是细粒度节点追踪）

### Requirement: V2 评估器明确 out of scope

以下评估器不在本 change 范围内：

#### Scenario: BehavioralEquivalenceEvaluator V2

- **THEN** BehavioralEquivalenceEvaluator **不**在本 change 实现
- **AND** 留作 OpenSpec follow-up change（提议名: `ship-evaluator-v2-composite`）

#### Scenario: CompositeEvaluator V2

- **THEN** CompositeEvaluator **不**在本 change 实现
- **AND** 留作 OpenSpec follow-up change（提议名: `ship-evaluator-v2-composite`）

---

## Normative Schemas

### EvaluationResult Event Schema (evaluation.result)

```json
{
  "evaluation_id": "string (format: eval_<uuid>, globally unique)",
  "schema_version": "string (fixed: '1.0')",
  "evaluator_type": "string (concrete evaluator class name)",
  "trace_ref": "string (trace_id of evaluated execution)",
  "quality": "string (Excellent|Acceptable|Poor)",
  "scalar": "number (range: [-1.0, 1.0])",
  "confidence": "number (range: [0.0, 1.0], default: 1.0)",
  "evaluation_refs": "array of string (evaluation_id refs, V1: empty)"
}
```

### ExecutionTrace Schema

```cpp
struct ExecutionTrace {
  std::string trace_id;          // 关联的 trace 标识
  ToolResult final_result;        // 任务的最终结果
  nlohmann::json metadata;       // 可选：额外元数据
};
```

### RewardSignal Schema

```cpp
struct RewardSignal {
  enum class Quality { Excellent, Acceptable, Poor };
  Quality quality;
  double scalar;   // [-1.0, 1.0], invalid range throws std::out_of_range
  double confidence; // [0.0, 1.0], default 1.0

  // 工厂方法
  static RewardSignal excellent(double confidence = 1.0);
  static RewardSignal acceptable(double confidence = 0.5);
  static RewardSignal poor(double confidence = 1.0);
};
```

### IEvaluator Interface

```cpp
class IEvaluator {
 public:
  virtual ~IEvaluator() = default;

  // 输入: 一次完整执行的结果
  // 输出: 标量 reward
  // 约束: side-effect-free, thread-safe
  virtual RewardSignal evaluate(const ExecutionTrace& trace) = 0;

  // 输入: 两个轨迹
  // 输出: +1 (a 更好), -1 (b 更好), 0 (平局)
  virtual int compare(const ExecutionTrace& a, const ExecutionTrace& b) = 0;
};
```
