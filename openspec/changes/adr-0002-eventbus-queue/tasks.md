## 实现 (5h)

### EventBus Core

- [ ] 1.1 新建 `src/common/contract/event_bus.h` — EventBusQueue 声明 + BusEvent/Priority 类型
- [ ] 1.2 新建 `src/common/contract/event_bus.cpp` — 实现
  - `try_emit()`: 优先级感知入队 + 背压
  - `try_consume()`: 阻塞读取 + 超时
  - `size()` / `dropped()`: 统计
- [ ] 1.3 新建 `tests/test_event_bus_core.cpp` — 5 个 test case
  - 正常入队出队 (FIFO)
  - 优先级排序 (Critical 先于 Normal 先于 Low)
  - 背压: 满时丢弃 Low + 保留 Critical
  - 并发: 2 producer + 1 consumer (1000 events)
  - 空队列 consume 超时返回 nullopt

### InMemoryBus 替换

- [ ] 2.1 `inmemory_bus.cpp` 内部 `std::queue` → `EventBusQueue`
  - `emit()` 调用 `queue_.try_emit()`
  - `dispatch_thread` 调用 `queue_.try_consume()`
- [ ] 2.2 CMakeLists: `src/common/contract/CMakeLists.txt` 新增 `event_bus.cpp`
- [ ] 2.3 验证: `ctest -R test_interaction_bus` 全绿（零回归）

### 验收

- [ ] 3.1 `ctest -R test_event_bus_core` 5/5 PASS
- [ ] 3.2 `ctest -R test_interaction_bus` 全绿
- [ ] 3.3 `ctest` 全量零回归