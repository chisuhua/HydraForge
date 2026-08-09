## Context

Phase B Step 3 已 ship loop_agent entry + NodeExecutor + ToolCoordinator 的 stop_token wiring。Step 4 修复审计断开点 #5：

| Break | File:Line | Fix |
|-------|-----------|-----|
| #5a | `react_loop.h:80` | ReactLoop::run 添加 token 参数 |
| #5b | `plan_execute_loop.h:198-256` | PlanExecuteLoop 添加 token + 替换内部 2 处 `{}` |
| #5c | `fork_join_loop.h:138-257` | ForkJoinLoop 添加 token + CV wait predicate |

## Goals / Non-Goals

**Goals:**
- 3 个 loop APIs 接受 `std::stop_token` 参数（default `{}` 保持向后兼容）
- PlanExecuteLoop 内部 2 处 `std::stop_token{}` 替换为 token
- ForkJoinLoop CV wait predicate 增加 `token.stop_requested()` 谓词
- 取消时调用 `pool_->stop()`
- 更新所有调用方

**Non-Goals:**
- 不实现 Mock provider / E2E（→ Step 5）
- 不修改其他 loop APIs
- 不修改 main.cpp

## Decisions

### Decision 1: 参数追加策略 — trailing + default

**选择**: 3 个 `run()` 方法追加 trailing `std::stop_token token = {}` 参数

**理由**:
- **零破坏**: default 参数保持向后兼容
- **明确意图**: 调用方显式提供 token
- **编译器辅助**: 类型不匹配时编译失败

### Decision 2: ForkJoinLoop 取消传播 — pool stop

**选择**: ForkJoinLoop CV wait 检测到 `token.stop_requested()` 时调用 `pool_->stop()` 终止所有 worker

**理由**:
- **彻底清理**: 终止 worker 线程避免悬挂
- **与 DomainWorkerPool stop API 对齐**: 项目已有 `pool_->stop()` (Sprint 3 ship)

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| 3 BREAKING API 调用方遗漏 | grep all callers + 编译器交叉验证 |
| PlanExecuteLoop 内部替换行号漂移 | 实际文件读取确认 |
| ForkJoinPool 取消后 worker 泄漏 | 复用现有 `pool_->stop()` RAII 路径 |

## Migration Plan

### BREAKING 处理

3 处签名变更追加默认参数（向后兼容）：
- `ReactLoop::run(prompt, ctx)` → `(prompt, ctx, token = {})`
- `PlanExecuteLoop::run(prompt, ctx, branches)` → `(..., token = {})`
- `ForkJoinLoop::run(branches, ctx)` → `(branches, ctx, token = {})`

### 回滚

3 处签名 revert 即可。

## Open Questions

无 — Step 1+2+3 提供稳定基础，Step 4 是直接的参数添加 + 内部替换。