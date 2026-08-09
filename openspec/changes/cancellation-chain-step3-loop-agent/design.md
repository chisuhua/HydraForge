## Context

Phase B 7 步 wiring 已完成 Step 1+2（Cancellation Registry + ChatSession state）。Step 3 修复审计列出的断开点 #3, #4, #6, #7：

| Break | File:Line | Issue |
|-------|-----------|-------|
| #3 | `pdk_entry.cpp:170-190` | loop_agent entry doesn't parse `cancellation_id` |
| #4 | `pdk_entry.cpp:229` | Provider bridge forces `std::stop_token{}` |
| #6 | `node_executor.cpp:349-477` | dispatch_to_tool + YieldNode no token param |
| #7 | `tool_coordinator.cpp:195-203` | execute() no token param |

## Goals / Non-Goals

**Goals:**
- loop_agent 入口解析 `cancellation_id` 并 resolve token
- Provider bridge 转发 token 至 `provider_.generate()`
- NodeExecutor dispatch_to_tool + YieldNode 转发 token
- ToolCoordinator 接受 token + short-circuit + audit log
- 3 unit tests 验证各 hop 正确性

**Non-Goals:**
- 不修改 3 loop APIs（→ Step 4）
- 不实现 Mock provider / E2E（→ Step 5）
- 不修改 main.cpp（单独 follow-up）

## Decisions

### Decision 1: loop_agent Cancellation Registry — file-static

**选择**: loop_agent 内部维护 file-static `CancellationRegistry` 实例

**理由**:
- **作用域隔离**: loop_agent 是独立 plugin，无需跨 plugin 共享 registry
- **测试友好**: file-static 易测试隔离
- **零依赖**: 不引入 ChatSession 内部 state

**替代方案**:
- **外部参数**: 复杂化 loop_agent 接口
- **共享 ChatSession registry**: plugin boundary 复杂化

### Decision 2: token 参数传递策略 — default `{}` for backward compat

**选择**: `NodeExecutor::dispatch_to_tool` + `ToolCoordinator::execute` 追加 trailing `std::stop_token token = {}` 默认参数

**理由**:
- **零调用方破坏**: 默认参数保持向后兼容
- **明确意图**: 调用方显式提供 token
- **C++20 标准**: `std::stop_token` 默认构造是 no-op

### Decision 3: ToolCoordinator Short-circuit — audit emit

**选择**: Token 已 cancelled 时，`execute()` 立即返回 + emit `tool.audit.denied` 事件 (reason="cancelled")

**理由**:
- **可观察性**: 审计日志记录 cancellation 决策
- **一致性**: 与 ADR-0031 §决策 8 audit 模式对齐
- **可测试**: 测试可断言 audit event

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| NodeExecutor 调用方未更新 token 参数 | 默认 `{}` 保持向后兼容，编译错误强制更新 |
| ToolCoordinator short-circuit 引入新 audit event 路径 | 测试覆盖 audit emit |
| loop_agent file-static registry 在多线程下竞争 | std::mutex 保护（继承 Step 1+2 设计） |

## Migration Plan

### BREAKING 处理

2 处签名变更追加默认参数（向后兼容）：
- `dispatch_to_tool(name, path, args)` → `(name, path, args, token = {})`
- `execute(meta, ctx, args)` → `(meta, ctx, args, token = {})`

### 回滚

3 处修改可单独 revert。Step 1+2 不受影响。

## Open Questions

无 — Step 1+2 提供稳定集成基础，Step 3 是直接的 wiring 任务。