# Execution Baseline v1 — V1 vs V2 vs V3 Prompt 首次测量

> **Date**: 2026-08-18
> **Mock mode**: true (CI sanity check, not real LLM)
> **Golden suite**: 51 tasks (L1=20, L2=20, L3=11)
> **Companion YAML reports**: `2026-08-18-execution-baseline-v{1,2,3}.yaml`
> **OpenSpec change**: `from-roadmap-phase-6c-execution-baseline`
> **Git commit (worktree HEAD)**: `9274f71` (baseline 代码为未提交 worktree 状态)

## §1 Ship Gate 评分

| Metric | Target | V1 (actual) | V2 (actual) | V3 (actual) |
|--------|--------|-------------|-------------|-------------|
| parse_valid_rate | ≥ 0.85 | **0.8824** ✅ | **0.8824** ✅ | **0.8824** ✅ |
| task_success L1  | ≥ 0.70 | **0.8500** ✅ | **0.8500** ✅ | **0.8500** ✅ |
| task_success L2  | ≥ 0.50 | **0.6000** ✅ | **0.6000** ✅ | **0.6000** ✅ |
| task_success L3  | ≥ 0.20 | **0.1818** ❌ | **0.1818** ❌ | **0.1818** ❌ |

> **说明**: mock 模式下 MockLLMProvider 不区分 prompt 内容, 3 个版本数值完全相同属预期; 该测量仅验证测量管线端到端可用 (CLI → golden suite → YAML 报告), 不构成真实 LLM 能力结论。L3 未达 0.20 阈值, 真实 LLM 复测由 evidence-gate change 负责。

## §2 V1 vs V2 vs V3 对比表

| 维度 | V1 (schema-only) | V2 (+few-shot) | V3 (+two-stage) | Δ V3−V1 |
|------|------------------|----------------|-----------------|---------|
| parse_valid_rate | 0.8824 | 0.8824 | 0.8824 | 0.0000 |
| task_success 整体 | 0.6078 | 0.6078 | 0.6078 | 0.0000 |
| task_success L1 (20) | 0.8500 | 0.8500 | 0.8500 | 0.0000 |
| task_success L2 (20) | 0.6000 | 0.6000 | 0.6000 | 0.0000 |
| task_success L3 (11) | 0.1818 | 0.1818 | 0.1818 | 0.0000 |
| parse_valid 95% CI | [0.8124, 0.9424] | [0.8124, 0.9424] | [0.8124, 0.9424] | — |

> mock 模式下 3 版本输出同分布, 差异为 0 — 这是 mock provider 的固有局限, 已在 §5 Q-3 跟踪。

## §3 per-dimension 分解

| Dimension | V1 | V2 | V3 |
|-----------|-----|-----|-----|
| `parse_valid` | 0.8824 | 0.8824 | 0.8824 |
| `task_success` | 0.6078 | 0.6078 | 0.6078 |
| `budget_hit` | 0.1000 | 0.1000 | 0.1000 |
| `error_recovery` | 0.6500 | 0.6500 | 0.6500 |

- `budget_hit`: mock 模式固定返回 0.1 (假设数据, mock provider 无真实 token 计费)
- `error_recovery`: mock 模式固定返回 0.65 (假设数据, 无真实错误注入)

## §4 测量日志

| 项 | 值 |
|----|-----|
| 时间戳 | 2026-08-18T02:55:40Z |
| Git commit | `9274f71` (worktree `openspec/from-roadmap-phase-6c-execution-baseline`) |
| 命令 | `./build/tools/measure_prompt_baseline --prompt {V1,V2,V3} --golden-dir lib/prompts/golden/ --output docs/audits/2026-08-18-execution-baseline-v{1,2,3}.yaml --mock-mode` |
| 数据来源 | `lib/prompts/golden/` 51 tasks (L1=20, L2=20, L3=11) |
| Mock mode 局限 | MockLLMProvider 不解析 prompt 内容, 3 版本数值相同; 仅验证管线, 不代表真实 LLM 表现 |
| Hold-out 保证 | `scripts/verify_golden_holdout.sh` exit 0 (golden 51 task_ids 与 fewshot 32 examples 零重叠) |

## §5 Open Issues

- **Q-1**: V3 是否需要 Stage 1 subgraph 选择 (Phase 6d C5 deferred)
- **Q-2**: few-shot 来源是否可以迁移 examples/ (架构组 review 待)
- **Q-3**: mock vs real LLM baseline 偏差待 evidence-gate 真实 LLM 测量
- **Q-4**: L3 task_success 0.1818 < 0.20 阈值 — mock 模式下不作结论, evidence-gate 用真实 LLM 复测后裁决 Go/No-Go

---

## 附录 A — 原始 YAML 报告

<details>
<summary>2026-08-18-execution-baseline-v1.yaml</summary>

```yaml
baseline_id: 2026-08-18-V1
prompt_version: V1
golden_tasks_total: 51
parse_valid_rate: 0.8823529411764706
task_success_rate:
  L1: 0.85
  L2: 0.6
  L3: 0.18181818181818182
per_dimension:
  parse_valid: 0.8823529411764706
  task_success: 0.6078431372549019
  budget_hit: 0.1
  error_recovery: 0.65
confidence_interval:
  parse_valid: [0.8123529411764705, 0.9423529411764706]
mock_mode: true
timestamp: 2026-08-18T02:55:40Z
```

</details>

<details>
<summary>2026-08-18-execution-baseline-v2.yaml</summary>

```yaml
baseline_id: 2026-08-18-V2
prompt_version: V2
golden_tasks_total: 51
parse_valid_rate: 0.8823529411764706
task_success_rate:
  L1: 0.85
  L2: 0.6
  L3: 0.18181818181818182
per_dimension:
  parse_valid: 0.8823529411764706
  task_success: 0.6078431372549019
  budget_hit: 0.1
  error_recovery: 0.65
confidence_interval:
  parse_valid: [0.8123529411764705, 0.9423529411764706]
mock_mode: true
timestamp: 2026-08-18T02:55:40Z
```

</details>

<details>
<summary>2026-08-18-execution-baseline-v3.yaml</summary>

```yaml
baseline_id: 2026-08-18-V3
prompt_version: V3
golden_tasks_total: 51
parse_valid_rate: 0.8823529411764706
task_success_rate:
  L1: 0.85
  L2: 0.6
  L3: 0.18181818181818182
per_dimension:
  parse_valid: 0.8823529411764706
  task_success: 0.6078431372549019
  budget_hit: 0.1
  error_recovery: 0.65
confidence_interval:
  parse_valid: [0.8123529411764705, 0.9423529411764706]
mock_mode: true
timestamp: 2026-08-18T02:55:40Z
```

</details>
