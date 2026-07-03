# Tasks: Phase 5 Stage 1 Step 1 — Session Registry + Session Vars (C11)

> **STATUS: PLACEHOLDER** ⚠️
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/session-registry/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.2
> **前置依赖**: C10 ✅
> **估时**: 2-3 天
> **最后更新**: 2026-07-03

---

## 1. SessionRegistry 类实现

- [ ] 1.1 `src/modules/scheduler/session_registry.h` 新建 — 类定义 + 4 公开方法
- [ ] 1.2 `src/modules/scheduler/session_registry.cpp` 新建 — 4 方法实现
- [ ] 1.3 线程安全: `std::mutex mutex_` 保护 `unordered_map`
- [ ] 1.4 析构安全: 析构时逐个清理 unique_ptr<UserSession>

---

## 2. DSLEngine 集成

- [ ] 2.1 `src/core/engine.h` — 加 `session_registry_` PIMPL-lite 成员
- [ ] 2.2 `src/core/engine.cpp` — 构造时初始化 SessionRegistry
- [ ] 2.3 公开 `get_session_registry()` 访问器 (const + non-const)

---

## 3. ExecutionSession 扩展

- [ ] 3.1 `src/modules/scheduler/execution_session.h` — 加 `session_id_: std::string` 字段
- [ ] 3.2 `src/modules/scheduler/execution_session.h` — 加 `session_vars_: nlohmann::json` 字段
- [ ] 3.3 默认值: `session_id_ = ""`, `session_vars_ = {}`
- [ ] 3.4 构造时接收 session_id 参数 (与 SessionRegistry.create_session 协同)

---

## 4. 4 个 Session 工具注册

- [ ] 4.1 `src/common/tools/registry.cpp` — 注册 `session.create` 工具
- [ ] 4.2 `src/common/tools/registry.cpp` — 注册 `session.destroy` 工具 (含 cleanup 验证)
- [ ] 4.3 `src/common/tools/registry.cpp` — 注册 `session.set_var` 工具
- [ ] 4.4 `src/common/tools/registry.cpp` — 注册 `session.get_var` 工具

---

## 5. 单元测试

- [ ] 5.1 `tests/test_session_registry.cpp` 新建
- [ ] 5.2 test case: create + get + list_session
- [ ] 5.3 test case: destroy_session 清理所有 TaskSession + module_states (Oracle 风险点)
- [ ] 5.4 test case: 4 工具通过 DSL tool_call 可调用
- [ ] 5.5 test case: session_vars per-run 隔离 (不同 run 互不影响)
- [ ] 5.6 test case: SessionRegistry 多线程 100x 并发 create/destroy (TSan 验证)
- [ ] 5.7 test case: Session 销毁后 0 ASan leak
- [ ] 5.8 test case: SessionRegistry 与 ToolRegistry 对称模式验证

---

## 6. 验证

- [ ] 6.1 `ctest --output-on-failure` ≥ 61/61 + 新增 test_session_registry 6-8 case 全绿
- [ ] 6.2 `python3 tools/adr_lint.py` exit 0 (零 ADR 修改)
- [ ] 6.3 `python3 tools/docs_drift_audit.py` 0 DRIFT
- [ ] 6.4 `cmake --preset asan && ctest` 零 ASan error (关键: Session 销毁清理)
- [ ] 6.5 `cmake --preset tsan && ctest` 零 TSan warning (关键: 并发 Session 操作)
- [ ] 6.6 `openspec validate 2026-07-03-phase5-stage1-step1-session-registry` exit 0

---

## 7. 同步与归档

- [ ] 7.1 提交 (推荐 2 commits: `feat(c11): add SessionRegistry` + `feat(c11): register 4 session.* tools`)
- [ ] 7.2 `git push origin main`
- [ ] 7.3 `openspec archive 2026-07-03-phase5-stage1-step1-session-registry`
- [ ] 7.4 写 §十一 调整日志到 master plan (C11 ship 状态)
- [ ] 7.5 更新 master plan §四 C11 行状态

---

## 验证检查清单 (C11 ship gate)

- [ ] 1. SessionRegistry 类已 ship
- [ ] 2. DSLEngine.session_registry_ 已 ship
- [ ] 3. ExecutionSession.session_id_/session_vars_ 已 ship
- [ ] 4. 4 个 session.* 工具已注册
- [ ] 5. test_session_registry.cpp ≥ 6 test case 全绿
- [ ] 6. ctest 零回归
- [ ] 7. ASan 0 leak (关键)
- [ ] 8. TSan 0 race (关键)
- [ ] 9. ADR-0033 状态保持 ✅ Approved
- [ ] 10. master plan §四 C11 行更新
- [ ] 11. change 已 archive
