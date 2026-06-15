# Spec: ToolResult 标准化 P1-P4

> **关联**: [proposal.md](proposal.md) | [design.md](design.md)

## ADDED Requirements

### REQ-TR-001: ToolResult.error_code (P2)

`ToolResult` **MUST** 支持 `error_code` 字段，类型为 `enum class ErrorCode`：

```cpp
enum class ErrorCode {
  Unknown = 0,
  // P1 已有
  PermissionDenied,
  PathViolation,
  DangerousCommand,
  ToolNotRegistered,
  // P2 新增
  Retry,        // 建议重试
  Skip,         // 建议跳过此节点
  Abort,        // 终止整个流程
  Audit,        // 需要审计
  Timeout,      // 工具执行超时
  ResourceExhausted,  // 资源耗尽
};
```

#### Scenario: RETRY 错误码识别

- **WHEN** 工具返回 `{"ok": false, "error_code": "Retry", "data": {...}}`
- **THEN** `ToolResult::error_code` == `ErrorCode::Retry`
- **AND** `CognitiveWorker` 看到 Retry 时**SHOULD** 重试该工具调用

#### Scenario: ABORT 错误码识别

- **WHEN** 工具返回 `{"ok": false, "error_code": "Abort", "data": {...}}`
- **THEN** `ToolResult::error_code` == `ErrorCode::Abort`
- **AND** `NodeExecutor` 看到 Abort 时**MUST** 抛出异常终止整个 Graph

### REQ-TR-002: ToolResult.latency_ms (P3)

`ToolResult` **MUST** 支持 `latency_ms` 字段，类型为 `std::optional<uint64_t>`。

#### Scenario: 工具执行自动记录延迟

- **WHEN** `NodeExecutor` 调用 `ToolRegistry::call_tool()`
- **THEN** 调用结束后 `ToolResult.latency_ms` 自动填充为实际耗时

### REQ-TR-003: ToolResult.trace_id (P3)

`ToolResult` **MUST** 支持 `trace_id` 字段，类型为 `std::optional<std::string>`。

#### Scenario: trace_id 透传

- **WHEN** 调用方传入 `trace_id` 参数
- **THEN** `ToolResult.trace_id` 等于传入值
- **AND** `IInteractionBus::push` 推送的 Event 也保留 `trace_id`

### REQ-TR-004: ToolResult.metadata (P3)

`ToolResult` **MUST** 支持 `metadata` 字段，类型为 `std::optional<nlohmann::json>`。

#### Scenario: metadata 与 meta 共存

- **WHEN** 调用方同时设置 `meta`（MVP）和 `metadata`（P3）
- **THEN** 两个字段独立保留
- **AND** `meta` 保留 P1 MVP 语义，`metadata` 保留 P3 扩展语义

### REQ-TR-005: IInteractionBus 结构化推送

`IInteractionBus::push` **MUST** 支持 `ToolResult` payload，类型为 `std::variant<std::string, ToolResult>`。

#### Scenario: ToolResult 推送保留结构

- **WHEN** 调用方 `bus->push({"tool_completed", ToolResult{...}})`
- **THEN** 订阅者收到的 Event.payload 持有 ToolResult
- **AND** 调用 `std::get<ToolResult>(event.payload)` 可获取结构化结果

#### Scenario: 向后兼容 string 推送

- **WHEN** 调用方 `bus->push({"legacy_topic", std::string("...")})`
- **THEN** 订阅者仍可通过 `std::get<std::string>` 获取

## MODIFIED Requirements

### REQ-TR-MOD-001: NodeExecutor 替换启发式分支

**原** (`src/modules/executor/node_executor.cpp:execute_tool_call`):
```cpp
if (result.is_object()) { /* success */ }
```

**新**:
```cpp
auto tool_result = ToolResult::from_json(raw_result);
if (tool_result.ok) { /* success path */ }
else { /* use tool_result.error_code for dispatch */ }
```

#### Scenario: 工具结果结构化解析

- **WHEN** NodeExecutor 调用 `ToolRegistry::call_tool()` 返回原始 JSON
- **THEN** 调用 `ToolResult::from_json()` 解析
- **AND** 不再使用 `is_object()` 等启发式判断

## REMOVED Requirements

无。

## Cross-References

- [ADR-0023 §3.1 - 信封格式](../../docs/adr/adr-0023-tool-result-standard.md)
- [ADR-0019 §3.2 - 事件 payload](../../docs/adr/adr-0019-iinteraction-bus-mvp.md)
- [ToolResult MVP X 阶段实现](../../src/core/types/tool_result.h)
