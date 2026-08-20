# mock-bus-canonical-extract

## Why

依据 `docs/architecture/defect-truth-table-2026-08.md` 缺陷 6.1（v1.1 校正：实测 **9 处** MockBus 重复，非 v1.0 所述 7 处）：

1. `tests/test_skill_interpreter.cpp:59`
2. `tests/test_budget_agent_hooks.cpp:26`
3. `tests/test_context_compactor.cpp:122`
4. `tests/test_escalation_triggers.cpp:25`
5. `tests/test_tool_coordinator.cpp:54`
6. `tests/test_tool_coordinator_hooks.cpp:80`
7. `examples/pdk_chat_demo/tests/test_session_persistence.cpp:30`
8. `examples/pdk_chat_demo/tests/test_e2e_mock.cpp:69`
9. `examples/pdk_chat_demo/tests/test_budget_alert.cpp:33`

9 处 MockBus 实现重复（不同 test 文件的 fixture 行为不一致（有的 filter，有的不 filter；有的 sync emit，有的 async））。生产代码 InMemoryBus 唯一，9 处测试 fixture 行为不一致导致断言语义分散。

**性质重判**：工程债，**非架构缺陷**。无需 ADR 方案，提取 canonical fixture 即可。

## What Changes

**In Scope**:

- **第 0 步：9 处 MockBus 行为差异矩阵**（Metis 评审要求）——迁移前先盘点每处行为：
  | # | 文件 | 类名 | BusEvent 存储 | string emit 存储 | payload 存储 | subscribe 支持 | 断言依赖 |
  |---|------|------|---------------|------------------|---------------|----------------|---------|
  | 1 | test_skill_interpreter.cpp:59 | MockBus | ❌ noop | ✅ `string_emits` (pair<type, content>) | ❌ | ❌ stub (return 0) | 记录 string emit 内容 |
  | 2 | test_budget_agent_hooks.cpp:26 | MockBus | ✅ topic only → `topics_` | ✅ topic → `topics_` (merged) | ❌ | ❌ stub | topics_ vector |
  | 3 | test_context_compactor.cpp:122 | MockBus | ✅ full `events` (BusEvent) | ❌ noop | ✅ events | ❌ stub | events vector full payload |
  | 4 | test_escalation_triggers.cpp:25 | MockBusForEscalation | ✅ topic only → `events_` | ✅ topic → `events_` (merged) | ❌ | ❌ stub | events_ topic string |
  | 5 | test_tool_coordinator.cpp:54 | MockInteractionBus | ✅ topic only → `emit_log_` | ✅ topic → `emit_log_` (merged) | ❌ | ❌ stub | emit_log_ topic string |
  | 6 | test_tool_coordinator_hooks.cpp:80 | MockInteractionBus | ✅ topic only → `emit_log_` + payload → `payloads_` | ✅ topic → `emit_log_` | ✅ payloads_ | ❌ stub | emit_log_ + payloads_ |
  | 7 | test_session_persistence.cpp:30 | MockBus | ✅ pair<topic, meta> → `events` + notify subscribers | 委托给 BusEvent 路径 | ✅ events[i].second = meta | ✅ **完整支持** (token + notify) | events pair<topic,meta> + subscribers |
  | 8 | test_e2e_mock.cpp:69 | MockBus | ✅ pair<topic, meta> → `events` + notify | 委托给 BusEvent 路径 | ✅ events[i].second = meta | ✅ 完整支持 | events pair + subscribers |
  | 9 | test_budget_alert.cpp:33 | MockBus | ✅ EventRecord (topic/data/meta/ok) → `events` + notify | 委托给 BusEvent 路径 | ✅ full payload | ✅ 完整支持 | EventRecord 完整字段 |

**关键发现**：
- 9 处行为差异显著（3 大类：仅 topic string / 仅 BusEvent 完整 / 仅 pair<topic,meta>）
- 仅 3 处（#7/#8/#9）实现完整 subscribe + notify；6 处 stub return 0
- #1 是最简版本（string emit only）
- canonical fixture 需要**至少覆盖**：`std::vector<BusEvent> events`（兼容 #3/#7/#8/#9）+ `std::vector<std::string> topics`（兼容 #2/#4/#5/#6）+ subscribers 完整支持（兼容 #7/#8/#9），并提供 `count()`/`last()`/`clear()` helpers
- 新建 `tests/test_helpers/mock_bus.h`：canonical MockBus
  - 继承 `IInteractionBus`
  - 含 `std::vector<BusEvent> events_` 记录 + subscribe（同步）/ subscribe_async（异步）
  - 支持 topic glob 过滤（`llm.*` / `*`）
  - 含 `clear()` / `count(topic)` / `last(topic)` helper
