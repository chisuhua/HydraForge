## ADDED Requirements

### Requirement: BusEvent 统一事件信封
EventBus 系统 SHALL 使用 `BusEvent` 结构体作为事件发射和订阅的唯一信封类型，包含 5 字段：topic (string)、payload (ToolResult)、timestamp (steady_clock_time_point)、causal_time (uint64_t, 默认 0)、priority (EventPriority, 默认 Normal)。

#### Scenario: BusEvent 默认构造
- **WHEN** 构造一个默认的 `BusEvent` 实例
- **THEN** topic 为空字符串，causal_time 为 0，priority 为 EventPriority::Normal

#### Scenario: BusEvent 全字段构造
- **WHEN** 使用 5 字段初始化 `BusEvent`（topic, payload, timestamp, causal_time, priority）
- **THEN** 所有字段值与构造参数一致

#### Scenario: InMemoryBus 发射 BusEvent 并订阅
- **WHEN** 调用 `bus.emit(BusEvent{...})` 且存在对应 topic 的 subscriber
- **THEN** subscriber 回调收到与发射一致的 BusEvent 副本

### Requirement: IInteractionBus emit 签名一致性
IInteractionBus SHALL 提供 `emit(const BusEvent&)` 纯虚方法作为主发射入口，同时保持 `emit(const string&, const string&)` 向后兼容重载（内部包装为 ToolResult + BusEvent）。

#### Scenario: emit(BusEvent) 主入口
- **WHEN** 调用 `bus.emit(BusEvent{...})`
- **THEN** BusEvent 进入队列，dispatch 线程按 topic 分发

#### Scenario: emit(string, string) 向后兼容
- **WHEN** 调用 `bus.emit("topic", "content")`
- **THEN** 内部构造 `BusEvent{"topic", ToolResult::success({}, {{"content", "content"}}), now()}` 并发射，subscriber 收到 BusEvent 且 payload.meta["content"] 等于 "content"

### Requirement: 全局通配符订阅 (glob subscribe)
InMemoryBus SHALL 支持通配符模式匹配订阅，使用 `*`（匹配任意字符序列）和 `?`（匹配单个字符）。订阅者数量 < 50 时性能不降级。

#### Scenario: 精确匹配 (无通配符)
- **WHEN** subscriber 订阅 `"inference.lifecycle.idle"` 且 emit `"inference.lifecycle.idle"`
- **THEN** subscriber 被触发
- **WHEN** emit `"inference.lifecycle.running"`
- **THEN** subscriber 不被触发

#### Scenario: 单通配符 *
- **WHEN** subscriber 订阅 `"inference.*"` 且 emit `"inference.lifecycle.idle"`、`"inference.lifecycle.running"`、`"inference.lifecycle.error"`
- **THEN** subscriber 被触发 3 次

#### Scenario: 多通配符 *.error.*
- **WHEN** subscriber 订阅 `"*.error.*"` 且 emit `"inference.error.oom"` 和 `"temporal.error.timeout"`
- **THEN** subscriber 被触发 2 次
- **WHEN** emit `"inference.timeout.oom"`（无 "error" 段）
- **THEN** subscriber 不被触发

#### Scenario: 无匹配
- **WHEN** subscriber 订阅 `"other.*"` 且 emit `"inference.lifecycle.idle"`
- **THEN** subscriber 不被触发

#### Scenario: 通配符 unsubscribe
- **WHEN** subscriber 订阅 `"inference.*"`，收到事件后 unsubscribe，再次 emit `"inference.lifecycle.running"`
- **THEN** subscriber 不再被触发

### Requirement: 高并发零丢失
InMemoryBus SHALL 支持 10000 事件并发发射（4 线程 × 2500 次），所有事件被 subscriber 接收，零丢失。

#### Scenario: 10000 事件 soak test
- **WHEN** 4 个线程各发射 2500 事件到 `"soak"` topic，subscriber 原子计数
- **THEN** subscriber 最终计数为 10000（所有事件收到）

### Requirement: 测试 Mock 一致实现
所有实现 IInteractionBus 的测试 Mock 类 SHALL 提供 `emit(const BusEvent&)` 重载，保持与生产代码一致的最小实现。

#### Scenario: MockBusForEscalation 记录 BusEvent topic
- **WHEN** 调用 `MockBusForEscalation::emit(BusEvent{"topic.x", ...})`
- **THEN** `events_` 向量包含 `"topic.x"`

#### Scenario: MockBus 记录 BusEvent
- **WHEN** 调用 `MockBus::emit(BusEvent{...})`
- **THEN** 不抛出异常，subscribe 返回 0

#### Scenario: MockInteractionBus 记录 BusEvent topic
- **WHEN** 调用 `MockInteractionBus::emit(BusEvent{"topic.y", ...})`
- **THEN** `emit_log_` 向量包含 `"topic.y"`