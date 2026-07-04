# Tasks: Phase 5 Stage 1 Step 1 — Session Registry + Session Vars (C11)

> **STATUS: SHIPPED** ✅ (2026-07-04)
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/session-registry/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.2
> **前置依赖**: C10 ✅
> **估时**: 2.5-3.5 天 (实际: ~1 天)
> **最后更新**: 2026-07-04 (C11 实施完成, 63/63 ctest 零回归)

---

## 1. SessionRegistry 类实现

- [x] 1.0 `include/agenticdsl/types/session_config.h` 新建 — SessionConfig 结构 (Oracle Risk 7)
- [x] 1.1 `src/core/types/session_registry.h` 新建 — 类定义 + 5 公开方法 (含 is_in_flight)
  - 注：最终位置从 `src/modules/scheduler/` 调整为 `src/core/types/`（避免 common→scheduler 循环依赖）
- [x] 1.2 `src/core/types/session_registry.cpp` 新建 — 6 方法实现
- [x] 1.3 线程安全: `std::shared_mutex mutex_` 保护 `unordered_map` (Oracle Risk 4: shared_mutex 读共享/写独占, 与 ToolRegistry 一致)
- [x] 1.4 析构安全: 析构时逐个清理 unique_ptr<UserSession>
- [x] 1.5 `include/agenticdsl/types/session_registry_fwd.h` 新建 — `class SessionRegistry;` 前向声明 (Oracle Risk 5: ADR-0019 §1.4 前向声明策略)

---

## 1a. Oracle Risk Mitigation — 并发安全

- [x] 1a.1 `session_registry.cpp` — `is_in_flight(id)` 方法: 检查 UserSession 是否有 running TaskSession (Oracle Risk 1)
- [x] 1a.2 `session_registry.cpp` — `destroy_session()` 先调 is_in_flight, 若 running 等待 (timeout 5s) 或拒绝 (Oracle Risk 1)
- [x] 1a.3 `session.h` — UserSession + TaskSession 添加显式析构函数 (Oracle Risk 6)
- [x] 1a.4 `session.h` — 验证 `std::shared_ptr<IExecutionPolicy>` 无循环引用 (Oracle Risk 6 析构链完整性)

---

## 2. DSLEngine 集成

- [x] 2.1 `src/core/engine.h` — 加 `session_registry_` PIMPL-lite 成员
- [x] 2.2 `src/core/engine.cpp` — 构造时初始化 SessionRegistry
- [x] 2.3 公开 `get_session_registry()` 访问器 (const + non-const)

---

## 3. ExecutionSession 扩展

- [x] 3.1 `src/modules/scheduler/execution_session.h` — 加 `session_id_: std::string` 字段
- [x] 3.2 `src/modules/scheduler/execution_session.h` — 加 `session_vars_: nlohmann::json` 字段
- [x] 3.3 默认值: `session_id_ = ""`, `session_vars_ = {}`
- [x] 3.4 构造时接收 session_id 参数 (与 SessionRegistry.create_session 协同)

---

## 4. 4 个 Session 工具注册 (Oracle Risk 3 mitigation)

- [x] 4.1 `src/core/engine.cpp` — 注册 `session.create` (category=StateModify, ApprovalPolicy{false,true,false,false})
  - 注：工具注册从 registry.cpp 移至 engine.cpp（避免 common→core 循环依赖，SessionRegistry 在 core 中）
- [x] 4.2 `src/core/engine.cpp` — 注册 `session.destroy` (category=StateModify, ApprovalPolicy{true,true,false,true})
- [x] 4.3 `src/core/engine.cpp` — 注册 `session.set_var` (category=StateModify)
- [x] 4.4 `src/core/engine.cpp` — 注册 `session.get_var` (category=ReadOnly)
- [x] 4.5 4 工具全部使用 ToolMetadata V2 (name, desc, domain, category, layer, approval) 内联构造
  - 注：未使用 DECLARE_TOOL 宏（工具注册在 engine 构造体而非编译期），使用直接 `register_tool()` 调用
