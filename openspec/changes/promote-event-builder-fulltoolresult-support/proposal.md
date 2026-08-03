## Why

ADR-0068 (Event Emission Contract) Wave 1 partial ship 与 2026-08-03 archive,但 §5.11 grep 验收保留 **8 处 raw BusEvent 构造** (`tool.completed` / `execution.failed` / `cognitive.task.completed` / `domain.task.{completed,failed}` x3 / `tool.audit.denied` x2),因 EventBuilder 当前 API 强制 `payload.ok=true` 且无 `latency_ms` / `trace_id` / `error_code` setters,不能表达 operation-result 业务语义 (bus subscriber 依赖 `payload.ok` 字段判定成败)。

本 change 扩展 EventBuilder API 覆盖上述场景,迁移 8 处 raw BusEvent → EventBuilder,达到 §5.11 grep 验收标准 (返回 0 行),同时解锁 ADR-0068 状态从 🟡 Partial → ✅ Approved。

## What Changes

- **扩展 `include/agenticdsl/contract/event_builder.h`** (header-only L1 契约层):
  - **新增构造器** `EventBuilder(std::string topic, ToolResult payload)` — 接管完整 ToolResult (含 ok/data/meta/error_code/latency_ms/trace_id/metadata 7 字段),兼容 8 处 raw BusEvent 迁移场景
  - **新增 setters** 覆盖 4 个 P2-P4 optional 字段:
    - `.ok(bool)` — 显式控制 `payload.ok` (默认 true)
    - `.error_code(ErrorCode)` — 设置 `payload.error_code`
    - `.latency_ms(uint64_t)` — 设置 `payload.latency_ms`
    - `.trace_id(std::string)` — 设置 `payload.trace_id`
    - `.metadata(nlohmann::json)` — 设置 `payload.metadata` (P4 REQ-TR-004)
  - **`build()`** 在 8 字段全部就位后构造 `BusEvent` + 自动填充 `timestamp` (steady_clock)
- **迁移 8 处 raw BusEvent → EventBuilder**:
  - `src/modules/executor/node_executor.cpp:196` — `tool.completed`
  - `src/modules/executor/node_executor.cpp:411` — `execution.failed`
  - `src/modules/cognitive/cognitive_worker.cpp:198` — `cognitive.task.completed`
  - `src/modules/cognitive/domain_worker_pool.cpp:226` — `domain.task.failed` (no-handler path)
  - `src/modules/cognitive/domain_worker_pool.cpp:260` — `domain.task.completed`
  - `src/modules/cognitive/domain_worker_pool.cpp:262` — `domain.task.failed` (in-flight)
  - `src/common/tools/tool_coordinator.cpp:220` — `tool.audit.denied` (Layer check denial)
  - `src/common/tools/tool_coordinator.cpp:245` — `tool.audit.denied` (Approval denied)
- **新增单元测试** `tests/test_event_builder_v2.cpp`:
  - 5 new test cases: full ToolResult constructor / ok setter / error_code setter / latency_ms setter / trace_id setter / metadata setter
- **更新 ADR-0068**:
  - §决策 7 (Operation-Result vs Telemetry 分类) 状态从 "open follow-up" → "✅ 已解决"
  - §附录 B (8 处 raw BusEvent 清单) 标记 ✅ 已迁移
  - §状态: 🟡 Partial → ✅ **Approved** (8/8 验收条件全部满足)
- **更新 AGENTS.md Recent Changes**: 追加 2026-08-03 ship 行
- **更新 `docs/active-status.md` §五**: 追加 2026-08-03 ship 行

## Capabilities

### Modified Capabilities
- `event-emission-contract`: EventBuilder API 扩展支持 operation-result 事件 (8 处 raw BusEvent 全部迁移), §5.11 grep 验收返回 0 行; ADR-0068 状态 🟡 Partial → ✅ Approved.

## Impact

- **生产代码**:
  - `include/agenticdsl/contract/event_builder.h` (扩展构造函数 + 5 setters, 零破坏)
  - `src/modules/executor/node_executor.cpp` (2 处替换)
  - `src/modules/cognitive/cognitive_worker.cpp` (1 处替换)
  - `src/modules/cognitive/domain_worker_pool.cpp` (3 处替换)
  - `src/common/tools/tool_coordinator.cpp` (2 处替换)
- **测试代码**:
  - `tests/test_event_builder_v2.cpp` (新增, 6 test cases)
- **文档**:
  - `docs/adr/adr-0068-event-emission-contract.md` (状态翻转 + 附录 B 标记)
  - `AGENTS.md` (Recent Changes 2026-08-03 行)
  - `docs/active-status.md` (ship 行)
- **API 兼容性**:
  - ✅ EventBuilder 现有 API (`.args(json)` / `.meta(json)` / `.build()`) 完全保持
  - ✅ 新增构造函数和 setters 是 additive-only, 现有调用零修改
  - ✅ 8 处 raw BusEvent 迁移后 subscriber 行为不变 (ToolResult 字段全透传)
- **依赖**:
  - ✅ 无新外部依赖
- **风险**:
  - 低 — EventBuilder API 扩展 + 8 处 one-to-one 替换, 每个替换都保持 ToolResult 字段完整
  - 现有 `test_event_builder.cpp` 3 test cases 全部继续 PASS (api 兼容性保证)
  - 现有 `test_engine_bus_integration` / `test_tool_execution_events` 等依赖 `payload.ok` 字段的测试不受影响 (raw BusEvent `payload.ok` 字段透传保留)

## Acceptance Criteria

1. `EventBuilder` 现有 3 test cases (`test_event_builder.cpp`) 全部 PASS — **API 兼容性**
2. 新增 6 test cases (`test_event_builder_v2.cpp`) 全部 PASS — **API 扩展正确**
3. `grep -rn "bus_->emit(BusEvent{" src --include="*.cpp" | grep -v event_builder` 返回 0 行 — **§5.11 全迁移**
4. 8 处迁移点 1:1 替换, 行为等价 (ToolResult 字段全透传)
5. `ctest` 110/111 所有 non-pre-existing 测试 PASS — **零回归**
6. `tools/adr_lint.py` 0 errors
7. `tools/docs_drift_audit.py` 0 DRIFT
8. `openspec validate promote-event-builder-fulltoolresult-support --strict` exit 0
9. ADR-0068 状态 🟡 Partial → ✅ Approved (8/8 验收满足)
10. AGENTS.md + docs/active-status.md 同步 (新增 2026-08-03 行)
