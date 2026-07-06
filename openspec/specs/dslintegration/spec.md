# dslintegration Specification

## Purpose
Phase 1 Sprint 1b ADR-0019 P2 — `DSLEngine` 提供 `set_interaction_bus(shared_ptr<IInteractionBus>)` 注入入口与 `subscribe(topic, callback)` 透传 + `NodeExecutor` 构造接受可选 `IInteractionBus*` (non-owning 引用) 在 `execute_dsl_node` / `execute_tool_call` 前后推送 `dsl.call.{started,completed}` / `tool.completed` / `execution.failed` (Abort/Retry) 事件;`bus = nullptr` 时所有推送静默 no-op,零回归。
## Requirements
### Requirement: DSLEngine IInteractionBus 注入 (REQ-BUS-001)

`DSLEngine` **MUST** 提供 `set_interaction_bus(std::shared_ptr<IInteractionBus>)` 方法用于注入 bus 实例。
`DSLEngine::get_interaction_bus()` **MUST** 返回当前注入的 bus (可能为 nullptr)。

#### Scenario: 注入 custom bus

- **WHEN** 调用 `engine.set_interaction_bus(std::make_shared<InMemoryBus>())`
- **THEN** `engine.get_interaction_bus()` 返回非 nullptr 的 shared_ptr 指向该 bus

#### Scenario: 默认 nullptr 路径

- **WHEN** DSLEngine 构造时未注入 bus
- **THEN** `engine.get_interaction_bus()` 返回 nullptr
- **AND** 现有 `run()` 行为零变化 (与 Sprint 1a 之前一致)

#### Scenario: 替换 bus 实例

- **WHEN** 第二次调用 `set_interaction_bus(other_bus)`
- **THEN** 旧 bus 的引用计数 -1 (自动析构若无其他引用)
- **AND** `get_interaction_bus()` 返回新 bus

### Requirement: DSLEngine 订阅透传 (REQ-BUS-002)

`DSLEngine::subscribe(topic, callback)` **MUST** 透传到 `IInteractionBus::subscribe()`, 返回 bus 的 token。

#### Scenario: 透传到 InMemoryBus

- **WHEN** 调用 `engine.subscribe("test.topic", cb)` 且 bus 已注入
- **THEN** 调用 `bus->subscribe("test.topic", cb)` 并返回其 token
- **AND** 当 `bus->emit("test.topic", payload)` 被调用时, callback 触发

#### Scenario: bus 为 nullptr 时

- **WHEN** 调用 `engine.subscribe(topic, cb)` 但 bus 未注入
- **THEN** 返回 0 (无效 token)
- **AND** 不抛异常 (静默 no-op)

### Requirement: NodeExecutor 集成 IInteractionBus (REQ-BUS-003)

`NodeExecutor` 构造函数 **MUST** 接受可选的 `IInteractionBus* bus = nullptr` 参数 (非 owning 引用)。

#### Scenario: 注入 bus 后推送 dsl.call.started 事件

- **WHEN** `execute_dsl_node` 被调用
- **THEN** 进入时 `bus_->emit("dsl.call.started", ToolResult::success({...}, {{"prompt", rendered}}))`

#### Scenario: 注入 bus 后推送 dsl.call.completed 事件

- **WHEN** `execute_dsl_node` 完成后
- **THEN** 退出时 `bus_->emit("dsl.call.completed", ToolResult::success({...}, {{"text", result_text}}))`

#### Scenario: 注入 bus 后推送 tool.completed 事件

- **WHEN** `execute_tool_call` 完成后
- **THEN** `bus_->emit("tool.completed", tool_result)` 其中 `tool_result` 包含 Sprint 1a 注入的 4 个 P2-P4 字段

#### Scenario: 默认 nullptr 路径 (零回归)

- **WHEN** NodeExecutor 构造时 bus = nullptr
- **THEN** execute_xxx 方法不调用 `bus_->emit` (Sprint 1a 行为零变化)

### Requirement: 错误事件推送 (REQ-BUS-004)

`NodeExecutor` **MUST** 在异常路径 (Retry/Abort/error_code 异常) 推送 `execution.failed` 事件, 在 throw 之前。

#### Scenario: Abort 错误码触发 execution.failed

- **WHEN** 工具返回 `{"ok": false, "error_code": "Abort"}`
- **THEN** `bus_->emit("execution.failed", ToolResult::error(ErrorCode::Abort, msg))` 被调用
- **AND** 随后抛出 `std::runtime_error` with `[ABORT]` 标记

#### Scenario: Retry 错误码触发 execution.failed

- **WHEN** 工具返回 `{"ok": false, "error_code": "Retry"}`
- **THEN** `bus_->emit("execution.failed", ToolResult::error(ErrorCode::Retry, msg))` 被调用
- **AND** 随后抛出 `std::runtime_error` with `[RETRY]` 标记

#### Scenario: Skip 错误码不推送事件

- **WHEN** 工具返回 `{"ok": false, "error_code": "Skip"}`
- **THEN** 不调用 `bus_->emit` (Skip 是软失败, 不破坏 graph)
- **AND** 不抛异常, 返回原 context

