## Why

ADR-0002 EventBus 有界队列架构 — 状态 **❌ Not Implemented**。全系统搜索 `EventBus|DispatchMode` 为 0 hits，当前 `InMemoryBus` 仅为简化实现（mutex + queue），无 Taskflow/async_simple 依赖。

Phase 6a 不要求完整 EventBus，但需要最小化落地以保证后续可扩展性：
- 当前 InMemoryBus 的 `dispatch_thread` + MPMC queue 复用 EventBus 设计模式
- 需要从中提取可复用的 EventBus 核心（有界队列 + 优先级丢弃）

目标：提取 EventBus Core（`event_bus.h/cpp`）作为可替换的 MPMC 队列组件，供 InMemoryBus 内部使用。

## What Changes

- 新建 `src/common/contract/event_bus.h/cpp` — EventBus Core（有界队列 + 优先级 + 背压）
- InMemoryBus 内部 queue 替换为 EventBus Core（API 不变）
- 新增 `PriorityEvent` 类型（Critical/Normal/Low）+ `try_emit()` 背压语义
- 新增 `tests/test_event_bus_core.cpp`（≥5 cases: 入队/出队/优先级/背压/并发）

## Capabilities

- `eventbus-core`: 提取 EventBus 有界队列核心组件

## Impact

- `src/common/contract/event_bus.h/cpp`：新增
- `src/common/contract/inmemory_bus.cpp`：内部实现替换
- 不修改 IInteractionBus 公开接口

## Non-Goals

- 不实现完整 EventBus（DispatchMode/Taskflow/CoroSpawn 延迟至 Phase 2）
- 不实现 Per-Session 隔离（栈上分配，Phase 2）
- 不实现 Agent 间 EventBus（Phase 2）