## Why

ADR-0019 IInteractionBus — 状态 **🟡 Partial**。P0 review (2026-07-06) 触发扩展需求：
- ADR-0046 PDK Plugin 间通信需要 `subscribe_topic(topic_pattern, callback)` — 而当前接口仅支持 session-based
- InMemoryBus 需要在内部实现中支持 topic-based dispatch

当前实现：`IInteractionBus` 只有 `subscribe_events(session_id, callback)`，缺少 `subscribe_topic()` 和 `unsubscribe()`

## What Changes

- `IInteractionBus` 新增 `subscribe_topic(topic_pattern, callback)` + `unsubscribe(token)` 方法
- `InMemoryBus` 实现 topic-based dispatch（复用现有 dispatch_thread）
- Glob 匹配支持：`"inference.lifecycle.*"` 匹配 `inference.lifecycle.idle/running/error`
- 新增 `tests/test_interaction_bus_topic.cpp`（≥3 cases: 精确匹配/glob/退订）

## Capabilities

- `iinteractionbus-topic-subscribe`: IInteractionBus topic-based subscribe 扩展

## Impact

- `include/agenticdsl/contract/iinteraction_bus.h`：新增 2 纯虚方法
- `src/common/contract/inmemory_bus.cpp`：实现 topic dispatch
- `tests/test_interaction_bus_topic.cpp`：新增测试
- 影响范围：所有 IInteractionBus 实现类（当前仅 InMemoryBus）

## Non-Goals

- 不实现 Layer-based 订阅权限检查（后续 ADR-0046 扩展）
- 不实现 regex pattern（仅 glob）