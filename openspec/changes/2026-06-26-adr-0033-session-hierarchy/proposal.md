# Proposal: ADR-0033 — Session Hierarchy (三层会话模型)

> **STATUS: ACTIVE** 🟢 — 设计完成，待实施
> **Oracle 审查**: ses_0e28703caffeBUB7tgDqsClZiw (2026-07-01)
> **Metis 审查**: ses_0e26217edffeHrMpGUPLuhTecP (2026-07-01)
> **预估工时**: 4.5 人天
> **关联 ADR**: docs/adr/adr-0033-session-hierarchy.md (🟡 Partial → 实施后 ✅ Approved)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C5
> **前置依赖**: 无

## Why

ADR-0033 Session Hierarchy 当前状态 🟡 Partial，仅 `include/agenticdsl/types/session_fwd.h` 前向声明存在。
当前代码库无会话层级支持，导致以下问题：

1. **无 multi-turn 状态**：多次 `DSLEngine::run()` 调用彼此独立，无法追溯历史
2. **分支状态丢失**：fork/join 的 `execute_single_branch()` 返回临时 `Context`，无法追溯分支历史
3. **IPER retry 无会话**：失败重试无法判断"同一任务已失败 n 次"
4. **执行策略无上下文**：`IExecutionPolicy` 挂在 DSLEngine 级别，无法 per-task 切换

## 设计修订（基于 Oracle + Metis 审查）

与原 ADR-0033 相比，本 change 采纳以下修订：

| 修订项 | 原设计 | 采纳方案 | 原因 |
|--------|--------|---------|------|
| R1 | ExecutionSession → DagExecutionContext 重命名 | **保留 ExecutionSession** | Sprint 19 PIMPL-lite 刚交付，零功能收益 |
| R2 | BudgetController 新增 cost_limit API | **不新增** | 现有 CostTracker + record_llm_call() 已满足 |
| R3 | TaskSession 持 unique_ptr<IExecutionPolicy> | **持 shared_ptr** | DSLEngine 已用 shared_ptr，避免所有权分裂 |
| R4 | TopoScheduler 返回 BranchExecutionResult | **DSLEngine 层包装** | TopoScheduler 不耦合会话模型 |
| R5 | 容器用 vector | **改用 deque** | vector 扩容导致引用/指针失效（Metis F1/F2） |
| R6 | failure_count 未定义触发条件 | **可重试错误才递增** | 非可重试错误不触发 session 分裂 |
| R7 | 无 LayeredContext 重载 | **提供桥接重载** | 与 Sprint 20 API 风格一致 |
| R8 | 新 run 只接受 Context | **Context + LayeredContext 双重载** | 渐进迁移 |

## What Changes

### 1. 三层会话类型 (`src/core/types/session.h` + `.cpp`)

- `UserSession`：顶层，对应一次对话周期
  - `messages: vector<ToolResult>`（追加写，ADR-0023）
  - `task_sessions: deque<TaskSession>`（**deque** 保地址稳定）
  - `current_task_session: TaskSession*`（指向 deque 元素）
  - `user_id: string`, `created_at: timestamp`

- `TaskSession`：单次 `DSLEngine::run()` 执行
  - `user_session: UserSession&`（反向引用）
  - `subtask_sessions: deque<SubtaskSession>`（**deque** 保地址稳定）
  - `current_policy: shared_ptr<IExecutionPolicy>`（持有策略）
  - `failure_count: u32`
  - `status: active | completed | failed`
  - `context: Context`（当前执行上下文）

- `SubtaskSession`：fork/join 最小执行单元
  - `initial_context / final_context: Context`（快照）
  - `execution_trace: vector<TraceRecord>`
  - `status: pending | running | completed | failed`

### 2. DSLEngine 重载 (`engine.h` + `.cpp`)

新增两个重载：
```cpp
ExecutionResult run(UserSession& user_sess, const std::string& message,
                    const Context& initial_ctx = Context{});

ExecutionResult run(UserSession& user_sess, const std::string& message,
                    const LayeredContext& initial_lctx);  // 桥接到 Context 版本
```

`message` 写入 `ctx["user_input"]`。复用 TaskSession 时 `initial_ctx` 替换现有 context。
现有 `run(Context)` 和 `run(LayeredContext)` 不受影响。

### 3. TopoScheduler 不动（Design D5）

`execute_single_branch()` 签名完全不变。SubtaskSession 创建/归档全在 `DSLEngine::run_impl()` 中。TopoScheduler 不增加 Session 相关 include。

### 4. 失败重试策略

- 可重试错误（Network/Timeout/Retryable）递增 `failure_count`
- <3 次失败：`KeepSession`，复用当前 TaskSession
- ≥3 次失败：`NewSession`，自动创建新 TaskSession
- 非可重试错误（PermissionDenied/InvalidInput）不递增 failure_count

## Capabilities

### ADDED Requirements

| ID | 描述 |
|----|------|
| `session-three-layers` | UserSession/TaskSession/SubtaskSession 完整实现 |
| `session-dslengine-overload` | DSLEngine::run(UserSession&, ...) 重载 |
| `session-fork-isolation` | Fork/Join 分支自动创建/归档 SubtaskSession |
| `session-iper-retry` | IPER retry 复用 TaskSession，3 次失败自动分裂 |
| `session-messages-append` | UserSession.messages 追加写保护 (ADR-0023) |

## Impact

### 修改文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/core/types/session.h` | 新建 | 三层会话类型定义 |
| `src/core/types/session.cpp` | 新建 | 方法实现 |
| `src/core/engine.h` | 修改 | 新增 2 个 run() 重载 (Context + LayeredContext) |
| `src/core/engine.cpp` | 修改 | 实现 run_impl、SubtaskSession 包装、to_tool_result helper |
| `CMakeLists.txt` (根) | 修改 | 添加 session.cpp 到 agenticdsl_core |
| `tests/test_session.cpp` | 新建 | 5 个 TEST_CASE（单元） |
| `tests/test_dslengine_session.cpp` | 新建 | 2 个 TEST_CASE（集成） |

### API 兼容性

- **非 breaking**：现有 `run(Context)` 和 `run(LayeredContext)` 保持可用
- `run(UserSession&, ...)` 是新入口，optional
- 不标记 deprecated

## Non-goals

- **不重命名** ExecutionSession
- **不修改** BudgetController 接口
- **不处理** 持久化（Phase 5 范围）
- **不处理** 上下文压缩（ADR-0007，独立）
- **不修改** TopoScheduler 签名
- **不修改** CognitiveWorker 内部循环

## Estimated Effort

| Phase | 内容 | 人天 |
|-------|------|:----:|
| Phase 1 | Session 类型定义 (.h + .cpp + CMake) | 1.5 |
| Phase 2 | DSLEngine 会话集成 (含 LayeredContext 桥接) | 1.5 |
| Phase 3 | TopoScheduler 适配（签名不改，仅验证） | 0.5 |
| Phase 4 | 测试 + ASan/TSan + ADR 状态更新 | 1.0 |
| **总计** | | **4.5 人天** |