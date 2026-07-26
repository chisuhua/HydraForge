## Why

ADR-0037 因果排序 — 状态 **🔍 Proposed (2026-06-26)**。解决跨 CognitiveWorker 和 DomainWorkerPool 的事件顺序问题。

当前问题：
- IInteractionBus 异步事件不保证到达顺序
- 跨 Worker 无法通过时间戳判断 `happens-before` 关系
- 日志中跨 Worker 事件交织，难以重建执行序列

决策：采用单进程逻辑时钟 + 因果向量（非 Lamport 分布式时间戳）

## What Changes

- 新增 `src/common/contract/causal_clock.h/cpp` — CausalClock 单增逻辑时钟
- IInteractionBus emit 时自动附加 causal_clock 值
- BusEvent 新增 `causal_time` 字段
- 新增 `tests/test_causal_clock.cpp`（≥4 cases: 单调性/线程安全/序列化/happens-before 判定）

## Capabilities

- `causal-clock`: 单进程逻辑时钟 + 因果向量基础设施

## Impact

- `src/common/contract/causal_clock.h/cpp`：新增
- `src/common/contract/inmemory_bus.cpp`：emit 时 tick + attach
- `include/agenticdsl/contract/bus_event.h`：新增 causal_time 字段
- `tests/test_causal_clock.cpp`：新增

## Non-Goals

- 不实现全 Lamport 分布式时间戳（仅单进程）
- 不修改 IInteractionBus 公开接口（causal_time 为内部字段）