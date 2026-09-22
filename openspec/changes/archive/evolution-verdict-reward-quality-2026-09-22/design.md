# EvolutionVerdict Reward Quality — Design

## Context

C4 Decision Record §3 摩擦 1 (`eval_quality:"Unknown"` 硬编码) 明确记录：
- `harness_rsi.cpp:117` 发射 `evolution.readiness.denied` 事件时 `eval_quality` 字段填 `"Unknown"` 字符串
- 根因: `EvolutionVerdict` (transition_guard.h:31-36) 只有 `can_proceed` + `failed_conditions` + `reason` + `recommended_next`，不携带 reward quality
- 影响: 事件 subscriber 无法从 `eval_quality` 推断回归质量
- Wave 3 路径 (per Decision Record): `EvolutionVerdict` 加 `reward_quality: agenticdsl::RewardSignal::Quality` 字段 (沿用 ADR-0086 v1.1 AttributionRecord 模式)

关键事实: `evaluate_readiness()` 内部已经调用了 `evaluator.evaluate(sentinel_trace)` 并得到 `RewardSignal` (transition_guard.h:72)，其中含 `quality` 字段——只是当前没把它放进 `EvolutionVerdict`。所以这是纯接线，无新逻辑。

## Goals / Non-Goals

**Goals**:
- `EvolutionVerdict` 携带 `reward_quality` 字段
- `evaluate_readiness()` 从 evaluator 结果填充该字段
- `harness_rsi.cpp` 发射事件时用真实值

**Non-Goals**:
- 不改 `RewardSignal::Quality` enum 语义（**enum 实际值 = Excellent / Acceptable / Poor，无 Unknown、无 Good** — reward_signal.h:14-18）
- 不引入新的质量评估逻辑 (evaluate 已返回 quality)
- 不修其他 3 项摩擦 (remove 治理 → harness-rsi-remove-governance 独立 change; bus nullptr → fail-open 可接受; spec drift → 已修)

## Decisions

### D1: 字段命名 + 默认值 `reward_quality = Quality::Acceptable`

**决策**: `EvolutionVerdict` 增加 `agenticdsl::RewardSignal::Quality reward_quality = agenticdsl::RewardSignal::Quality::Acceptable;` 字段。

**Rationale**:
- **值域事实**: `RewardSignal::Quality` enum 只有 Excellent / Acceptable / Poor（reward_signal.h:14-18）——**不存在 `Unknown`**。design 早期版本引用 `Quality::Unknown` 是虚构值，编译不过，已修正。
- **默认值选择**: `Quality::Acceptable` 是既有语义中立默认（evidence_gate.h:88 `last_reward_{RewardSignal::Quality::Acceptable, ...}` 先例）——比新增 `Unknown` 枚举值侵入性小（改 enum 触碰 RLHF 梯度加权消费方），比 `std::optional<Quality>` 简单（Quality::Acceptable 承担"未评估"哨兵，与 evidence_gate 一致）。
- 与 ADR-0086 v1.1 AttributionRecord 模式对齐 (reward_quality 字段名)。

**Alternatives**:
- (a) 加 `Unknown` 枚举值到 `RewardSignal::Quality` → 修改共享 enum，影响 RLHF 消费方 (mutation_governor.cpp:179 / skill_compiler.cpp:140 / evidence_gate.h:88)，侵入性大，**否决**
- (b) `std::optional<RewardSignal::Quality>` → 增加空态复杂度，Quality::Acceptable 已是合格哨兵，**否决**
- (c) 用整个 `RewardSignal` → 字段膨胀，verdict 只需要 quality，**否决**

### D1a: ≤4 字段约束显式修订

**决策**: `transition_guard.h:32` 注释 "D2: EvolutionVerdict struct (4 字段, ≤4 字段约束 per Oracle Q8)" 显式修订为 "(5 字段, Oracle Q8 约束修订 2026-09-21: reward_quality 为 Wave 3 决策必需信号, 追加于字段末尾, 默认值兜底不破坏既有构造)".

**Rationale**: 加 `reward_quality` 后 5 字段。Oracle Q8 的 ≤4 约束是早期 YAGNI 决策，Wave 3 Model-RSI 变异决策依赖质量信号，追加 1 个带默认值字段不破坏任何既有构造点（聚合初始化少给尾参合法）。设计文档显式修订约束，避免 docs_drift。

### D2: `evaluate_readiness()` 填充时机

**决策**: `evaluate_readiness()` 内 `auto reward = evaluator.evaluate(...)` 之后，立即 `verdict.reward_quality = reward.quality;` (无论 can_proceed 与否，quality 都要反映)。

**Rationale**: 即使回归失败 (regression_failed)，subscriber 也需要知道"失败时的质量是什么"——这是区分"质量差导致失败" vs "其他原因失败"的关键信号。

### D3: `harness_rsi.cpp` eval_quality 接线（复用既有 helper）

**决策**: L117 `{"eval_quality", "Unknown"}` 改为 `{"eval_quality", quality_name(verdict.reward_quality)}`，其中 `quality_name()` **复用 `include/agenticdsl/contract/evaluation_events.h:28-34` 既有 inline helper**（已映射 Excellent/Acceptable/Poor + Unknown fallback）——**不新增第三个同义函数**。

**Rationale**: 事件 payload 反映真实质量。quality_name 已存在于 contract 层 (evaluation_events.h:28)，harness_rsi.cpp 已在 namespace agenticdsl::evolution，可直接调用 `agenticdsl::quality_name`。重复实现会造轮子（Oracle Major-2.3 修正）。

## Risks / Trade-offs

- [Quality enum 序列化命名不一致] → 复用 evaluation_events.h:28 `quality_name()` 既有映射 (Excellent/Acceptable/Poor + "Unknown" fallback)，测试断言锁定
- [evaluate() 抛异常时 quality 未设置] → 保持默认 Acceptable (fail-safe，不 crash)，与 evidence_gate.h:88 先例一致
- [加字段影响既有构造点] → 默认值兜底 (Quality::Acceptable)，聚合初始化少给尾参合法——test_transition_guard.cpp + test_harness_rsi_pilot.cpp 仅需新增断言，无需改构造
- [≤4 字段约束被违反] → D1a 显式修订约束 + 头文件注释同步，避免 docs_drift

## Migration Plan

1. 独立 ship (原子 commit)
2. 同步更新 test_transition_guard (字段断言) + test_harness_rsi_pilot (Case 2 eval_quality)
3. 回归守卫: 既有 9 cases / 43 assertions 零回归
4. 回滚: 单 commit revert

## Open Questions

- (无 — 纯接线，Oracle bg_5db13fe0 已确认方向)
