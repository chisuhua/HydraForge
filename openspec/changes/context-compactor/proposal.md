# context-compactor

## Why

- ADR-0007 快照机制已 ship 但无 LLM 摘要压缩——长对话上下文窗口溢出风险，是 pi-agent 借鉴路径 §七 的落点。
- `SessionConfig.compact_threshold_tokens` 已声明但**无触发逻辑**（声明-实施脱节）。
- `context.compact.{before,after}` 主题在 ADR-0068 附录 A 登记，但发射依赖本提案的 Compactor 落地。
- pi-agent compaction 模式：threshold/overflow 触发 + LLM 摘要 + 完整历史保留（摘要替换显示层，不删原始记录）。

## What Changes

**In Scope**:

- 新建 `src/core/context_compactor.{h,cpp}`（LLM 摘要调用接口 + 阈值检测钩子）
- 每轮结束 token 计数与阈值触发
- `context.compact.before/after` 事件发射（遵循 ADR-0068 附录 A）
- pdk_chat_demo `/compact` 命令（经 DECLARE_COMMAND 注册，依赖 adr-0070）
- 摘要-原始双层保留策略

**Out of Scope**:

- 摘要 prompt 调优（迭代项）
- 多级压缩层级
- ADR-0061-06 Trajectory IR 集成

## Capabilities

- MUST 压缩不删除原始消息（摘要仅替换 LLM 调用视图），与 pi-agent "完整历史保留" 对齐。
- MUST LLM 摘要调用经过 Decorator 链（成本计入 budget，合规记录）。
- MUST 事件字段遵循 ADR-0068 附录 A；MUST NOT 在 Compactor 内自造事件格式。
- SHOULD 阈值触发失败（LLM 错误）时降级为继续使用未压缩上下文 + 告警，不中断会话。

## Impact

- 内存：长对话 token 占用从 O(n) 降至 O(1) 摘要大小
- 延迟：每轮结束增加一次 token 计数 + 可选 LLM 摘要调用（异步不阻塞主流程）
- 二进制大小：新增 context_compactor.o (~50KB)
- 无破坏性变更：所有现有 API 零修改

## Acceptance

- [ ] 阈值自动触发 + `/compact` 手动触发两条路径测试通过
- [ ] `context.compact.before/after` 真实发射测试通过（字段含 tokens_before/tokens_after）
- [ ] ADR-0007 状态可从 🟡 Partial 提升 ✅ Approved
- [ ] ctest 全量零回归

