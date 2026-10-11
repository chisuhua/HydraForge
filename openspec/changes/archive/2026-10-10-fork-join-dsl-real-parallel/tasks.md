# Tasks: Fork/Join DSL real parallel semantics repair (path 2)

**STATUS**: 准备开始
**关联 change**: `openspec/changes/2026-10-10-fork-join-dsl-real-parallel/`
**supersede**: `archive/2026-10-10-fix-fork-join-agent-next-chain-blocked-by-pattern8-dual-agent-review/`
**前置**: commit `b3a83fc` (fix-fork-dsl-context-propagation Phase 5+6) — main HEAD

## Phase 0: 准备 (0.5d)

- [ ] 0.1 切 worktree `feat/fork-join-dsl-real-parallel` 隔离主工作树
- [ ] 0.2 在 worktree 跑 baseline ctest `-LE must_realllm` 确认 264/264 PASS (含 1 pre-existing flaky test_skill_interpreter)
- [ ] 0.3 读 `topo_scheduler.cpp` 192-250 + 711-740 行 (D1+D2 实施点)
- [ ] 0.4 读 `node_factory.cpp` 208-260 行 (D3 实施点)
- [ ] 0.5 读 `lib/loop/fork_join.agent.md` 全文确认 4 处 `next: [/main/end]` 精确位置

## Phase 1: RED (regression guards, 0.5d)

### 1.1 核心 tree 测试 (`tests/test_fork_join_dsl_real_parallel.cpp` 新增 binary)

- [ ] 1.1.1 Case 1 — parser 双 schema 兼容: 验证扁平 `branches: [{path}]` + 嵌套 `fork: {branches: []}` 都正确解析 (RED: 当前 parser 只支持嵌套 → Case 1 FAIL)
- [ ] 1.1.2 Case 2 — build_dag 防双执行: 验证 fork 的 branches 不在 ready_queue_ (RED: 当前无防护 → Case 2 FAIL)
- [ ] 1.1.3 Case 3 — routing-FORK 前移: 验证 FORK 节点不被 execute_node 调用 (RED: 当前已修, 此 case 验证维持)
- [ ] 1.1.4 Case 4 — routing-JOIN 前移: 验证 JOIN 节点不被 execute_node 调用, 不抛 logic_error (RED: 当前 JOIN 仍抛 logic_error → Case 4 FAIL)
- [ ] 1.1.5 Case 5 — update_successors on fork path: 验证 fork 处理后 fork.next 边在_degree 正确递减 (RED: 当前 fork 不调 update_successors → Case 5 FAIL)

### 1.2 Examples tree 测试 (`examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` 升级 + Test 4)

- [ ] 1.2.1 Test 4 新增: fork/join 真 e2e — `engine->get_tool_registry().call_tool("loop/run", ...)` loop_type=fork_join + 用真 DSL (临时 disable mock fallback 让 fork_join.agent.md 加载) + 断言 success=true + steps >= 6 + result 含 task_a/b/c 输出
- [ ] 1.2.2 更新 line 79-84 注释: 删除 "NodeExecutor::execute_fork 未实现" 陈旧断言 (本 change 改写语义)
- [ ] 1.2.3 RED 验证: 跑 Test 4 确认 FAIL (Unmet dependencies 错误 — 当前修复前必现)

## Phase 2: GREEN (6 个修复, 1.5d)

### 2.1 修复 BUG-2 (routing-JOIN 前移)

- [ ] 2.1.1 `src/modules/scheduler/topo_scheduler.cpp` line 192-204 后新增 JOIN 前移块 (与 FORK 对称, D1)
- [ ] 2.1.2 删除原 line 228-230 post-execute JOIN 检查 (死代码)
- [ ] 2.1.3 编译验证 + 跑 Case 3+4 确认 GREEN

### 2.2 修复 BUG-3 (fork path update_successors)

- [ ] 2.2.1 `src/modules/scheduler/topo_scheduler.cpp` line 195 后加 `handle_node_completion(state, NodeResult{true, ""}, found.node, found.path)` (D2)
- [ ] 2.2.2 编译验证 + 跑 Case 5 确认 GREEN

### 2.3 修复 BUG-4 (parser 双 schema 兼容)

- [ ] 2.3.1 `src/modules/parser/node_factory.cpp` make_fork (line 208-223) 替换为双 schema 支持 (D3 优先扁平, fallback 嵌套 + deprecated warn)
- [ ] 2.3.2 `src/modules/parser/node_factory.cpp` make_join (line 226-247) 替换为双 wait_for schema 支持 (同样模式)
- [ ] 2.3.3 编译验证 + 跑 Case 1 确认 GREEN

### 2.4 修复 BUG-5 (DSL next 链修复)

- [ ] 2.4.1 `lib/loop/fork_join.agent.md` line 26 task_a: `next: [/main/end]` → `next: [/main/join_results]`
- [ ] 2.4.2 `lib/loop/fork_join.agent.md` line 34 task_b: `next: [/main/end]` → `next: [/main/join_results]`
- [ ] 2.4.3 `lib/loop/fork_join.agent.md` line 42 task_c: `next: [/main/end]` → `next: [/main/join_results]`
- [ ] 2.4.4 保留 line 56 synthesize `next: [/main/end]` 不变 (synthesize→end 是正确设计)

