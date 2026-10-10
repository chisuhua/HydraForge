# Tasks: fork-join-dsl-migration (fork/join + plan_execute DSL 路径完整化)

**OpenSpec change**: `openspec/changes/2026-10-10-fork-join-dsl-migration/`
**governance**: 完整 OpenSpec 流程 + cooling-off override (per ADR-0089 v1.4 HARD pause override precedent)

---

## Phase 0 — OpenSpec artifacts 立 (commit 1, 已完成)

- [x] `proposal.md` 起草 (Why / What Changes / Capabilities 三大段, 含 Oracle 评估引用)
- [x] `design.md` 起草 (D1 BUG-2 修复 / D2 BUG-3 修复 / D3 TDD 5-step / D4 文档同步 + 5 risks)
- [x] `specs/fork-join-dsl-completion/spec.md` 起草 (4 需求 + 8 scenarios)
- [x] `tasks.md` (本文档)
- [x] `.openspec.yaml` 由 openspec CLI 自动生成 (人工不写)

---

## Phase 1 — docs 同步 (commit 1)

- [ ] AGENTS.md Pattern #12 follow-up 段更新: 提及 v1.5 (fork_join_dsl_migration) ship, 移除 D9.future-change-tracker "未实施" 状态
- [ ] `.rddf/improvements/fork_join_dsl_migration.md` BUG-2/BUG-3 状态: `⚠️ 未修复` → `✅ 已修复 (commit <hash>)`. Gate 条件移除. 启动条件更新为 "用户决策启动" (剩余)
- [ ] `docs/adr/adr-0089-v1-4-amendment-soft-deprecate-cpp-agent-loops.md` ship gate checklist `[ ] must_realllm: NOT-RUN` 项关闭 → `[x]` (本会话已用 DEEPSEEK_API_KEY 跑验 11/11 PASS)

## Phase 2 — BUG-3 修复 (commit 2, TDD 5-step 简版)

- [ ] **Step 1 RED**: 改 `examples/pdk_chat_demo/tests/test_loop_agent_plan_execute_realllm.cpp` Test 1 断言为 success=true 严格模式 (当前是 "链路断言" 兼容模式). 测试预期 FAIL.
- [ ] **Step 2 RED verify**: 跑 `ctest --test-dir build/examples/pdk_chat_demo/tests -L must_realllm -R test_loop_agent_plan_execute_realllm --output-on-failure` 应 FAIL (real LLM plan → execute_plan 解析失败).
- [ ] **Step 3 GREEN**: 改 `lib/loop/plan_execute.agent.md` plan 节点 `prompt_template` (复用 `loop_phases::run_plan_phase` prompt 语义, D2).
- [ ] **Step 4 GREEN verify**: 再跑 must_realllm 测试, 应 PASS.
- [ ] **Step 5 REFACTOR**: 无 (单文件改动, 无重复逻辑)

## Phase 3 — BUG-2 真实状态文档化 (commit 3, 替代原修复计划)

> **2026-10-10 实施阶段关键发现**: 实测 `lib/loop/fork_join.agent.md` 错误是 `Template render error: variable 'user_input' not found`, **不是** `ForkNode/JoinNode reached NodeExecutor` routing bug. 真实 BUG 是 fork 节点**未把 working context 传递给各 branch 节点**, 修复涉及 `handle_fork_node` + `start_fork_simulation` ctx 快照机制改造, **超出本会话范围** (风险中-高, 调度器状态机 + ctx 生命周期).

- [x] (取消) 修 TopoScheduler main loop (~10 行) - 原假设错误
- [x] (取消) 新增 tests/test_topo_scheduler_fork_join.cpp - 因 routing bug 不成立, 测试会立即 PASS 不能证明 BUG 已修
- [x] **文档化真实根因**: 更新 proposal.md + design.md + spec.md (BUG-2 真实根因为 context 快照传递, 修复路径留待 fork_join_dsl_migration future change)
- [ ] 更新 `.rddf/improvements/fork_join_dsl_migration.md` BUG-2 状态: `⚠️ 未修复 (routing bug 假设)` → `⚠️ 未修复 (template context 快照 bug 实际根因)` + 修复路径 + 风险评估
- [ ] 更新 AGENTS.md Pattern #12 follow-up: 提及 fork_join_dsl_migration 观察期 BUG-2 仍 pending, 需后续 future change
- [ ] 确认 ADR-0087 benchmark 不退化: ctest -R `test_pdk_fork_join` (5 cases C++ ForkJoinLoop 单元测试) 全过## Phase 4 — test 升级 + 必须_realllm (commit 4)