- 9 处 MockBus 实现全部迁移到 canonical fixture
  - 删除本地 MockBus 类
  - include `tests/test_helpers/mock_bus.h`
  - 调整类型引用（如 `MockBus` → `test::MockBus`）
- 新建 `tests/test_mock_bus_canonical.cpp`：canonical fixture 自测试（≥ 10 cases）

**Out of Scope**:

- InMemoryBus 行为修改（生产代码不变）
- 测试断言修改（保持现有断言语义，仅换 fixture 实现）
- examples/pdk_chat_demo 的 mock_mode 行为修改

### 断言调整白名单（Metis 评审要求——AI 面对红测试时的护栏）

仅在以下情况允许微调断言（**逐条记录 + 在 diff 中标注**）：
1. **语义等价调整**：原断言检查"事件被记录"，canonical fixture 记录方式一致（events_ vector），只是访问 API 变化（`mock.events` → `mock.events()`）
2. **时序调整**：原测试对 async emit 用 sleep 轮询，canonical fixture 提供显式 wait/同步 API 后改为确定性等待（等价的同步断言）
3. **类型调整**：`MockBus` → `test::MockBus` 命名空间变化导致的限定名调整

**禁止**：
- 改变断言数量（删除/合并 REQUIRE）
- 改变断言语义（放宽/收紧检查条件）
- 为"通过"而修改被测逻辑

### 关键场景

- **GIVEN** 9 处现有 MockBus 实现（行为各异）
  **WHEN** 迁移到 canonical fixture
  **THEN** 9 处文件 include canonical header，删除本地 MockBus 类，**测试断言不变**

- **GIVEN** canonical MockBus 订阅 `subscribe("llm.request", handler)`
  **WHEN** EventLog emit `llm.request` 事件
  **THEN** handler 被同步触发 + `events_` vector 追加事件

- **GIVEN** canonical MockBus 订阅 `subscribe_async("tool.*", handler)`
  **WHEN** EventLog emit `tool.execution.start` 事件
  **THEN** handler 入队 + 后台线程消费（与现有 InMemoryBus 行为一致）

- **GIVEN** canonical MockBus `count("llm.request")`
  **WHEN** 调用 helper
  **THEN** 返回 events_ 中 topic=`llm.request` 的事件数

- **GIVEN** canonical MockBus `clear()`
  **WHEN** 调用 helper
  **THEN** events_ 清空 + 所有 subscribe handler 保留（不取消订阅）

**Out of Scope**:

- (no items specified)

## Capabilities

- **MUST** 9 处现有测试**不改断言**（仅换 fixture 实现）
- **MUST** canonical MockBus 提供同步 + 异步两种 subscribe 模式（与 InMemoryBus 行为对齐）
- **SHOULD** canonical MockBus 含事件过滤（topic glob / agent_id / time range）
- **MUST** helper API 与现有 InMemoryBus 一致（subscribe / subscribe_async / unsubscribe / emit）
- **MUST NOT** 修改 InMemoryBus 任何代码（生产代码不变）
- **MUST NOT** 修改现有测试断言（仅换 fixture 实现）

## Impact

- 测试代码统一，断言语义一致
- 不影响生产代码
- 不增加依赖（仅创建新 header + 9 处 include）

## Acceptance

- [ ] `tests/test_helpers/mock_bus.h` 实现
- [ ] 9 处现有 MockBus 全部迁移到 canonical fixture（grep 验证零旧 MockBus 类）
- [ ] `tests/test_mock_bus_canonical.cpp` ≥ 10 cases（同步 / 异步 / glob / count / clear / last / 多订阅者）
- [ ] ctest 全量零回归（现有测试断言不变）
- [ ] `docs/architecture/defect-truth-table-2026-08.md` 缺陷 6.1 状态从"MockBus 重复 9 处"更新为"MockBus canonical 已 ship"