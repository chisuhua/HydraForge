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
- 不改 `RewardSignal::Quality` enum (Good/Excellent/Poor/Unknown 既有值)
- 不引入新的质量评估逻辑 (evaluate 已返回 quality)
- 不修其他 3 项摩擦 (remove 治理 → harness-rsi-remove-governance 独立 change; bus nullptr → fail-open 可接受; spec drift → 已修)

## Decisions

### D1: 字段命名 `reward_quality` 对齐 RewardSignal

**决策**: `EvolutionVerdict` 增加 `agenticdsl::RewardSignal::Quality reward_quality = agenticdsl::RewardSignal::Quality::Unknown;` 字段。

**Rationale**: 与 ADR-0086 v1.1 AttributionRecord 模式一致 (reward_quality 字段名对齐)，默认 Unknown 保持向后兼容 (旧构造点不破坏)。

**Alternatives**:
- (a) 用 `std::optional<RewardSignal::Quality>` → 过度，Quality::Unknown 已是哨兵值
- (b) 用整个 `RewardSignal` → 字段膨胀，verdict 只需要 quality

### D2: `evaluate_readiness()` 填充时机

**决策**: `evaluate_readiness()` 内 `auto reward = evaluator.evaluate(...)` 之后，立即 `verdict.reward_quality = reward.quality;` (无论 can_proceed 与否，quality 都要反映)。

**Rationale**: 即使回归失败 (regression_failed)，subscriber 也需要知道"失败时的质量是什么"——这是区分"质量差导致失败" vs "其他原因失败"的关键信号。

### D3: `harness_rsi.cpp` eval_quality 接线

**决策**: L117 `{"eval_quality", "Unknown"}` 改为 `{"eval_quality", quality_name(verdict.reward_quality)}`，新增 `quality_name()` helper (映射 Quality enum → 字符串)。

**Rationale**: 事件 payload 反映真实质量。helper 与现有 `attribution_verdict_name` / `evolution_state_name` 模式一致。

## Risks / Trade-offs

- [Quality enum 序列化命名不一致] → 用与 RewardSignal 一致的字符串名 (Good/Excellent/Poor/Unknown)，测试断言锁定
- [evaluate() 抛异常时 quality 未设置] → 保持默认 Unknown (fail-safe，不 crash)
- [BREAKING 影响既有构造点] → 默认值兜底，指定初始化检查 test_transition_guard.cpp + test_harness_rsi_pilot.cpp

## Migration Plan

1. 独立 ship (原子 commit)
2. 同步更新 test_transition_guard (字段断言) + test_harness_rsi_pilot (Case 2 eval_quality)
3. 回归守卫: 既有 9 cases / 43 assertions 零回归
4. 回滚: 单 commit revert

## Open Questions

- (无 — 纯接线，Oracle bg_5db13fe0 已确认方向)
