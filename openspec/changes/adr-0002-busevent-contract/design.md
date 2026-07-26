# BusEvent 公开契约设计

## Context

Oracle 评审结论（2026-07-26, ses_0634abe18ffe）：当前三个 EventBus 相关 change 引入三种回调形态（ToolResult → (string,json) → BusEvent），订阅方需经历 2-3 次破坏性迁移。正确做法：一次性收敛到 BusEvent，之后全部增量变更。

## Design

### BusEvent 结构

```cpp
// include/agenticdsl/contract/bus_event.h
namespace agenticdsl {

enum class EventPriority { Critical = 0, Normal = 1, Low = 2 };

struct BusEvent {
    std::string topic;
    ToolResult payload;                              // ADR-0023 标准载荷
    std::chrono::steady_clock::time_point timestamp; // wall time
    uint64_t causal_time{0};                         // 预留，Change C 填充
    EventPriority priority{EventPriority::Normal};    // 预留，Phase 2 使用
};

} // namespace agenticdsl
```

**设计决策**：
- `payload` 保留 `ToolResult`（非 json）— 架空 ADR-0023 不可接受
- `causal_time` 默认 0 保证向后兼容 — Change C 非破坏增量
- `priority` 默认 Normal — 将来有界队列 change 零 API 影响

### IInteractionBus BREAKING 变更

```cpp
class IInteractionBus {
public:
    virtual ~IInteractionBus() = default;

    virtual void emit(const BusEvent& event) = 0;                         // 新签名
    virtual void emit(const std::string& event_type,
                      const std::string& content) = 0;                    // 保留（兼容）

    virtual size_t subscribe(const std::string& event_type,
                             std::function<void(const BusEvent&)>) = 0;   // 新签名

    virtual void unsubscribe(size_t token) = 0;                           // 不变
};
```

### 迁移指南（4 个实现者）

**InMemoryBus**：
```cpp
// 内部变更：
// 旧: std::queue<std::pair<std::string, ToolResult>> queue_;
// 新: std::queue<BusEvent> queue_;

void InMemoryBus::emit(const std::string& event_type, const ToolResult& payload) {
    queue_.push({event_type, payload, now()});
}

// 适配
void InMemoryBus::emit(const BusEvent& event) {
    queue_.push(event);
}

void InMemoryBus::emit(const std::string& event_type, const std::string& content) {
    ToolResult tr = ToolResult::success(nlohmann::json::object(),
                                        nlohmann::json{{"content", content}});
    queue_.push({event_type, tr, now()});
}
```

dispatch_loop 变更：
```cpp
// 旧: std::pair<std::string, ToolResult> event = queue_.front();
// 新: BusEvent event = queue_.front();

// 回调调用：
// 旧: cb(event.second); // ToolResult
// 新: cb(event);        // BusEvent
```

**3 个测试 Mock**：`MockInteractionBus` / `MockBus` / `MockBusForEscalation` — 更新 `emit(substr, ToolResult)` → `emit(BusEvent)`，订阅回调更新签名。

### 不做的事

- **不实现 priority queue**：`std::queue<BusEvent>` 保持 FIFO。有界队列 + backpressure 推迟至 Phase 2（需实测 emit >10K/s 或慢 subscriber 拖垮队列）。
- **原因**：零消费者；InMemoryBus MPMC 异步分发（Sprint 12 P2）已解决背压主场景。

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| 4 个实现者手动迁移，漏改风险 | tasks.md 逐文件核对清单 |
| 回调签名变更影响所有订阅方 | 仅此一次破坏，后续为零破坏增量 |
| `emit(string, string)` 重载语义 | 内部自动包装为 BusEvent，调用方无感知 |