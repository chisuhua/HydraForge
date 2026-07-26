## Change A: BusEvent 公开契约 + emit/subscribe 迁移 (~1d)

### BusEvent 类型定义

- [ ] 1.1 新建 `include/agenticdsl/contract/bus_event.h` — BusEvent + EventPriority 枚举
- [ ] 1.2 验证：`#include "bus_event.h"` 可被独立编译

### IInteractionBus 接口 (BREAKING)

- [ ] 2.1 `include/agenticdsl/contract/iinteraction_bus.h`:
  - `emit(string, ToolResult)` → `emit(const BusEvent&)` override 声明
  - `subscribe(string, function<void(ToolResult)>)` → `subscribe(string, function<void(const BusEvent&)>)`
  - `emit(string, string)` 重载不变（兼容旧调用）
  - `unsubscribe(size_t)` 不变（已存在）
- [ ] 2.2 验证：4 个实现者编译失败（尚未迁移）

### InMemoryBus 适配

- [ ] 3.1 `inmemory_bus.h`:
  - `std::queue<pair<string,ToolResult>>` → `std::queue<BusEvent>`
  - 新增 `emit(const BusEvent&)` 声明
- [ ] 3.2 `inmemory_bus.cpp`:
  - 实现 `emit(const BusEvent&)` — 直接入队
  - `emit(string, string)` 内部包装 `ToolResult::success(...)` → `BusEvent`
  - `emit(string, ToolResult)` 改为调用 `emit(BusEvent{...})`
  - `dispatch_loop()`: 从 `pair` → `BusEvent` 解构
  - 回调调用: `cb(event.second)` → `cb(event)`
- [ ] 3.3 `try_pop` 签名: `(string&, ToolResult&)` → `(BusEvent&)` 或移除（取决于是否有消费者）

### 3 个测试 Mock 适配

- [ ] 4.1 `tests/test_tool_coordinator.cpp:54` — MockInteractionBus: 更新 emit/subscribe 签名
- [ ] 4.2 `tests/test_skill_interpreter.cpp:59` — MockBus: 更新 emit/subscribe 签名
- [ ] 4.3 `tests/test_escalation_triggers.cpp:25` — MockBusForEscalation: 更新 emit/subscribe 签名

### 测试

- [ ] 5.1 `test_inmemory_bus` — 适配新 BusEvent 回调签名（已有 test 的逻辑不变）
- [ ] 5.2 `test_interaction_bus` — 适配新签名
- [ ] 5.3 **新增 soak test**: `test_event_bus_soak` — dispatch_thread + 10000 events 入队/出队验证无丢事件
- [ ] 5.4 `ctest` 全量零回归

### 验收

- [ ] 6.1 `cmake --preset tests && make -j$(nproc)` 编译通过
- [ ] 6.2 `ctest` 全量零回归
- [ ] 6.3 `nm -C libagenticdsl_core.a | grep "emit.*BusEvent"` 确认 ABI