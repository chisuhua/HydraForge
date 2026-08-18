# lib/prompts/golden/ — 51 Held-out Golden Tasks

> **Hold-out 约束**: 本目录数据 **禁止** 用于 prompt 构造 (few-shot / schema 注入), 仅供 `tools/measure_prompt_baseline` 测量 V1/V2/V3 表现。ship 前由 `scripts/verify_golden_holdout.sh` 强制 grep 0 匹配验证。

## Golden Task Schema (YAML)

每个 `{domain}_{NN}.yaml` 文件 5 字段:

| Field | Type | Description |
|-------|------|-------------|
| `task_id` | string (unique) | `golden_{domain}_{NN}` 格式, NN ∈ [01..N], 零填充两位 |
| `input` | string | user task input |
| `expected_output` | string | 参考答案 (LLM 输出的 ground truth) |
| `dimension` | enum | `parse_valid` / `task_success` / `budget_hit` / `error_recovery` |
| `difficulty` | enum | `L1` (easy) / `L2` (medium) / `L3` (hard) |

示例 (`auth_01.yaml`):
```yaml
task_id: golden_auth_01
input: "Verify JWT token signature"
expected_output: '{"valid": true, "user_id": "u_123"}'
dimension: parse_valid
difficulty: L1
```

## Distribution (51 tasks total)

- L1 = 20 tasks (easy, ≤ 3 reasoning steps)
- L2 = 20 tasks (medium, 4-7 steps + 1 tool call)
- L3 = 11 tasks (hard, ≥ 8 steps + multi-tool orchestration)
- Domain coverage: auth / human / math / utils / inference / mcp (6 domains)

### 文件分布

| Domain | L1 | L2 | L3 | 文件编号 |
|--------|----|----|----|---------|
| auth | 4 | 3 | 2 | 01-09 |
| human | 4 | 3 | 1 | 01-08 |
| math | 4 | 3 | 1 | 01-08 |
| utils | 4 | 3 | 2 | 01-09 |
| inference | 4 | 4 | 2 | 01-10 |
| mcp | 0 | 4 | 3 | 01-07 |
| **合计** | **20** | **20** | **11** | **51** |

## 评分规则

- `parse_valid`: 输出可被 JSON Schema parser 解析通过记 1, 否则 0
- `task_success`: 输出与 `expected_output` 语义一致记 1 (mock mode 下按 difficulty 概率模拟)
- `budget_hit`: 调用 token 消耗低于阈值记 1
- `error_recovery`: 注入前次错误后能恢复并给出正确输出记 1
