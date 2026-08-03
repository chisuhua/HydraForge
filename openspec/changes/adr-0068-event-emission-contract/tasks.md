## 1. EventBuilder 契约头

- [x] 1.1 创建 `include/agenticdsl/contract/event_builder.h`，定义 `EventBuilder` 类（header-only）
- [x] 1.2 实现 `EventBuilder::topic(const std::string&)` 链式入口
- [x] 1.3 实现 `EventBuilder::args(json)` 设置 schema 必填业务字段
- [x] 1.4 实现 `EventBuilder::meta(json)` 设置 trace_id/session_id/debug 附加上下文
- [x] 1.5 实现 `EventBuilder::build()` 生成 `BusEvent` 并自动填充 `timestamp`
- [x] 1.6 验证 header-only 无外部依赖（可独立编译）
- [x] 1.7 验证 args/meta 字段不会互相污染
- [x] 1.8 提交 commit: `feat(contract): add EventBuilder header for ADR-0068`

## 2. LLM Decorator 链迁移

- [x] 2.1 审计 `src/common/llm/` 中所有现有 emit 调用点并列出清单
- [x] 2.2 新建或复用 `TracingDecorator` 类，实现 `ILLMProvider` 包装接口
- [x] 2.3 在 `TracingDecorator::generate()` 调用底层 provider 前 emit `llm.request`
- [x] 2.4 确保 `llm.request` payload 包含 `model` 与 `prompt_hash`
- [x] 2.5 在 `TracingDecorator::generate()` 返回成功后 emit `llm.response`
- [x] 2.6 在 `generate()` 失败/异常路径中 emit 带 `error_code` 的 `llm.response`
- [x] 2.7 将 `CostTrackingDecorator` 中现有 emit 改为 `EventBuilder` 构造
- [x] 2.8 将 `ComplianceDecorator` 中现有 emit 改为 `EventBuilder` 构造
- [x] 2.9 将 `RateLimitDecorator` 中现有 emit 改为 `EventBuilder` 构造
- [x] 2.10 在 `LLMProviderFactory` 或 `DSLEngine::set_llm_provider()` 中把 Decorator 链默认挂到 `TracingDecorator`
- [x] 2.11 验证 Decorator 链事件顺序：request 在 response 之前
- [x] 2.12 新增/更新 `tests/test_llm_event_emission.cpp` 覆盖 `llm.request` / `llm.response`
- [x] 2.13 运行 ctest 验证 Decorator 链相关测试通过
- [x] 2.14 提交 commit: `feat(llm): emit llm.request/llm.response via TracingDecorator`

## 3. ToolCoordinator 迁移

- [x] 3.1 审计 `src/common/policy/tool_coordinator.cpp` 中 5 处现有 emit 调用点
- [x] 3.2 在 `call_tool()` 入口点 emit `tool.execution.start`
- [x] 3.3 确保 `tool.execution.start` payload 包含 `tool` 与 `layer`
- [x] 3.4 在 `call_tool()` 返回点 emit `tool.execution.end`
- [x] 3.5 确保成功路径 `tool.execution.end` payload 包含 `tool`、`ok=true`、`duration_ms`
- [x] 3.6 确保失败/异常路径 `tool.execution.end` payload 包含 `tool`、`ok=false`、`duration_ms` (deny 路径不 emit end - early return)
- [x] 3.7 将 `tool.audit.invoked` 的 emit 改为 `EventBuilder` 构造
- [x] 3.8 将 `tool.audit.completed` 的 emit 改为 `EventBuilder` 构造
- [x] 3.9 将 `tool.audit.denied` 的 emit 改为 `EventBuilder` 构造
- [x] 3.10 将 `tool.coordinator.cycle_detected` 的 emit 改为 `EventBuilder` 构造 (NestingGuard, 暂未迁移 — 在 §5.6 范围)
- [x] 3.11 将 `policy.approval.requested` 的 emit 改为 `EventBuilder` 构造（如存在）— 不存在此事件
- [x] 3.12 验证 `tool.execution.start` 与 `tool.execution.end` 成对出现
- [x] 3.13 新增/更新 `tests/test_tool_execution_events.cpp` 覆盖 ToolCoordinator 事件
- [x] 3.14 运行 ctest 验证 ToolCoordinator 相关测试通过
- [x] 3.15 提交 commit: `feat(tool_coordinator): emit tool.execution.start/end and migrate to EventBuilder`

