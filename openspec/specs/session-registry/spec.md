# session-registry Specification

## Purpose
Phase 5 Stage 1 Step 1 (C11, IP-001 §Step 1, Oracle Q2 Option A) — `DSLEngine` 持有 `SessionRegistry` (与 `tool_registry_` 并列, `std::unordered_map<std::string, std::unique_ptr<UserSession>>` 多 UserSession 注册表),支持 Agent 通过 DSL 跨会话隔离;与 ADR-0033 (C5 ship) 三层 UserSession/TaskSession/SubtaskSession 正交(SessionRegistry 管多 user,UserSession::task_sessions_ 管单 user 多 task);`SessionVars` 放在 `ExecutionSession` (per-run) 而非 `UserSession` (per-conversation)。
## Requirements
### Requirement: session-registry-creation

SessionRegistry MUST 持有 `unordered_map<string, unique_ptr<UserSession>>` + `std::shared_mutex` (Oracle Risk 4)。

#### Scenario: 成员存在

- **WHEN** 检查 `src/modules/scheduler/session_registry.h`
- **THEN** 存在 `unordered_map<string, unique_ptr<UserSession>> sessions_` 成员
- **AND** 存在 `std::shared_mutex mutex_` 成员 (不是 std::mutex)

---

### Requirement: session-registry-thread-safe

create/destroy/get MUST 线程安全 (mutex 保护)。

#### Scenario: 100x 并发 create

- **WHEN** 100 个线程同时调用 `registry.create_session(config)`
- **THEN** 100 个不同 session_id 全部返回
- **AND** TSan 报告 0 data race

#### Scenario: 100x 并发 destroy

- **WHEN** 100 个线程并发 destroy 100 个 session
- **THEN** 所有 session 清理完成, 0 ASan leak
- **AND** 0 use-after-free (TSan 验证)

---

### Requirement: session-vars-isolation

ExecutionSession.session_vars_ MUST per-run 隔离 (不跨 run 共享)。

#### Scenario: per-run 隔离

- **WHEN** run #1 设置 `session_vars_["max_iter"] = 5`, run #2 读取
- **THEN** run #2 的 session_vars_["max_iter"] 不存在 (或为默认值, 不为 5)
- **AND** run #2 调用 `set_var("max_iter", 10)` 不影响 run #1 的 state

---

### Requirement: session-lifecycle-cleanup

`destroy_session` MUST 清理所有 TaskSession + module_states (无泄漏)。

#### Scenario: destroy 完整清理

- **WHEN** Session 拥有 3 个 TaskSession, 每个 TaskSession 的 ExecutionSession 持有 2 个 module_state
- **THEN** 调用 `destroy_session(id)` 后:
  - session_registry_ 不再包含该 id
  - 所有 TaskSession 析构
  - 所有 module_state 释放
  - ASan 报告 0 leak

#### Scenario: 析构 SessionRegistry 清理所有

- **WHEN** SessionRegistry 持有 10 个 Session, 全部含 TaskSession + module_state
- **THEN** SessionRegistry 析构后 ASan 报告 0 leak (所有 unique_ptr 自动释放)

---

### Requirement: session-tools-exposed

MUST 注册 `session.create` / `session.destroy` / `session.set_var` / `session.get_var` 4 个工具。

#### Scenario: 4 工具通过 DSL 可调用

- **WHEN** DSL 节点 `tool_call: "session.create"` + `tool_call: "session.destroy"`
- **THEN** 调用成功, 工具结果正确返回
- **AND** `session.set_var` / `session.get_var` 写入/读取 session_vars_ 正确

