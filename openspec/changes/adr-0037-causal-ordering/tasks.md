## 实现 (3h)

### CausalClock

- [ ] 1.1 新建 `src/common/contract/causal_clock.h` — CausalClock 类声明
- [ ] 1.2 新建 `src/common/contract/causal_clock.cpp` — 实现
  - `tick()`: `fetch_add(1, memory_order_relaxed)`
  - `now()`: `load(memory_order_relaxed)`
  - `merge(external)`: `fetch_max(external)`
  - `happens_before(a, b)`: `a < b`
- [ ] 1.3 新建 `tests/test_causal_clock.cpp` — 4 个 test case
  - 单调性: `t1 = tick(); t2 = tick(); REQUIRE(t1 < t2)`
  - 线程安全: 10 threads × 1000 ticks → 最终值 = 10000
  - merge: `clock.merge(1000); REQUIRE(clock.now() >= 1000)`
  - happens_before: `REQUIRE(CausalClock::happens_before(1, 2))`

### BusEvent 扩展

- [ ] 2.1 `include/agenticdsl/contract/bus_event.h` — 新增 `causal_time` 字段
  - 若 BusEvent 为独立文件，新增字段
  - 若事件结构定义在 IInteractionBus 中，新增 `CausalClock::TimePoint causal_time{0}` 默认值

### InMemoryBus 集成

- [ ] 3.1 `inmemory_bus.h` — 新增 `CausalClock clock_` 成员
- [ ] 3.2 `inmemory_bus.cpp::emit()` — `auto causal = clock_.tick()` + attach 到 BusEvent
- [ ] 3.3 CMakeLists: `src/common/contract/CMakeLists.txt` 新增 `causal_clock.cpp`

### 验收

- [ ] 4.1 `ctest -R test_causal_clock` 4/4 PASS
- [ ] 4.2 `ctest -R test_interaction_bus` 全绿（causal_time 不影响已有行为）
- [ ] 4.3 `ctest` 全量零回归