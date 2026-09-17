# PDK Loop Agent Plugin (`pdk/loop_agent/`)

> **STATUS**: Active (Sprint 32 ship + 2026-09-16 fix-loop-run-return-contract 升级契约 + C1 loop-agent-tools)

## 概述

`loop_agent` 是 HydraForge DSL 执行引擎的 Loop Agent 插件。它实现了 6 个 loop-specific 工具供 ChatSession 通过 ToolRegistry 调用，以及 thread_local provider/capture-mode 基础设施。

关联 ADR:
- ADR-0067 (Layered Plugin Architecture)
- ADR-0023 (ToolResult Standard)
- ADR-0068 (Event Emission Contract)

## 注册工具

### `loop/set_parent_provider`（配置）

设置父引擎 LLM provider 引用（thread_local 存储）。

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

### `loop/run`（顶层循环执行器）

加载 `lib/loop/<loop_type>.agent.md` → 创建 DSLEngine 子实例 → 注入 ProviderLLMTool → 同步执行。

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

**输出契约**:

| 路径 | 触发条件 | `ok` | `success` | `error_code` | `response` | `steps` |
|------|---------|:---:|:---:|----------|----------|:---:|
| #1 | Invalid loop_type | `false` | `false` | `"InvalidParams"` | — | — |
| #2 | Mock fallback (parent provider 未设) | `true` | `true` | `null` | `"[loop_agent/...]"` | `1` |
| #3 | cancellation_token stop_requested | `false` | `false` | `"Cancelled"` | `""` | `0` |
| #4 | DSL 执行成功 | `true` | `true` | `null` | `<LLM 输出>` | `1` |
| #4' | DSL 执行失败 | `false` | `false` | `"Unknown"` | `<fallback>` | `1` |
| #5 | catch 异常 | `false` | `false` | `"Unknown"` | `""` | `0` |

### `loop/decide_react`（ReAct 决策解析器，C1）

三级 fallback 将 LLM 文本解析为 ReAct 决策（action 或 final）。

**输入**:
```json
{ "response": "<LLM 响应文本>" }
```

**输出**:

| 路径 | `final` | `action_tool` | `action_args` | `response` |
|------|:-------:|:-----------:|:------------:|:---------:|
| L1 JSON function_call | `false` | `"fs/read"` | `{"path": "a.txt"}` | 原文 |
| L2 XML `<tool>/<args>` | `false` | `"fs/read"` | `{"path": "a.txt"}` | 原文 |
| L3 Final Answer | `true` | `""` | `null` | Final Answer 后文本 |
| L3 自然语言 fallback | `true` | `""` | `null` | 原文（非错误） |

**错误**: 缺 `response` 参数 → `error_code: "InvalidParams"`

### `loop/execute_plan`（子图执行器，C1）

提取 ```yaml AgenticDSL fenced block → 子 DSLEngine 同步执行。

**输入**:
```json
{
  "plan": "完整 markdown（含 AgenticDSL fenced block）",
  "context": "{可选 JSON, merge_patch RFC 7396}"
}
```

**输出**: `{ok, success, error_code, results(json), total_steps}`

**错误**:
- 非 DSL plan → `InvalidParams`
- 未设 parent provider → `Unknown`
- DSL 解析/执行异常 → `Unknown`

### `loop/process_task`（单分支执行器，C1）

单线程单次 LLM 调用（经 ProviderLLMTool 桥接 tls_parent_provider）。

**输入**:
```json
{ "task_id": "a", "input": "summarize X", "tools": "(可选)" }
```

**输出**: `{ok, success, error_code, branch_id(=task_id), result}`

**错误**:
- 缺 `task_id` / `input` → `InvalidParams`
- 未设 parent provider → `Unknown`

### `loop/set_capture_mode`（Data-RSI 采集开关，C1）

thread_local atomic 开关（0=None, 1=Training）。真实采集属 C2/ADR-0080。

**输入**:
```json
{ "mode": "None" | "Training" }
```

**输出**: `{ok, success, error_code}`

**错误**: 非法 mode → `InvalidParams`

## 双循环架构（C1 决策矩阵）

`lib/loop/*.agent.md` DSL 循环与 `include/agenticdsl/pdk/agent_loops/*` C++ 循环是**两套并行实现**：

| 层 | 拥有者 | 节奏 | 同步语义 |
|---|--------|------|---------|
| Chat-loop | ChatSession::chat() | user-turn 边界、队列、取消注册、持久化 | 每 user turn 一次 |
| Agent-loop | `lib/loop/<loop_type>.agent.md` | turn 内 think-act 推理 | lock-step（ChatResult 同 turn 返回） |
| 原语层 | C++ `agent_loops/*` (Sprint 4/20) | 性能敏感、编译期绑定场景 | 由调用方决定 |

**规则**:
- `loop/run` 及全部 4 个 C1 工具一律 **lock-step 同步**
- fire-and-forget 长任务不属于 loop 工具语义（走 `pdk/temporal_agent`）
- **统一重构触发条件**: 任一循环实现出现第 3 个消费者（当前: DSL 层消费者 = ChatSession；C++ 层消费者 = tests）

## bus_ptr 边界规则（D6）

- `bus_ptr` 字符串裸指针透传**仅保留**于 `loop/run`（legacy, C0 ship），`chat_session.cpp:495` 处有审计注释
- C1 新 3 工具（`loop/decide_react` / `loop/execute_plan` / `loop/process_task`）**不接收** bus_ptr，**不发射事件**
- 事件发射由 `loop/run` wrapper 层统一负责（ADR-0068 附录 A 语义单点）

## ErrorCode 值域

| 值 | 映射到枚举 | 触发 |
|----|----------|------|
| `"InvalidParams"` | `ErrorCode::InvalidParams` | Invalid loop_type / 缺参数 / 非 DSL plan / 非法 mode |
| `"Cancelled"` | `ErrorCode::Cancelled` | cancellation_token fires |
| `"ToolNotRegistered"` | `ErrorCode::ToolNotRegistered` | Tool not found in DSL execution |
| `"Unknown"` | `ErrorCode::Unknown` | DSL parse error, LLM provider 异常, catch-all |

## 事件发射 (per ADR-0068)

成功路径: `loop.turn.start` → `loop.decision` → `loop.turn.end`
失败路径 (registry call_tool 抛异常): 无事件（由 ChatSession 捕获后 emit `loop.error`）

## 测试

```bash
ctest -R test_loop_agent_plugin --output-on-failure
```

22 个 TEST_CASE（C1 loop-agent-tools 后）:
- `LoopAgent loads and registers tools` (加载验证)
- `loop/set_parent_provider callable` (配置)
- `loop/run mock fallback when no provider` (#2)
- `loop/run rejects invalid loop_type` (#1)
- `loop/run file-not-found error path` (#5)
- `all loop agent DSL files exist and are loadable`
- `loop/decide_react: OpenAI function_call JSON (L1)` (7 cases)
- `loop/execute_plan: valid AgenticDSL plan` (4 cases)
- `loop/process_task: normal call with MockProvider` (2 cases)
- `loop/set_capture_mode: valid modes` (2 cases)
- `E2E: PluginLoader + MockProvider + loop/run` (自动化)

## 关联文件

- `src/pdk_entry.cpp` - 工具注册 + 6 工具实现 (~580 行)
- `../chat_session/include/agenticdsl/pdk/chat_session.h` - ChatResult / ChatSession
- `../chat_session/src/chat_session.cpp` - 消费方（三层 fallback）
- `lib/loop/*.agent.md` - DSL 模板