# Tasks: Fork/Join DSL completion feedback (full e2e success=true)

**STATUS**: 准备开始
**关联 change**: `openspec/changes/fork-join-dsl-completion-feedback/`
**supersede (部分)**: `archive/2026-10-10-fork-join-dsl-real-parallel/` Oracle Critical-1 Option (a) deferred
**前置**: commit `7bce855` (fork/join DSL 规范扩展 + AGENTS.md Recent Changes 同步) — main HEAD

## Phase 0: 准备 (0.25d)

- [ ] 0.1 切 worktree `feat/fork-join-dsl-completion-feedback` 隔离主工作树
- [ ] 0.2 在 worktree 跑 baseline ctest `-LE must_realllm` 确认 264/264 PASS (含 1 pre-existing flaky test_skill_interpreter)
- [ ] 0.3 读 `topo_scheduler.cpp` 389-460 行 (D1+D2 实施点)
- [ ] 0.4 读 `src/modules/scheduler/topo_scheduler.h` 117-180 行 (D3 头文件同步点, per Oracle C1 修正 — **非** `include/agenticdsl/pdk/agent_loops/fork_join_loop.h`, 该 PDK 头不动)
- [ ] 0.5 读 `tests/test_fork_join_dsl_real_parallel.cpp` 全文 (Case 1-5 已有, Case 6 新增)

## Phase 1: RED (regression guards, 0.25d)

### 1.1 核心 tree 测试升级 (`tests/test_fork_join_dsl_real_parallel.cpp` Case 6 新增)

- [ ] 1.1.1 Case 6 — 完整 fork/join e2e success=true path: 加载 `lib/loop/fork_join.agent.md` 经 `DSLEngine::from_markdown` + `MockLLMProvider` + 跑 `engine->run(...)` + 断言 `result.success == true` + `engine->get_last_traces().size() >= 5` (per Oracle C3 修正: `ExecutionResult` 无 `steps` 字段, 用 `get_last_traces` 表面; 起算节点 start + task×3 + synthesize + end = 6 节点, fork_tasks/join_results 是调度节点不发 trace record, `>= 5` 留 mock 抖动 buffer) + result.final_context 含 task_a/b/c 输出聚合
- [ ] 1.1.2 RED 验证: 跑 Case 6 确认 FAIL (确定性 FAIL, **非 hang** per Oracle m2 静态分析: in_degree[join_results] 永不为 0 → join_results 永远不可达 → main loop `ready_queue_` 空 → `dispatch_ready_nodes` 返回 monostate → break → `finalize_execution` 算 unexecuted 含 join_results + synthesize + end → 返回 `{false, "Unmet dependencies or cycles. Unexecuted nodes: ..."}`)
- [ ] 1.1.3 还原性 RED 验证: 临时在 `execute_fork_branches` 主循环注释掉未来 D1+C2 新增的 `update_successors` + `executed_.insert` 几行, 跑 Case 6 确认 FAIL (验证测试是 root cause 守卫而非 mock 假阳性)

### 1.2 Examples tree 测试升级 (`examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2 强化)

- [ ] 1.2.1 Test 2 (per 2026-10-10 ship commit `21bca13` 已升级 GREEN-only case) 强化严格 e2e 断言: `result.success == true` + `engine->get_last_traces().size() >= 5` (per Oracle C3 修正, `ExecutionResult` 无 `steps` 字段; **保留** Test 2 现有 `output["steps"] >= 7` 断言, 数字差异源于 mock 工具调用)
- [ ] 1.2.2 RED 验证: 跑 Test 2 确认 FAIL (同一 RED 根因)

## Phase 2: GREEN (~15 行修复, 0.25d)

### 2.1 修复 D1 (execute_fork_branches branch→main feedback) + C2 (executed_.insert)

- [ ] 2.1.1 `src/modules/scheduler/topo_scheduler.cpp` line 405 改 `auto [branch_final_ctx, last_node_path] = execute_single_branch(...)` (D2 解包 pair)
- [ ] 2.1.2 line 405-407 之间加 ~5 行: `auto last_node_it = node_map_.find(last_node_path); if (last_node_it != node_map_.end()) { update_successors(last_node_it->second, last_node_path); executed_.insert(last_node_path); }` (D1 调 update_successors + **C2 executed_.insert 修复 per Oracle 静态分析**)
  - 注: 删 `.get()` (per M1, `Node*` 已直接是 raw pointer, `unordered_map<NodePath, Node*>` value type)
  - 注: `executed_.insert(last_node_path)` **必须** 与 update_successors **同位置** 写入, 否则 finalize_execution 算 unexecuted 时 task_a/b/c 永远不在 executed_ → result.success = false (per C2 静态分析)
