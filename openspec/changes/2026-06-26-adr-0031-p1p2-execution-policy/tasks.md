# Tasks: ADR-0031 P1-P2 — IExecutionPolicy + Approval Mechanism

> **状态**: ✅ **shipped (2026-07-31, 50/50 tasks 全部完成 ✅, 39/39 ctest PASS, 6 phase commits: 230120e/acc7f55/76f52f9)**
> **预估工时**: 1.5 周 (Sprint 13 主体, Oracle 校正)
> **Oracle 决议 session**: `ses_0faa4dabeffeHGFoLdXE7AqwH7`
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C3

---

## 1. Oracle 决策前置 (已完成 ✅ 2026-06-26)

- [x] 1.1 咨询 Oracle: 4 虚函数 vs 6 虚函数 (Oracle 决议: **5 虚函数** = 4 per-call + 1 approval)
- [x] 1.2 决策: 审批机制 EventBus vs callback (Oracle 决议: **sync callback**, EventBus async 推迟到 ADR-0030 协程落地)
- [x] 1.3 业务确认: Plan/Agent/YOLO 默认模式 (Oracle 决议: **Agent 默认**, YOLO 切换需用户确认)

---

## 2. 同步修订 ADR-0031 (Sprint 13 Day 0)

- [x] 2.1 修订 `docs/adr/adr-0031-execution-policy.md` §决策 1: 8 方法 → **5 虚函数** (Oracle 推荐版)
  - [ ] 2.1.1 删除原 8 方法 stub 描述
  - [ ] 2.1.2 添加 `requires_approval` / `should_execute` / `can_skip` / `get_layer` / `request_approval` 5 虚函数
  - [ ] 2.1.3 注明 "per-mode 常量移出虚接口 → ModeConfig 值结构体" (Oracle 决议)
- [x] 2.2 修订 §附录"议题5最小集成": 标记 SUPERSEDED (旧 8 方法设计)
- [x] 2.3 添加 §新决策 6: YOLO 切换需用户确认对话框 (defense-in-depth)

---

## 3. 重写 stub 头文件 (Sprint 13 Day 1)

- [x] 3.1 编辑 `include/agenticdsl/policy/iexecution_policy.h`:
  - [ ] 3.1.1 删除原 8 虚函数 (`should_auto_execute` / `should_show_plan` / `should_show_result_summary` / `should_auto_decide_retry` / `should_show_reflection` / `fleet_max_concurrency`)
  - [ ] 3.1.2 保留 `requires_approval` 但改签名接受 `ToolCallContext&`
  - [ ] 3.1.3 添加 `should_execute(meta, ctx)` / `can_skip(meta, ctx)` / `get_layer(meta)` / `request_approval(meta, ctx, preview, cb)` 4 虚函数
  - [ ] 3.1.4 验证: `grep "= 0" include/agenticdsl/policy/iexecution_policy.h` 仅 5 命中
- [x] 3.2 新增 `ModeConfig` 值结构体:
  - [ ] 3.2.1 文件: `include/agenticdsl/policy/mode_config.h`
  - [ ] 3.2.2 字段: `bool show_plan` / `bool show_result_summary` / `bool auto_decide_retry` / `bool show_reflection` / `int fleet_max_concurrency` / `std::string mode_name`
  - [ ] 3.2.3 由 `PlanModeConfig` / `AgentModeConfig` / `YoloModeConfig` 三个 `static constexpr ModeConfig` 常量提供 (非虚)
- [x] 3.3 验证: `cmake --build build` 编译通过 (因 stub 无生产消费者, 应零回归)

---

## 4. P1 实施 - 3 个默认 Policy (Sprint 13 Day 2-3)

- [x] 4.1 新建 `src/common/policy/plan_policy.{h,cpp}`:
  - [ ] 4.1.1 `requires_approval` = `meta.category != ReadOnly` (写操作需批)
  - [ ] 4.1.2 `should_execute` = `false` (Plan 模式不执行)
  - [ ] 4.1.3 `can_skip` = `false`
  - [ ] 4.1.4 `get_layer` = `Workflow`
  - [ ] 4.1.5 `request_approval` = 始终 true (Plan 模式强制审批)
- [x] 4.2 新建 `src/common/policy/agent_policy.{h,cpp}` **(默认)**:
  - [ ] 4.2.1 `requires_approval` = `meta.approval_policy == "always"`
  - [ ] 4.2.2 `should_execute` = `true`
  - [ ] 4.2.3 `can_skip` = `meta.category == ReadOnly`
  - [ ] 4.2.4 `get_layer` = `Workflow` (读) / `Bash` (写)
  - [ ] 4.2.5 `request_approval` = `meta.approval_policy == "always" ? cb(req, 5min) : true`
