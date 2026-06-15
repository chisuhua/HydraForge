# Design: ToolResult 标准化 P1-P4

> **关联**: [proposal.md](proposal.md)
> **ADR**: [ADR-0023](../../docs/adr/adr-0023-tool-result-standard.md)

## 架构合规性检查

| 约束 | 状态 | 备注 |
|------|------|------|
| 中文注释优先 | ✅ | 所有新增注释中文 |
| 文件头 4 项 | ✅ | proposal/design/specs/tasks 均有作者+日期 |
| C++20 + CMake 3.20+ | ✅ | 沿用现有 |
| 2 空格缩进 | ✅ | 沿用现有 |
| nlohmann_json 使用 | ✅ | `metadata` 字段使用 `nlohmann::json` |
| Anti-pattern 避免 | ✅ | 不删失败测试，提交前 ctest |

## 关键设计决策

### 决策 1: ToolResult 字段扩展（向后兼容）

**问题**: P1 MVP 已有 `ok/data/meta` 字段。P2-P4 需扩展 `error_code/latency_ms/trace_id/metadata`，但 PDK 用户可能已经依赖 MVP 字段。

**方案**: 保留所有 MVP 字段，新增字段为 **optional**。MVP 字段：
```cpp
struct ToolResult {
  bool ok = false;
  nlohmann::json data;
  nlohmann::json meta;  // 已有
  // P2-P4 新增:
  std::optional<ErrorCode> error_code;  // P2
  std::optional<uint64_t> latency_ms;    // P3
  std::optional<std::string> trace_id;   // P3
  std::optional<nlohmann::json> metadata;  // P3 (避免与 MVP meta 冲突)
};
```

**理由**: `optional` 字段保证向后兼容，PDK 迁移时渐进式添加。

### 决策 2: NodeExecutor 启发式分支替换

**问题**: `node_executor.cpp:execute_tool_call()` 用 `if(result.is_object())` 启发式判断成功/失败。

**方案**: 用 `try/catch` 显式调用 `ToolResult` 工厂方法：
```cpp
// 旧
if (result.is_object()) { /* success */ } else { /* fail */ }

// 新
auto tool_result = ToolResult::from_json(raw_result);  // 解析为 ToolResult
if (tool_result.ok) { /* success path */ }
else { /* fail path with structured error_code */ }
```

**理由**: 显式类型化分发，避免字符串/对象混合判断。

### 决策 3: IInteractionBus 推送结构化

**问题**: ADR-0019 §3.2 推送 `Event { std::string content }`，结构化数据丢失。

**方案**: 扩展 Event payload 支持 `ToolResult`：
```cpp
struct Event {
  std::string topic;
  std::variant<std::string, ToolResult> payload;  // 兼容旧 string
  std::chrono::system_clock::time_point timestamp;
};
```

**理由**: `std::variant` 提供类型安全，旧代码继续工作。

## 测试设计

### 单元测试 (test_tool_result.cpp)

1. `ToolResult P2 ErrorCode 分类` (RETRY/SKIP/ABORT/AUDIT)
2. `ToolResult P3 latency_ms 计算`
3. `ToolResult P3 trace_id 透传`
4. `ToolResult P3 metadata 与 meta 共存`
5. `ToolResult from_json 工厂方法`

### 集成测试 (test_executor_with_mock_provider.cpp)

1. `Mock 工具返回 RETRY error_code → NodeExecutor 重试`
2. `Mock 工具返回 ABORT error_code → NodeExecutor 终止`
3. `Mock 工具携带 trace_id → IInteractionBus 推送保留`

## 实施计划

参考 `.omo/plans/archive/2026-06-15-expired-plans/p0-p1-p2-phase0-cleanup.md` 的 Sprint 1 切分。

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| ToolResult 字段扩展破坏 MVP 24 测试 | 中 | 回归 | optional 字段，零侵入 |
| NodeExecutor 重构引入 bug | 中 | 集成测试失败 | 逐 case 替换，回归测试覆盖 |
| variant payload 不被 InMemoryBus 序列化 | 低 | Event 丢失 | 提供 `to_string()` 兜底 |

## 引用

- [ADR-0023](../../docs/adr/adr-0023-tool-result-standard.md) — ToolResult 标准化
- [ADR-0019](../../docs/adr/adr-0019-iinteraction-bus-mvp.md) — IInteractionBus
- [.omo/decisions/phase1-entry.md](../../.omo/decisions/phase1-entry.md) — 入口决策
- [docs/phase1-roadmap.md](../../docs/phase1-roadmap.md) — Phase 1 实施路线