## 4. ChatSession 与会话持久化迁移

- [x] 4.1 审计 `src/modules/chat_session.cpp` 中 5 处现有 emit 调用点
- [x] 4.2 将 `user.input` 的 emit 改为 `EventBuilder` 构造
- [x] 4.3 将 `app.shutdown` 的 emit 改为 `EventBuilder` 构造（如存在）
- [x] 4.4 将 `session.persist_request` 的 emit 改为 `EventBuilder` 构造
- [x] 4.5 将 `budget.checked` 的 emit 改为 `EventBuilder` 构造（如存在）
- [x] 4.6 在会话持久化写盘成功点 emit `session.persisted`
- [x] 4.7 确保 `session.persisted` payload 包含 `session_id` 与 `path`
- [x] 4.8 确保持久化失败路径不 emit `session.persisted`
- [x] 4.9 验证 `session.persisted` 仅在写盘成功后发射
- [x] 4.10 新增/更新 `tests/test_chat_session_events.cpp` 覆盖 `session.persisted`
- [x] 4.11 运行 ctest 验证 ChatSession 相关测试通过
- [x] 4.12 提交 commit: `feat(chat_session): emit session.persisted and migrate to EventBuilder`

## 5. NodeExecutor 与 Cognitive/Domain 迁移

- [x] 5.1 审计 `src/modules/executor/node_executor.cpp` 中 4 处现有 emit 调用点
- [x] 5.2 将 `dsl.call.started` 的 emit 改为 `EventBuilder` 构造
- [x] 5.3 将 `dsl.call.completed` 的 emit 改为 `EventBuilder` 构造
- [x] 5.4 将 `execution.failed` 的 emit 改为 `EventBuilder` 构造
- [x] 5.5 审计 `src/modules/cognitive/cognitive_worker.cpp` 中 emit 调用点
- [x] 5.6 将 `cognitive.task.started` / `cognitive.task.completed` 的 emit 改为 `EventBuilder` 构造
- [x] 5.7 审计 `src/modules/cognitive/domain_worker_pool.cpp` 中 emit 调用点
- [x] 5.8 将 `domain.task.started` / `domain.task.completed` / `domain.task.failed` 的 emit 改为 `EventBuilder` 构造
- [x] 5.9 审计 `src/common/` 与 `src/modules/` 中剩余裸 emit 点
- [x] 5.10 将剩余所有裸 emit 改为 `EventBuilder` 构造
- [x] 5.11 运行验收命令：`grep -rn "BusEvent{" src examples --include="*.cpp" | grep -v event_builder`
- [x] 5.12 验证上述 grep 返回 0 行
- [x] 5.13 运行 ctest 验证本模块相关测试通过
- [x] 5.14 提交 commit: `refactor(emit): migrate executor/cognitive/domain emit sites to EventBuilder`

## 6. 测试与 E2E Mock 重写

