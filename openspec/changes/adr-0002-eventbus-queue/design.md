# EventBus 有界队列核心组件设计

## Context

ADR-0002 §决策 2 定义了事件优先级与背压策略。本 change 提取核心有界队列组件，供 InMemoryBus 内部使用，为 Phase 2 EventBus 落地铺路。

## Design

### EventBus Core API

```cpp
// src/common/contract/event_bus.h
namespace agenticdsl::event {

enum class Priority { Critical = 0, Normal = 1, Low = 2 };

struct BusEvent {
    std::string topic;
    nlohmann::json payload;
    Priority priority;
    std::chrono::steady_clock::time_point timestamp;
};

class EventBusQueue {
public:
    explicit EventBusQueue(size_t capacity = 1024);

    // 非阻塞写入；满时按优先级丢弃最旧 Normal/Low 事件
    bool try_emit(BusEvent event);

    // 阻塞读取（可超时）
    std::optional<BusEvent> try_consume(std::chrono::milliseconds timeout = 100ms);

    // 统计
    size_t size() const;
    size_t dropped() const;

private:
    // 内部：std::priority_queue + std::mutex + std::condition_variable
    std::priority_queue<BusEvent, ...> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    size_t capacity_;
    std::atomic<size_t> dropped_{0};
};
```

### 背压策略

```
当队列满时:
  if (event.priority == Critical):
    → 丢弃最旧的 Normal/Low，插入 Critical
  elif (event.priority == Normal):
    → 丢弃最旧的 Low，插入 Normal
  else: // Low
    → 丢弃新事件（不插入），++dropped_
```

### InMemoryBus 集成

```cpp
// inmemory_bus.cpp 内部替换:
// 旧: std::queue<std::pair<std::string, nlohmann::json>> event_queue_;
// 新: agenticdsl::event::EventBusQueue queue_{1024};

void InMemoryBus::emit(const std::string& topic, const nlohmann::json& payload) {
    queue_.try_emit({topic, payload, Priority::Normal, now()});
}
```

**API 不变**: IInteractionBus 公开接口零修改，`emit()` / `subscribe_events()` / `subscribe_topic()` 签名完全保持。

### 线程安全

- `try_emit()` 和 `try_consume()` 内部加锁
- `dropped_` 使用 `std::atomic<size_t>` 无锁读取
- 与现有 InMemoryBus `dispatch_thread` 模式完全兼容

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| 优先级队列引入额外开销 | `std::priority_queue` 对数插入，可接受（<10K events/s） |
| 与现有 API 不兼容 | EventBusQueue 仅在 InMemoryBus 内部使用，零公开 API 变更 |
| Phase 2 可能需要不同队列结构 | EventBusQueue 封装内部实现，Phase 2 可替换为 lock-free SPSC queue |