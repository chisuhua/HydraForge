# Tasks: ADR-0033 — Session Hierarchy (三层会话模型)

> **STATUS: ACTIVE** 🟢 — 设计完成，待实施
> **预估工时**: 4.5 人天（含内存安全加固 + LayeredContext 重载）
> **Oracle 审查**: ses_0e28703caffeBUB7tgDqsClZiw (2026-07-01)
> **Metis 审查**: ses_0e26217edffeHrMpGUPLuhTecP (2026-07-01)
> **设计决策**: design.md（10 项决策）
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C5

---

## Phase 1: Session 类型定义 (1.5 天)

### 1.1 创建 session.h — 类型定义

- [ ] 1.1.1 新建 `src/core/types/session.h` — SubtaskSession struct
  - `branch_path: string`, `initial_context: Context`, `final_context: Context`
  - `execution_trace: vector<TraceRecord>`, `status: string` (pending/running/completed/failed)
  - `started_at`, `completed_at: optional<steady_clock::time_point>`
  - 静态工厂 `SubtaskSession::make(path, initial_ctx)` 便捷构造

- [ ] 1.1.2 session.h — TaskSession class
  - `user_session_: UserSession&`（引用，绑定 UserSession 本身）
  - `subtask_sessions_: std::deque<SubtaskSession>`（**非 vector**，保证地址稳定性）
  - `current_policy_: shared_ptr<IExecutionPolicy>`（与 DSLEngine 共享）
  - `failure_count_: u32`, `status_: string` (active/completed/failed)
  - `context_: Context`（当前执行上下文）
  - `FailureMode` enum: `KeepSession` / `NewSession`
  - 方法：
    - `SubtaskSession& create_subtask(path, initial_ctx)` — 返回 deque 元素引用（地址稳定）
    - `void archive_subtask_result(SubtaskSession)` — std::move 归档
    - `FailureMode determine_failure_mode()` — <3→KeepSession, ≥3→NewSession
    - `void record_failure(const ExecutionResult&)` — 仅在成功=false 且错误可重试时递增
    - `set_policy(shared_ptr<IExecutionPolicy>)`, `current_policy()`

- [ ] 1.1.3 session.h — UserSession class
  - `user_id_: string`, `created_at_: timestamp`
  - `messages_: vector<ToolResult>`（无 public mutator 暴露）
  - `task_sessions_: std::deque<TaskSession>`（**非 vector**，保证 `current_task_session_` 地址稳定）
  - `current_task_session_: TaskSession*`（指向 deque 中元素，地址不因 push_back 失效）
  - 方法：`append_message(ToolResult)`, `create_task_session()`, `messages() const`
  - 最大历史限制：`task_sessions_` 自动清理超过 100 条的旧历史

### 1.2 创建 session.cpp — 方法实现

- [ ] 1.2.1 `session.cpp` — SubtaskSession::make() 实现
- [ ] 1.2.2 `session.cpp` — TaskSession 构造 + create_subtask() + archive_subtask_result()
- [ ] 1.2.3 `session.cpp` — TaskSession::determine_failure_mode() + record_failure() 实现
  - failure_count 递增条件：`!success && is_retryable_error(result.error_code)`
  - 非可重试错误（PermissionDenied/InvalidInput）不递增
- [ ] 1.2.4 `session.cpp` — UserSession 构造 + append_message() + create_task_session()
- [ ] 1.2.5 `session.cpp` — UserSession 自动裁剪历史（deque 前端 pop，超过 100 条时）

### 1.3 CMake 注册

- [ ] 1.3.1 在根 `CMakeLists.txt` 的 `agenticdsl_core` target 中添加 `session.cpp`
  - 注意：`src/core/CMakeLists.txt` **不存在**，应在根 CMakeLists.txt L120-122 区域添加 `${CMAKE_CURRENT_SOURCE_DIR}/src/core/types/session.cpp`
- [ ] 1.3.2 验证 `make -j$(nproc)` 编译通过

---

## Phase 2: DSLEngine 会话集成 (1.5 天)

### 2.1 engine.h — 声明新重载

- [ ] 2.1.1 添加 `#include "core/types/session.h"`（或前向声明 + PIMPL-lite）
- [ ] 2.1.2 公开方法：`ExecutionResult run(UserSession& user_sess, const std::string& message, const Context& initial_ctx = Context{})`
- [ ] 2.1.3 公开方法（LayeredContext 桥接）：`ExecutionResult run(UserSession& user_sess, const std::string& message, const LayeredContext& initial_lctx)`
- [ ] 2.1.4 私有方法：`ExecutionResult run_impl(TaskSession& task_sess, const std::string& message)`

### 2.2 engine.cpp — 实现会话感知 run

- [ ] 2.2.1 `run(UserSession&, ...)` 实现：创建/复用 TaskSession
  - 若 `user_sess.current_task_session() == nullptr` 或状态为 `completed`/`failed`，调用 `create_task_session()`
  - 若 `determine_failure_mode() == NewSession`（≥3 次失败），创建新 TaskSession
  - 否则复用当前 TaskSession

- [ ] 2.2.2 `run(UserSession&, ...)` 实现：执行 + 结果归档
  - 调用 `run_impl(task_sess, message)`（委托到现有 TopoScheduler::execute）
  - `record_failure()` 记录结果
  - `append_message(to_tool_result(result))` 追加消息
  - 设置 task_sess.status = "completed" / "failed"
  - 返回 ExecutionResult