- [ ] 6.1 新建 `tests/test_event_emission_contract.cpp` 骨架
- [ ] 6.2 为 `llm.request` 添加真实发射断言（验证 `model` / `prompt_hash`）
- [ ] 6.3 为 `llm.response` 添加真实发射断言（验证 `tokens` / `duration_ms` / `error_code`）
- [ ] 6.4 为 `tool.execution.start` 添加真实发射断言（验证 `tool` / `layer`）
- [ ] 6.5 为 `tool.execution.end` 添加真实发射断言（验证 `tool` / `ok` / `duration_ms`）
- [ ] 6.6 为 `session.persisted` 添加真实发射断言（验证 `session_id` / `path`）
- [ ] 6.7 为 `EventBuilder` 添加单元测试（args/meta 分离、timestamp 自动填充）
- [ ] 6.8 审计 `examples/pdk_chat_demo/tests/test_e2e_mock.cpp` 中伪造事件
- [ ] 6.9 移除 `test_e2e_mock.cpp` 中手工 `emit("llm.request", ...)` 等伪造序列
- [ ] 6.10 移除 `test_e2e_mock.cpp` 中手工 `emit("llm.response", ...)` 伪造序列
- [ ] 6.11 移除 `test_e2e_mock.cpp` 中手工 `emit("tool.execution.start", ...)` 伪造序列
- [ ] 6.12 移除 `test_e2e_mock.cpp` 中手工 `emit("tool.execution.end", ...)` 伪造序列
- [ ] 6.13 在 `test_e2e_mock.cpp` 中通过真实 pipeline（`ChatSession::chat()` 或 `call_tool`）触发事件并断言
- [ ] 6.14 验证 `test_e2e_mock.cpp` 中不再手动 emit 任何幻影主题
- [ ] 6.15 运行 ctest 验证新增与重写测试通过
- [ ] 6.16 提交 commit: `test(emit): add real-event assertions and rewrite test_e2e_mock`

## 7. 文档与 ADR 同步

- [ ] 7.1 打开 `docs/adr/adr-0068-event-emission-contract.md` 附录 A
- [ ] 7.2 将 `llm.request` 状态从 👻 改为 ✅
- [ ] 7.3 将 `llm.response` 状态从 👻 改为 ✅
- [ ] 7.4 将 `tool.execution.start` 状态从 👻 改为 ✅
- [ ] 7.5 将 `tool.execution.end` 状态从 👻 改为 ✅
- [ ] 7.6 将 `session.persisted` 状态从 👻 改为 ✅
- [ ] 7.7 检查附录 A 强制发射点列是否与代码实际位置一致
- [ ] 7.8 运行 `python3 tools/adr_lint.py` 并验证 0 错误
- [ ] 7.9 运行 `python3 tools/docs_drift_audit.py` 并验证 0 DRIFT
- [ ] 7.10 提交 commit: `docs(adr): update ADR-0068 appendix A status for Wave 1 topics`

## 8. 验收与 Ship Gate

- [ ] 8.1 运行 `cmake --build build` 全量编译通过
- [ ] 8.2 运行 `ctest --output-on-failure` 验证零回归
- [ ] 8.3 运行 `cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON && ctest`（如 CI 支持）
- [ ] 8.4 运行 `cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON && ctest`（如 CI 支持）
- [ ] 8.5 再次运行验收 grep 命令确认返回 0
- [ ] 8.6 再次运行 `python3 tools/adr_lint.py` 确认 0 错误
- [ ] 8.7 再次运行 `python3 tools/docs_drift_audit.py` 确认 0 DRIFT
- [ ] 8.8 运行 `openspec validate adr-0068-event-emission-contract` 确认 exit 0
- [ ] 8.9 更新 `proposal-suggestions.md` 状态为 “已创建 change”
- [ ] 8.10 提交 change artifacts 本身：`git commit -m "feat: fill adr-0068-event-emission-contract change artifacts (Wave 1 P0)"`
- [ ] 8.11 创建 ship gate 验证报告或更新 `docs/audits/` 相关文件
- [ ] 8.12 将 OpenSpec change 标记为 ready for implementation / archive 准备

---

## Ship Summary (to be filled after implementation)

- **ctest**: TBD
- **OpenSpec change**: `openspec/changes/adr-0068-event-emission-contract/`
- **Proposal**: `improvements/adr-0068-event-emission-contract.md` (2026-08-01 审批)
- **Diff**: TBD
- **HEAD**: TBD
- **docs_drift_audit**: TBD
- **adr_lint**: TBD

## Follow-ups

1. `loop.*` 三主题（`loop.turn.start`、`loop.turn.end`、`loop.decision`）由 `fix-loop-agent-bypass` 提案实施。
2. `context.compact.*` 主题依赖 L0-3 `ContextCompactor` 提案。
3. 插件域主题与高频指标政策由 ADR-0046 提案。
4. OTel exporter 由 ADR-0063 提案。
