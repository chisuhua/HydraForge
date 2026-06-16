# Spec: ToolResult 标准化 P1-P4

> **关联**: [proposal.md](../proposal.md) | [design.md](../design.md)

## ADDED Requirements

### Requirement: ToolResult.error_code (REQ-TR-001, P2)

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

### Requirement: ToolResult.latency_ms (REQ-TR-002, P3)

`ToolResult` **MUST** 支持 `latency_ms` 字段，类型为 `std::optional<uint64_t>`。

#### Scenario: 工具执行自动记录延迟

- **WHEN** `NodeExecutor` 调用 `ToolRegistry::call_tool()`
- **THEN** 调用结束后 `ToolResult.latency_ms` 自动填充为实际耗时

### Requirement: ToolResult.trace_id (REQ-TR-003, P3)

`ToolResult` **MUST** 支持 `trace_id` 字段，类型为 `std::optional<std::string>`。

#### Scenario: trace_id 透传

- **WHEN** 调用方传入 `trace_id` 参数
- **THEN** `ToolResult.trace_id` 等于传入值
- **AND** `IInteractionBus::emit` 推送的 Event 也保留 `trace_id`

### Requirement: ToolResult.metadata (REQ-TR-004, P3)

`ToolResult` **MUST** 支持 `metadata` 字段，类型为 `std::optional<nlohmann::json>`。

#### Scenario: metadata 与 meta 共存

- **WHEN** 调用方同时设置 `meta`（MVP）和 `metadata`（P3）
- **THEN** 两个字段独立保留
- **AND** `meta` 保留 P1 MVP 语义，`metadata` 保留 P3 扩展语义

### Requirement: IInteractionBus 结构化推送 (REQ-TR-005)

`IInteractionBus::emit` **MUST** 提供 ToolResult 推送主路径 + std::string 兼容重载。

> **设计决策**（Sprint 1a 实施期调整）：原 spec 草案 `std::variant<std::string, ToolResult>` 因以下
> 理由被替换为 **emit 重载方案**：
> 1. `IInteractionBus` 现有实现已采用 ToolResult-only 接口（ADR-0019），零代码调用 string
>    载荷，引入 `std::variant` 仅为满足"兼容旧 string"的语义意图而非真实迁移需求。
> 2. `std::variant` 会迫使所有订阅者实现访问者（visitor），增加订阅端样板代码。
> 3. `std::string` 内容可通过 `ToolResult::success({}, {{"content", s}})` 自然包装为信封，
>    保留在 `meta["content"]` 供消费者提取（`from_json` 反序列化亦可见）。
>
> 最终方案：`IInteractionBus::emit(event_type, std::string)` 重载内部包装为 ToolResult 后
> 转发到主 emit 路径。订阅者接口保持 `void(const ToolResult&)` 不变。

#### Scenario: ToolResult 推送保留结构

- **WHEN** 调用方 `bus.emit("tool_completed", tool_result)`
- **THEN** 订阅者收到的 callback 参数是 `const ToolResult&`
- **AND** `result.error_code` / `result.latency_ms` / `result.trace_id` / `result.metadata` 均可访问

#### Scenario: 向后兼容 string 推送

- **WHEN** 调用方 `bus.emit("legacy_topic", std::string("..."))`
- **THEN** 订阅者 callback 收到 `ToolResult`，其中 `meta["content"] == "..."`
- **AND** `payload.ok == true` (success 信封)

## ADDED Requirements

### Requirement: NodeExecutor 结构化解析 (REQ-TR-MOD-001)

`NodeExecutor::execute_tool_call` **MUST** 不再使用 `if(result.is_object())` 启发式判断成功/失败。

**原** (`src/modules/executor/node_executor.cpp:execute_tool_call`):
```cpp
if (result.is_object()) { /* success */ }
```

**新**:
```cpp
// 信封模式：解析 ok + 4 个 P2-P4 字段
// 旧式裸 JSON 模式：包装为 success 信封保留 P0 行为
ToolResult tool_result;
if (raw_result.is_object() && raw_result.contains("ok")
    && raw_result["ok"].is_boolean()) {
  tool_result = ToolResult::from_json(raw_result);
} else {
  tool_result = ToolResult::success(raw_result);
}
// 自动注入 latency_ms / trace_id
// error_code 分发 Retry/Abort/Skip
```

#### Scenario: 工具结果结构化解析

- **WHEN** NodeExecutor 调用 `ToolRegistry::call_tool()` 返回原始 JSON
- **THEN** 调用 `ToolResult::from_json()` 解析信封或包装裸 JSON 为 success
- **AND** 不再使用 `is_object()` 等启发式判断成功/失败

## REMOVED Requirements

无。

## Cross-References

- [ADR-0023 §3.1 - 信封格式](../../docs/adr/0023-toolresult-standardization.md)
- [ADR-0019 §3.2 - 事件 payload](../../docs/adr/0019-iinteraction-bus-mvp.md)
- [ToolResult MVP X 阶段实现](../../src/core/types/tool_result.h)