- [ ] 2.1.3 显式 **禁止** `if (last_node_path.empty()) continue;` 形式 (per M2: `current_fork_branch_index_` 不增 → 死循环 + results 数量错位). 仅保留 `find()` 守卫 (空路径 `node_map_.find("")` 返回 end() 自然跳过 update_successors + executed_.insert)
- [ ] 2.1.4 编译验证: `cmake --build build --target agenticdsl_core` exit 0
- [ ] 2.1.5 跑 Case 6 确认 GREEN (`result.success == true` + `engine->get_last_traces().size() >= 5`)
- [ ] 2.1.6 跑 Case 1-5 确认零回归 (parser schema + build_dag 防双执行 + routing 前移 + update_successors on fork path)
- [ ] 2.1.7 还原性 RED 二次验证: 临时注释 `executed_.insert(last_node_path)` 单行 (D1 update_successors 保留) → 跑 Case 6 确认 FAIL (验证 C2 修复的回归守卫)

### 2.2 修复 D2 (execute_single_branch 返回值扩展)

- [ ] 2.2.1 `src/modules/scheduler/topo_scheduler.cpp` line 420 改 `std::pair<Context, NodePath> execute_single_branch(...)` (函数签名)
- [ ] 2.2.2 line 427 后加 `NodePath last_node_path;` (内部跟踪变量)
- [ ] 2.2.3 line 445 后加 `last_node_path = current_path;` (每次 execute_node 成功后更新)
- [ ] 2.2.4 line 458 改 `return {std::move(current_ctx), last_node_path};` (返回 pair)
- [ ] 2.2.5 编译验证 + 跑 Case 1-6 确认全 GREEN

### 2.3 修复 D3 (头文件同步)

- [ ] 2.3.1 `src/modules/scheduler/topo_scheduler.h` line 1 加 `#include <utility>` (std::pair 标准库, 验证是否被 transitive include 覆盖; 现代 stdlib 通常通过 `<tuple>` `<utility>` 链入 topo_scheduler.h 既有 include, 可能无需新增)
- [ ] 2.3.2 line 135 改 `Context execute_single_branch(...)` → `std::pair<Context, NodePath> execute_single_branch(...)` (per C1 修正, 头文件实际声明位置)
- [ ] 2.3.3 **不**动 `include/agenticdsl/pdk/agent_loops/fork_join_loop.h` (per ADR-0089 v1.4 §D8.inv.forkjoin + C1 修正, 该文件是 C++ PDK `ForkJoinLoop` class 头, **无** `execute_single_branch` 声明)
- [ ] 2.3.4 编译验证: 整 project rebuild + 跑 Case 1-6 + examples Test 2 确认全 GREEN

### 2.4 Test 2 examples 路径验证

- [ ] 2.4.1 跑 `examples/pdk_chat_demo/tests/test_loop_agent_fork_join` Test 1+2+3 确认全 GREEN (Test 1 mock fallback, Test 2 e2e 严格断言, Test 3 DSL schema)
- [ ] 2.4.2 跑 `examples/pdk_chat_demo/tests/test_loop_agent_fork_join` 整 binary 确认 5 cases 全 PASS (per commit `21bca13` baseline 5 cases + 本 change 强化 Test 2)

## Phase 3: REGRESSION (全量 ctest, 0.5d)

- [ ] 3.1 核心 tree `ctest --test-dir build -LE must_realllm` 全 PASS (确认 D1+D2+D3 修复无回归)
- [ ] 3.2 examples tree `ctest --test-dir build/examples/pdk_chat_demo/tests -LE must_realllm` 全 PASS
- [ ] 3.3 examples tree `ctest --test-dir build/examples/pdk_chat_demo_evolution/tests -LE must_realllm` 全 PASS
- [ ] 3.4 `tests/test_pdk_fork_join.cpp` 5 cases 全 PASS (C++ ForkJoinLoop 4-worker 行为零变化 — D6)
- [ ] 3.5 `tests/test_loop_agent_plan_execute_realllm.cpp` 3 cases 全 PASS (plan_execute 路径零影响)
- [ ] 3.6 `tests/test_domain_worker_pool.cpp` 12 cases 全 PASS (DomainWorkerPool 4-worker 行为零变化 — D6)
- [ ] 3.7 ADR-0087 benchmark 复跑 ≥ 3× 数据点不退化 (若 CI 环境允许, post-merge verification)
- [ ] 3.8 `python3 tools/adr_lint.py` exit 0 (无 ADR 漂移)
- [ ] 3.9 `python3 tools/docs_drift_audit.py` SUMMARY 已知 3 DRIFT (无新增)

## Phase 4: SHIP-WITH-FIXES (Oracle post-impl review + apply)

