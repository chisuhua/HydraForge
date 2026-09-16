# PDK Loop Agent Plugin (`pdk/loop_agent/`)

> **STATUS**: Active (Sprint 32 ship + 2026-09-16 fix-loop-run-return-contract 升级契约)

## 概述

`loop_agent` 是 HydraForge DSL 执行引擎的 Loop Agent 插件。它实现了 `loop/run` 顶层工具 + `loop/set_parent_provider` 配置工具，供 ChatSession 通过 ToolRegistry 调用。

关联 ADR:
- ADR-0067 (Layered Plugin Architecture)
- ADR-0023 (ToolResult Standard)
- ADR-0068 (Event Emission Contract)

## 注册工具

### `loop/set_parent_provider`

设置父引擎 LLM provider  引用（thread_local 存储）。

**输入**:
```json
{ "provider_ptr": "<uintptr_t as string>" }
```

**输出（成功）**:
```json
{ "success": true, "ok": true, "error_code": null }
```

**输出（失败）**:
```json
{ "success": false, "ok": false, "error_code": "InvalidParams", "error": "..." }
```

### `loop/run`

加载 `lib/loop/<loop_type>.agent.md` → 创建 DSLEngine 子实例 → 注入 ProviderLLMTool → 执行。

**输入**:
```json
{
  "loop_type": "react | plan_execute | fork",
  "prompt": "用户提示",
  "system_prompt": "系统提示",
  "history": "[JSON history]",
  "tools": "[tool names]",
  "max_steps": "50",
  "bus_ptr": "<uintptr_t>",
  "session_id": "session id",
  "cancellation_id": "cancellation id (optional)"
}
```

**输出契约**（per fix-loop-run-return-contract, 2026-09-16）:

| 路径 | 触发条件 | `ok` | `success` | `error_code` | `response` | `steps` |
|------|---------|:---:|:---:|----------|----------|:---:|
| #1 | Invalid loop_type | `false` | `false` | `"InvalidParams"` | — | — |
| #2 | Mock fallback (parent provider 未设) | `true` | `true` | `null` | `"[loop_agent/...]"` | `1` |
| #3 | cancellation_token stop_requested | `false` | `false` | `"Cancelled"` | `""` | `0` |
| #4 | DSL 执行成功 (result.success=true) | `true` | `true` | `null` | `<LLM 输出>` | `1` |
| #4' | DSL 执行失败 (result.success=false) | `false` | `false` | `"Unknown"` | `<fallback>` | `1` |
| #5 | catch 异常 (DSL parse / LLM provider / 其他) | `false` | `false` | `"Unknown"` | `""` | `0` |

**字段定义**:
- `ok` (新): 工具执行成功标志，对齐 `ToolResult.ok` (ADR-0023)
- `success` (旧): 保持向后兼容，与 `ok` 同值
- `error_code` (新): 对齐 `ToolResult::ErrorCode` 枚举字符串值
- `response`: LLM 响应文本或 fallback
- `steps` / `tokens_used` / `cost_usd`: 步数/token/成本（硬编码 steps=1, tokens=0, cost=0）
- `error`: 错误描述（仅错误路径）

**ErrorCode 值域**（4 个 remap 值，对齐 `ToolResult::ErrorCode` 枚举）:

| 值 | 映射到枚举 | 触发 |
|----|----------|------|
| `"InvalidParams"` | `ErrorCode::InvalidParams` (line 57, ADR-0073 D3) | Invalid loop_type |
| `"Cancelled"` | `ErrorCode::Cancelled` (line 62) | cancellation_token fires |
| `"ToolNotRegistered"` | `ErrorCode::ToolNotRegistered` (line 34) | Tool not found in DSL execution |
| `"Unknown"` | `ErrorCode::Unknown` (line 28) | DSL parse error, LLM provider 异常, catch-all |

## 事件发射 (per ADR-0068)

成功路径: `loop.turn.start` → `loop.decision` → `loop.turn.end`
失败路径 (registry call_tool 抛异常): 无事件（由 ChatSession 捕获后 emit `loop.error`）

## 测试

```bash
ctest -R test_loop_agent_plugin --output-on-failure
```

5 个 TEST_CASE（fix-loop-run-return-contract 后）：
- `loop/set_parent_provider callable`
- `loop/run mock fallback when no provider` (路径 #2)
- `loop/run rejects invalid loop_type` (路径 #1)
- `loop/run file-not-found error path covered by catch block` (路径 #5)
- `all loop agent DSL files exist and are loadable`

## 关联文件

- `src/pdk_entry.cpp` - 工具注册 + `loop/run` 实现 (330 行)
- `../chat_session/include/agenticdsl/pdk/chat_session.h` - ChatResult / ChatSession
- `../chat_session/src/chat_session.cpp` - 消费方（三层 fallback）
- `lib/loop/*.agent.md` - DSL 模板