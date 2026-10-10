# Tasks: consolidate-loop-phases-to-shared-helpers

**Change ID**: `consolidate-loop-phases-to-shared-helpers`
**Status**: 🔍 Proposed (24h cooling-off 起点 = OpenSpec change 立项完成时, ~2026-10-10T18:53Z 满点)
**关联 design**: [design.md](design.md) · [proposal.md](proposal.md) · [ADR-0089 v1.3](../../docs/adr/adr-0089-v1-3-amendment-loop-phase-shared-helpers.md)

> **任务组织**: 按 `## 1` `## 2` ... 分 phase, 每 phase 内部 `- [ ] X.Y` checkbox。TDD 5 步: 1. RED (写失败测试) → 2. GREEN (实现最小代码让测试过) → 3. REFACTOR (消除重复) → 4. ASSERT (契约断言) → 5. GATE (ship gate 全量回归)。

---

## 1. Setup & 环境

- [ ] 1.1 准备工作树: `git worktree add ../hydraforge-consolidate-loop -b feat/consolidate-loop-phases`
- [ ] 1.2 验证 OpenSpec change artifacts 4 件套完整 (proposal.md + design.md + tasks.md + specs/loop-phases-shared-helpers/spec.md)
- [ ] 1.3 验证 base commit: `git log HEAD~5..HEAD --oneline` 显示 pre-change 状态干净
- [ ] 1.4 验证 build 环境: `cmake --preset release -B build && cmake --build build -j$(nproc)` exit 0

## 2. Phase 0: helper 抽取 (commit 1 of 5)

**TDD Step 1: RED — 写失败测试**

- [ ] 2.1 新增 `tests/test_loop_phases.cpp` 包含 3 测试组 (plan/verify/execute):
  - [ ] 2.1.1 `loop_phases::run_plan_phase` 接受 ILLMProvider ref + goal + ctx → MockLLMProvider 返回非空 → std::optional<string> 有值
  - [ ] 2.1.2 `loop_phases::run_plan_phase` 当 MockLLMProvider 返回空 → std::optional<string> nullopt
  - [ ] 2.1.3 `loop_phases::run_plan_phase` 显式 `req.params.model.clear()` (用 Recording Provider 验证 req.params.model.empty())
  - [ ] 2.1.4 `loop_phases::run_execute_phase` 接受 DSLEngine ref + valid DSL → 返回 true + engine 累积子图
  - [ ] 2.1.5 `loop_phases::run_execute_phase` 当 DSL 解析失败 → 返回 false + execute_error_out 填充
  - [ ] 2.1.6 `loop_phases::run_verify_phase` 接受 ILLMProvider ref + goal + result_data → MockLLMProvider 返回 "yes" → 返回 true
  - [ ] 2.1.7 `loop_phases::run_verify_phase` MockLLMProvider 返回 "no" → 返回 false
  - [ ] 2.1.8 `loop_phases::run_verify_phase` MockLLMProvider 返回空 → 返回 false
  - [ ] 2.2 `tests/CMakeLists.txt` 注册 `add_catch_test(test_loop_phases ...)` + link `agenticdsl_core`
- [ ] 2.3 验证 RED: `cmake --build build --target test_loop_phases && ctest -R test_loop_phases` 编译失败 (loop_phases.h 不存在)

**TDD Step 2: GREEN — 最小实现**

- [ ] 2.4 新增 `include/agenticdsl/pdk/agent_loops/loop_phases.h` (3 自由函数声明 + namespace)
- [ ] 2.5 新增 `src/modules/agent_loops/loop_phases.cpp` (3 自由函数实现)
- [ ] 2.6 **关键**: 移植 `req.params.model.clear()` 从 `plan_execute_loop.h:225/289` 到 `loop_phases.cpp::run_plan_phase` + `run_verify_phase`, 含完整 "NOT redundant" 注释 (D1.inv.model)
- [ ] 2.7 `src/modules/CMakeLists.txt` 加 `agenticdsl_loop_phases.cpp` 到 `agenticdsl_core` 静态库源列表
- [ ] 2.8 验证 GREEN: `ctest -R test_loop_phases` 8 cases PASS

**TDD Step 3: REFACTOR — 消除重复**

- [ ] 2.9 提取 3 个 `req.params.model.clear()` 注释到 helper 内部 (DRY)
- [ ] 2.10 验证 GREEN 不退化: `ctest -R test_loop_phases` 仍 8 cases PASS

**TDD Step 4: ASSERT — 契约断言**

