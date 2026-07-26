## Why

ADR-0037 因果排序 — 状态 **🔍 Proposed (2026-06-26)**。解决跨 CognitiveWorker 和 DomainWorkerPool 的事件顺序问题。

Change A 完成后，BusEvent 已预留 `causal_time` 字段（默认 0）。本 change 实现 CausalClock 并在 InMemoryBus::emit() 时自动填充。纯增量变更：不影响已有代码，不修改接口。

当前单进程场景下，原子计数器（`memory_order_relaxed`）已足够。若将来出现跨进程/分布式需求，可升级为 Lamport 时间戳。

## What Changes

- 新建 `src/common/contract/causal_clock.h/cpp` — CausalClock 单增逻辑时钟
- `InMemoryBus::emit()` 调用 `clock_.tick()` → 填充 `BusEvent.causal_time`
- BusEvent 已有 `causal_time` 字段（Change A 定义），默认 0
- 新增 `tests/test_causal_clock.cpp`（≥4 cases + **causal monotonicity soak**）

## Capabilities

- `causal-clock`: 单进程逻辑时钟 + happens-before 判定

## Impact

- `src/common/contract/causal_clock.h/cpp`：新增
- `src/common/contract/inmemory_bus.cpp`：emit 时 tick + attach
- `tests/test_causal_clock.cpp`：新增
- 不影响任何现有接口或测试

## Non-Goals

- 不实现 Lamport 分布式时间戳（仅单进程）
- 不修改 BusEvent 结构（字段已在 Change A 预留）
- `happens_before` 判定为工具函数，不强加于所有消费者