- [ ] 2.2.3 `run_impl` 实现：SubtaskSession 包装（fork/join 路径）
  - fork 分支处：`task_sess.create_subtask(path, ctx)` 创建 SubtaskSession
  - 执行后：填充 `sub.final_context`，`archive_subtask_result(std::move(sub))`
  - TopoScheduler::execute_single_branch 签名不变

- [ ] 2.2.4 LayeredContext 桥接实现
  - `run(user_sess, message, lctx)` → `return run(user_sess, message, to_context(lctx))`

- [ ] 2.2.5 新增 `to_tool_result(const ExecutionResult&)` helper
  - 确保 error_code、message 正确映射到 ToolResult 信封格式

### 2.3 向后兼容

- [ ] 2.3.1 验证旧 `run(Context)` 和 `run(LayeredContext)` 仍正常工作（52/52 baseline）
- [ ] 2.3.2 不标记任何 deprecated

---

## Phase 3: TopoScheduler 适配 (0.5 天，签名不改)

### 3.1 topo_scheduler.h — 签名不改

- [ ] 3.1.1 `execute()` 和 `execute_single_branch()` 签名**保持不动**
- [ ] 3.1.2 验证：TopoScheduler 不增加任何 Session 相关头文件 include

### 3.2 级联检查

- [ ] 3.2.1 确认所有调用 `execute()` 和 `execute_single_branch()` 的调用点在 Phase 2 后编译通过
- [ ] 3.2.2 验证 `ctest` baseline 52 零回归

---

## Phase 4: 测试 + 验证 (1 天)

### 4.1 测试文件

- [ ] 4.1.1 新建 `tests/test_session.cpp` — 5 个 TEST_CASE：
  - **Session 创建与层级**：UserSession → create_task_session() → create_subtask() 链条正确
  - **Subtask 归档**：分支执行后 `task_sess.subtask_sessions()` 包含归档结果
  - **失败分裂**：模拟 3 次可重试失败，确认第 4 次返回 NewSession
  - **IPER retry 复用**：失败计数 < 3 时返回 KeepSession；非可重试错误不递增
  - **messages 追加写保护**：append_message 后 const& 不可修改

- [ ] 4.1.2 新建 `tests/test_dslengine_session.cpp` — 2 个集成 TEST_CASE：
  - **DSLEngine session 重载 (Context)**：`engine.run(user_sess, "hello")` 正确创建 TaskSession，`ctx["user_input"]` 正确
  - **多轮复用 + LayeredContext 桥接**：2 次 run() 复用同一 TaskSession；LayeredContext → Context 转换正确

### 4.2 编译验证

- [ ] 4.2.1 `cmake --preset tests && make -j$(nproc)` — 0 error
- [ ] 4.2.2 `ctest --output-on-failure` — **≥ 59/59 PASS**（52 baseline + 5 test_session + 2 test_dslengine_session）

### 4.3 ASan/TSan 验证

- [ ] 4.3.1 `cmake --preset asan && ctest` — 0 memory error
- [ ] 4.3.2 `cmake --preset tsan && ctest` — 0 data race

### 4.4 文档同步

- [ ] 4.4.1 更新 `docs/adr/adr-0033-session-hierarchy.md`：🟡 Partial → ✅ Approved
- [ ] 4.4.2 更新 `docs/adr/adr-0033-session-hierarchy.md` §决策：反映 Oracle 4 项修订 + 本 change 10 项设计决策
- [ ] 4.4.3 更新 `docs/roadmap-status.md` §一 Phase 3 进度
- [ ] 4.4.4 更新 `docs/README.md` § adr/ 状态表（ADR-0033 ✅ Approved + ADR-0031 ✅ Approved）
- [ ] 4.4.5 更新 `AGENTS.md` § Recent Changes
- [ ] 4.4.6 修正 `docs/README.md` ADR-0031 状态（当前错误显示 🟡 Partial，应改为 ✅ Approved）

### 4.5 Ship Gate

- [ ] 4.5.1 `openspec validate 2026-06-26-adr-0033-session-hierarchy` exit 0
- [ ] 4.5.2 `git status` clean
- [ ] 4.5.3 更新 master plan C5 状态：⚪ placeholder → ✅ shipped
- [ ] 4.5.4 `openspec archive 2026-06-26-adr-0033-session-hierarchy --yes`

---

## 验证检查清单 (C5 ship gate)

- [ ] 1. 三层 Session 完整工作（创建/引用/归档/分裂）
- [ ] 2. DSLEngine::run(UserSession&, const string&, Context) 可用
- [ ] 3. DSLEngine::run(UserSession&, const string&, LayeredContext) 桥接可用
- [ ] 4. Fork/Join SubtaskSession 隔离归档（TopoScheduler 不感知 Session）
- [ ] 5. failure_count 语义正确（可重试错误递增，非可重试不递增）
- [ ] 6. IPER retry 复用（<3）vs 分裂（≥3）
- [ ] 7. UserSession.messages 追加写保护
- [ ] 8. 容器地址稳定性（deque 取代 vector）
- [ ] 9. ctest ≥ 59/59 PASS
- [ ] 10. ASan 100% clean
- [ ] 11. TSan 100% clean
- [ ] 12. `openspec validate` exit 0
- [ ] 13. ADR-0033 status ✅ Approved
- [ ] 14. master plan C5 状态更新