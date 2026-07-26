# IInteractionBus Topic Subscribe 扩展设计

## Context

ADR-0019 §状态变更日志 (2026-07-06): ADR-0046 §2.1 要求 PDK Plugin 间通信支持 topic-based subscribe。当前接口只有 session-based `subscribe_events()`。

## Design

### IInteractionBus 接口扩展

```cpp
class IInteractionBus {
public:
    // 现有 (V2.1): session-based
    using EventCallback = std::function<void(const std::string&, const nlohmann::json&)>;
    virtual void subscribe_events(const std::string& session_id,
                                  EventCallback callback) = 0;

    // 新增 (V2.2): topic-based for PDK plugin 间通信
    using TopicCallback = std::function<void(const std::string&, const nlohmann::json&)>;
    virtual size_t subscribe_topic(
        const std::string& topic_pattern,   // glob: "inference.lifecycle.*"
        TopicCallback callback) = 0;
    virtual void unsubscribe(size_t token) = 0;

    // ... emit / shutdown 等现有方法保持不变
};
```

**设计原则**:
- `subscribe_topic()` 返回 `size_t token` 作为退订句柄（与 POSIX `epoll_ctl` / `timerfd_create` 一致）
- `TopicCallback` 与 `EventCallback` 签名相同（`topic, payload`），简化内部 dispatch

### Glob 匹配

```cpp
// 内部 helper: glob_match("inference.lifecycle.*", "inference.lifecycle.idle") → true
// 支持 * 匹配任意字符; ? 匹配单个字符
// 不使用 regex (编译开销大，glob 足够覆盖 PDK 事件命名)
static bool glob_match(const std::string& pattern, const std::string& topic);
```

Topic 命名约定（ADR-0046 §2.1）:
- `temporal.workflow.*` → Temporal Agent 事件
- `inference.lifecycle.*` → 推理引擎事件
- `*.error` → 所有错误事件

### InMemoryBus 实现变更

```cpp
class InMemoryBus : public IInteractionBus {
    // 新增字段
    struct TopicSubscription {
        std::string pattern;
        TopicCallback callback;
    };
    std::unordered_map<size_t, TopicSubscription> topic_subscribers_;
    std::atomic<size_t> next_token_{1};

public:
    size_t subscribe_topic(const std::string& pattern, TopicCallback cb) override {
        size_t token = next_token_++;
        std::lock_guard lock(mutex_);
        topic_subscribers_[token] = {pattern, std::move(cb)};
        return token;
    }

    void unsubscribe(size_t token) override {
        std::lock_guard lock(mutex_);
        topic_subscribers_.erase(token);
    }
};
```

**dispatch_thread 扩展**:
```cpp
// 原有: 仅 dispatch 到 session subscribers
// 新增: 同时 dispatch 到匹配的 topic subscribers
void dispatch_loop() {
    while (running_) {
        auto event = queue_.try_consume();
        if (!event) continue;

        // 分发到 session subscribers (现有逻辑)
        for (auto& [sid, cb] : session_subscribers_) {
            cb(event->topic, event->payload);
        }

        // 分发到 topic subscribers (新增)
        for (auto& [token, sub] : topic_subscribers_) {
            if (glob_match(sub.pattern, event->topic)) {
                sub.callback(event->topic, event->payload);
            }
        }
    }
}
```

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| glob_match 性能（每事件遍历所有 subscriber） | 期望 <50 subscribers; 若超 100 改用 trie |
| unsubscribe 后 token 泄漏 | `next_token_` 64-bit，滚动溢出概率极低 |
| API BREAKING | 所有 IInteractionBus 实现类需要 override 新方法 |