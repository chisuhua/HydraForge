# 因果逻辑时钟设计

## Context

Change A 已定义 `BusEvent.causal_time{0}`。本 change 实现 CausalClock 并在 InMemoryBus 集成。纯增量：不影响接口签名、不改变回调契约。

## Design

### CausalClock

```cpp
// src/common/contract/causal_clock.h
namespace agenticdsl::event {

class CausalClock {
public:
    using TimePoint = uint64_t;

    TimePoint tick();
    TimePoint now() const;
    void merge(TimePoint external);
    static bool happens_before(TimePoint a, TimePoint b);

private:
    std::atomic<TimePoint> clock_{0};
};

} // namespace agenticdsl::event
```

`tick()`: `fetch_add(1, memory_order_relaxed)` — 最小开销。
`merge(external)`: `fetch_max(external)` — 合并外部时钟（预留 Phase 2 跨进程扩展）。
`happens_before(a, b)`: `a < b` — 简单高效。

### InMemoryBus 集成

```cpp
class InMemoryBus {
    CausalClock clock_;

    void emit(const std::string& event_type, const ToolResult& payload) {
        auto event = BusEvent{event_type, payload, now(), clock_.tick()};
        queue_.push(std::move(event));
    }
};
```

**emit 时自动 tick + attach** — 所有事件附带因果序。

### 消费者侧

```cpp
// happens-before 判定为工具函数，不强加于消费者：
void consumer(const BusEvent& e1, const BusEvent& e2) {
    if (CausalClock::happens_before(e1.causal_time, e2.causal_time)) {
        // e1 因果先于 e2
    }
}
```

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| uint64_t 溢出 | 1 tick/emit, 10K events/s → 58M 年后溢出 |
| atomic overhead | memory_order_relaxed，与普通 load 相当 |
| 跨 Worker 无强因果 | 单进程内 tick 在 emit 前，保证 emit 顺序 = tick 顺序 |
| 升级触发条件 | 出现跨进程/分布式需求 → 升级为 Lamport timestamp |