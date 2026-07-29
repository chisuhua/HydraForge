## Context

当前 `InMemoryBus` 在 Sprint 12 (C2, ADR-0030 V2) 已升级为 MPMC 有界队列后端，但事件类型仍为 `pair<string, ToolResult>` 非正式形式。ADR-0002 要求统一的 `BusEvent` 信封契约。Oracle 评审 (2026-07-26) 确认将 emit/subscribe 一次性迁移到 `BusEvent` 为唯一 BREAKING 变更，后续所有 EventBus 扩展（glob subscribe、causal clock）均为增量非破坏。

现有状态：
- `IInteractionBus` 接口 (`include/agenticdsl/contract/iinteraction_bus.h`) 已有 `emit(const BusEvent&)` 纯虚方法 + `emit(const string&, const string&)` 向后兼容入口
- `InMemoryBus` 实现已使用 `queue<BusEvent>` 存储，dispatch 线程传递 `BusEvent`
- 3 个测试 Mock 已实现 `emit(const BusEvent&)` 重载
- `bus_event.h` 和 `causal_clock.h` 已存在
- `test_event_bus_soak.cpp` 已验证 10000 事件零丢失

## Goals / Non-Goals

**Goals:**
- 正式化 `BusEvent` 作为 EventBus 唯一事件信封契约
- 确保 `IInteractionBus` 接口 5 字段对齐 (topic, payload, timestamp, causal_time, priority)
- 确保所有 4 个实现者 (`InMemoryBus` + 3 Mock) 一致实现 `emit(const BusEvent&)`
- 保持向后兼容字符串发射入口 `emit(const string&, const string&)`
- 新增 glob subscribe 能力（`inference.*` 模式匹配）

**Non-Goals:**
- 不修改 `subscribe` 回调签名（保持 `void(const BusEvent&)`）
- 不修改 `DSLEngine::subscribe` 回调签名（保持 `void(const ToolResult&)` 内部包装）
- 不引入 lock-free 队列
- 不实现 causal clock 的 consume 端逻辑（字段已预留，消费在 Phase 2）

## Decisions

### Decision 1: BusEvent 5 字段设计
- **选择**: 5 字段 struct（topic, payload:ToolResult, timestamp, causal_time, priority）
- **理由**: 覆盖 Oracle 评审要求的全部场景；timestamp 使用 `steady_clock`（非 wall clock）避免时钟跳跃影响因果序；causal_time 和 priority 预留为 0/Normal 默认值，Phase 2 填充
- **替代方案**: 继承 `ToolResult` 扩展 — 拒绝，组合优于继承，emit 与 subscribe 调用方解耦更干净

### Decision 2: emit/subscribe 签名一致性
- **选择**: `emit(const BusEvent&)` 为主入口，`emit(const string&, const string&)` 为向后兼容包装
- **理由**: 一次性 BREAKING 比渐进式迁移更干净；字符串重载内部构造 `BusEvent{event_type, ToolResult::success({}, {{"content", content}}), now()}`，保持与旧 `pair<string, ToolResult>` 行为等价
- **替代方案**: 保留 `emit(string, ToolResult)` 重载 — 拒绝，此重载与 BusEvent 高度重叠，增加混淆

### Decision 3: 存储与分发
- **选择**: `InMemoryBus` 内部 `queue<BusEvent>` + dispatch 线程 + 精确/通配符双路径分发
- **理由**: Sprint 12 已验证 MPMC 队列模式，`queue<BusEvent>` 自然延续；glob 匹配使用 `glob_match()` 函数（生产级实现，支持 `*` 和 `?`），wildcard 订阅者 < 50 保证性能
- **替代方案**: `vector<BusEvent>` 环形缓冲区 — 拒绝，queue 语义更清晰，环形缓冲区在 dispatch 竞争下需要额外原子操作

### Decision 4: 测试 Mock 迁移策略
- **选择**: 3 个 Mock 各自实现 `emit(const BusEvent&)`，记录 topic 到 `vector<string>`（与旧实现行为一致）
- **理由**: Mock 最小化实现，不关心 payload/timestamp 细节；与旧 `emit(string, string)` 重载记录方式对齐
- **替代方案**: 抽出公共 Mock 基类 — 推迟，3 个 Mock 各只有 1-2 行实现差异，公共基类引入耦合

## Risks / Trade-offs

- `DSLEngine::subscribe` 回调签名保持 `std::function<void(const ToolResult&)>` — 内部包装 `BusEvent` 为 `ToolResult`，上层代码零感知
- glob subscribe 慢路径 O(w) — w = wildcard subscriber 数量 < 50，不影响性能 (soak 测试 10000 events 零丢失)
- 3 个 Mock 不完整实现 `emit(const BusEvent&)` — 仅记录 topic，不校验 payload/timestamp，测试中不依赖这些字段
- `steady_clock` timestamp 在序列化/反序列化场景不可用 — 当前无跨进程 EventBus 需求，后续可扩展 wall clock 字段