### 2.5 修复 D4 (build_dag 防双执行)

- [ ] 2.5.1 `src/modules/scheduler/topo_scheduler.cpp` seed_initial_ready_queue (line 139-148) 加 ForkNode branches 跳过 (用 `dynamic_cast<const ForkNode*` 标记 branches 不 seed)
- [ ] 2.5.2 编译验证 + 跑 Case 2 确认 GREEN

### 2.6 Test 4 e2e 验证

- [ ] 2.6.1 跑 `examples/pdk_chat_demo/tests/test_loop_agent_fork_join` Test 4 确认 GREEN (success=true + steps >= 6)
- [ ] 2.6.2 跑 Test 1+2+3 确认无回归

## Phase 3: REGRESSION (全量 ctest, 0.5d)

- [ ] 3.1 核心 tree `ctest -LE must_realllm` 全 PASS (确认 BUG-2/3/4/5 修复无回归)
- [ ] 3.2 examples tree `ctest -LE must_realllm` examples/pdk_chat_demo/tests 全 PASS
- [ ] 3.3 examples tree `ctest` examples/pdk_chat_demo_evolution/tests 全 PASS
- [ ] 3.4 `tests/test_pdk_fork_join.cpp` 5 cases 全 PASS (C++ ForkJoinLoop 4-worker 行为零变化 — D8)
- [ ] 3.5 `tests/test_loop_agent_plan_execute_realllm.cpp` 3 cases 全 PASS (plan_execute 路径零影响)
- [ ] 3.6 ADR-0087 benchmark 复跑 ≥ 3× 数据点不退化 (若 CI 环境允许)
- [ ] 3.7 `python3 tools/adr_lint.py` exit 0 (无 ADR 漂移)
- [ ] 3.8 `python3 tools/docs_drift_audit.py` SUMMARY 已知 3 DRIFT (无新增)

## Phase 4: SHIP-WITH-FIXES (Oracle post-impl review + apply)

- [ ] 4.1 git add + 准备 commit (per AGENTS.md Pattern #4 atomicity, 不 amend baseline)
- [ ] 4.2 派 Oracle post-impl review (`task(subagent_type="oracle", run_in_background=true, ...)`) 拿 SHIP / SHIP-with-fixes / BLOCK 判定
- [ ] 4.3 若 SHIP-with-fixes: 按 Oracle 修正清单 (按严重度排序) 修复, 新增 atomic commit on worktree (不 amend)
- [ ] 4.4 若 BLOCK: 重新评估 + 修复根因
- [ ] 4.5 merge worktree → main (fast-forward + merge commit, `--no-ff` 保留 worktree history)
- [ ] 4.6 main working tree 重新 build + 跑 Phase 3 全量 ctest 验证 (worktree merge 不自动 sync main working tree)

## Phase 5: archive + tracker closure + AGENTS.md 同步

- [ ] 5.1 `git mv openspec/changes/2026-10-10-fork-join-dsl-real-parallel/ openspec/changes/archive/2026-10-10-fork-join-dsl-real-parallel/` (per AGENTS.md Day 5 4-file 完整性)
- [ ] 5.2 archive 后 `git ls-files` 验证 6 文件完整 (.openspec.yaml + proposal.md + design.md + tasks.md + specs/fork-join-dsl-real-parallel/spec.md)
- [ ] 5.3 更新 `.rddf/improvements/fork_join_dsl_migration.md`: BUG-2/3/4/5 状态 ❌ → ✅, 标记 tracker 完成
- [ ] 5.4 更新 `docs/specs/dsl.md` §5.2 schema 段: 补充 `branches: [{path}]` 扁平对象数组规范
- [ ] 5.5 更新 `AGENTS.md` Recent Changes: 本 change ship 登记 (含 5 commits hash + Reverse Indicator 5 字段)

## 5 字段 Reverse Indicator (per AGENTS.md Reverse Indicator Rule)

```
[Reverse Indicator]
+ new_up:
  - 4 个 fork/join 结构性 bug 修复 (routing-JOIN + update_successors + branches schema + DSL next 链)
  - parser 双 schema 兼容 (新扁平 / 旧嵌套 deprecated)
  - build_dag 防双执行机制
  - test_fork_join_dsl_real_parallel.cpp 新 binary (5 cases)
  - test_loop_agent_fork_join.cpp Test 4 新 e2e
- old_down: drop_ratio = 0% (零行为破坏, 仅修复 + 扩展 schema 兼容 + 测试断言修正)
failure_traces:
  - bug 1: routing-JOIN 物理死代码 (Oracle 静态验证 line 228-230)
  - bug 2: fork.next 死边 (Oracle 静态验证 line 192-196 continue 无 update_successors)
  - bug 3: branches schema 不匹配 (Oracle 静态验证 line 208-223 vs fork_join.agent.md:13-17)
  - bug 4: task_a/b/c next 指向 end (fork_join.agent.md:26,34,42)
ablation:
  - 3-segment baseline/mutated/rerun 全 mock-identical (mock 行为不变, 仅修复空转机制)
  - post-fix 期望: 同一 fixture 跑 5 次结果 100% 一致 (因 fork/join 调度语义确定性, mock canned response)
context_ids:
  - N/A (本 change 不涉及 ContextRequest schema, 仅 fork/join DAG 节点调度机制)
```