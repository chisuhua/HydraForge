# session-registry Specification

> **Purpose**: 追踪 C11 SessionRegistry + SessionVars 实施范围
> **关联 proposal**: `../proposal.md`
> **最后更新**: 2026-07-03

## ADDED Requirements

### Requirement: session-registry-creation

SessionRegistry MUST 持有 `unordered_map<string, unique_ptr<UserSession>>` + `std::mutex`。

#### Scenario: 成员存在

- **WHEN** 检查 `src/modules/scheduler/session_registry.h`
- **THEN** 存在 `unordered_map<string, unique_ptr<UserSession>> sessions_` 成员
- **AND** 存在 `std::mutex mutex_` 成员

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

## 备注

本 change 不修改 ADR-0033 Session Hierarchy 3-层设计。SessionRegistry 与 UserSession::task_sessions_ 职责正交, 前者管多 UserSession 生命周期, 后者管单 user 多 TaskSession 生命周期。