- [ ] 2.11 验证 Recording Provider 实际收到 `req.params.model.empty() == true` (per AGENTS.md Pattern #2)
- [ ] 2.12 验证 `loop_phases.cpp` 内 `req.params.model.clear()` 注释 grep 命中次数 = 2 (plan + verify)

**TDD Step 5: GATE — 全量回归**

- [ ] 2.13 `ctest --test-dir build -LE must_realllm` 263/263 PASS (零回归, helper 未被 PlanExecuteLoop 调用, 仅 const 副作用)
- [ ] 2.14 Commit: `feat(loop_phases): 抽 plan/verify/execute helper 自由函数 (Phase 0)` per AGENTS.md Pattern #4 atomic commit + [Reverse Indicator] 段

## 3. Phase 1: PlanExecuteLoop 薄壳化 (commit 2 of 5)

**TDD Step 1: REFACTOR (无需 RED, 现有测试已是契约)**

- [ ] 3.1 修改 `include/agenticdsl/pdk/agent_loops/plan_execute_loop.cpp::run()`:
  - [ ] 3.1.1 `plan_phase()` 私有方法 → 替换为直接调 `loop_phases::run_plan_phase(*llm, goal, ctx, token)`
  - [ ] 3.1.2 `execute_phase()` 私有方法 → 替换为直接调 `loop_phases::run_execute_phase(*engine_, *plan_output, exec_err)`
  - [ ] 3.1.3 `verify_phase()` 私有方法 → 替换为直接调 `loop_phases::run_verify_phase(*llm, goal, data, token)`
  - [ ] 3.1.4 `while(true)` retry 编排 + `retries_used` 计数 + `state_ = State::Retry` 全部保留 (D2.inv.retry)
  - [ ] 3.1.5 `engine_` 成员保留, 跨 retry 累积 `continue_with_generated_dsl` 调用同一 engine_ (D2.inv.engine)
- [ ] 3.2 验证: `plan_phase()` / `execute_phase()` / `verify_phase()` 私有方法**保留**在 .cpp 文件 (或直接 inline 到 run()) — 选择不破坏测试覆盖
- [ ] 3.3 验证: `result.message` 字符串字面量零变化 (must_realllm 断言):
  - [ ] `"PlanExecuteLoop: completed successfully"` (line 153 test_plan_execute_realllm.cpp)
  - [ ] `"PlanExecuteLoop: plan phase failed (empty LLM response)"` (line 150-152)
  - [ ] `"PlanExecuteLoop: execute phase failed: " + exec_err`
  - [ ] `"PlanExecuteLoop: verify failed after " + retries_used + " retries"`

**TDD Step 2: GATE — 全量回归**

- [ ] 3.4 `ctest --test-dir build -LE must_realllm` 263/263 PASS (零回归, PlanExecuteLoop 公开 API 零变化)
- [ ] 3.5 `tests/test_pdk_plan_execute.cpp` 5/5 PASS (状态机 mock 测试, 公开 API 零变化)
- [ ] 3.6 `tests/test_plan_execute_restart.cpp` 3/3 PASS (retry 状态机测试, 行为零变化)
- [ ] 3.7 Commit: `refactor(plan_execute): 内部委托 helper, 公开 API 不变 (Phase 1)` per AGENTS.md Pattern #4 atomic commit + [Reverse Indicator] 段

## 4. Phase 2: ReactLoop 薄壳化 + G1 注释 (commit 3 of 5)

- [ ] 4.1 修改 `include/agenticdsl/pdk/agent_loops/react_loop.h/.cpp::run_once()`:
  - [ ] 4.1.1 调用 `loop_phases::run_plan_phase` (Plan 阶段单轮简化版)
  - [ ] 4.1.2 `result.message` 字符串字面量零变化 (must_realllm R1-R3 断言)
  - [ ] 4.1.3 公开 API `run_once()` / `state()` 签名零变化
- [ ] 4.2 `include/agenticdsl/pdk/agent_macros.h:52` 加 ADR-0089 注释 (D3):
  ```cpp
  // ADR-0089 v1.3 amendment (2026-10-09): 委托 shared phase helpers (per ADR-0021 §3.2 amendment)
  // LoopDispatcher specializations 保留 ReactLoop/PlanExecuteLoop/ForkJoinLoop 引用,
  // 类变薄壳委托 helper (见 loop_phases.h), 公开 API 零变化 (G1 + test_pdk_macros 不动)
  ```
- [ ] 4.3 验证 G1 编译: `cmake --build build --target g1_coding_assistant -j$(nproc)` exit 0
- [ ] 4.4 验证 G1 symbols: `nm build/pdk/g1_coding_assistant/libG1CodingAssistant.so | grep -E "ReactLoop|DEFINE_AGENT"` 应命中
- [ ] 4.5 `ctest -R test_pdk_macros` PASS (DEFINE_AGENT 测试, 零修改)
- [ ] 4.6 Commit: `refactor(react_loop): 内部委托 helper + agent_macros.h 加 ADR-0089 注释 (Phase 2)` per AGENTS.md Pattern #4 atomic commit + [Reverse Indicator] 段

## 5. Phase 3: pdk_entry 新工具 (commit 4 of 5)

- [ ] 5.1 修改 `pdk/loop_agent/src/pdk_entry.cpp`:
  - [ ] 5.1.1 注册 `loop/run_plan` 工具函数, 调用 `loop_phases::run_plan_phase(*tls_parent_provider, goal, ctx)`
  - [ ] 5.1.2 注册 `loop/run_verify` 工具函数, 调用 `loop_phases::run_verify_phase(*tls_parent_provider, goal, result_data)`
  - [ ] 5.1.3 遵循 C1 工具规则 (D6 bus_ptr 边界): 不接收 bus_ptr, 不发射 `loop.turn.*` 事件
  - [ ] 5.1.4 `ApprovalPolicy::requires_approval_in_agent = true` (与现有 loop/run 一致)
- [ ] 5.2 验证: `cmake --build build --target loop_agent -j$(nproc)` exit 0
- [ ] 5.3 验证: `nm build/pdk/loop_agent/libLoopAgent.so | grep -E "loop/run_plan|loop/run_verify"` 应命中
- [ ] 5.4 **不强制改写 `lib/loop/*.agent.md`**: 现有 react/plan_execute/fork_join DSL 文件零修改, 新工具只是能力扩展
- [ ] 5.5 Commit: `feat(pdk_entry): 新增 loop/run_plan + loop/run_verify 工具注册 (Phase 3)` per AGENTS.md Pattern #4 atomic commit + [Reverse Indicator] 段

## 6. Phase 4: 文档同步 (commit 5 of 5)

- [ ] 6.1 改写 `pdk/loop_agent/README.md` "双循环架构 (C1 决策矩阵)" 段:
  - [ ] 6.1.1 删除 "统一重构触发条件: 任一循环实现出现第 3 个消费者" 段 (已满足, 已重构)
  - [ ] 6.1.2 新增 "Phase helper 共享" 段, 描述 `loop_phases.h` + 薄壳模式
  - [ ] 6.1.3 新增 "DSL 可选调用" 段, 描述 `loop/run_plan` / `loop/run_verify` 工具
- [ ] 6.2 更新 `docs/specs/architecture.md` L3 契约层 docMap:
  - [ ] 6.2.1 加入 `loop_phases::run_plan_phase` / `run_execute_phase` / `run_verify_phase` 到 L3 contract layer entry
  - [ ] 6.2.2 加入引用 ADR-0089 v1.3 amendment
- [ ] 6.3 更新 `docs/adr/adr-0021-pdk-design.md` §3.2:
  - [ ] 6.3.1 在 §3.2 末尾加 "**实施注记 (2026-10-09)**" 段, 描述 ADR-0089 v1.3 amendment
  - [ ] 6.3.2 加指回 ADR-0089 的链接
- [ ] 6.4 验证 `python3 tools/docs_drift_audit.py` 只报已知 drift (本 change 不新增)
- [ ] 6.5 Commit: `docs: pdk/loop_agent/README.md 双循环架构段改写 + specs/architecture.md L3 契约层 docMap 更新 (Phase 4)` per AGENTS.md Pattern #4 atomic commit + [Reverse Indicator] 段

## 7. 必过验证门 (Ship Gate)

### 7.1 阶段 1: 单元测试 + mock (必过, 快速)

- [ ] 7.1.1 `ctest --test-dir build -LE must_realllm --output-on-failure` (核心 tree, 期望 263/263 PASS, 100%)
- [ ] 7.1.2 `ctest --test-dir build/examples/pdk_chat_demo/tests -LE must_realllm --output-on-failure` (examples tree, 期望 33/33 PASS, 100%)
- [ ] 7.1.3 `ctest --test-dir build/examples/pdk_chat_demo_evolution/tests -LE must_realllm --output-on-failure` (evolution tree, 期望 9/9 PASS, 100%)

### 7.2 阶段 2: must_realllm 真实 LLM (消耗 token, 需 DEEPSEEK_API_KEY)

- [ ] 7.2.1 `ctest --test-dir build -L must_realllm --output-on-failure` (核心 tree, 期望 10/10 PASS 含 test_plan_execute_realllm 3 cases)
- [ ] 7.2.2 `ctest --test-dir build/examples/pdk_chat_demo/tests -L must_realllm --output-on-failure` (examples tree, 期望 4/4 PASS 含 test_e2e_real_llm ChatSession case)

### 7.3 阶段 4: ABI / G1 兼容

- [ ] 7.3.1 `cmake --build build --target g1_coding_assistant -j$(nproc)` exit 0
- [ ] 7.3.2 `nm build/pdk/g1_coding_assistant/libG1CodingAssistant.so | grep -E "ReactLoop|DEFINE_AGENT"` 命中
- [ ] 7.3.3 `cmake --build build --target loop_agent -j$(nproc)` exit 0
- [ ] 7.3.4 `nm build/pdk/loop_agent/libLoopAgent.so | grep -E "loop/run_plan|loop/run_verify"` 命中
- [ ] 7.3.5 `ctest -R test_adr_0087_step5_1_benchmark --output-on-failure` 4-worker 3.3× 加速数据不退化

### 7.4 阶段 5: 治理

- [ ] 7.4.1 `python3 tools/adr_lint.py 2>&1 | tail -3` 期望 "✓ 所有 ADR 通过 lint 检查" (新增 ADR-0089, ADR-0021 §3.2 修订注记)
- [ ] 7.4.2 `python3 tools/docs_drift_audit.py 2>&1 | grep SUMMARY` 期望 "SUMMARY: N DRIFT (已知 3 + 1), M WARNING, 本 change 不新增"
- [ ] 7.4.3 `bash -n scripts/sync-pdk.sh` 语法 PASS (若改 macro 注释需同步 sync-pdk.sh, 实际本次不动 macro, 仅加注释)
- [ ] 7.4.4 `git status --short` 干净或仅含本 change 范围

## 8. Oracle Post-Impl SHIP-with-fixes Review (per AGENTS.md Pattern #4)

- [ ] 8.1 派 Oracle 复评 (`task(subagent_type="oracle", run_in_background=true, ...)`), 收集 verdict (SHIP / SHIP-with-fixes / BLOCK)
- [ ] 8.2 应用 SHIP-with-fixes 修正 (不 amend baseline commit, 新增 atomic commit per Pattern #4)
- [ ] 8.3 派 Oracle 复核, 拿 APPROVE 才 ship
- [ ] 8.4 验证 5 字段 Reverse Indicator 在每个 commit message: new_up / old_down / failure_traces / ablation / context_ids

## 9. Archive (per rdd-builder P3)

- [ ] 9.1 5 atomic commits + SHIP-with-fixes fixes 全部在主分支
- [ ] 9.2 `git mv openspec/changes/consolidate-loop-phases-to-shared-helpers/ openspec/changes/archive/consolidate-loop-phases-to-shared-helpers/`
- [ ] 9.3 验证 archive 4 文件完整性 (per AGENTS.md Day-5 4-file integrity lesson):
  - [ ] `git ls-files openspec/changes/archive/consolidate-loop-phases-to-shared-helpers/ | wc -l` 期望 5 (README.md + proposal.md + design.md + tasks.md + specs/loop-phases-shared-helpers/spec.md)
- [ ] 9.4 AGENTS.md Recent Changes 登记 ship 条目 (含 5 字段 Reverse Indicator + dual-agent review sessions)

## 10. Non-goals (明确不做)

- ❌ 删除 `agent_loops/*.h` 公开头 (DEFINE_AGENT + G1 编译期断)
- ❌ 删除 `lib/loop/*.agent.md` DSL 文件 (ChatSession 生产路径)
- ❌ 扩展 DSL 引擎加循环节点 (超 scope, 需独立 ADR)
- ❌ 改造 `lib/loop/plan_execute.agent.md` 实现 retry 循环 (DAG 不可达)
- ❌ 修改 `ForkJoinLoop` 内部实现 (用户决策 D4)
- ❌ 迁移 must_realllm 测试到 DSL 路径 (C++ 类公开 API 不变)
- ❌ 替换 `g1_coding_assistant` 的 ReactLoop 消费者为 DSL (M3 兼容)