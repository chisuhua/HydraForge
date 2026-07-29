## ADDED Requirements

### Requirement: CausalClock 单调递增逻辑时钟
CausalClock SHALL 提供基于 `std::atomic<uint64_t>` 的单调递增逻辑时钟，支持 `tick()` / `now()` / `merge()` / `happens_before()` 4 种操作。

#### Scenario: tick 单调递增
- **WHEN** 依次调用 `tick()` 三次
- **THEN** 返回值依次为 1, 2, 3，且 `now()` 返回 3

#### Scenario: 线程安全 tick
- **WHEN** 10 个线程各调用 `tick()` 1000 次
- **THEN** `now()` 返回 10000（无竞争丢失）

#### Scenario: merge 取最大值
- **WHEN** 在 `tick()` 一次后调用 `merge(1000)`
- **THEN** `now()` >= 1000
- **WHEN** 再次 `tick()`
- **THEN** `now()` >= 1001

#### Scenario: happens_before 判定
- **WHEN** 比较 `happens_before(1, 2)`
- **THEN** 返回 true
- **WHEN** 比较 `happens_before(2, 1)`
- **THEN** 返回 false
- **WHEN** 比较 `happens_before(5, 5)`
- **THEN** 返回 false (同时发生)

### Requirement: InMemoryBus emit 自动 tick + attach
InMemoryBus 的两个 `emit()` 重载 SHALL 在发射时自动调用 `CausalClock::tick()` 并将返回值写入 `BusEvent.causal_time`。调用方传入的 causal_time 值 SHALL 被覆盖，以确保因果序正确性。

#### Scenario: emit(BusEvent) 自动 tick
- **WHEN** 调用 `bus.emit(BusEvent{...})` 且 causal_time 未被设置
- **THEN** 事件队列中的 BusEvent 的 causal_time 被设置为时钟的 tick 值（> 0）
- **WHEN** 调用 `bus.emit(BusEvent{...})` 两次
- **THEN** 第二个事件的 causal_time > 第一个事件的 causal_time

#### Scenario: emit(string, string) 自动 tick
- **WHEN** 调用 `bus.emit("topic", "content")`
- **THEN** 内部构造的 BusEvent 的 causal_time 被设置为时钟的 tick 值（> 0）

### Requirement: 3 生产者 per-producer 单调性
当多个生产者并发通过 `CausalClock::tick()` 获取时间戳时，SHALL 保证每个生产者线程观察到的时间戳序列是严格单调递增的（即每个生产者获得的 tick 值序列中，后一个值大于前一个值）。

#### Scenario: 3 生产者 per-producer 单调
- **WHEN** 3 个线程各调用 `tick()` 100 次，记录每次的返回值
- **THEN** 每个线程记录的时间戳序列中，第 i+1 个值 > 第 i 个值（per-producer 单调）
- **THEN** `now()` 返回 300（无竞争丢失）