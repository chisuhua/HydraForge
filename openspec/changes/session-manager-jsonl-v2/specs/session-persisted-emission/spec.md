# session-persisted-emission Specification

## Purpose

定义 `SessionManager::flush_append` 成功路径发射 `session.persisted` 事件的行为契约，遵循 ADR-0068 §决策 5（EventBuilder 统一构造）与附录 A 字段定义。本规范仅描述 v2 新增的事件发射语义，SessionManager 核心 API（v1 已 ship）与 JSONL 存储格式（v1 已 ship，详见 `openspec/specs/jsonl-session-storage/spec.md`）不在本规范范围。

## ADDED Requirements

### Requirement: emit-on-successful-flush

`SessionManager::flush_append` MUST 在 `::fsync(fd)` 成功 + `::close(fd)` 之后、in-memory index 更新之前，调用 `bus_->emit(EventBuilder("session.persisted", ToolResult).args(...).build())` 一次。保证订阅方在调用方观察到 `flush_append` 返回时已收到事件（事件先于 `flush_append` 返回，事件先于 index 更新）。

#### Scenario: 成功追加触发发射
- **GIVEN** `SessionManager` 已通过 `set_bus(bus)` 注入非空 `IInteractionBus` shared_ptr
- **WHEN** `flush_append(node)` 完成 `::fsync(fd)` + `::close(fd)` 后
- **THEN** 调用 `bus_->emit(EventBuilder("session.persisted", ToolResult).args(...).build())` 一次
- **AND** 事件 topic 字符串为 `"session.persisted"`
- **AND** payload 4 字段（`session_id` / `node_id` / `branch_id` / `timestamp`）值非空且类型正确

#### Scenario: 失败路径不发射
- **GIVEN** `SessionManager` 已注入非空 bus
- **WHEN** `flush_append` 因 `::write(fd, ...)` 返回 -1 或 `::fsync(fd)` 抛 `std::system_error` 而失败
- **THEN** `bus_->emit(...)` 不得被调用
- **AND** 异常（`std::system_error` with errno）传播给调用方
- **AND** JSONL 文件不应保留部分写入的行（`::write < 0` 时 fd 已 close，fsync 失败时仍可 close 但已写内容已落 page cache）

### Requirement: payload-matches-adr-0068-appendix-a

事件 payload 字段顺序与类型 MUST 与 ADR-0068 附录 A 第 4.7 节 `session.persisted` 定义完全一致，不允许新增或删除字段。

#### Scenario: payload 字段完整
- **WHEN** `session.persisted` 事件被发射
- **THEN** `args` JSON 包含 4 个字段：
  - `session_id`（string）— 当前 session 的 basename（`current_path_` 文件名去 `.jsonl` 后缀）
  - `node_id`（string）— 新追加节点的 `node.id`（`next_node_id()` 输出）
  - `branch_id`（string）— 新节点的 `node.branch_id`（`append_to_branch` 设置的当前 branch）
  - `timestamp`（number，uint64）— `flush_append` 调用时刻的 `std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()` unix 毫秒

#### Scenario: session_id 提取规则
- **GIVEN** `current_path_ = "/tmp/sessions/sess_abc123.jsonl"`
- **WHEN** 发射 `session.persisted` 事件
- **THEN** `session_id` 字段 = `"sess_abc123"`（去 `.jsonl` 后缀 + 去目录前缀）

### Requirement: use-eventbuilder-constructor

事件构造 MUST 通过 `include/agenticdsl/contract/event_builder.h` 中的 `EventBuilder(topic, ToolResult)` 构造器（ADR-0068 Wave 2 V2 全 payload 构造器），禁止直接构造 `BusEvent` 或绕过 EventBuilder。

#### Scenario: EventBuilder 链式调用
- **WHEN** `flush_append` 准备发射事件
- **THEN** 调用形式为：
  ```cpp
  if (bus_) {
    bus_->emit(
      EventBuilder("session.persisted", ToolResult{...})
        .args({{"session_id", session_id},
               {"node_id", node.id},
               {"branch_id", node.branch_id},
               {"timestamp", now_ms}})
        .build()
    );
  }
  ```
- **AND** 不直接调用 `BusEvent{...}` 构造
- **AND** 不绕开 EventBuilder 直接调用 `bus_->emit(BusEvent{...})`

### Requirement: skip-emit-when-bus-null

若 `SessionManager` 未注入 `IInteractionBus`（`bus_ == nullptr`，默认构造状态），`flush_append` MUST 跳过 `bus_->emit` 调用且不抛异常，保证单元测试（v1 已 ship `tests/test_session_manager.cpp` 24 cases + v1 stash 恢复 `test_session_manager_legacy.cpp` 4 cases）可在无 bus 环境下运行。

#### Scenario: bus 为 null 不抛异常
- **GIVEN** `SessionManager` 构造后未调用 `set_bus(bus)`（`bus_ == nullptr`）
- **WHEN** `flush_append(node)` 成功
- **THEN** `if (bus_) bus_->emit(...)` 跳过 emit
- **AND** `flush_append` 正常返回（无异常）
- **AND** index 更新正常（`nodes_[node.id] = node` + `children_[node.parent_id].insert(node.id)`）

#### Scenario: 注入 bus 后正常发射
- **GIVEN** `SessionManager` 构造后调用 `set_bus(bus)` 注入非空 bus
- **WHEN** `flush_append(node)` 成功
- **THEN** `bus_->emit(...)` 被调用一次
- **AND** EventBus 收到 `session.persisted` topic 事件

### Requirement: persist-emits-session-persisted-on-flush-success

`SessionManager::flush_append` MUST 在每次成功写入 JSONL 记录后发射 `session.persisted` 事件（一次且仅一次），覆盖 `append_to_branch` / `migrate_legacy_json` / `compact` 重写后的新节点等所有 flush_append 调用点。

#### Scenario: append_to_branch 触发发射
- **GIVEN** `SessionManager` 注入 bus
- **WHEN** 调用 `append_to_branch("user msg")` 完成（内部调用 `flush_append(new_node)`）
- **THEN** 发射一次 `session.persisted`，payload `node_id` = 新生成节点 ID

#### Scenario: migrate_legacy_json 触发多次发射
- **GIVEN** 旧格式文件含 N 条 messages
- **WHEN** 调用 `migrate_legacy_json(legacy_path)` 完成（内部对每条 msg 调 `flush_append`）
- **THEN** 发射 N 次 `session.persisted`（每条 msg 一次），最后 1 次是 branch meta 写入的 `flush_append_internal`（不触发事件，因 `flush_append_internal` 是 private 不经 emit 路径）
