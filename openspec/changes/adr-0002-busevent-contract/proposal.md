## Why

当前 IInteractionBus 事件模型存在两个问题：

1. **无统一事件信封**：`emit(event_type, ToolResult)` 和 `subscribe(event_type, callback(ToolResult))` 直接传递裸类型。后续要加 timestamp、priority、causal_time 字段时无处附加。
2. **ADR-0002 EventBus 有界队列设计** 需要统一的 BusEvent 结构作为队列元素，但当前没有此类型。

目标：定义 `BusEvent` 作为公开契约类型（`include/agenticdsl/contract/bus_event.h`），一次性将 `emit`/`subscribe` 迁移到 BusEvent。此次破坏性变更后，后续所有 EventBus 扩展（glob subscribe、causal clock）均为增量非破坏变更。

## What Changes

- 新建 `include/agenticdsl/contract/bus_event.h` — BusEvent 结构体：
  ```cpp
  struct BusEvent {
    std::string topic;
    ToolResult payload;             // ADR-0023 标准载荷
    std::chrono::steady_clock::time_point timestamp;
    uint64_t causal_time{0};       // 预留，Change C 填充
    Priority priority{Normal};     // 预留，Phase 2 使用
  };
  ```
- `IInteractionBus` 接口 BREAKING：
  - `emit(string, ToolResult)` → `emit(BusEvent)`
  - `subscribe(string, callback(ToolResult))` → `subscribe(string, callback(BusEvent))`
  - 保留 `emit(string, string)` 重载（内部构造 BusEvent）
  - `unsubscribe(size_t)` **不变**（已存在，非新增）
- 更新 **全部 4 个实现者**：
  - `InMemoryBus`（`include/agenticdsl/contract/inmemory_bus.h` + `.cpp`）
  - `MockInteractionBus`（`tests/test_tool_coordinator.cpp:54`）
  - `MockBus`（`tests/test_skill_interpreter.cpp:59`）
  - `MockBusForEscalation`（`tests/test_escalation_triggers.cpp:25`）
- `std::queue<pair<string,ToolResult>>` → `std::queue<BusEvent>`

## Impact

- `include/agenticdsl/contract/bus_event.h`：新增
- `include/agenticdsl/contract/iinteraction_bus.h`：BREAKING emit/subscribe
- `include/agenticdsl/contract/inmemory_bus.h` + `.cpp`：适配
- `tests/` 下 3 个 mock 文件：适配

## Non-Goals

- 不在此 change 实现 priority queue / backpressure（推迟至 ADR-0030 V2 真实需求出现时）
- 不在此 change 填充 causal_time（Change C 负责）
- glob pattern — Change B 负责