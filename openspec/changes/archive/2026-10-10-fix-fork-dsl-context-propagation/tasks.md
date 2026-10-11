# Tasks: fix-fork-dsl-context-propagation (DSL fork 节点 working context 快照传递修复)

**OpenSpec change**: `openspec/changes/2026-10-10-fix-fork-dsl-context-propagation/`
**governance**: 完整 OpenSpec 流程 (24h cooling-off + Oracle dual-review pre-impl) — **不**用 cooling-off override (与 fork-join-dsl-migration 不同, 因涉及调度器核心改动, 严格 review 流程)

---

## Phase 1 — RED tests + Oracle dual-review prep (commit 1, 部分完成)

- [x] **Step 1 RED**: 新增 `tests/test_fork_dsl_context_propagation.cpp` (5 cases) + 升级 `test_loop_agent_fork_join.cpp` Test 2 严格断言 — commit `adb7910`
- [x] **Step 2 RED verify (2026-10-10)**: Test 2 在 `loop/run fork_join` 调下失败错误: **"Node execution failed: ForkNode/JoinNode reached NodeExecutor - scheduler routing bug"** (commit `adb7910`)
- [x] **Step 3 关键发现**: BUG-2 实际为**双 bug**:
  1. **routing bug** (`TopoScheduler::run_main_loop` line 209-221 顺序错误)
  2. **template context bug** (`handle_fork_node` + `start_fork_simulation` working context 快照传递, per fork-join-dsl-migration commit `469f495`)
- [x] **Step 4 RED 失败堆栈捕获**: routing bug 已捕获. template context bug 需更深 RED 测试 (Test 1 期望 FAIL 但实际因 routing bug 在前崩溃, 暴露不全). Phase 3 GREEN 修复 routing bug 后 Test 1 会进一步暴露 template context bug
- [ ] **Step 5**: 更新 proposal.md/design.md 反映双 bug (已 commit `adb7910` 后) + 派 Oracle dual-review bg 评审双 bug 修复方案

## Phase 2 — Oracle dual-review (post RED 失败堆栈, 阻塞 RED + 修复 commit)

- [ ] 派 Oracle bg dual-agent review:
  - 输入: RED 失败堆栈 + design.md D2 4 候选方案 + 当前 fork 路径代码 (`src/modules/scheduler/topo_scheduler.cpp:316-353` + `execution_session.cpp:155-280`)
  - 输出: 推荐修复方案 (A/B/C/D) + 风险评估 + 实施步骤
- [ ] 派 Metis bg dual-agent review (并行):
  - 输入: BUG-2 用户原问题"能否通过 help tool with loop/run fork_join agent.md" + 当前 fork/join behavior
  - 输出: 意图分析 + 歧义点 + AI 失败模式风险
- [ ] 收集 Oracle + Metis verdict, 更新 design.md D2 决策 (选 A/B/C/D 之一)
- [ ] 24h cooling-off (per OpenSpec governance): 用户 review + 决策

## Phase 3 — BUG-2 修复 (commit 2, atomic)

- [ ] **Step 1 GREEN**: 按 Oracle 推荐方案修 TopoScheduler/ExecutionSession/ContextEngine (~10-30 行, 方案 A/B/C/D 之一). 单 atomic commit
- [ ] **Step 2 GREEN verify**: 跑 `tests/test_fork_dsl_context_propagation.cpp` 5 cases — Test 1 GREEN (success=true + 无 Template render error). Test 2-5 不退化
- [ ] **Step 3**: 跑 `test_loop_agent_fork_join` Test 2 — 升级断言应 PASS
- [ ] **Step 4 REFACTOR**: 清理 (若有) 调试代码, 加必要注释 (per AGENTS.md Pattern #4 SHIP-with-fixes)
- [ ] **Step 5**: capture commit hash `127c83a` (或下一编号) 记录到 design.md D2 + spec.md scenarios

## Phase 4 — 回归验证 (commit 3)

- [ ] 跑 `ctest --test-dir build -LE must_realllm -j1` — 100% PASS (baseline 265/265 + 新增 5/5 + 升级 1/1)
- [ ] 跑 `ctest --test-dir build/examples/pdk_chat_demo/tests` — loop_agent 5/5 PASS (含升级后的 Test 2)
- [ ] 跑 `ctest --test-dir build -L must_realllm` (DEEPSEEK_API_KEY 注入) — 12/12 PASS (真 LLM 跑验, 含 test_loop_agent_plan_execute_realllm 3/3)
- [ ] ADR-0087 benchmark 复跑: 跑 `tests/test_adr_0087_step5_1_benchmark` 或等价 — 4-worker 3.3× speedup 不退化
- [ ] git status 干净 (无意外文件)

## Phase 5 — docs 同步 + archive (commit 4)

- [ ] 更新 `.rddf/improvements/fork_join_dsl_migration.md` BUG-2 状态: `⚠️ 未修复` → `✅ 已修复 (commit <hash>)`. Gate 条件移除
- [ ] 更新 AGENTS.md Pattern #12 follow-up 段: 记录本 change ship, BUG-2 修复完成
- [ ] 更新 ADR-0089 v1.4 amendment ship gate checklist (10/10 项已全部完成)
- [ ] AGENTS.md Recent Changes 登记本次 ship (含 fork-join-dsl-migration 续作 + 关键 commit hash)
- [ ] `git mv openspec/changes/2026-10-10-fix-fork-dsl-context-propagation/ openspec/changes/archive/` (5 文件 artifacts)

---

## Acceptance Criteria (per OpenSpec spec)

1. **BUG-2 修复** (D2): `lib/loop/fork_join.agent.md` 经 `loop/run loop_type=fork_join` + mock provider 成功执行, task_a/b/c 节点的 `{{user_input}}` template render 成功
2. **零回归**: `ctest -LE must_realllm` 100% PASS (baseline 265 + 新增 5 + 升级 1)
3. **C++ ForkJoinLoop 4-worker 行为不变** (D8.inv.forkjoin-class-unchanged): `tests/test_pdk_fork_join.cpp` 5 cases 全过
4. **fork DSL e2e 完整**: `test_loop_agent_fork_join` Test 2 升级后 success=true + 非 mock fallback + steps >= 3
5. **YIELD + fork 互不干扰**: YIELD 后 resume fork 仍正确
6. **ADR-0087 benchmark 不退化**: 4-worker 3.3× speedup 数据点不退化
7. **OpenSpec archive**: 5 文件 artifacts (proposal/design/spec/tasks/specs/*/spec.md) 完整, archive Day-5 4-file integrity

## Ship Gate (per ADR-0089 v1.4 §Reverse Indicator Rule 5 字段)

每个 commit message 含:
- `+ new_up`: 新增 capability (fork context propagation 真 LLM 闭环 + 调度器 working context 快照传递机制)
- `- old_down`: drop_ratio (本 change 预期 ≤ 5%, 实际 0% — 修复是 additive, 既有路径不变)
- `failure_traces`: BUG-2 修复前失败样本 → constraint/hook + ship commit hash
- `ablation`: 3 段对照 (fork_join 修复前/后 × 同任务/同 Harness × fork branch 拦截保留率)
- `context_ids`: N/A

## 治理风险

- **24h cooling-off 严格走**: 不 override (与 fork-join-dsl-migration precedent 不同, 因 BUG-2 修复涉及调度器核心)
- **Oracle dual-review pre-impl**: 必须 (修复方案选择需 dual review 深度证据)
- **典型 worktree**: 不使用 (per AGENTS.md, 改动小且调度器核心 + Oracle review 充分)