- [ ] 4.1 git add + 准备 commit (per AGENTS.md Pattern #4 atomicity, 不 amend baseline)
- [ ] 4.2 派 Oracle post-impl review (`task(subagent_type="oracle", run_in_background=true, ...)`) 拿 SHIP / SHIP-with-fixes / BLOCK 判定
- [ ] 4.3 若 SHIP-with-fixes: 按 Oracle 修正清单 (按严重度排序) 修复, 新增 atomic commit on worktree (不 amend)
- [ ] 4.4 若 BLOCK: 重新评估 + 修复根因
- [ ] 4.5 merge worktree → main (fast-forward + merge commit, `--no-ff` 保留 worktree history)
- [ ] 4.6 main working tree 重新 build + 跑 Phase 3 全量 ctest 验证 (worktree merge 不自动 sync main working tree, per AGENTS.md Pattern #11 G1 lesson)

## Phase 5: archive + tracker closure + AGENTS.md 同步

- [ ] 5.1 `git mv openspec/changes/fork-join-dsl-completion-feedback/ openspec/changes/archive/2026-10-12-fork-join-dsl-completion-feedback/` (per AGENTS.md Day 5 4-file 完整性)
- [ ] 5.2 archive 后 `git ls-files openspec/changes/archive/2026-10-12-fork-join-dsl-completion-feedback/` 验证 5 文件完整 (.openspec.yaml + proposal.md + design.md + tasks.md + specs/fork-join-dsl-completion/spec.md)
- [ ] 5.3 更新 `.rddf/improvements/fork_join_dsl_migration.md`:
  - 顶部 status update: "e2e gap **前置闭合**" (per Oracle M5 修正措辞, 避免"tracker 完全闭合"语义错配)
  - Status 字段: 保持 "🔍 Future Change Tracker" (完整 ForkJoinLoop→DSL 工具迁移仍 pending 1 Sprint per ADR-0089 v1.4 D9)
  - "e2e success=true" 子段: 标记 "✅ Closed (per `2026-10-12-fork-join-dsl-completion-feedback` ship)"
  - 末尾维护规则: e2e gap closed ≠ tracker closed (D9 启动条件 v1.4 观察期 ≥1-2 月 + 用户决策 + benchmark 3.3× 退化, 仍按原触发)
- [ ] 5.4 更新 `docs/specs/dsl.md` §5.5 fork/join DSL 段: 补充 branch→main completion feedback 设计意图 (D1 5 行段说明 + D2 1 句返回 pair 解释)
- [ ] 5.5 更新 AGENTS.md Recent Changes: 本 change ship 登记 (含 commit hash + Reverse Indicator 5 字段 + Pattern #11 G1 lesson 提示)

## 5 字段 Reverse Indicator (per AGENTS.md Reverse Indicator Rule)

```
[Reverse Indicator]
+ new_up:
  - 完整 fork_join DSL e2e success=true 闭环 (per Oracle Critical-1 收尾)
  - execute_fork_branches 加 branch→main scheduler completion feedback (~10 行)
  - execute_single_branch 返回值扩展 pair<Context, NodePath> (~5 行)
  - **execute_single_branch 内 executed_.insert(last_node_path) 修复 (per Oracle C2 静态分析 2026-10-12, ~1 行) — 关键, 漏写即 result.success=false**
  - test_fork_join_dsl_real_parallel.cpp Case 6 新 e2e (锁住 e2e success=true 路径 + get_last_traces.size() >= 5 表面, per Oracle C3 修正)
  - examples test_loop_agent_fork_join.cpp Test 2 强化 (success == true + get_last_traces.size() >= 5 锁住, 保留原 output["steps"] >= 7 断言)
  - **Pre-implementation dual-agent review (per AGENTS.md Pattern #8)**: 3 Critical (C1 D3 错误路径 cross-validated + C2 executed_ Oracle-only + C3 steps 字段 Oracle-only) + 6 Major/Minor 全部 ship-with-fixes 关闭
  - fork_join_dsl_migration.md tracker **前置闭合** (e2e gap ✅, 完整 ForkJoinLoop→DSL 工具迁移仍 pending 1 Sprint per ADR-0089 v1.4 D9, per Oracle M5 修正)
- old_down: drop_ratio = 0% (零行为破坏, 仅修复 fork dispatch 调度机制 + executed_ 成员同步 + 测试断言升级)
failure_traces:
  - bug 1 (Oracle 2026-10-10 verdict 100%): execute_fork_branches 主循环不调 update_successors → in_degree_[join_results] 永不为 0 → join_results 永远不可达
  - bug 2 (Oracle C2 static analysis 2026-10-12): execute_single_branch 只插本地 branch_executed, 不写成员 executed_ → finalize_execution 算 unexecuted 包含 task_a/b/c → result.success = false 即便 in_degree 全清零
  - bug 3 (Oracle C3 static analysis 2026-10-12): 原提案断言 result.steps >= 6 编译失败, ExecutionResult 无 steps 字段
  - bug 4 (Metis C1 + Oracle C1 cross-validated): 原提案 D3 头文件路径 fork_join_loop.h:135 错误, 实际声明在 src/modules/scheduler/topo_scheduler.h:135
ablation:
  - 3-segment baseline/mutated/rerun 全 mock-identical (mock 行为不变, 仅修复 fork dispatch 调度 + executed_ 同步)
  - post-fix 期望: 同一 fixture 跑 5 次结果 100% 一致 (因 fork/join 调度语义确定性 + mock canned response)
context_ids:
  - N/A (本 change 不涉及 ContextRequest schema, 仅 fork/join DAG 节点分支完成反馈机制 + executed_ 成员同步)
```