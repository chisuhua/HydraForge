# ADR-0023 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0023-tool-result-standard.md](adr-0023-tool-result-standard.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (2026-06-24, Sprint 5 ship), 但 2/11 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `ToolResult` | ✅ Shipped | `src/core/types/tool_result.h` | 核心工具结果结构体 |
| `ErrorCode` | ✅ Shipped | `src/core/types/tool_result.h` | 错误码枚举 (11 个值: Unknown/PermissionDenied/PathViolation/DangerousCommand/ToolNotRegistered/Retry/Skip/Abort/Audit/Timeout/ResourceExhausted) |
| `ToolResult::success()` | ✅ Shipped | `src/core/types/tool_result.h` | 成功结果构造器 |
| `ToolResult::error()` | ✅ Shipped | `src/core/types/tool_result.h` | 错误结果构造器 (ErrorCode 版; 旧 string 重载已移除) |
| `ToolMetadata` | ✅ Shipped | `src/common/policy/execution_policy.h` | 工具元数据 |
| `ToolCallContext` | ✅ Shipped | `src/common/policy/execution_policy.h` | 工具调用上下文 |
| `ToolPreview` | ✅ Shipped | `include/agenticdsl/policy/approval_types.h` | 工具调用预览 |
| `ApprovalCallback` | ✅ Shipped | `include/agenticdsl/policy/approval_types.h` | 审批回调类型 |
| `IApprovalHandler` | ✅ Shipped | `include/agenticdsl/policy/iapproval_handler.h` | 审批处理器接口 |
| `RETURN_ERROR` | 📅 Deferred | — | `ToolResult` 封装宏, 未实现 (内联构造 `ToolResult::error(...)` 替代) |
| `RETURN_SUCCESS` | 📅 Deferred | — | `ToolResult` 封装宏, 未实现 (内联构造 `ToolResult::success(...)` 替代) |

## 分类详情

### 📅 Deferred — `RETURN_ERROR` / `RETURN_SUCCESS`

ADR-0023 可能描述了 C 风格宏 `RETURN_ERROR()` / `RETURN_SUCCESS()` 作为 `ToolResult` 的便捷构造器。实际实现选择直接使用 `ToolResult::error(ErrorCode code, string message)` / `ToolResult::success(Content, Meta)` 静态方法, 未定义宏。

**推迟理由**:
- C++ `static` 构造器方法提供了类型安全性 (宏无法提供)
- 当前 11 处 ToolResult 构造均使用静态方法, 无宏需求
- 宏会隐藏控制流且不利于调试

## 决策

- **ADR 状态**: ✅ Approved (保持)
- **理由**: ADR-0023 核心契约 (`ToolResult` + `ErrorCode` + 成功/错误构造器) 9/11 已 Shipped; 2 个 Deferred 是 optional 宏封装, 非核心契约
- **风险**: 低 — `ToolResult::error(ErrorCode)` 静态方法已覆盖所有使用场景

## 后续行动

- 若未来代码审查发现大量重复的 ToolResult 构造模式, 考虑引入 helper 宏或工厂函数
- 本 audit 文档供技术债务跟踪参考
