# Tasks: Phase 5 Stage 1 Step 1 — Session Registry + Session Vars (C11)

> **STATUS: ACTIVE** 🟡 (Oracle 深度审查完成 2026-07-03)
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/session-registry/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.2
> **前置依赖**: C10 ✅
> **估时**: 2.5-3.5 天 (Oracle 审查后调整: +1.5d for 7 risk mitigations)
> **最后更新**: 2026-07-03 (Oracle 深度审查 session `ses_0d5985f3effeS1npyEV6SYk2RW`)

---

## 1. SessionRegistry 类实现

- [ ] 1.0 `include/agenticdsl/types/session_config.h` 新建 — SessionConfig 结构 (Oracle Risk 7)
- [ ] 1.1 `src/modules/scheduler/session_registry.h` 新建 — 类定义 + 5 公开方法 (含 is_in_flight)
- [ ] 1.2 `src/modules/scheduler/session_registry.cpp` 新建 — 5 方法实现
- [ ] 1.3 线程安全: `std::shared_mutex mutex_` 保护 `unordered_map` (Oracle Risk 4: shared_mutex 读共享/写独占, 与 ToolRegistry 一致)
- [ ] 1.4 析构安全: 析构时逐个清理 unique_ptr<UserSession>
- [ ] 1.5 `include/agenticdsl/types/session_registry_fwd.h` 新建 — `class SessionRegistry;` 前向声明 (Oracle Risk 5: ADR-0019 §1.4 前向声明策略)

---

## 1a. Oracle Risk Mitigation — 并发安全

- [ ] 1a.1 `session_registry.cpp` — `is_in_flight(id)` 方法: 检查 UserSession 是否有 running TaskSession (Oracle Risk 1)
- [ ] 1a.2 `session_registry.cpp` — `destroy_session()` 先调 is_in_flight, 若 running 等待 (timeout 5s) 或拒绝 (Oracle Risk 1)
- [ ] 1a.3 `session.h` — UserSession + TaskSession 添加显式析构函数, 遍历清理 SubtaskSession + module_states_ (Oracle Risk 6)
- [ ] 1a.4 `session.h` — 验证 `std::shared_ptr<IExecutionPolicy>` 无循环引用 (Oracle Risk 6 析构链完整性)

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

## 4. 4 个 Session 工具注册 (Oracle Risk 3 mitigation)

- [ ] 4.1 `src/common/tools/registry.cpp` — 注册 `session.create` (category=standard, PlanPolicy 需审批)
- [ ] 4.2 `src/common/tools/registry.cpp` — 注册 `session.destroy` (**category=dangerous, approval=force_approval_always**, 任何 Policy 下都需审批)
- [ ] 4.3 `src/common/tools/registry.cpp` — 注册 `session.set_var` (category=standard)
- [ ] 4.4 `src/common/tools/registry.cpp` — 注册 `session.get_var` (category=readonly)
- [ ] 4.5 4 工具全部使用 C6 DECLARE_TOOL 4 参数宏 (name, desc, category, approval_policy) — 强制 ToolMetadata V2
- [ ] 4.6 4 工具接入 ToolCoordinator audit log (tool.audit.{invoked,completed,denied}) — C4 ship 模式

---

## 5. 单元测试

- [ ] 5.1 `tests/test_session_registry.cpp` 新建
- [ ] 5.2 test case: create + get + list_session
- [ ] 5.3 test case: destroy_session 清理所有 TaskSession + module_states (Oracle 风险点)
- [ ] 5.4 test case: 4 工具通过 DSL tool_call 可调用
- [ ] 5.5 test case: session_vars per-run 隔离 (不同 run 互不影响)
- [ ] 5.6 test case: SessionRegistry 多线程 100x 并发 create/destroy (TSan 验证)
- [ ] 5.6a test case: 并发 destroy+run 竞态 — 1 worker 执行 TaskSession 时主线程 destroy (Oracle Risk 1, P0)
- [ ] 5.6b test case: 死锁检测 — mutex 顺序验证 (Oracle Risk 4 shared_mutex 升级)
- [ ] 5.7 test case: Session 销毁后 0 ASan leak (Oracle Risk 6: 析构链完整)
- [ ] 5.8 test case: SessionRegistry 与 ToolRegistry 对称模式验证
- [ ] 5.9 test case: 4 session.* 工具 audit log 事件验证 (Oracle Risk 3)
- [ ] 5.10 test case: session_vars_ 命名空间前缀隔离 (`/session/` vs `/module/`) (Oracle Risk 2)

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