- [ ] 更新 `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2 断言为完整 e2e (success=true + steps >= 3 + 无 error, 当前是 "链路断言")
- [ ] 更新 `examples/pdk_chat_demo/tests/test_loop_agent_plan_execute_realllm.cpp` Test 1 断言为完整 e2e (success=true + steps >= 4 + 无 error)
- [ ] 跑 `ctest --test-dir build/examples/pdk_chat_demo/tests -R loop_agent --output-on-failure` 应 PASS (含新断言 + Test 2/3 不变)
- [ ] 跑 `ctest --test-dir build -L must_realllm` (DEEPSEEK_API_KEY 注入) 应 PASS 11/11 + 新增 must_realllm 测试

## Phase 5 — 全量回归 + ship gate (commit 5)

- [ ] 跑 `cmake --build build` exit 0
- [ ] 跑 `ctest --test-dir build -LE must_realllm -j1` 100% PASS (零回归, baseline 264/264 + 新增测试)
- [ ] 跑 `ctest --test-dir build -L must_realllm` (DEEPSEEK_API_KEY) 11/11 PASS + 新增测试 PASS
- [ ] 跑 `ctest --test-dir build/examples/pdk_chat_demo/tests` 全 PASS (loop_agent 5/5 + 新增 0/0 回归)
- [ ] ADR-0087 benchmark regression check: 跑 `tests/test_adr_0087_step5_1_benchmark` 或等价 — 4-worker 3.3× speedup 不退化 (D1.inv.forkjoin-class-unchanged 验证)
- [ ] `git status` 干净 (无意外文件)

## Phase 6 — archive + AGENTS.md Recent Changes (commit 5/6)

- [ ] `git mv openspec/changes/2026-10-10-fork-join-dsl-migration/ openspec/changes/archive/`
- [ ] AGENTS.md Recent Changes 顶部插入新 entry: fork_join_dsl_migration ship + 关键 commit hash + ship gate 验证 + Reverse Indicator 5 字段
- [ ] AGENTS.md Pattern #12 follow-up: v1.5 已 ship 描述

---

## Acceptance Criteria (per OpenSpec spec)

1. **BUG-2 修复** (D1.inv.execute-order): `TopoScheduler::run_main_loop` 在 `execute_node` 之前路由 FORK/JOIN, NodeExecutor safety net 永不触发 (正常路径)
2. **BUG-3 修复** (D2.inv.helper-align): `lib/loop/plan_execute.agent.md` plan prompt 与 `loop_phases::run_plan_phase` 单源对齐
3. **零回归**: `tests/test_pdk_fork_join.cpp` 5 cases + `tests/test_scheduler.cpp` 7 cases + `tests/test_loop_phases.cpp` 8 cases 全 PASS
4. **真 LLM 闭环**: `test_loop_agent_plan_execute_realllm` 11/11 PASS + `test_react_loop_real_llm` 7/7 PASS + `test_loop_agent_autonomous` real LLM 2/2 PASS
5. **新测试全绿**: `tests/test_topo_scheduler_fork_join.cpp` 5 cases + `examples/pdk_chat_demo/tests/test_loop_agent_fork_join` Test 2 升级 + `test_loop_agent_plan_execute_realllm` Test 1 升级
6. **ADR-0087 benchmark 不退化**: C++ ForkJoinLoop 4-worker 3.3× speedup 数据点不退化
7. **OpenSpec archive**: 5 文件 artifacts (proposal/design/spec/tasks/specs/*) 完整, archive Day-5 4-file integrity (per AGENTS.md §Reverse Indicator)

## Ship gate checklist (per ADR-0089 v1.4 §Reverse Indicator Rule 5 字段强制)

每个 commit message 含:
- `+ new_up`: 新增 capability
- `- old_down`: drop_ratio (本 change 预期 ≤ 5%, 实际 0% — 修复是 additive)
- `failure_traces`: 失败样本 → constraint/hook/event + ship commit hash
- `ablation`: 3 段对照 (新/旧 × 同任务/同 Harness × 拦截保留率)
- `context_ids`: N/A (本 change 不改 context schema)