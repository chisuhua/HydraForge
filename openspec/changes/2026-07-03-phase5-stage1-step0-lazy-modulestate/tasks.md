# Tasks: Phase 5 Stage 1 Step 0 — Lazy ModuleState (C10)

> **STATUS: PLACEHOLDER** ⚠️
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/lazy-modulestate/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.1
> **前置依赖**: C9 ✅
> **估时**: 1-1.5 天
> **最后更新**: 2026-07-03

---

## 1. ExecutionSession 扩展

- [ ] 1.1 `src/modules/scheduler/execution_session.h` — 加 `module_states_` 成员到 PIMPL struct
- [ ] 1.2 `src/modules/scheduler/execution_session.cpp` — 实现 `ensure_module_state()` + `get_module_state()`
- [ ] 1.3 析构时 module_states_ 自动释放 (PIMPL 模式天然支持, 验证 ASan)

---

## 2. NodeExecutor dsl_call 集成

- [ ] 2.1 `src/modules/executor/node_executor.cpp` — dsl_call 节点执行时传入 `ExecutionSession&`
- [ ] 2.2 sub-execution 共享 session.module_states_ (按 module_path 隔离)
- [ ] 2.3 验证: counter example 跨 dsl_call 调用 count 累加

---

## 3. 单元测试

- [ ] 3.1 `tests/test_module_state.cpp` 新建
- [ ] 3.2 test case: 首次访问创建空 json
- [ ] 3.3 test case: 多次访问返回同一对象
- [ ] 3.4 test case: 不同 module_path 隔离
- [ ] 3.5 test case: 析构时无内存泄漏 (ASan/TSan 验证)
- [ ] 3.6 test case: dsl_call 间状态持久化 (counter)
- [ ] 3.7 test case: ExecutionSession snapshot 含 module_states_

---

## 4. 验证

- [ ] 4.1 `ctest --output-on-failure` ≥ 61/61 + 新增 test 全绿
- [ ] 4.2 `python3 tools/adr_lint.py` exit 0 (零 ADR 修改)
- [ ] 4.3 `python3 tools/docs_drift_audit.py` 0 DRIFT
- [ ] 4.4 `cmake --preset asan && ctest` 零 ASan error
- [ ] 4.5 `cmake --preset tsan && ctest` 零 TSan warning
- [ ] 4.6 `openspec validate 2026-07-03-phase5-stage1-step0-lazy-modulestate` exit 0

---

## 5. 同步与归档

- [ ] 5.1 提交 (推荐 1 commit: `feat(c10): add Lazy ModuleState to ExecutionSession`)
- [ ] 5.2 `git push origin main`
- [ ] 5.3 `openspec archive 2026-07-03-phase5-stage1-step0-lazy-modulestate`
- [ ] 5.4 写 §十一 调整日志到 master plan (C10 估时调整 +0/-0.5 天)
- [ ] 5.5 更新 master plan §四 C10 行状态

---

## 验证检查清单 (C10 ship gate)

- [ ] 1. ExecutionSession.module_states_ 已 ship
- [ ] 2. dsl_call 间状态持久化验证通过
- [ ] 3. test_module_state.cpp ≥ 5 test case 全绿
- [ ] 4. ctest 零回归
- [ ] 5. ASan/TSan 零 leak/race
- [ ] 6. ADR-0008 状态保持 ✅ Approved (无修改)
- [ ] 7. master plan §四 C10 行更新
- [ ] 8. change 已 archive
