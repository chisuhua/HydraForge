# context-compactor Specification

## Purpose

实现 LLM 驱动的语义上下文压缩，通过 token 阈值检测自动触发摘要，保持完整历史可查。参考 pi-agent compaction 模式。

## ADDED Requirements

### Requirement: context-compactor-token-threshold-detects

`ContextCompactor::should_compact()` MUST 当 token_count > compact_threshold_tokens 时返回 true，触发压缩流程。

#### Scenario: threshold detection
- **GIVEN** compact_threshold_tokens=4096，token_count=5000
- **WHEN** 调用 should_compact(5000)
- **THEN** 返回 true，触发 compact()

#### Scenario: below threshold
- **GIVEN** compact_threshold_tokens=4096，token_count=3000
- **WHEN** 调用 should_compact(3000)
- **THEN** 返回 false，不触发压缩

### Requirement: context-compactor-dual-layer-preservation

压缩 MUST 不删除原始消息，摘要仅替换 LLM 调用视图，原始历史存入 context.original_messages。

#### Scenario: dual preservation on compact
- **GIVEN** 会话超阈值触发压缩
- **WHEN** compact() 执行完成
- **THEN** context.original_messages 含完整原始历史 AND context.working 中 LLM 调用视图已替换为摘要

### Requirement: context-compactor-event-emit-before-after

压缩 MUST 在 context.compact.before 事件后触发 LLM 摘要，并在 context.compact.after 事件中记录 tokens_before/tokens_after。

#### Scenario: event emission sequence
- **GIVEN** 超阈值触发压缩
- **WHEN** 压缩流程执行
- **THEN** 先发射 context.compact.before (tokens_before)，再调用 LLM 生成摘要，后发射 context.compact.after (tokens_before/tokens_after)

### Requirement: context-compactor-decorator-chain-cost-tracking

LLM 摘要调用 MUST 经过 Decorator 链（CostTrackingDecorator）计入 budget，合规记录。

#### Scenario: cost tracked via decorator
- **GIVEN** compact() 调用 LLM
- **WHEN** LLM generate() 执行
- **THEN** CostTrackingDecorator 记录成本到 budget

### Requirement: context-compactor-command-registered

`/compact` 命令 MUST 经 DECLARE_COMMAND 注册，可手动触发压缩不等阈值。

#### Scenario: /compact manual trigger
- **GIVEN** 用户输入 /compact
- **WHEN** 命令派发
- **THEN** 立即执行一次压缩（不等阈值）并返回摘要结果
