# lib/prompt — T21 Prompt Evidence Gate 数据集

## few_shots/
32 个真实 few-shot 演示 (.md), 覆盖 auth/human/math/utils/inference/loop/engine 领域。每个文件含 input/output/error 三部分。

## golden/
54 个 held-out golden task (.json), 跨多领域 (auth/human/math/utils/inference/loop), 每个含 input + expected_output + validation_rules 三字段。

## validation_rules 前缀约定

- `P:` 语法/结构规则 (parse) — 未满足触发 llm.dsl.parse_failed (重试一次)
- `S:` 语义/约束规则 (schema) — 未满足触发 llm.dsl.schema_validation_failed (不重试)

设计依据: ADR-0074 §决策 1/2/4 + t21-prompt-evidence-gate
