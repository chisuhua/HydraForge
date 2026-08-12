# context-compactor — Implementation Tasks

## 1. 模块骨架

- [ ] 1.1 创建 `include/agenticdsl/types/context_compactor.h` (IContextCompactor 抽象接口 + create_context_compactor 工厂声明)
- [ ] 1.2 创建 `src/core/context_compactor.cpp` (空实现骨架)
- [ ] 1.3 添加 `src/core/CMakeLists.txt` 条目 (context_compactor.h + context_compactor.cpp)
- [ ] 1.4 注册到 `agenticdsl_core` 库 target

## 2. 核心接口实现

- [ ] 2.1 实现 `count_tokens()` — 调用 LLM provider tokenize 或简单字符计数代理
- [ ] 2.2 实现 `should_compact(threshold)` — token_count > threshold 判断
- [ ] 2.3 实现 `compact(history_json, llm)` — 调用 LLM 生成摘要
- [ ] 2.4 实现 `on_compact_before()` / `on_compact_after()` 事件发射 (ADR-0068)

## 3. LLM 摘要调用 (Decorator 链)

- [ ] 3.1 设计 Summary Prompt 模板 (200 字以内保留关键信息)
- [ ] 3.2 确保 compact() 调用经过 Decorator 链 (CostTracking + RateLimit)
- [ ] 3.3 错误处理: LLM 失败时降级为继续使用未压缩上下文 + 告警

## 4. 双层保留策略

- [ ] 4.1 原始消息存入 `context.original_messages` (只追加不删除)
- [ ] 4.2 摘要替换 `context.working` 中 LLM 调用视图
- [ ] 4.3 metadata.compaction_record 写入 {before_tokens, after_tokens, summary_length, timestamp}

## 5. /compact 命令注册 (DECLARE_COMMAND)

- [ ] 5.1 设计 `/compact` 命令签名 (无需参数)
- [ ] 5.2 实现命令处理函数 (调用 compact + 返回摘要)
- [ ] 5.3 注册到 CommandRegistry (DECLARE_COMMAND 宏, 依赖 ADR-0070)

## 6. DSLEngine 集成

- [ ] 6.1 每轮结束 `check_and_compact()` 调用 (在 `run()` 循环尾部)
- [ ] 6.2 `SessionConfig.compact_threshold_tokens` 配置注入
- [ ] 6.3 验证 /compact 手动触发路径

## 7. 事件契约 (ADR-0068 附录 A)

- [ ] 7.1 `context.compact.before` payload: {session_id, tokens_before}
- [ ] 7.2 `context.compact.after` payload: {session_id, tokens_before, tokens_after, summary_length}
- [ ] 7.3 EventBuilder 链式调用 (参考 tool.execution.start/end 模式)

## 8. 单元测试

- [ ] 8.1 Token 计数 + 阈值判断测试
- [ ] 8.2 自动触发 (超阈值) 测试
- [ ] 8.3 手动 /compact 触发测试
- [ ] 8.4 事件 payload 验证测试
- [ ] 8.5 LLM 错误降级测试

## 9. 集成测试 + 验收

- [ ] 9.1 `test_context_compactor.cpp` 全部 case PASS
- [ ] 9.2 ctest 全量零回归
- [ ] 9.3 ADR-0007 状态 🟡 Partial → ✅ Approved (impl-scope 审计同步)
