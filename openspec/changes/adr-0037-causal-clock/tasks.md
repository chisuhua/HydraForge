## Change C: CausalClock (~3h)

### CausalClock 实现

- [ ] 1.1 新建 `src/common/contract/causal_clock.h` — 类声明 + `using TimePoint = uint64_t`
- [ ] 1.2 新建 `src/common/contract/causal_clock.cpp` — 实现
  - `tick()`: `fetch_add(1, memory_order_relaxed)`
  - `now()`: `load(memory_order_relaxed)`
  - `merge(external)`: `fetch_max(external)` — 预留跨进程
  - `happens_before(a, b)`: `a < b`
- [ ] 1.3 `CMakeLists.txt` — `src/common/contract/` 新增 `causal_clock.cpp`

### InMemoryBus 集成

- [ ] 2.1 `inmemory_bus.h` — 新增 `CausalClock clock_` 成员
- [ ] 2.2 `inmemory_bus.cpp::emit()` — `clock_.tick()` + attach 到 BusEvent
  - `emit(string, ToolResult)` 路径: `BusEvent{..., clock_.tick()}`
  - `emit(string, string)` 路径: 同上
- [ ] 2.3 验证：`inmemory_bus.cpp` 不依赖 causal_clock.h 以外的头文件

### 测试

- [ ] 3.1 新建 `tests/test_causal_clock.cpp` — 5 个 test case
  - 单调性: `t1 < t2 < t3` after 3x tick()
  - 线程安全: 10 threads × 1000 ticks → final = 10000
  - merge: `clock.merge(1000)` → `now() >= 1000`
  - happens_before: `happens_before(1, 2)` → true, `happens_before(2, 1)` → false
  - **Causal monotonicity soak**: 3 producer emit → consumer assert per-producer monotonic（Oracle 要求的新增）
- [ ] 3.2 验证：所有已有 test 使用 BusEvent 回调不受 causal_time 默认 0 影响

### 验收

- [ ] 4.1 `ctest -R test_causal_clock` 5/5 PASS
- [ ] 4.2 `ctest -R test_interaction_bus` 全绿
- [ ] 4.3 `ctest` 全量零回归