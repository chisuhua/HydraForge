# lib/prompts/ — Prompt Engineering Library

## Structure

- `fewshot/` — 32 few-shot examples (4 维度 × 8), used by V2/V3 prompt builders
- `golden/` — 51 held-out golden tasks (L1=20 + L2=20 + L3=11), **NOT used in prompt construction**
  用于 `tools/measure_prompt_baseline` 测量 V1/V2/V3 表现

## 4 Dimensions Taxonomy

| Dimension | Description |
|-----------|-------------|
| `parse_valid` | LLM 输出可被 JSON Schema 解析为合法 JSON |
| `task_success` | LLM 输出在语义上完成任务请求 |
| `budget_hit` | LLM 调用未超出 budget 限制 |
| `error_recovery` | LLM 从前次错误中恢复并给出正确下一步 |

## Few-shot Example Schema (YAML)

每个 `{dimension}_{NN}.yaml` 文件必须含以下 4 字段:

| Field | Type | Description |
|-------|------|-------------|
| `dimension` | enum | `parse_valid` / `task_success` / `budget_hit` / `error_recovery` |
| `input` | string | user task input |
| `output` | string | expected LLM output (或 recovery 后的最终 output) |
| `rationale` | string | 解释为何这是该维度的 representative example (≥ 30 字) |

示例 (`parse_valid_01.yaml`):
```yaml
dimension: parse_valid
input: "List user permissions"
output: '{"permissions": ["read", "write"]}'
rationale: "JSON 输出可被 Schema parser 直接解析为 list, 演示 parse_valid 维度的成功路径"
```

## Hold-out Guarantee

51 个 golden tasks 与 few-shot 完全物理隔离, ship 前用 `scripts/verify_golden_holdout.sh` 强制 `grep` 0 匹配验证。

## V1/V2/V3 Prompt Builders

### V1 Schema Constraint
- `src/common/prompts/v1.cpp`
- Embedded JSON Schema in system message
- 对应 ADR-0073 ToolMetadata V3 schema

### V2 Few-shot
- `src/common/prompts/v2.cpp`
- V1 + 随机抽样 ≤ 5 个 few-shot examples from `lib/prompts/fewshot/`
- 确定性 RNG seed = 42 (测试友好)

### V3 Two-stage
- `src/common/prompts/v3.cpp`
- V2 + 两阶段顺序固定 (SystemFirst → UserSecond)
- Token > 8k 报警 (Risk-3 缓解)

## measure_prompt_baseline CLI

Usage example:
```bash
./build/tools/measure_prompt_baseline \
    --prompt V3 \
    --golden-dir lib/prompts/golden/ \
    --output docs/audits/my-baseline.yaml \
    --mock-mode

# 指定任务数
./build/tools/measure_prompt_baseline --prompt V1 --max-tasks 10 --output /tmp/test.yaml --mock-mode
```

## Hold-out verification

```bash
./scripts/verify_golden_holdout.sh
# Expected: "✅ Hold-out PASSED: 51 golden task_ids clean"
```
