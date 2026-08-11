# Tasks: PDK SafeExec Tests + Minimal jthread Fix

> **变更类型**: 真实实现 (TDD 5 步: 写失败测试 → 验证 fail → 实施 → 验证 pass → commit)
> **关联 plan**: `.rddf/plans/2026-08-10-pdk-safe-exec-tests.md` (详细 TDD 步骤)
> **关联 proposal**: `openspec/changes/2026-08-10-pdk-safe-exec-tests/proposal.md`
> **关联 spec**: `openspec/changes/2026-08-10-pdk-safe-exec-tests/specs/pdk-safe-exec-tests/spec.md`
> **关联 ADR**: ADR-0021 §3.3 SafeExec (本 change 实施后 §3.3 追加 jthread + stop_token 设计依据)

## T1-T3: TDD Red (写失败测试)

- [ ] **T1**: 写失败测试 `test_pdk_safe_exec_timeout_returns_quickly` (5ms timeout + 500ms sleep → run() MUST ≤ 100ms 返回, 旧实现阻塞 500ms)
- [ ] **T2**: 验证 T1 测试 FAIL (`cmake --build build --target test_pdk_safe_exec && ./build/tests/test_pdk_safe_exec_timeout_returns_quickly`) — 旧 std::async 阻塞至 fn 完成
- [ ] **T3**: 写失败测试 `test_pdk_safe_exec_thread_does_not_leak` (100 次超时调用 → 线程数 ≤ baseline + 10)

## T4: 实施 SafeExec 修复 (Green)

- [ ] **T4.1**: 修改 `include/agenticdsl/pdk/safe_exec.h` 替换 `std::async` 为 `std::jthread` + `std::stop_source` + grace_period
  - 新增 `grace_period_{50ms}` 成员 + `with_grace_period()` chain method
  - 改写 `run()` 模板函数使用 jthread + condition_variable + wait_for
  - 保留 `timeout()` / `layer_profile()` 测试 API
  - 保留异常透传语义 (不变)
- [ ] **T4.2**: 验证 T1 测试 PASS (jthread 实现下, run() ≤ 100ms 返回 + 抛 runtime_error)
- [ ] **T4.3**: 验证 T3 测试 PASS (100 次调用后线程数 ≤ baseline + 10)

## T5-T8: 补齐 SafeExec 测试 (8 cases 全部 PASS)

- [ ] **T5**: 写 `test_pdk_safe_exec_stop_token_cooperative` (lambda 接受 stop_token, 超时后 stop_requested() 返回 true)
- [ ] **T6**: 写 `test_pdk_safe_exec_grace_period_then_detach` (lambda 忽略 stop_token + 5s sleep, run() 在 timeout+grace 后 detach 而非 join)
- [ ] **T7**: 写 `test_pdk_safe_exec_return_types` (int / string / json / void 4 类型, 类型推导正确)
- [ ] **T8**: 写 `test_pdk_safe_exec_exception_propagation` (std::runtime_error + std::invalid_argument 透传)
- [ ] **T9**: 写 `test_pdk_safe_exec_default_values` (timeout=30s + grace_period=50ms 默认值)
- [ ] **T10**: 写 `test_pdk_safe_exec_chainable_config` (with_timeout + with_grace_period + with_layer_profile 链式)
- [ ] **T11**: 验证 T5-T10 测试全部 PASS (新增 6 cases + T1 + T3 = 8 total)
- [ ] **T12**: 验证现有 `test_pdk_macros` 5 cases 零回归 (BACKWARD 兼容)

## T13: Doxygen 覆盖率审计工具

- [ ] **T13.1**: 新建 `tools/check_doxygen_coverage.sh` (~80 行, shell + grep, 扫描 public API)
- [ ] **T13.2**: 给 `safe_exec.h` 补 Doxygen 注释 (@file / @brief / @tparam / @param / @return / @throws, ≥90% 覆盖)
- [ ] **T13.3**: 验证 `./tools/check_doxygen_coverage.sh include/agenticdsl/pdk/safe_exec.h` exit 0

## T14: PDK README 扩展

