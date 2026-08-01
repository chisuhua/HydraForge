## Why

运行时生命周期事件的应用语义层缺乏契约：生产代码中 28 处 emit 调用存在三种构造方言（裸 JSON、`ToolResult::success(args, meta)`、聚合初始化），`python3 tools/doc_metrics.py --emit` 可复现；7 个幻影主题（`llm.request`、`llm.response`、`tool.execution.start`、`tool.execution.end`、loop 相关、`session.persisted`）在生产代码零 emit，其中 loop 相关主题仅在 `tests/test_e2e_mock.cpp` 伪造发射。下游流式渲染、compaction、steering 等能力无法依赖稳定事件，测试与实现关系倒置。

## What Changes

- **新增** `include/agenticdsl/contract/event_builder.h`：header-only L1 契约层，统一三种方言；`args` 放 schema 必填业务字段，`meta` 放 trace_id/session_id/debug 等附加上下文。
- **修改** LLM Decorator 链（`src/common/llm/`）：在 `TracingDecorator`（或现有 Decorator 链）中真实发射 `llm.request` 与 `llm.response`，覆盖 error 路径。
- **修改** `ToolCoordinator`：在 `call_tool` 线性流首尾补齐 `tool.execution.start` / `tool.execution.end`，与现有 `tool.audit.*` 同源点。
- **修改** `ChatSession` / session 持久化：在持久化写盘成功点发射 `session.persisted`；将 `chat_session.cpp` 现有裸 emit 迁移到 EventBuilder。
- **迁移** 28 处既有 emit 到 EventBuilder（按模块分批 commit）。
- **新增** 5 个幻影主题的真实发射测试。
- **重写** `examples/pdk_chat_demo/tests/test_e2e_mock.cpp`：移除伪造事件，替换为真实管线测试。
- **修改** `docs/adr/adr-0068-event-emission-contract.md` 附录 A：将 5 个非 loop 幻影主题状态列从 👻 改为 ✅。
- **不修改** `BusEvent` 结构（`causal_time` / `priority` 预留字段不动）。
- **不修改** `loop.turn.start` / `loop.turn.end` / `loop.decision` 三主题（归 `fix-loop-agent-bypass` 提案）。
- **不修改** `context.compact.*` 主题（依赖 L0-3 ContextCompactor）。
- **不新增** 插件域主题（归 ADR-0046）。

## Capabilities

### New Capabilities
- `event-emission-contract`: Canonical Topic Registry 维护 + EventBuilder 统一构造 + 28 处既有 emit 迁移 + 5 个幻影主题真实发射。

### Modified Capabilities
- `llm-provider-decorator-chain`: Decorator 链新增 `llm.request` / `llm.response` 真实发射，payload 字段受 ADR-0068 附录 A 约束；现有 Decorator 相关 emit 改用 EventBuilder。
- `tool-coordinator`: `call_tool` 入口/出口新增 `tool.execution.start` / `tool.execution.end` 事件；现有 audit 与 cycle 相关 emit 改用 EventBuilder 构造。
- `chat-session`: 持久化成功后发射 `session.persisted`；用户输入/会话相关 emit 改为 EventBuilder 构造。

## Impact

- **生产代码**:
  - `include/agenticdsl/contract/event_builder.h` (new)
  - `src/common/llm/tracing_decorator.cpp` / `.h` (or equivalent decorator)
  - `src/common/policy/tool_coordinator.cpp` / `.h`
  - `src/modules/chat_session.cpp` / `.h`
  - `src/modules/executor/node_executor.cpp` (and other 28 emit sites)
- **测试代码**:
  - `tests/test_event_emission_contract.cpp` (new)
  - `examples/pdk_chat_demo/tests/test_e2e_mock.cpp` (rewrite)
- **API 兼容性**:
  - ✅ 不修改 `BusEvent` 结构，序列化不变
  - ✅ 公开 API 仅新增 `EventBuilder` 头，不破坏现有接口
  - ✅ 默认行为无变化（未订阅的 topic 与现有行为一致）
- **依赖**:
  - ✅ 无新外部依赖
- **文档**:
  - `docs/adr/adr-0068-event-emission-contract.md` 附录 A 状态列更新
  - `proposal-suggestions.md` 状态行从 “已批准” 标记为 “已创建 change”
- **风险**:
  - 中低 — 仅追加事件发射点与构造方式统一，不修改调度/执行语义
  - 主要风险：28 处迁移遗漏导致 `grep` 验收失败； mitigations：分模块 commit + 验收命令