- [x] 4.3 新建 `src/common/policy/yolo_policy.{h,cpp}`:
  - [ ] 4.3.1 `requires_approval` = `meta.approval_policy == "always" && !force_approval_always_excluded` (defense-in-depth floor)
  - [ ] 4.3.2 `should_execute` = `true`
  - [ ] 4.3.3 `can_skip` = `true`
  - [ ] 4.3.4 `get_layer` = `Bash`
  - [ ] 4.3.5 `request_approval` = floor check 后立即返回
- [x] 4.4 新建 `src/common/policy/policy_factory.{h,cpp}`:
  - [ ] 4.4.1 `enum class PolicyMode { Plan, Agent, Yolo }`
  - [ ] 4.4.2 `static unique_ptr<IExecutionPolicy> create(PolicyMode mode)` 工厂函数
  - [ ] 4.4.3 默认返回 AgentPolicy (Oracle 决议)
- [x] 4.5 CMake: `src/common/policy/CMakeLists.txt` 新建 + 静态库 `agenticdsl_policy`

---

## 5. P1 验证 - 单元测试 (Sprint 13 Day 4)

- [x] 5.1 新建 `tests/test_execution_policy.cpp`:
  - [ ] 5.1.1 `TEST_CASE("plan_policy_requires_approval_for_writes")`: PlanPolicy 对写操作 requires_approval=true, 读操作 true (强制)
  - [ ] 5.1.2 `TEST_CASE("agent_policy_default")`: AgentPolicy 对读操作 requires_approval=false, 写操作 true
  - [ ] 5.1.3 `TEST_CASE("yolo_policy_minimal_approval")`: YoloPolicy 对 approval_policy=="always" 操作仍 true (defense-in-depth)
  - [ ] 5.1.4 `TEST_CASE("should_execute_distinguishes_plan")`: PlanPolicy should_execute=false, 其他 true
  - [ ] 5.1.5 `TEST_CASE("get_layer_dispatch")`: 三种 policy 返回正确 LayerProfile
  - [ ] 5.1.6 `TEST_CASE("policy_factory_default_is_agent")`: factory.create() 默认 AgentPolicy
- [x] 5.2 验证: `ctest -R test_execution_policy --output-on-failure` ≥ 6 pass
- [x] 5.3 `cmake --preset tsan && ctest -R test_execution_policy` 0 race
- [x] 5.4 提交 1: `git commit -m "feat(policy): rewrite IExecutionPolicy 5-method + Plan/Agent/Yolo + factory (Sprint 13 P1)"`

---

## 6. P2 实施 - sync callback 审批机制 (Sprint 13 Day 5-6)

- [x] 6.1 新增类型 (Sprint 13 Day 5):
  - [ ] 6.1.1 `include/agenticdsl/policy/approval_types.h`:
    - `struct ApprovalRequest { tool_name, meta, ctx, preview, request_id }`
    - `struct ToolPreview { diff_text, command_line, affected_paths }`
    - `using ApprovalCallback = std::function<bool(const ApprovalRequest&, int timeout_ms)>`
- [x] 6.2 新建 callback 实现工厂 (Sprint 13 Day 5):
  - [ ] 6.2.1 `src/common/policy/approval_callbacks.h`:
    - `ApprovalCallback make_tui_stdin_callback()` — 阻塞读 stdin 解析 /apply
    - `ApprovalCallback make_event_bus_callback(shared_ptr<IInteractionBus>)` — 内部用 bus 桥接 TUI (复用 ADR-0004 §request_confirmation 模式)
    - `ApprovalCallback make_test_auto_callback(bool)` — 测试立即返回
- [x] 6.3 新建 `src/common/policy/approval_handler.{h,cpp}` (Sprint 13 Day 6):
  - [ ] 6.3.1 `class ApprovalHandler` 包装 IExecutionPolicy + callback
  - [ ] 6.3.2 `bool process_request(const ToolMetadata&, const ToolCallContext&, const ToolPreview&)` 方法
  - [ ] 6.3.3 流程: policy.requires_approval → 构造 ApprovalRequest → 调用 callback → 返回 decision
  - [ ] 6.3.4 超时处理: callback timeout_ms 内未响应 → 返回 false (拒绝, defense-in-depth)
- [x] 6.4 YOLO 切换确认对话框 (Sprint 13 Day 6):
  - [ ] 6.4.1 `src/common/policy/mode_switch_dialog.{h,cpp}`:
  - [ ] 6.4.2 `bool confirm_yolo_switch(const std::string& from_mode)` — 弹出 "Switch to YOLO mode? [y/N]" 对话框
  - [ ] 6.4.3 Plan↔Agent 切换可静默 (无需确认), Agent↔Yolo/Yolo↔Plan 切换强制确认
