## Why

ADR-0019 P0 review (2026-07-06) 触发扩展需求：ADR-0046 PDK Plugin 间通信需要 topic-based event subscription。当前 `subscribe(event_type, callback)` 仅支持精确事件类型匹配。

目标：扩展现有 `subscribe()` 让其接受 glob pattern（`*`、`?`），无通配符的字符串退化为精确匹配（零开销）。不新增单独的 `subscribe_topic` API，保持单一接口。

> **Oracle 评审修正**（2026-07-26）：`unsubscribe(size_t)` 已存在于 IInteractionBus（`iinteraction_bus.h:73`），无需在此 change 新增。

## What Changes

- `IInteractionBus::subscribe()` 接受 glob pattern — 与精确 event_type 同一接口
- InMemoryBus 分发逻辑：O(1) 精确匹配 map 先行，仅对含通配符 pattern 遍历 glob match
- 新建 `tests/test_interaction_bus_glob.cpp`（≥5 cases，含 race test）
- `callback` 签名保持 `void(const BusEvent&)` — 与 Change A 一致，零破坏

## Capabilities

- `subscribe-glob`: 扩展 subscribe 支持 glob pattern 匹配

## Impact

- `include/agenticdsl/contract/iinteraction_bus.h`：subscribe 注释更新（接口签名不变）
- `include/agenticdsl/contract/inmemory_bus.h` + `.cpp`：dispatch_loop 新增 glob 分支
- `tests/test_interaction_bus_glob.cpp`：新增
- `tests/` 下 3 个 mock 文件：更新 `subscribe()` 参数注释

## Non-Goals

- 不新增 `subscribe_topic` API（合并入 subscribe）
- 不实现 regex pattern（仅 glob）
- 不实现 Layer-based 订阅权限检查