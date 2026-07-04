# lazy-modulestate Specification

> **Purpose**: 追踪 C10 Lazy ModuleState 实施范围
> **关联 proposal**: `../proposal.md`
> **最后更新**: 2026-07-03

## ADDED Requirements

### Requirement: lazy-modulestate-injection

ExecutionSession MUST 在 PIMPL Impl struct 持有 `module_states_: std::map<std::string, nlohmann::json>` 成员。

#### Scenario: 成员存在

- **WHEN** 检查 `src/modules/scheduler/execution_session.h` PIMPL Impl struct
- **THEN** 存在 `std::map<std::string, nlohmann::json> module_states_` 成员

---

### Requirement: lazy-modulestate-init

首次调用 `ensure_module_state(path)` MUST 自动创建空 json 对象。

#### Scenario: lazy init

- **WHEN** 调用 `session.ensure_module_state("/lib/inference/kv_cache")`
- **AND** module_states_ 中不存在该路径
- **THEN** 自动创建 json{} 对象
- **AND** 返回该对象的引用 (可修改)

#### Scenario: 重复调用不重建

- **WHEN** `ensure_module_state("/lib/inference/kv_cache")` 第二次调用
- **THEN** 返回已存在的 json 对象引用 (不重建)
- **AND** 所有修改保持不变

---

### Requirement: lazy-modulestate-isolation

不同 module_path 的 state MUST 完全隔离。

#### Scenario: 命名空间隔离

- **WHEN** `ensure_module_state("/module/a")` 设置 `count = 5`
- **AND** `ensure_module_state("/module/b")` 设置 `count = 10`
- **THEN** `get_module_state("/module/a")` 返回 json["count"] = 5
- **AND** `get_module_state("/module/b")` 返回 json["count"] = 10
- **AND** 两个 module 互不影响

---

### Requirement: lazy-modulestate-cleanup

ExecutionSession 析构时 PIMPL 自动释放 module_states_ (无泄漏)。

#### Scenario: ASan 0 leak

- **WHEN** 创建 ExecutionSession, 初始化 3 个 module_state (各含嵌套 json)
- **AND** ExecutionSession 析构
- **THEN** ASan 报告 0 heap-use-after-free
- **AND** 0 memory leak

---

### Requirement: lazy-modulestate-dsl-call

NodeExecutor dsl_call 执行时 MUST 传入 ExecutionSession& 引用, 使子图可访问 module_states_。

#### Scenario: dsl_call 透传

- **WHEN** DSL 节点执行 `dsl_call("/lib/inference/some_subgraph")`
- **THEN** NodeExecutor 传递 `ExecutionSession&` 给子图执行器
- **AND** 子图可通过 session 访问 module_states_["/lib/inference/some_subgraph"]