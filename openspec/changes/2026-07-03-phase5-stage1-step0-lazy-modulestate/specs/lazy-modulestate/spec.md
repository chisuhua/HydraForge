# lazy-modulestate Specification

> **Purpose**: 追踪 C10 Lazy ModuleState 实施范围
> **关联 proposal**: `../proposal.md`
> **最后更新**: 2026-07-03

## ADDED Requirements

### Requirement: lazy-modulestate-injection

`ExecutionSession` MUST 持有 `module_states_` map<string, json> 成员。

#### Scenario: 成员存在

- **WHEN** 检查 `src/modules/scheduler/execution_session.h`
- **THEN** 存在 `std::map<std::string, nlohmann::json> module_states_` 成员
- **AND** 位于 PIMPL struct 内 (与 Sprint 19 解耦模式一致)

---

### Requirement: lazy-modulestate-init

首次访问 module_path 时 MUST 自动创建空 json 对象。

#### Scenario: 首次访问

- **WHEN** 调用 `session.ensure_module_state("lib/inference/prefix_cache")`
- **THEN** `module_states_["lib/inference/prefix_cache"]` 存在, 值为空 json `{}`
- **AND** 后续 `get_module_state()` 调用返回该对象引用

#### Scenario: 已存在则不覆盖

- **WHEN** 已存在 `module_states_["foo"] = {"count": 5}` 时再次调用 `ensure_module_state("foo")`
- **THEN** `module_states_["foo"]` 保持 `{"count": 5}`, 不重置

---

### Requirement: lazy-modulestate-isolation

不同 module_path 的 state MUST 完全隔离。

#### Scenario: 隔离验证

- **WHEN** `ensure_module_state("module_a")["counter"] = 1` 与 `ensure_module_state("module_b")["counter"] = 2`
- **THEN** 两个 module_path 的 state 互不影响
- **AND** `get_module_state("module_a")["counter"] == 1`
- **AND** `get_module_state("module_b")["counter"] == 2`

---

### Requirement: lazy-modulestate-cleanup

ExecutionSession 析构时 MUST 释放所有 module_states。

#### Scenario: 析构无泄漏

- **WHEN** ExecutionSession 持有 10 个 module_states 后析构
- **THEN** ASan 报告 0 leak
- **AND** 0 use-after-free (TSan 验证)

#### Scenario: dsl_call 间状态持久化

- **WHEN** ExecutionSession 内 module `lib/test/counter` 累加 count 5 次
- **THEN** 每次 dsl_call 重新进入时, `module_states_["lib/test/counter"]` 保持累加值
- **AND** 不同 ExecutionSession 间的 counter 互不影响 (Session 隔离)

## 备注

本 change 不修改 ADR-0008 LayeredContext 5-层设计。LayeredContext 与 ModuleState 保持正交关系 (Oracle 决议 Q1 依据)。后续 C11/C12 可选地引入桥接便利工具, 不在 C10 范围。