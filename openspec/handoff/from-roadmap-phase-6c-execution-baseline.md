# Handoff: from-roadmap-phase-6c-execution-baseline → from-roadmap-phase-6c-evidence-gate

> **Date**: 2026-08-18
> **Source change**: `from-roadmap-phase-6c-execution-baseline` (Wave 1)
> **Consumer change**: `from-roadmap-phase-6c-evidence-gate` (Wave 2, C4 Evidence Gate)

## What was delivered (this change)

1. **32 few-shot examples** in `lib/prompts/fewshot/{dimension}_{NN}.yaml`
2. **51 held-out golden tasks** in `lib/prompts/golden/{domain}_{NN}.yaml` (L1=20, L2=20, L3=11)
3. **V1/V2/V3 prompt builders** in `src/common/prompts/{v1,v2,v3}.cpp`
4. **measure_prompt_baseline CLI** at `build/tools/measure_prompt_baseline`
5. **Mock-mode baseline measurements** at `docs/audits/2026-08-18-execution-baseline-v{1,2,3}.yaml`
6. **Human-readable report** at `docs/audits/2026-08-18-execution-baseline-v1.md`

## What evidence-gate needs to consume

For Go/No-Go decision making in C4 Evidence Gate:

1. **Golden suite data**: 51 tasks YAML, validated by `tests/test_golden_suite.cpp`
2. **Hold-out guarantee**: `scripts/verify_golden_holdout.sh` returns 0 (clean)
3. **Baseline measurements**: 3 YAML reports showing V1 vs V2 vs V3 mock-mode rates
4. **Statistical sample**: 51 tasks (扩 CI 通过 3 模型 × 50 tasks = 150 样本)
5. **Risk register**: 6 risks documented in `proposal.md` with mitigation strategies

## Deferred to evidence-gate change

- **Real LLM measurements**: This change used mock mode; real baseline requires OpenAI/Anthropic API access (held in `evidence-gate` scope)
- **3-model × 50-tasks expansion**: 50 → 150 samples for narrower CI (evidence-gate spec requirement)
- **Go/No-Go decision**: Whether V3 is shipped to production based on parse-valid ≥ 85% threshold (cross-validated against real LLM)
- **Stage 1 subgraph choice** (ADR-0074 D-5): Deferred to Phase 6d C5

## Schema contract

YAML output schema (consumed by evidence-gate analytics):
- `parse_valid_rate`: float [0, 1]
- `task_success_rate.{L1,L2,L3}`: nested dict, floats [0, 1]
- `per_dimension.{parse_valid, task_success, budget_hit, error_recovery}`: nested dict
- `confidence_interval.parse_valid`: 2-tuple [lower_95ci, upper_95ci]
- `mock_mode`: bool (true for this change's outputs)
- `timestamp`: ISO-8601

All fields are mandatory in output schema per design.md D-4.

## Statistical sample notes

- 本 change 样本量 N=51, parse_valid 95% CI 宽度约 ±0.065 (Wilson interval approximation via mock 测量)
- mock 模式下 3 版本输出同分布 (MockLLMProvider 不解析 prompt), CI 仅反映样本量噪声, 不反映 prompt 策略差异
- evidence-gate 扩展至 3 模型 × 50 tasks = 150 样本后, 预期 CI 宽度收窄至 ±0.04 以内, 可区分 ≥ 5pp 的真实差异
