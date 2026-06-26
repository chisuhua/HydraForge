# Proposal: ADR-0033 — Session Hierarchy (三层会话模型)

> **STATUS: PLACEHOLDER** ⚠️
> **本 change 详细 design/spec/tasks 待 Sprint 14 启动前填充**
> **触发条件**: 无硬依赖 (与 C3 关联更自然, 建议在 C3 完成后启动)
> **关联 ADR**: docs/adr/adr-0033-session-hierarchy.md (🟡 Partial, 仅前向声明)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C5

## Why

ADR-0033 Session Hierarchy 当前状态 🟡 Partial, 仅 `include/agenticdsl/types/session_fwd.h` 前向声明存在. docs/adr/adr-0033-session-hierarchy.md §决策 1 定义三层会话模型: UserSession / TaskSession / SubtaskSession.

Sprint 15 启动本 change 实施:
- UserSession: 用户级 (跨多次任务)
- TaskSession: 单次任务 (DSLEngine::run 一次)
- SubtaskSession: fork/join 分支 (IPER 子任务)

不解决此问题: (a) multi-turn 对话无法持久化; (b) fork/join 分支状态无法追溯; (c) IPER retry 复用会话不可行; (d) DSLEngine 当前 stateless `run(Context)` 无会话支持.

## What Changes (待 Sprint 14 收官后详细制定)

### 1. 三层会话模型实施 (Sprint 15 Day 1-5)

1. `class UserSession`:
   - `messages: vector<ToolResult>` (追加写, ADR-0023 集成)
   - `task_sessions: vector<TaskSession>` (历史)
   - `current_task_session: optional<TaskSession>`
   - `user_id: string`
   - `created_at: timestamp`

2. `class TaskSession`:
   - `user_session: weak_ptr<UserSession>` (反向引用)
   - `task_id: string`
   - `subtask_sessions: vector<SubtaskSession>`
   - `execution_context: DagExecutionContext`
   - `started_at` / `completed_at`

3. `class SubtaskSession`:
   - `task_session: weak_ptr<TaskSession>`
   - `subtask_id: string`
   - `branch_id: int` (fork 分支标识)
   - `isolated_context: Context` (隔离的 JSON)

### 2. DSLEngine 重构 (Sprint 15 Day 6-9)

1. `DSLEngine::run(session_id, ...)` 重载:
   - 替代当前 stateless `run(Context)`
   - 注入 UserSession, 创建新 TaskSession
2. `ExecutionSession` 重组:
   - DagExecutionContext + TaskSession 合并
3. Fork/Join 分支隔离:
   - TopoScheduler 自动创建 SubtaskSession
4. IPER retry 复用:
   - 失败重试在同一 TaskSession 内

## Capabilities (待详细制定)

### ADDED Requirements (placeholder)

- `session-hierarchy-three-layers`: UserSession/TaskSession/SubtaskSession MUST 完整实现
- `session-hierarchy-dslengine-overload`: DSLEngine MUST 提供 `run(session_id, ...)` 重载
- `session-hierarchy-fork-isolation`: Fork/Join 分支 MUST 自动创建/销毁 SubtaskSession
- `session-hierarchy-iper-retry`: IPER retry MUST 复用同一 TaskSession
- `session-hierarchy-messages-append`: UserSession.messages MUST 追加写保护 (ADR-0023 集成)

## Impact (待 Sprint 14 收官后评估)

**预期修改文件**:
- `include/agenticdsl/types/session.h` (新建, 替代 session_fwd.h)
- `src/core/types/session.cpp` (新建)
- `src/core/types/user_session.{h,cpp}` (新建)
- `src/core/types/task_session.{h,cpp}` (新建)
- `src/core/types/subtask_session.{h,cpp}` (新建)
- `src/core/engine.{h,cpp}` (DSLEngine::run 重载)
- `src/modules/scheduler/topo_scheduler.cpp` (Fork/Join SubtaskSession 创建)
- `src/modules/cognitive/cognitive_worker.cpp` (IPER retry 复用)
- `tests/test_session_hierarchy.cpp` (新建)
- `tests/test_user_session.cpp` (新建)
- `tests/test_task_session.cpp` (新建)
- `tests/test_subtask_session.cpp` (新建)

**API 兼容性**:
- **breaking change**: DSLEngine::run(Context) 弃用, 新增 run(session_id, ...) 优先
- 向后兼容: 旧 run(Context) 内部自动创建临时 UserSession, 保持现有测试通过
- 后续 Sprint 16+ 移除旧 run(Context)

## Non-goals (placeholder)

- **不重写** CognitiveWorker (Sprint 2 已 ship)
- **不实质化** ADR-0007 (上下文压缩) — 与本 change 并行, 不耦合
- **不修改** IPER 循环控制 (Phase 5 范围)

## Estimated Effort (placeholder)

**总计**: 1.5-2 周 (Sprint 15 主体)

**前置依赖**: 无硬依赖 (建议在 C3 完成后启动, session 持有 policy 更自然)
**后续依赖**: 无

## 详细制定 TODO (待 Sprint 14 启动前执行)

- [ ] 1. 评估: Session 持久化 (文件/内存) — 默认内存, 持久化推迟到 Phase 5
- [ ] 2. 决策: breaking change vs 向后兼容 (建议: 向后兼容, 旧 run(Context) 内部自动建 UserSession)
- [ ] 3. 写本 change proposal.md (What Changes 详细化)
- [ ] 4. 写 design.md (5 个 Decision: Session 生命周期 / DSLEngine 重载策略 / Fork 隔离 / IPER retry 复用 / messages 写保护)
- [ ] 5. 写 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 6. 写 specs/session-hierarchy/spec.md (5-8 ADDED Requirements)
- [ ] 7. 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行
- [ ] 8. `openspec validate 2026-06-26-adr-0033-session-hierarchy` exit 0
- [ ] 9. 更新 master plan C5 状态: ⚪ placeholder → 🟡 active
- [ ] 10. 启动 Sprint 15 实施
