# t21-payload-redact Specification

## ADDED Requirements

### Requirement: 3 事件 Payload 改为 hash-only 模式

The `llm.dsl.parse_failed`, `llm.dsl.schema_validation_failed`, `prompt.token_limit_exceeded` 事件 MUST NOT contain raw prompt text in `payload.data`. MUST use `prompt_hash` (SHA-256 hex 16 chars) + `prompt_length` (int) instead.

#### Scenario: parse_failed 事件 payload

- **WHEN** Evidence Gate 检测到 parse 错误并发射 `llm.dsl.parse_failed` 事件
- **THEN** `payload.data["prompt_hash"]` MUST 为 16 char hex string
- **AND** `payload.data["prompt_length"]` MUST 为 int (原始 prompt 长度)
- **AND** `payload.data["prompt"]` MUST NOT 存在（原文不入事件流）
- **AND** `payload.data["error_position"]` + `payload.data["retry_count"]` 保留

#### Scenario: schema_validation_failed 事件 payload

- **WHEN** Evidence Gate 检测到 schema 错误并发射 `llm.dsl.schema_validation_failed` 事件
- **THEN** `payload.data["prompt_hash"]` MUST 为 16 char hex string
- **AND** `payload.data["prompt_length"]` MUST 为 int
- **AND** `payload.data["violation"]` MUST 为结构化错误描述（非原文）
- **AND** `payload.data["prompt"]` MUST NOT 存在

#### Scenario: token_limit_exceeded 事件 payload

- **WHEN** PromptAssembler 触发 token limit 检查并发射 `prompt.token_limit_exceeded` 事件
- **THEN** `payload.data["prompt_hash"]` MUST 为 16 char hex string
- **AND** `payload.data["prompt_length"]` MUST 为 int
- **AND** `payload.data["token_estimate"]` MUST 为 int (estimate_tokens 输出)
- **AND** `payload.data["stage"]` + `payload.data["actual_tokens"]` 保留
- **AND** `payload.data["prompt"]` MUST NOT 存在

### Requirement: hash_prompt 函数确定性 + 唯一性

The `hash_prompt(prompt)` function MUST return deterministic hash (same input → same output) and collision-resistant (different input → different output with high probability).

#### Scenario: 确定性（相同输入）

- **WHEN** 调用 `hash_prompt("test prompt")` 两次
- **THEN** 两次返回值 MUST 完全相同

#### Scenario: 唯一性（不同输入）

- **WHEN** 调用 `hash_prompt("prompt A")` 和 `hash_prompt("prompt B")`（A ≠ B）
- **THEN** 两个返回值 MUST 不同

#### Scenario: 空字符串边界

- **WHEN** 调用 `hash_prompt("")`
- **THEN** MUST 返回 valid hex string（不抛异常）

### Requirement: estimate_tokens V1 简化算法

The `estimate_tokens(prompt)` MUST use `chars/4` algorithm (V1 simplification). Result MUST be `ceil(prompt.size() / 4)`.

#### Scenario: 标准输入

- **WHEN** 调用 `estimate_tokens("12345678")` (8 chars)
- **THEN** MUST 返回 2

#### Scenario: 非 4 倍数

- **WHEN** 调用 `estimate_tokens("12345")` (5 chars)
- **THEN** MUST 返回 2 (ceil(5/4) = 2)

#### Scenario: 空字符串

- **WHEN** 调用 `estimate_tokens("")` (0 chars)
- **THEN** MUST 返回 0

### Requirement: 公开 API 零修改

The fix MUST NOT modify IInteractionBus / EventBuilder / ToolResult public APIs. Only event payload field names change (additive change per ADR-0068 §决策 5).

#### Scenario: 契约文件未变更

- **WHEN** `git diff HEAD -- include/agenticdsl/contract/iinteraction_bus.h include/agenticdsl/contract/event_builder.h include/agenticdsl/core/types/tool_result.h`
- **THEN** 不应有 V1 implementation 相关 diff

### Requirement: ADR-0068 附录 A 同步更新

The ADR-0068 附录 A v1.4 → v1.5 MUST update payload field descriptions for 3 affected topics:
- `llm.dsl.parse_failed`
- `llm.dsl.schema_validation_failed`
- `prompt.token_limit_exceeded`

#### Scenario: 附录 A v1.5 字段定义

- **WHEN** 静态检查 `grep -E '("prompt_hash"|"prompt_length"|"violation")' docs/adr/adr-0068-event-emission-contract.md`
- **THEN** 3 主题的 payload 字段 MUST 包含 `prompt_hash` + `prompt_length`（不再含 `prompt` 原文字段）

### Requirement: ctest 全量零回归

The `ctest --output-on-failure` MUST report ALL tests PASS with zero regressions relative to T21 ship baseline (19 cases / 338 assertions).

#### Scenario: ctest 全量 PASS

- **WHEN** 运行 `ctest --output-on-failure`
- **THEN** 所有测试 PASS, 0 failures（pre-existing 1 timing flake 不计入）
- **AND** 测试计数 ≥ T21 ship baseline（动态计数）

#### Scenario: test_prompt_evidence_gate 专项

- **WHEN** 运行 `ctest --output-on-failure -R test_prompt_evidence_gate`
- **THEN** 19 cases PASS（3 事件断言已修正）

### Requirement: Payload 全文静态检查

After ship, the source code MUST NOT contain `args(nlohmann::json{{"prompt"` pattern in `src/modules/prompt/`.

#### Scenario: 静态 grep 验证

- **WHEN** `grep -rE 'args\(nlohmann::json\{\{"prompt"' src/modules/prompt/`
- **THEN** MUST 返回 0 行（payload 全文已清除）

#### Scenario: hash 字段已应用

- **WHEN** `grep -rE 'prompt_hash' src/modules/prompt/`
- **THEN** MUST 返回 ≥ 3 行（3 事件均使用 hash 字段）
