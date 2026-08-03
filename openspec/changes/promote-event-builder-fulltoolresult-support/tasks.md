# Tasks

## 1. EventBuilder API 扩展

- [ ] 1.1 添加 `EventBuilder(std::string topic, ToolResult payload)` 构造函数 (header-only)
- [ ] 1.2 添加 `.ok(bool)` setter (覆盖默认 ok=true)
- [ ] 1.3 添加 `.error_code(ErrorCode)` setter
- [ ] 1.4 添加 `.latency_ms(std::uint64_t)` setter
- [ ] 1.5 添加 `.trace_id(std::string)` setter
- [ ] 1.6 添加 `.metadata(nlohmann::json)` setter (P4 REQ-TR-004)
- [ ] 1.7 修改 `build()` 在 7 字段全部就位后构造 BusEvent
- [ ] 1.8 编译验证 (header-only 无外部依赖)
- [ ] 1.9 提交 commit: `feat(contract): extend EventBuilder for operation-result events`

## 2. 单元测试 (test_event_builder_v2.cpp)

- [ ] 2.1 新建 `tests/test_event_builder_v2.cpp` 骨架
- [ ] 2.2 添加 test: full ToolResult constructor 5 字段透传
- [ ] 2.3 添加 test: `.ok(false)` setter 设置 payload.ok=false
- [ ] 2.4 添加 test: `.error_code(ErrorCode::PermissionDenied)` setter
- [ ] 2.5 添加 test: `.latency_ms(123)` setter
- [ ] 2.6 添加 test: `.trace_id(std::string)` setter
- [ ] 2.7 添加 test: `.metadata(json)` setter
- [ ] 2.8 运行 ctest: `test_event_builder_v2` 全部 PASS
- [ ] 2.9 运行 ctest: `test_event_builder` 现有 3 cases 继续 PASS (API 兼容性)
- [ ] 2.10 提交 commit: `test(contract): add EventBuilder v2 operation-result setters`

## 3. 迁移 8 处 raw BusEvent → EventBuilder

- [ ] 3.1 `src/modules/executor/node_executor.cpp:196` — `tool.completed`
- [ ] 3.2 `src/modules/executor/node_executor.cpp:411` — `execution.failed`
- [ ] 3.3 `src/modules/cognitive/cognitive_worker.cpp:198` — `cognitive.task.completed`
- [ ] 3.4 `src/modules/cognitive/domain_worker_pool.cpp:226` — `domain.task.failed` (no-handler)
- [ ] 3.5 `src/modules/cognitive/domain_worker_pool.cpp:260` — `domain.task.completed`
- [ ] 3.6 `src/modules/cognitive/domain_worker_pool.cpp:262` — `domain.task.failed` (in-flight)
- [ ] 3.7 `src/common/tools/tool_coordinator.cpp:220` — `tool.audit.denied` (Layer check)
- [ ] 3.8 `src/common/tools/tool_coordinator.cpp:245` — `tool.audit.denied` (Approval denied)
- [ ] 3.9 编译验证 (cmake --build build)
- [ ] 3.10 提交 commit: `refactor(emit): migrate 8 operation-result events to EventBuilder`

## 4. 验证 §5.11 grep 验收

- [ ] 4.1 运行 `grep -rn "bus_->emit(BusEvent{" src --include="*.cpp" | grep -v event_builder` → 0 行
- [ ] 4.2 运行 `grep -rn "BusEvent{" src examples --include="*.cpp" | grep -v event_builder` → 0 行 (因 test_e2e_mock 已在 main 重写)
- [ ] 4.3 提交 commit: `chore: verify §5.11 grep returns 0`

## 5. ctest 全量验证

- [ ] 5.1 运行 `cmake --build build -j$(nproc)` 全量编译
- [ ] 5.2 运行 `ctest --output-on-failure` 验证零回归 (期望 110/111, 1 pre-existing fail)
- [ ] 5.3 运行 `ctest -R "test_event_builder|test_tool_execution_events|test_engine_bus_integration|test_domain_worker_pool"` 验证本次迁移相关测试 PASS

## 6. ADR-0068 状态翻转

- [ ] 6.1 打开 `docs/adr/adr-0068-event-emission-contract.md`
- [ ] 6.2 §状态: 🟡 Partial → ✅ **Approved** (8/8 验收满足)
- [ ] 6.3 §决策 7 注释: "8 处 raw BusEvent 保留" → "已迁移 (commit ref)"
- [ ] 6.4 §附录 B: 8 处清单标记 ✅ 已迁移
- [ ] 6.5 提交 commit: `docs(adr): ADR-0068 status 🟡 Partial → ✅ Approved`

## 7. 文档同步

- [ ] 7.1 更新 `AGENTS.md` Recent Changes: 追加 2026-08-03 ship 行
- [ ] 7.2 更新 `docs/active-status.md` §五: 追加 2026-08-03 ship 行
- [ ] 7.3 更新 `proposal-approved.md` §已实施: adr-0068 状态稳定 (本次无新增, 仍指向 2026-08-03 archive)
- [ ] 7.4 提交 commit: `docs: sync ship record for promote-event-builder`

## 8. Ship Gate 验证

- [ ] 8.1 运行 `python3 tools/adr_lint.py` → 0 errors
- [ ] 8.2 运行 `python3 tools/docs_drift_audit.py` → 0 DRIFT
- [ ] 8.3 运行 `openspec validate promote-event-builder-fulltoolresult-support --strict` → exit 0
- [ ] 8.4 创建 ship gate 验证报告 `docs/audits/2026-08-03-promote-event-builder-ship-gate.md`
- [ ] 8.5 OpenSpec change archive: `openspec archive promote-event-builder-fulltoolresult-support --yes --skip-specs`
- [ ] 8.6 提交 archive commit

## Follow-ups

1. **ADR-0068 §5.11 grep 0 行达成** — 本 change 完成后, ADR-0068 §5.11 验收 100% 满足, 无 follow-up
2. **ADR-0068 状态 ✅ Approved** — 8/8 验收条件全部满足 (loop.* 三主题已在 main ship via fix-loop-agent-bypass)
