# Tasks: Phase 5 Stage 1 Step 0 — Lazy ModuleState (C10)

> **STATUS: ACTIVE** 🟡 (Oracle Q1 ✅ resolved — Option A, ModuleState 独立于 LayeredContext)
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/lazy-modulestate/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.1
> **前置依赖**: C9 ✅ archived (2026-07-03)
> **估时**: 1-1.5 天
> **最后更新**: 2026-07-03

---

## 1. ExecutionSession PIMPL 扩展

- [ ] 1.1 `src/modules/scheduler/execution_session.h` — PIMPL Impl struct 加 `std::map<std::string, nlohmann::json> module_states_`
- [ ] 1.2 `src/modules/scheduler/execution_session.h` — 声明 3 公开 API: `ensure_module_state()` / `get_module_state()` / `has_module_state()`
- [ ] 1.3 `src/modules/scheduler/execution_session.cpp` — 实现 `ensure_module_state()` lazy init: 首次访问自动创建空 json
- [ ] 1.4 `src/modules/scheduler/execution_session.cpp` — 实现 `get_module_state()` 只读访问 (返回 const*)
- [ ] 1.5 `src/modules/scheduler/execution_session.cpp` — 实现 `has_module_state()` (bool)
- [ ] 1.6 析构安全: PIMPL 自动释放 module_states_ + ASan verify

---

## 2. NodeExecutor dsl_call 集成

- [ ] 2.1 `src/modules/executor/node_executor.h` — dsl_call 方法签名增加 `ExecutionSession& session` 参数
- [ ] 2.2 `src/modules/executor/node_executor.cpp` — dsl_call 执行时传入 session 引用
- [ ] 2.3 子图 sub-execution 时共享 session.module_states_ (按 module_path 隔离)
- [ ] 2.4 零 Node/NodeType 签名修改

---

## 3. 单元测试

- [ ] 3.1 `tests/test_module_state.cpp` 新建
- [ ] 3.2 test case: lazy init — ensure_module_state 首次调用创建空 json
- [ ] 3.3 test case: persistence — dsl_call 间 module_state 可累积 (counter example)
- [ ] 3.4 test case: isolation — 不同 module_path 的 state 完全隔离
- [ ] 3.5 test case: cleanup — ExecutionSession 析构后 ASan 0 leak
- [ ] 3.6 test case: get_module_state — 不存在的 path 返回 nullptr
- [ ] 3.7 test case: has_module_state — 未初始化返回 false, 初始化后 true

---

## 4. 验证

- [ ] 4.1 `ctest --output-on-failure` ≥ 61/61 + 新增 test_module_state 5-7 case 全绿
- [ ] 4.2 `python3 tools/adr_lint.py` exit 0 (零 ADR 修改)
- [ ] 4.3 `python3 tools/docs_drift_audit.py` 0 DRIFT
- [ ] 4.4 `cmake --preset asan && ctest` 零 ASan error (关键: module_states_ 析构)
- [ ] 4.5 `openspec validate 2026-07-03-phase5-stage1-step0-lazy-modulestate` exit 0

---

## 5. 同步与归档

- [ ] 5.1 提交 (推荐 2 commits: `feat(c10): add module_states_ to ExecutionSession` + `test(c10): add test_module_state.cpp`)
- [ ] 5.2 `git push origin main`
- [ ] 5.3 `openspec archive 2026-07-03-phase5-stage1-step0-lazy-modulestate`
- [ ] 5.4 更新 master plan §四 C10 行状态

---

## 验证检查清单 (C10 ship gate)

- [ ] 1. ExecutionSession PIMPL module_states_ 已 ship
- [ ] 2. ensure_module_state() lazy init 已实现
- [ ] 3. NodeExecutor dsl_call 集成完成
- [ ] 4. test_module_state.cpp ≥ 5 test case 全绿
- [ ] 5. ctest 零回归 (61/61 baseline)
- [ ] 6. ASan 0 leak (关键)
- [ ] 7. ADR-0008 状态保持 ✅ Approved
- [ ] 8. change 已 archive