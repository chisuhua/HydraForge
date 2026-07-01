# session-hierarchy Specification

> **STATUS: ACTIVE** 🟢 — Ready for implementation
> **关联 ADR**: docs/adr/adr-0033-session-hierarchy.md (🟡 Partial → 实施后 ✅ Approved)
> **Oracle 审查**: ses_0e28703caffeBUB7tgDqsClZiw (2026-07-01)
> **Metis 审查**: ses_0e26217edffeHrMpGUPLuhTecP (2026-07-01)

## ADDED Requirements

### Requirement: session-three-layers

`UserSession` / `TaskSession` / `SubtaskSession` MUST 完整实现。

UserSession 和 TaskSession 为 class（方法封装），SubtaskSession 为 struct（POD-like 值类型）。

容器使用 `std::deque` 确保地址稳定性（非 `std::vector`）。

#### Scenario: 三层创建与引用

- **GIVEN** 一个 UserSession 实例 `user_sess`
- **WHEN** 调用 `user_sess.create_task_session()`
- **THEN** 返回 TaskSession 引用，其 `user_session()` 指向 `user_sess`
- **AND** `user_sess.current_task_session()` 指向该 TaskSession
- **AND** 可调用 `task_sess.create_subtask("branch-0", ctx)` 创建 SubtaskSession
- **AND** 归档后 `task_sess.subtask_sessions()` 包含该 SubtaskSession
- **AND** 多次 `create_task_session()` 调用后 `current_task_session_` 和 `task_sessions_` 内部元素的引用/指针仍有效（地址稳定性）

#### Scenario: UserSession messages 追加写保护

- **GIVEN** 一个 UserSession 实例
- **WHEN** 调用 `user_sess.append_message(tool_result)`
- **THEN** messages 增加 1 条
- **AND** 通过 `user_sess.messages()` 获取的 const 引用不可修改
- **AND** 编译期禁止通过 const 引用调用非 const 方法

### Requirement: session-dslengine-overload

`DSLEngine` MUST 提供两个新重载：

```cpp
// Context 版本（主入口）
ExecutionResult run(UserSession& user_sess, const std::string& message,
                    const Context& initial_ctx = Context{});

// LayeredContext 版本（桥接，委托到 Context 版本）
ExecutionResult run(UserSession& user_sess, const std::string& message,
                    const LayeredContext& initial_lctx);
```

`message` 写入 `ctx["user_input"]`，与现有 DSL 模板引用 `{{user_input}}` 一致。

#### Scenario: 首次调用创建 TaskSession

- **GIVEN** 一个 UserSession 实例，当前无活跃 TaskSession
- **WHEN** 调用 `engine.run(user_sess, "hello")`
- **THEN** 自动创建新 TaskSession
- **AND** `user_sess.current_task_session()` 不为空
- **AND** `user_sess.messages()` 包含本次执行结果
- **AND** `ctx["user_input"] == "hello"`

#### Scenario: 多次调用复用 TaskSession

- **GIVEN** 同一 UserSession，第一次 `engine.run()` 已完成
- **WHEN** 第二次调用 `engine.run(user_sess, "follow up", new_ctx)`
- **THEN** 复用已有的 TaskSession（不新建）
- **AND** `user_sess.task_sessions().size() == 1`（仅 1 个历史）
- **AND** `task_sess.context()` 被 `new_ctx` **替换**（顶层字段覆盖）

### Requirement: session-fork-isolation

Fork/Join 分支 MUST 自动创建/归档 `SubtaskSession`，隔离分支状态。

#### Scenario: Fork 分支归档

- **GIVEN** 包含 Fork/Join 节点的 DSL 工作流，TaskSession 已创建
- **WHEN** TopoScheduler 执行到 Fork 节点
- **THEN** `DSLEngine::run_impl` 为每个分支调用 `task_sess.create_subtask()`
- **AND** 分支完成后自动归档到 `task_sess.subtask_sessions()`
- **AND** 归档的 SubtaskSession 包含 `initial_context` 和 `final_context` 快照

### Requirement: session-iper-retry

失败重试 MUST 复用同一 `TaskSession`。累计 3 次可重试失败后自动创建新 TaskSession。

`failure_count_` 仅在以下条件全部满足时递增：
1. `ExecutionResult.success == false`
2. `error_code` 属于可重试类别（Retryable / Network / Timeout / RateLimited）

#### Scenario: 3 次失败自动分裂

- **GIVEN** 一个 TaskSession，`failure_count() == 2`
- **WHEN** 第 3 次执行失败且 `record_failure()` 触发递增
- **THEN** `failure_count() == 3`
- **AND** `determine_failure_mode()` 返回 `FailureMode::NewSession`
- **AND** `DSLEngine::run()` 自动创建新 TaskSession
- **AND** 旧 TaskSession 保留在 `user_sess.task_sessions()` 中（历史归档）

#### Scenario: retry 复用

- **GIVEN** 一个 TaskSession，`failure_count() == 1`
- **WHEN** 再次执行失败且 `record_failure()` 触发递增
- **THEN** `failure_count() == 2`
- **AND** `determine_failure_mode()` 返回 `FailureMode::KeepSession`
- **AND** `DSLEngine::run()` 继续使用当前 TaskSession

#### Scenario: 非可重试错误不递增

- **GIVEN** 一个 TaskSession，`failure_count() == 0`
- **WHEN** 执行失败返回 `PermissionDenied`
- **THEN** `record_failure()` 不递增 `failure_count()`
- **AND** `determine_failure_mode()` 返回 `KeepSession`
- **AND** 不触发 session 分裂

### Requirement: session-messages-append

`UserSession.messages` MUST 追加写保护（集成 ADR-0023 ToolResult 信封）。

#### Scenario: 不可修改历史

- **GIVEN** 已追加 3 条消息的 UserSession
- **WHEN** 通过 `messages()` 获取 const 引用
- **THEN** 编译期禁止修改已有消息
- **AND** 运行时不可通过 const 引用清空或修改 vector

## VERIFICATION

### Build & Test

```bash
cmake --preset tests && make -j$(nproc)   # 编译通过
ctest --output-on-failure                 # ≥ 59/59 PASS
cmake --preset asan && ctest               # 0 memory error
cmake --preset tsan && ctest               # 0 data race
```

### Ship Gate

- [ ] `openspec validate 2026-06-26-adr-0033-session-hierarchy` exit 0
- [ ] ctest ≥ 59/59 (52 baseline + 5 test_session + 2 test_dslengine_session)
- [ ] ASan 100% clean
- [ ] TSan 100% clean
- [ ] ADR-0033 状态更新为 ✅ Approved
- [ ] master plan C5 状态更新
- [ ] docs/roadmap-status.md §一 Phase 3 → 100%