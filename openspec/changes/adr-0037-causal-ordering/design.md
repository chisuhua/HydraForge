# 单进程逻辑时钟与因果向量设计

## Context

ADR-0037 §决策 1: 不选 Lamport 时间戳（单机多线程不需要跨节点同步），采用单进程逻辑时钟 + 因果向量。本 change 实现该基础设施。

## Design

### CausalClock

```cpp
// src/common/contract/causal_clock.h
namespace agenticdsl::event {

class CausalClock {
public:
    using TimePoint = uint64_t;

    // 单调递增 tick
    TimePoint tick();

    // 当前值（只读）
    TimePoint now() const;

    // 合并外部时间（取 max）
    void merge(TimePoint external_time);

    // 生成 happens-before 判定
    static bool happens_before(TimePoint a, TimePoint b);

private:
    std::atomic<TimePoint> clock_{0};
};

} // namespace agenticdsl::event
```

**线程安全**: `std::atomic<uint64_t>` + `fetch_add(1, memory_order_relaxed)` — 最小开销，单增保证。

### BusEvent 扩展

```cpp
struct BusEvent {
    std::string topic;
    nlohmann::json payload;
    Priority priority;
    std::chrono::steady_clock::time_point wall_time;
    CausalClock::TimePoint causal_time;  // 新增
};
```

### InMemoryBus 集成

```cpp
class InMemoryBus {
    CausalClock clock_;

    void emit(const std::string& topic, const nlohmann::json& payload) {
        auto causal = clock_.tick();
        queue_.try_emit({topic, payload, Priority::Normal, now(), causal});
    }
};
```

**消费者侧**:
- `EventCallback(topic, payload)` → 改为 `EventCallback(BusEvent)`, payload 内嵌 causal_time
- 消费者可通过 `event.causal_time` 判断 `happens-before` 关系

### happens-before 判定

```cpp
// 消费者可以使用 causal_time 重建因果顺序
std::map<CausalClock::TimePoint, BusEvent> received;

void on_event(const BusEvent& e) {
    // 检查是否早于已收到的所有事件
    for (auto& [time, prev] : received) {
        if (CausalClock::happens_before(time, e.causal_time)) {
            // e 发生在 prev 之后 → 正常
        } else {
            // 可能存在乱序（但时间戳仍然可排序）
        }
    }
}
```

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| `uint64_t` 溢出 | 1 tick/emit, 每秒 10K events → 58M 年后溢出 |
| atomic 开销 | `memory_order_relaxed` 最小开销，与普通 load 相当 |
| 跨 Worker 无强因果保证 | 单进程内 tick() 在 emit 前，保证 emit 顺序 = tick 顺序 |