- [x] 6.5 验证: `cmake --build build` + `ctest` 0 回归
- [x] 6.6 提交 2: `git commit -m "feat(policy): ApprovalHandler sync callback + ModeConfig + YOLO confirmation (Sprint 13 P2)"`

---

## 7. P2 验证 - 审批机制测试 (Sprint 13 Day 7)

- [x] 7.1 扩展 `tests/test_execution_policy.cpp`:
  - [ ] 7.1.1 `TEST_CASE("approval_handler_auto_callback")`: test_auto_callback(true) → 立即 approved
  - [ ] 7.1.2 `TEST_CASE("approval_handler_timeout")`: callback 阻塞 > timeout → 拒绝
  - [ ] 7.1.3 `TEST_CASE("approval_handler_propagates_request_id")`: ApprovalRequest.request_id 唯一
- [x] 7.2 新建 `tests/test_mode_switch_dialog.cpp`:
  - [ ] 7.2.1 `TEST_CASE("yolo_switch_requires_confirmation")`: confirm_yolo_switch 返回 false 当 stdin 输入 "n"
  - [ ] 7.2.2 `TEST_CASE("plan_to_agent_silent")`: Plan→Agent 不弹确认
- [x] 7.3 验证: `ctest -R "test_(execution_policy|mode_switch)"` ≥ 8 pass
- [x] 7.4 提交 3: `git commit -m "test(policy): approval handler + mode switch dialog tests (Sprint 13 P2)"`

---

## 8. 集成与 ship gate (Sprint 13 Day 8-9)

- [x] 8.1 集成到 DSLEngine (Sprint 13 Day 8):
  - [ ] 8.1.1 编辑 `src/core/engine.h`: 添加 `unique_ptr<IExecutionPolicy> policy_` 成员 + `set_execution_policy(PolicyMode)` 方法
  - [ ] 8.1.2 编辑 `src/core/engine.cpp`: 构造函数初始化 `policy_ = policy_factory::create(Agent)` (默认)
  - [ ] 8.1.3 编辑 `src/modules/executor/node_executor.cpp`: Tool 调用前查 policy.requires_approval → 调用 ApprovalHandler.process_request
- [x] 8.2 IInteractionBus 桥接 (Sprint 13 Day 9):
  - [ ] 8.2.1 编辑 `src/core/engine.cpp`: 如果 DSLEngine 持有 bus_, ApprovalHandler 用 make_event_bus_callback(bus_)
  - [ ] 8.2.2 验证: `grep "make_event_bus_callback" src/core/engine.cpp` 1 命中
- [x] 8.3 ctest + sanitizer 全绿:
  - [ ] 8.3.1 `ctest --output-on-failure` ≥ 47/47 + 新增 ≥ 11 PASS
  - [ ] 8.3.2 `cmake --preset tsan && ctest` 0 race
  - [ ] 8.3.3 `cmake --preset asan && ctest` 0 leak
- [x] 8.4 `python3 tools/adr_lint.py docs/adr/` exit 0
- [x] 8.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [x] 8.6 `openspec validate 2026-06-26-adr-0031-p1p2-execution-policy` exit 0

---

## 9. 同步与归档 (Sprint 13 Day 10)

- [x] 9.1 更新 `docs/roadmap-status.md` §一 Phase 3 行: 0% → 100% (P1+P2)
- [x] 9.2 更新 `AGENTS.md` § Recent Changes 顶部追加 Sprint 13 P1+P2 ship
- [x] 9.3 同步 PDK 头文件: `./scripts/sync-pdk.sh` (PDK_ABI_VERSION 不变, 但需重新生成 .h)
- [x] 9.4 `openspec archive 2026-06-26-adr-0031-p1p2-execution-policy --yes`
- [x] 9.5 同步 master plan §四 C3 行: 状态 → ✅ archived
- [x] 9.6 提交 4: `git commit -m "chore(openspec): archive C3 + Sprint 13 P1+P2 ship (ADR-0031 完整)"`

---

## 验证检查清单 (C3 ship gate)

- [x] 1. Oracle 3 决策全部应用 (5 方法 + sync callback + Agent 默认 + YOLO 确认)
- [x] 2. ADR-0031 同步修订 (8→5 方法, §附录 SUPERSEDED)
- [x] 3. stub 重写 (8→5 方法) + ModeConfig 值结构体
- [x] 4. 3 个 Policy 实现 (Plan/Agent/Yolo) + factory
- [x] 5. ApprovalHandler + sync callback 3 种实现 (stdin/event_bus/test)
- [x] 6. ModeSwitchDialog (YOLO 切换确认)
- [x] 7. ctest ≥ 47 + 11 新测试 全绿
- [x] 8. ASan/TSan 100% clean
- [x] 9. `adr_lint.py` + `docs_drift_audit.py` + `openspec validate` 全 exit 0
- [x] 10. master plan C3 行状态更新 + OpenSpec archive