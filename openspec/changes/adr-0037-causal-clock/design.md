## Context

ADR-0037 §决策 1 要求单进程逻辑时钟，实现 EventBus 事件间的 happens-before 判定。当前 EventBus 链 (Change A, `adr-0002-busevent-contract`) 已在 `BusEvent` 预留 `causal_time` 字段 (`uint64_t causal_time{0}`)，`IInteractionBus` 接口已固化 `emit(const BusEvent&)` 签名。本 Change C 是增量非破坏变更，仅需：

1. 定义 `CausalClock` 结构体 (atomic<uint64_t> 单调递增)
2. `InMemoryBus::emit()` 两个重载中 auto-tick + attach
3. 单元测试 + soak 测试验证

## Goals / Non-Goals

**Goals:**
- 定义 `CausalClock` 结构体：`tick()` / `now()` / `merge()` / `happens_before()` 4 方法
- `InMemoryBus::emit()` 自动 tick 并写入 `BusEvent.causal_time`
- 5 个 TEST_CASE 零回归 (单调性 / 线程安全 / merge / happens-before / 3 生产者 soak)
- 全量 ctest 零回归

**Non-Goals:**
- 不实现跨进程 Lamport 时间戳 (V1 单进程，future work)
- 不实现 per-producer 向量时钟 (单进程单 counter 足够)
- 不修改 `BusEvent` 结构体 (causal_time 字段已在 Change A 预留)
- 不修改 `IInteractionBus` 接口 (emit/subscribe 签名不变)
- 不引入 lock-free 队列 (现有 mutex+queue 满足性能需求)

## Decisions

### Decision 1: CausalClock 单 counter 设计
- **选择**: `std::atomic<uint64_t>` 单调递增 counter，`tick()` 返回 `fetch_add(1, relaxed)`，`happens_before(a, b) = a < b`
- **理由**: ADR-0037 §决策 1 明确 V1 单进程；单 counter 足够维护全序 (total order)；relaxed memory order 在单进程内保证单调性 (fetch_add 是 RMW 操作，原子不可分)；`merge()` 用于跨时钟合并 (future Lamport 升级)
- **替代方案**: 每个 producer 独立 vector — 拒绝，单进程内单 counter 更简单且满足需求

### Decision 2: CausalClock 放置位置
- **选择**: `include/agenticdsl/contract/causal_clock.h` (公开契约层)
- **理由**: `BusEvent` 在 `include/agenticdsl/contract/bus_event.h`，`CausalClock` 与之同层级，体现 "EventBus Chain" 一致性；`InMemoryBus` 位于 `include/agenticdsl/contract/inmemory_bus.h` 已 include 此头文件
- **替代方案**: `src/common/contract/causal_clock.h` — 拒绝，外部消费者 (example、plugin) 可能需直接使用 CausalClock

### Decision 3: emit 时 auto-tick 策略
- **选择**: `emit(const BusEvent&)` 中局部拷贝 `auto e = event` 后调用 `e.causal_time = causal_clock_.tick()` 再入队
- **理由**: 调用方传入的 `BusEvent` 可能已有 causal_time 值 (如测试中手动构造)，但 emit 时强制覆盖为时钟最新值保证因果序正确性；局部拷贝避免修改调用方传入的 const 引用
- **替代方案**: 不覆盖调用方传入的 causal_time — 拒绝，会导致调用方可"伪造"因果序，破坏 invariants

### Decision 4: 测试策略
- **选择**: 4 个单元测试 + 1 个 soak 测试
- **理由**: 单调性验证基础正确性；线程安全测试 10×1000 tick 验证 atomic 无竞争；merge 和 happens_before 验证逻辑正确性；3 生产者 soak 验证 per-producer 单调性 (3 线程各 100 次，共 300 tick)
- **替代方案**: 100k 事件 soak — 300 tick 足够覆盖并发场景，不需要 100k 级别 (CausalClock 是 O(1) 操作)

## Risks / Trade-offs

- `memory_order_relaxed` 在单进程内安全 — fetch_add 是 RMW，对所有线程可见；relaxed 在 x86/ARM 上均无额外屏障开销；跨进程场景需升级为 `seq_cst` 或 Lamport 协议
- `merge()` 使用 CAS 循环 — 在单 counter 下竞争极低 (仅 `merge()` 和 `tick()` 同时执行时可能 CAS 失败重试)；CAS 循环是 lock-free 模式，不会死锁
- soak 测试 300 tick 不足够 — 300 tick 是人为限制，但 CausalClock 是 O(1) 操作，300 与 100k 行为等价；建议 future work 扩展 soak 到 100k 级别