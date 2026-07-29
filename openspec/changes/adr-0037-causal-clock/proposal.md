## Why

ADR-0037 §决策 1 要求单进程逻辑时钟 + 因果向量，实现 happens-before 判定。当前 EventBus 链 (Change A, `adr-0002-busevent-contract`) 已在 `BusEvent` 预留 `causal_time` 字段，本 Change C 完成最后一公里落地：CausalClock 结构体 + InMemoryBus::emit() 自动 tick + attach。

## What Changes

- **新增** `CausalClock` 类 (`include/agenticdsl/contract/causal_clock.h`) — atomic<uint64_t> 单调递增逻辑时钟，提供 tick/now/merge/happens_before
- **修改** `InMemoryBus::emit(const BusEvent&)` — emit 时自动调用 `causal_clock_.tick()` 并写入 `event.causal_time`
- **修改** `InMemoryBus::emit(const string&, const string&)` — 向后兼容入口同样自动 tick + attach
- **新增** `tests/test_causal_clock.cpp` — 5 个 TEST_CASE (单调性 / 线程安全 / merge / happens_before / 3 生产者 soak)
- **依赖**: `change:adr-0002-busevent-contract` — BusEvent.causal_time 字段已在 Change A 预留

## Capabilities

### New Capabilities
- `causal-clock`: 单进程逻辑时钟 (CausalClock) 定义 + InMemoryBus emit 时自动 tick 附加 + happens-before 判定

### Modified Capabilities
- （无修改 — 纯增量非破坏）

## Impact

- **include/agenticdsl/contract/causal_clock.h**: 新增 — CausalClock struct (atomic tick, merge, happens_before)
- **include/agenticdsl/contract/inmemory_bus.h**: 新增 `#include "agenticdsl/contract/causal_clock.h"` + `event::CausalClock causal_clock_` 成员
- **src/common/contract/inmemory_bus.cpp**: 两个 emit() 重载中调用 `causal_clock_.tick()` 并赋值到 `e.causal_time`
- **tests/test_causal_clock.cpp**: 新增 — 5 test cases / 10 assertions

## Non-goals

- 不实现跨进程 Lamport 时间戳 (ADR-0037 §决策 1 明确 V1 单进程，跨进程升级 deferred)
- 不实现 per-producer 向量时钟 (单进程逻辑时钟 + `happens_before(a, b) = a < b` 足够)
- 不修改 `BusEvent` 结构体 (causal_time 字段已在 Change A 预留)
- 不修改 `IInteractionBus` 接口 (emit/subscribe 签名不变)