- [ ] 4.6 ~~4 工具接入 ToolCoordinator audit log~~ → **延后至 C14** (ToolCoordinator 审计日志全局集成)

---

## 5. 单元测试

- [x] 5.1 `tests/test_session_registry.cpp` 新建 (6 个 TEST_CASE, 171 assertions)
- [x] 5.2 test case: create + get + list_session
- [x] 5.3 test case: destroy_session 清理 (idempotent + 清理验证)
- [ ] 5.4 test case: 4 工具通过 DSL tool_call 可调用 → **延后** (需完整 DSL 执行管线, 不在 C11 单测范围)
- [x] 5.5 test case: session_vars per-run 隔离 (不同 run 互不影响)
- [x] 5.6 test case: SessionRegistry 多线程 100x 并发 create/destroy (TSan 验证)
- [ ] 5.6a test case: 并发 destroy+run 竞态 → **延后** (需 TaskSession 实际运行时 destroy, 待 C12+ 集成测试)
- [ ] 5.6b test case: 死锁检测 → **无需** (shared_mutex 单锁, 0 嵌套获取, CP.22 协议保证无死锁)
- [x] 5.7 test case: ASan 验证 — 6/6 PASS, 0 leak
- [x] 5.8 test case: SessionRegistry 与 ToolRegistry 对称模式验证 — 代码结构对齐 (PIMPL-lite, shared_mutex, DSLEngine 持有)
- [ ] 5.9 test case: 4 session.* 工具 audit log 事件验证 → **延后至 C14** (与 4.6 同步)
- [x] 5.10 test case: session_vars_ 命名空间前缀隔离 — 已验证多 session 间变量完全隔离

---

## 6. 验证

- [x] 6.1 `ctest --output-on-failure` — 63/63 PASS (基线 62 + 新增 1)
- [x] 6.2 `python3 tools/adr_lint.py` exit 0
- [ ] 6.3 `python3 tools/docs_drift_audit.py` → **略过** (C11 无文档变更, 无 ADR 新增)
- [x] 6.4 ASan — 零 error (6 用例 / 171 assertions)
- [x] 6.5 TSan — 产品代码 0 race (12 个 Catch2 框架噪音, pre-existing 模式)
- [x] 6.6 `openspec validate 2026-07-03-phase5-stage1-step1-session-registry` exit 0

---

## 7. 同步与归档

- [ ] 7.1 提交
- [ ] 7.2 `git push origin main`
- [ ] 7.3 `openspec archive 2026-07-03-phase5-stage1-step1-session-registry`
- [x] 7.4 更新 master plan (C11 ship 状态) — ✅ 2026-07-04
- [x] 7.5 更新 master plan §四 C11 行状态 — ✅ 2026-07-04

---

## 验证检查清单 (C11 ship gate)

- [x] 1. SessionRegistry 类已 ship
- [x] 2. DSLEngine.session_registry_ 已 ship
- [x] 3. ExecutionSession.session_id_/session_vars_ 已 ship
- [x] 4. 4 个 session.* 工具已注册
- [x] 5. test_session_registry.cpp ≥ 6 test case 全绿
- [x] 6. ctest 零回归 (63/63)
- [x] 7. ASan 0 leak
- [x] 8. TSan 0 product-code race (Catch2 framework noise only)
- [x] 9. ADR-0033 状态保持 ✅ Approved (未修改)
- [x] 10. master plan §四 C11 行更新 (✅ 2026-07-04)
- [ ] 11. change 已 archive (待 commit + archive 执行)

### 有意延后项 (4 项)

| 原任务 | 延后原因 | 目标 |
|--------|---------|------|
| 4.6 ToolCoordinator audit log | 全局集成 | C14 |
| 5.4 DSL tool_call 集成测试 | 需完整执行管线 | C12+ |
| 5.6a 并发 destroy+run | 需 TaskSession 执行 | C12+ |
| 5.9 audit log 事件验证 | 与 4.6 同步 | C14 |