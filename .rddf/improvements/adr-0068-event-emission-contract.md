# adr-0068-event-emission-contract

**优先级**: P0 | **来源**: ADR-0068（D2 立项, 2026-07-31）+ layer-based-missing-capabilities-analysis.md §三 X1 + active-status Wave 1 #2
**阶段**: wave-1 | **分类**: core-impl
**类型**: feature

## 架构依据
- ADR-0068 全文：管辖边界（运行时生命周期事件归 0068，插件域归 ADR-0046，总线机制归 ADR-0019）、附录 A Registry（22 主题四元组）、发射点指定、EventBuilder、测试契约、转 Approved 4 条件。
- 实证基线（`python3 tools/doc_metrics.py --emit` 可复现）：28 处 emit 三种构造方言并存；7 幻影主题仅存于 `test_e2e_mock.cpp` 伪造；14 个主题有 emit 无注册订阅方。
- ADR-0023 ToolResult P2-P4（payload 标准）；ADR-0031 §决策 7（tool.audit.* 审计事件设计，`tool.execution.*` 与其同源点，不重复不冲突）。

## 范围
- **In Scope**: `include/agenticdsl/contract/event_builder.h`（header-only，L1 契约层）；28 处既有 emit 全量迁移（已确认）；`TracingDecorator`（`llm.request` / `llm.response`，含 error 路径）；ToolCoordinator 补 `tool.execution.start` / `tool.execution.end`（audit 同源点）；`session.persisted` 发射（持久化写盘成功点）；5 个主题的真实发射测试；`test_e2e_mock.cpp` 伪造事件全部移除并替换为真实管线测试；ADR-0068 附录 A 状态列更新（👻→✅）。
- **Out Scope**: `loop.*` 三主题（fix-loop-agent-bypass 提案）；`context.compact.*`（依赖 L0-3 ContextCompactor）；插件域主题（归 ADR-0046）；OTel exporter（ADR-0063）；PDK 订阅宏（L3-4）。

## 关键场景
- GIVEN LLM `generate()` 调用，WHEN 经过 Decorator 链，THEN bus 依次收到 `llm.request`（`model`+`prompt_hash`）与 `llm.response`（`tokens`+`duration_ms`，error 路径含 `error_code`）。
- GIVEN 一次工具调用，WHEN ToolCoordinator 执行，THEN bus 收到 `tool.execution.start`（`tool`+`layer`）与 `tool.execution.end`（`tool`+`ok`+`duration_ms`）。
- GIVEN 迁移完成，WHEN `grep -rn "BusEvent{" src examples --include="*.cpp" | grep -v event_builder` 执行，THEN 返回 0（EventBuilder 内部除外）。
- GIVEN 会话持久化写盘成功，WHEN save 完成，THEN bus 收到 `session.persisted`（`session_id`+`path`）。

## 技术约束
- MUST EventBuilder 分工：`args` = schema 必填业务字段，`meta` = 附加上下文（trace_id/session_id/debug）。
- MUST 新增 emit 一律使用 EventBuilder；MUST NOT 修改 `BusEvent` 结构（`causal_time`/`priority` 预留字段不动）。
- MUST NOT 新增插件域主题（归 ADR-0046）；MUST NOT 高频推送性能指标（ADR-0046 频率政策沿用）。
- SHOULD 迁移按模块分 commit（contract/decorator/tool_coordinator/chat_session/executor/cognitive 各一）。

## 验收标准
- 5 个幻影主题（`llm.*`×2、`tool.execution.*`×2、`session.persisted`）全部真实发射 + 发射测试通过。
- EventBuilder 落地且 28/28 迁移完成（上方 grep 验收命令返回 0）。
- `test_e2e_mock.cpp` 伪造事件全部移除，替换为真实管线测试。
- ctest 全量零回归；`python3 tools/adr_lint.py` 0 错误；`python3 tools/docs_drift_audit.py` 0 DRIFT。
- ADR-0068 附录 A 状态列同步更新（👻→✅）。
