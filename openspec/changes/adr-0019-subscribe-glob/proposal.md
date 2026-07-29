## Why

Oracle 评审 (2026-07-26) 确定不新增 `subscribe_topic()` API，改为扩展 `subscribe()` 接受 glob pattern。`InMemoryBus` Change B 已实现双路径分发（精确匹配 O(1) + 通配符线性扫描），但缺少正式 OpenSpec 文档化和验收规格。本 Change 完成 Change B 的文档化、架构合规性审计和测试覆盖验证。

## What Changes

- **文档化**: `subscribe()` 参数从 `event_type` 重命名为 `pattern`，语义从精确匹配扩展为 glob pattern
- **文档化**: `InMemoryBus` 双路径分发架构（`exact_subscribers_` + `wildcard_subscribers_`）
- **文档化**: `glob_match()` 算法（支持 `*` 任意序列 + `?` 单字符）
- **验证**: `test_interaction_bus_glob.cpp` 6 个测试用例完成审计，确保场景覆盖完整
- **BREAKING**: 无 — `subscribe(const std::string&, callback)` 签名不变，仅语义扩展

## Capabilities

### New Capabilities
- `event-bus-glob`: 通配符订阅能力 — `subscribe("tool.*")` 匹配 `tool.call`、`tool.result` 等模式

### Modified Capabilities
- （无 — 首次正式定义 EventBus glob 功能）

## Impact

- **IInteractionBus** (`include/agenticdsl/contract/iinteraction_bus.h`): 参数 `event_type` → `pattern`（文档语义变更，签名不变）
- **InMemoryBus** (`include/agenticdsl/contract/inmemory_bus.h` + `src/common/contract/inmemory_bus.cpp`): 双路径分发已实现，本 Change 仅文档化
- **测试**: `test_interaction_bus_glob.cpp` 6 个测试用例（精确匹配 / 单通配符 / 多通配符 / 无匹配 / unsubscribe / 并发竞争）
- **依赖**: `change:adr-0002-busevent-contract` — `BusEvent.topic` 是 glob 匹配的目标字段

## Non-goals

- 不修改 `subscribe()` 回调签名（保持 `void(const BusEvent&)`）
- 不支持正则表达式（仅 glob：`*` 和 `?`）
- 不支持优先级路由（priority 字段预留，Phase 2 使用）
- 不引入 lock-free 订阅者列表（现有 mutex 满足性能需求）
- 不修改 `DSLEngine::subscribe` 回调签名（保持 `void(const ToolResult&)` 内部包装）