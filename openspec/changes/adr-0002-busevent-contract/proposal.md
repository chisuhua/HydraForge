## Why

Oracle 评审 (2026-07-26) 要求将 EventBus 从 `pair<string, ToolResult>` 非正式模式一次性收敛到 `BusEvent` 公开契约类型。当前 `InMemoryBus` 及其消费者（3 个测试 Mock）缺少统一事件信封，导致类型安全不足、扩展困难（causal clock、priority、glob subscribe 需额外字段）。ADR-0002 已定义 BusEvent 需求，ADR-0019 已定义 IInteractionBus 契约，本 Change 完成最后一公里落地。

## What Changes

- **BREAKING**: `IInteractionBus::emit(const BusEvent&)` 新增为纯虚接口 — 所有实现者必须提供此重载
- **BREAKING**: `InMemoryBus` 存储从 `pair<string, ToolResult>` 升级为 `queue<BusEvent>` — 5 字段信封 (topic, payload, timestamp, causal_time, priority)
- **BREAKING**: 3 个测试 Mock 类 (`MockBusForEscalation`, `MockBus`, `MockInteractionBus`) 新增 `emit(const BusEvent&)` 重载
- 新增 `BusEvent` 公开 struct 定义（`include/agenticdsl/contract/bus_event.h`）
- 新增 `event::CausalClock` 时钟工具（`include/agenticdsl/contract/causal_clock.h`）
- 新增 `emit(const string&, const string&)` 向后兼容入口（内部包装为 ToolResult+BusEvent）
- 新增全局 glob subscribe 能力（`inference.*` 模式匹配，`InMemoryBus` 双路径分发）
- 新增 `test_event_bus_soak.cpp` 10000 事件 soak test 验证零丢失

## Capabilities

### New Capabilities
- `eventbus-contract`: BusEvent 统一事件信封定义 + IInteractionBus emit/subscribe 签名一致性

### Modified Capabilities
- （无修改 — 这是首次正式定义 EventBus 契约）

## Impact

- **IInteractionBus** (`include/agenticdsl/contract/iinteraction_bus.h`): 新增 `emit(const BusEvent&)` 纯虚方法
- **InMemoryBus** (`include/agenticdsl/contract/inmemory_bus.h` + `src/common/contract/inmemory_bus.cpp`): 存储从二元组升级为 `queue<BusEvent>`，dispatch 线程传递 BusEvent
- **3 测试 Mock**: `test_escalation_triggers.cpp::MockBusForEscalation`, `test_skill_interpreter.cpp::MockBus`, `test_tool_coordinator.cpp::MockInteractionBus` — 新增 `emit(const BusEvent&)` 实现
- **下游消费者**: `src/common/tools/tool_coordinator.cpp` 已使用 `BusEvent{...}` 构造，无需修改；`src/modules/skill_interpreter/skill_interpreter.cpp` 使用 `bus_->emit(topic, payload)` 字符串重载，无需修改
- **ADR-0019** (IInteractionBus 契约): emit 签名对齐 BusEvent
- **ADR-0037** (因果序): causal_time 字段预留，后续增量实现

## Non-goals

- 不修改外部 subscriber 回调签名（保持 `void(const BusEvent&)` 不变）
- 不引入 lock-free 队列（现有 mutex+queue 满足性能需求）
- 不修改 `IInteractionBus::subscribe` 签名（保持 `std::function<void(const BusEvent&)>`）
- 不修改 `DSLEngine::subscribe` 回调签名（保持 `std::function<void(const ToolResult&)>` — 内部包装）