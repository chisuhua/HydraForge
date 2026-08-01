## Context

ADR-0019 已建立 `IInteractionBus` 机制（`emit` / `subscribe` / `subscribe_glob` / `CausalClock`），但应用语义层无契约。2026-07-31 复核数据（`python3 tools/doc_metrics.py --emit` 可复现）显示：

- 生产代码 28 处 emit 调用存在三种构造方言：
  - `tool_coordinator.cpp` 手搓裸 JSON payload；
  - `node_executor.cpp` 使用 `ToolResult::success(args, meta)` 双参数；
  - `chat_session.cpp` 使用 `ToolResult{...}` 聚合初始化。
- 7 个幻影主题在生产代码零 emit，部分仅在 `tests/test_e2e_mock.cpp` 中伪造。
- 14 个主题已有 emit 但无注册订阅方，审计链路无法消费。

本 change 为 Wave 1 P0，聚焦 5 个非 loop 幻影主题（`llm.request`、`llm.response`、`tool.execution.start`、`tool.execution.end`、`session.persisted`）的真实发射 + 28 处 emit 迁移 + EventBuilder 统一构造。

## Goals / Non-Goals

**Goals:**
- 建立 `EventBuilder` 统一构造方言，明确 `args` / `meta` 字段分工。
- 真实发射 `llm.request`、`llm.response`（含 error 路径）。
- 真实发射 `tool.execution.start`、`tool.execution.end`。
- 真实发射 `session.persisted`。
- 将 28 处既有 emit 全量迁移到 `EventBuilder`。
- 替换 `test_e2e_mock.cpp` 中的伪造事件为真实管线测试。
- 更新 `docs/adr/adr-0068-event-emission-contract.md` 附录 A 状态列。
- 保持 ctest 零回归。

**Non-Goals:**
- 不实现 `loop.turn.start` / `loop.turn.end` / `loop.decision`（归 `fix-loop-agent-bypass`）。
- 不实现 `context.compact.before` / `context.compact.after`（依赖 L0-3 ContextCompactor）。
- 不新增插件域主题（归 ADR-0046）。
- 不实现 OTel exporter（ADR-0063）。
- 不新增 PDK 订阅宏。
- 不高频推送性能指标（沿用 ADR-0046 频率政策）。
- 不修改 `BusEvent` 结构（`causal_time` / `priority` 预留字段不动）。

## Decisions

### Decision 1: EventBuilder 字段分工

`args` 只放 schema 必填业务字段；`meta` 只放 `trace_id` / `session_id` / debug 等附加上下文。

**Rationale**:
- 与 ADR-0023 ToolResult P2-P4 区分 payload 结构；
- 使下游消费者按字段语义过滤，避免业务字段与调试字段混放；
- 便于 lint 规则检查“必填字段是否进入 args”。

**Alternatives Considered**:
- 单 flat JSON 无分工 — 无法 lint schema 必填字段；
- 强制 `args` / `meta` / `debug` 三层 — 过度设计，当前 2 层足够。

### Decision 2: BusEvent 结构不可变

不修改 `BusEvent` 的 `causal_time` / `priority` 预留字段；仅通过 `EventBuilder` 填充现有字段。

**Rationale**:
- 保持与现有序列化/订阅代码兼容；
- 预留字段由 ADR-0037 因果排序后续填充，不在本 change 触及。

**Alternatives Considered**:
- 直接填充 `causal_time` — 需要 CausalClock 集成，超出 Wave 1。

### Decision 3: 插件域主题不归本提案

`inference.*` / `temporal.*` 等插件域事件由 ADR-0046 管辖，本 change 只引用其命名与频率政策。

**Rationale**:
- 避免 ADR 职责膨胀；
- 单一事实源原则。

**Alternatives Considered**:
- 在本 change 中新增插件域主题 — 跨 ADR 边界，导致 ADR-0046 与本提案重复定义。

### Decision 4: 迁移按模块分 commit

commit 顺序：contract → decorator → tool_coordinator → chat_session → executor → cognitive → tests → docs。

**Rationale**:
- 每模块独立可回滚；
- 便于 code review 聚焦；
- 每 commit 后运行验收命令可快速定位遗漏。

**Alternatives Considered**:
- 单 commit 全量迁移 — diff 过大，bisect 与回滚困难；
- 按主题分 commit — 同一文件需多次修改，增加冲突风险。

### Decision 5: 性能指标频率政策沿用

不新增高频性能指标 topic；现有事件发射保持 ADR-0046 “严禁高频指标推送” 政策。

**Rationale**:
- 避免总线过载；
- 与现有架构约束一致。

**Alternatives Considered**:
- 为每个调用增加微秒级性能指标 — 违反 ADR-0046 频率政策。

## Risks / Trade-offs

### Risk 1: 28 处 emit 迁移遗漏

**Mitigation**: 以 `grep -rn "BusEvent{" src examples --include="*.cpp" | grep -v event_builder` 作为验收命令；每模块 commit 前执行；CI 中加入该检查。

### Risk 2: `test_e2e_mock.cpp` 重写引入回归

**Mitigation**: 保留原 mock fallback 行为，仅移除手工 emit 序列，改为断言真实事件；先写新断言再删旧代码。

### Risk 3: Decorator 链事件顺序与并发

**Mitigation**: `TracingDecorator` 作为外层包装，在 `generate()` 调用前/后 emit；并发场景下多个请求通过 `trace_id` 区分；新增测试断言顺序。

### Risk 4: `session.persisted` 发射点定位错误

**Mitigation**: 通过代码审计确认持久化写盘成功函数；在返回成功前最后一个无异常点 emit；失败路径不 emit。

### Trade-off 1: 新增 EventBuilder 抽象 vs 直接改结构

**Trade-off**: EventBuilder 增加一层抽象，但统一方言。
**Decision**: 接受，因为 28 处调用点需统一，且未来新增 emit 可强制 lint。

### Trade-off 2: 一次全量迁移 vs 分批

**Trade-off**: 全量迁移可能产生大 diff，但分批可保持 bisect 可用。
**Decision**: 分批（见 Decision 4）。

## Migration Plan

1. 落地 `EventBuilder` 头文件与基础编译/单元测试。
2. LLM Decorator 链：新增 `llm.request` / `llm.response` 真实发射；迁移既有裸 emit。
3. ToolCoordinator：新增 `tool.execution.start` / `tool.execution.end`；迁移 audit 相关 emit。
4. ChatSession/Session：新增 `session.persisted`；迁移会话相关 emit。
5. NodeExecutor/Cognitive/Domain：迁移剩余 emit。
6. 测试：新增真实发射测试；重写 `test_e2e_mock.cpp`。
7. 文档：更新 `docs/adr/adr-0068-event-emission-contract.md` 附录 A。
8. 验证：运行验收 grep、ctest、adr_lint、docs_drift_audit、openspec validate。

## Open Questions

- `llm.request` 的 `prompt_hash` 实现方式：使用 SHA256 还是 `std::hash` 字符串摘要？待实现时与现有合规 hash 工具对齐。
- `TracingDecorator` 是否新建文件还是复用现有 `CostTrackingDecorator` / `ComplianceDecorator`？依赖现有实现布局，待代码审计后决定。
- `session.persisted` 的精确发射点（`ChatSession::save` 还是 `SessionPersistence` 工具）待代码确认。