- [ ] **T14.1**: 在 `pdk/README.md` 末尾追加 `## SafeExec 实战` 章节 (≤ 60 行, 含 stop_token 协同示例 + 异常传播示例 + DECLARE_TOOL 组合示例)
- [ ] **T14.2**: 追加 `## 3 种 Agent Loop 选择指南` 章节 (≤ 40 行, React vs PlanExecute vs ForkJoin 决策树)
- [ ] **T14.3**: 追加 `## AgentForge 衔接` 章节 (≤ 40 行, Phase 6b PDK 集成示例)
- [ ] **T14.4**: 验证 README 章节完整性 grep (3 章节标题存在)

## T15: 全量 release gate

- [ ] **T15.1**: `cmake --preset debug -B build && cmake --build build --target test_pdk_safe_exec test_pdk_macros -j$(nproc)` (零编译错误)
- [ ] **T15.2**: `cd build && ctest --output-on-failure` (40/40 PASS: 32 baseline + 8 new)
- [ ] **T15.3**: `cmake --preset asan -B build/asan && cmake --build build/asan && cd build/asan && ctest --output-on-failure` (40/40 ASan PASS)
- [ ] **T15.4**: `python3 tools/adr_lint.py docs/adr/` (exit 0, ADR-0021 §3.3 同步后)
- [ ] **T15.5**: `python3 tools/docs_drift_audit.py` (0 DRIFT)

## T16: 文档同步

- [ ] **T16.1**: 更新 `docs/adr/adr-0021-pdk-design.md` §3.3 (追加 jthread + stop_token 设计依据 + grace_period 默认值 + 修订 MVP 限制描述)
- [ ] **T16.2**: 更新 `docs/active-status.md` §一 Quick 状态 (total ctest 数字更新 + Phase 6a 任务 2 完成) + §五 最近完成追加 ship 行 + §六 Phase 6a 任务 2 标记完成
- [ ] **T16.3**: 更新 `roadmap.md` Phase 6a 任务 2 状态 + 完成条件勾选

## T17: OpenSpec Archive

- [ ] **T17.1**: 验证 `openspec validate 2026-08-10-pdk-safe-exec-tests --strict` exit 0
- [ ] **T17.2**: `git mv openspec/changes/2026-08-10-pdk-safe-exec-tests openspec/changes/archive/` (git rename 而非 mv)
- [ ] **T17.3**: 更新 archive OpenSpec YAML metadata (添加 ship 日期 + commits 引用 + ctest 数字)

## 提交策略 (4 commits, TDD 5 步结构)

```
T1-T3  → test(pdk): write failing SafeExec timeout semantics tests (TDD red)
T4     → fix(pdk): replace std::async with std::jthread + stop_token (TDD green)
T5-T12 → test(pdk): add 6 SafeExec tests + verify BACKWARD compat
T13-T14 → docs(pdk): Doxygen audit tool + README expansion
T15-T17 → chore(pdk): full release gate + ADR sync + active-status sync + archive
```

## Sprint 24 收官验收 (Phase 6a 任务 2)

- [ ] 40/40 ctest PASS (32 baseline + 8 new SafeExec)
- [ ] 5 commits 已 commit (T1-T3 → T4 → T5-T12 → T13-T14 → T15-T17)
- [ ] `openspec validate 2026-08-10-pdk-safe-exec-tests --strict` exit 0
- [ ] `tools/adr_lint.py docs/adr/` exit 0
- [ ] `tools/docs_drift_audit.py` 0 DRIFT
- [ ] ASan 40/40 PASS
- [ ] Doxygen 覆盖率 ≥ 90%
- [ ] pdk/README.md 3 新章节齐全
- [ ] BACKWARD 兼容 (现有 test_pdk_macros 5 cases 零修改通过)

## Phase 6a 后续范围 (顺延至 Phase 6b/Phase 7+)

- [ ] AgentForge 第 1 领域 agent (Phase 6b 任务 1, 4h, 验证 PDK 复用性)
- [ ] PDK 开发者指南完整化 (Phase 6b 任务 4, 22h, 6-10 章完整指南)
- [ ] pdk_chat_demo v2 + 真实 LLM 集成 (Phase 6b 任务 3, 12h)
- [ ] 完整 SafeExec (fork/cgroups/seccomp, Phase 7+, ADR-0021 §3.3 Phase 3 范围)
- [ ] PluginLifecycle / MockSandbox (Phase 3, ADR-0021 §2.3)
