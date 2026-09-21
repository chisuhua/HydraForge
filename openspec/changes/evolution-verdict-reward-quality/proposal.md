# EvolutionVerdict Reward Quality — Proposal

## Why

C4 `harness-rsi-pilot` Decision Record §3 摩擦 1 记录：`harness_rsi.cpp:117` 发射 `evolution.readiness.denied` 事件时 `eval_quality` 字段硬编码 `"Unknown"` 字符串。根因是 `EvolutionVerdict` (transition_guard.h:31-36) 不携带 reward quality 字段——只有 `can_proceed` + `failed_conditions` + `reason` + `recommended_next`。事件 subscriber 无法从 `eval_quality` 推断回归质量，只能从 `failed_conditions` 反推。Wave 3 (ADR-0078 Model-RSI pilot) 的变异决策直接依赖质量信号，必须让 verdict 携带真实质量值。

## What Changes

- **`EvolutionVerdict` 增加 `reward_quality` 字段**: `transition_guard.h` 的 `EvolutionVerdict` struct 增加 `agenticdsl::RewardSignal::Quality reward_quality` 字段（默认 `Unknown`），`evaluate_readiness()` 填充该字段（从 `evaluator.evaluate()` 返回的 `RewardSignal` 取 `quality`）
- **`harness_rsi.cpp` 接线**: 发射事件时 `eval_quality` 从 `verdict.reward_quality` 取（替代硬编码 `"Unknown"`）
- **事件 payload 完整性**: `evolution.readiness.denied` 事件的 `eval_quality` 字段反映真实评估质量

## Capabilities

### New Capabilities
- `evolution-verdict-reward-quality`: EvolutionVerdict 携带 reward quality + readiness 事件 eval_quality 真实值透传

### Modified Capabilities
- (none — 既有 spec 无行为变更，此 change 补 C3/C4 已知债务)

## Impact

- `include/agenticdsl/evolution/transition_guard.h` — `EvolutionVerdict` + 1 字段 + `evaluate_readiness()` 填充逻辑
- `src/evolution/harness_rsi.cpp` — L117 `eval_quality` 从 verdict 取
- `tests/test_transition_guard.cpp` — EvolutionVerdict 字段断言更新
- `tests/test_harness_rsi_pilot.cpp` — Case 2 eval_quality 断言更新
- **BREAKING**: `EvolutionVerdict` 是聚合结构，增加字段不影响既有构造（默认值兜底），但依赖字段顺序的指定初始化需检查
- 无外部依赖变更
