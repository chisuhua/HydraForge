# context-compactor — Technical Design

## 概述

实现 `ContextCompactor` 模块，提供 LLM 摘要压缩能力，在 token 计数超阈值时自动触发语义压缩，保持完整历史可查。

## 文件结构

```
src/core/context_compactor.h     # ContextCompactor 类声明
src/core/context_compactor.cpp   # 实现
include/agenticdsl/types/context_compactor.h  # Public API (L5)
tests/test_context_compactor.cpp # 单元测试
```

## 核心接口

```cpp
// 头文件: include/agenticdsl/types/context_compactor.h
class IContextCompactor {
public:
    virtual ~IContextCompactor() = default;
    // 压缩前回调 (发射 context.compact.before)
    virtual void on_compact_before(const std::string& session_id, size_t tokens_before) = 0;
    // 执行 LLM 摘要压缩
    virtual std::string compact(const std::string& history_json, ILLMProvider& llm) = 0;
    // 压缩后回调 (发射 context.compact.after)
    virtual void on_compact_after(const std::string& session_id, size_t tokens_before, size_t tokens_after) = 0;
    // 查询当前 token 计数
    virtual size_t count_tokens(const std::string& context_json) const = 0;
    // 检查是否需要压缩
    virtual bool should_compact(size_t token_count) const = 0;
};

// 注册到 Engine 的工厂函数
std::unique_ptr<IContextCompactor> create_context_compactor(
    size_t compact_threshold_tokens,
    std::shared_ptr<ILLMProvider> llm_provider,
    std::shared_ptr<IInteractionBus> event_bus
);
```

## 实现要点

### 1. Token 计数

- 使用 `DslEngine::count_tokens()` 或直接调用 LLM provider 的 token 计数接口
- `should_compact(threshold)` 比较 token_count > threshold

### 2. LLM 摘要调用

- 调用 `llm.generate(summary_prompt)` 经过 Decorator 链（成本计入 budget）
- Summary prompt 模板:
  ```
  请将以下对话历史压缩为 200 字以内的摘要，保留关键信息:
  {history_text}
  ```

### 3. 双层保留策略

- 原始消息存入 `context.original_messages` (不删除)
- 摘要替换 `context.working` 中的 LLM 调用视图
- `context.metadata.compaction记录 {before_tokens, after_tokens, summary_length, timestamp}`

### 4. 事件发射 (ADR-0068 附录 A)

```cpp
// context.compact.before
event_bus_->emit(EventBuilder("context.compact.before", ToolResult{}
    .args({{"session_id", session_id}, {"tokens_before", tokens_before}})
    .meta(trace_metadata())
    .build());

// context.compact.after
event_bus_->emit(EventBuilder("context.compact.after", ToolResult{}
    .args({
        {"session_id", session_id},
        {"tokens_before", tokens_before},
        {"tokens_after", tokens_after},
        {"summary_length", summary.size()}})
    .meta(trace_metadata())
    .build());
```

### 5. /compact 命令注册 (DECLARE_COMMAND)

```cpp
DECLARE_COMMAND("compact", "手动触发一次上下文压缩", Cognitive,
    YOLO,  // 或 Agent policy
    [](const std::vector<std::string>& args, Context& ctx) -> ToolResult {
        auto& compactor = ctx.get_compactor();
        size_t tokens = compactor.count_tokens(ctx.working.dump());
        std::string summary = compactor.compact(ctx.working.dump(), *ctx.llm_provider);
        compactor.on_compact_after(ctx.session_id, tokens, compactor.count_tokens(ctx.working.dump()));
        return ToolResult::success({{"summary", summary}});
    });
```

## 集成点

### DSLEngine 集成

在 `DSLEngine::run()` 每轮结束调用:
```cpp
void DSLEngine::check_and_compact(Context& ctx) {
    auto token_count = compactor_->count_tokens(ctx.working.dump());
    if (compactor_->should_compact(token_count)) {
        compactor_->on_compact_before(ctx.session_id, token_count);
        std::string summary = compactor_->compact(ctx.working.dump(), *llm_provider_);
        compactor_->on_compact_after(ctx.session_id, token_count,
            compactor_->count_tokens(ctx.working.dump()));
    }
}
```

### 阈值配置

- `SessionConfig.compact_threshold_tokens` (Sprint 0 已声明, 现实施触发逻辑)
- 默认值: 4096 tokens
- 可通过 `DSLEngine::set_compact_threshold(size_t)` 配置

## 测试策略

1. **Token 计数测试**: mock LLM provider 返回固定 token 数，验证阈值判断
2. **自动触发测试**: 模拟超阈值，验证 before/after 事件发射
3. **手动 /compact 测试**: 验证立即触发 + 返回摘要
4. **Decorator 链测试**: 验证成本计入 budget
5. **E2E 事件测试**: 验证 context.compact.{before,after} payload 字段

## 依赖

- `ILLMProvider` (Decorator 链)
- `IInteractionBus` (事件发射)
- `SessionConfig.compact_threshold_tokens`
- DECLARE_COMMAND (ADR-0070, ✅ 已 ship)
- EventBuilder (ADR-0068 V2, ✅ 已 ship)
