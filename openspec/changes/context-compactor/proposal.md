# context-compactor

## Why

- ADR-0007 快照机制已 ship 但无 LLM 摘要压缩——长对话上下文窗口溢出风险，是 pi-agent 借鉴路径 §七 的落点。
- `SessionConfig.compact_threshold_tokens` 已声明但**无触发逻辑**（声明-实施脱节）。
- `context.compact.{before,after}` 主题在 ADR-0068 附录 A 登记，但发射依赖本提案的 Compactor 落地。
- pi-agent compaction 模式：threshold/overflow 触发 + LLM 摘要 + 完整历史保留（摘要替换显示层，不删原始记录）。

## What Changes

**In Scope**:

- (TBD)

### 关键场景

- GIVEN 会话 token 数超过 `compact_threshold_tokens`，WHEN 本轮结束，THEN 自动触发压缩：先发射 `context.compact.before`，LLM 摘要生成后发射 `context.compact.after`，后续 LLM 调用使用摘要上下文。
- GIVEN 用户输入 `/compact`，WHEN 命令派发，THEN 立即执行一次压缩（不等阈值）。
- GIVEN 压缩已发生，WHEN 导出/持久化会话，THEN 原始完整历史仍可访问（双层保留）。

**Out of Scope**:

- (TBD)

## Capabilities

- MUST 压缩不删除原始消息（摘要仅替换 LLM 调用视图），与 pi-agent "完整历史保留" 对齐。
- MUST LLM 摘要调用经过 Decorator 链（成本计入 budget，合规记录）。
- MUST 事件字段遵循 ADR-0068 附录 A；MUST NOT 在 Compactor 内自造事件格式。
- SHOULD 阈值触发失败（LLM 错误）时降级为继续使用未压缩上下文 + 告警，不中断会话。

## Impact

- MUST 压缩不删除原始消息（摘要仅替换 LLM 调用视图），与 pi-agent "完整历史保留" 对齐。
- MUST LLM 摘要调用经过 Decorator 链（成本计入 budget，合规记录）。
- MUST 事件字段遵循 ADR-0068 附录 A；MUST NOT 在 Compactor 内自造事件格式。
- SHOULD 阈值触发失败（LLM 错误）时降级为继续使用未压缩上下文 + 告警，不中断会话。

## Acceptance

- 阈值自动触发 + `/compact` 手动触发两条路径测试通过。
- `context.compact.before/after` 真实发射测试通过（字段含 tokens_before/tokens_after）。
- ADR-0007 状态可从 🟡 Partial 提升 ✅ Approved（impl-scope 审计同步）。
- ctest 全量零回归。

