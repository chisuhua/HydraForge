# Spec: fork-join-dsl-completion-feedback (branch→main scheduler completion feedback)

**关联 OpenSpec change**: `openspec/changes/fork-join-dsl-completion-feedback/`
**supersede (部分)**: `archive/2026-10-10-fork-join-dsl-real-parallel/` Oracle Critical-1 Option (a) deferred
**目标**: 闭合 fork_join DSL 完整 e2e success=true 闭环, 解除 `.rddf/improvements/fork_join_dsl_migration.md` 最后 1 项 deferred 项. 实施量 ~17 行 `execute_fork_branches` 加 branch→main scheduler completion feedback + 1 个完整 e2e success=true 测试.

**2026-10-12 修正** (per Pattern #8 dual-agent review bg_05d4e505 + bg_6ebeb899):
- C1: D3 头文件路径从 `fork_join_loop.h` 修正为 `src/modules/scheduler/topo_scheduler.h:135` (TopoScheduler 内部 helper 实际声明位置)
- C2: 新增 `executed_.insert(last_node_path)` 修复 — `execute_single_branch` 内 branch 节点必须插入成员 `executed_`, 否则 `finalize_execution` (topo_scheduler.cpp:651-674) 计算 `all_nodes_ - executed_` → task_a/b/c 永远 unexecuted → `result.success = false` 即便 D1 正确实施
- C3: 测试断言从 `result.steps >= 6` 改用 `engine->get_last_traces().size() >= 5` (`ExecutionResult` 无 `steps` 字段 per `src/core/types/execution_result.h`)

---

## ADDED Requirements

### Requirement: TopoScheduler::execute_fork_branches MUST signal 主调度器每个 branch 完成

每个 branch 通过 `execute_single_branch` 完成 (即 branch 子图 ready_queue 耗尽) 后, MUST 显式调 `update_successors(last_node, last_node_path)` 让主调度器 `in_degree_[consumer]` 递减 + (若减到 0) push 到 `ready_queue_`. 当前 `execute_fork_branches` 仅维护本地 `branch_in_degree` 副本 (line 451-454), 主调度器 `in_degree_[join_results]` 永不为 0, join_results 永远不可达, 完整 fork_join DSL e2e 必现确定性 FAIL ("Unmet dependencies or cycles", **非** hang per Oracle m2 静态分析 2026-10-12).

#### Scenario: 3 branch 完成后 in_degree[join_results] 正确递减到 0 (主场景)

- **WHEN** 加载 `lib/loop/fork_join.agent.md` (fork + task_a/b/c + join_results) 经 `DSLEngine::from_markdown` + `MockLLMProvider`
- **AND** 主调度器 `build_dag` 完成, `in_degree_["/main/join_results"]` 初始 = 7 (fork.next +1 + task×3.next +3 + wait_for +3)
- **AND** 主循环 dispatch fork_tasks 节点 (per 2026-10-10 ship D2 fix `handle_node_completion` 推进 fork.next), `in_degree_["/main/join_results"]` 减 1 = 6
- **AND** `execute_fork_branches` 顺序执行 3 个 branch (task_a/b/c), 每个 branch 完成时**显式**调 `update_successors(last_node, last_node_path)` (per D1)
- **THEN** `in_degree_["/main/join_results"]` 减 3 (task_a.next + task_b.next + task_c.next) = 3
- **AND** wait_for 边递减 3 = 0 (task_a/b/c 在 wait_for_dependents_ 中)
- **AND** `join_results` 进入 `ready_queue_` 并被 dispatch
- **AND** `process_fork_join` 被调用, `ContextEngine::merge` 合并 3 个 branch 结果
- **AND** `engine->run(...)` 返回 `result.success == true` + `engine->get_last_traces().size() >= 5` (per Oracle C3 修正: `ExecutionResult` 无 `steps` 字段, 用 trace count 表面; start + task×3 + synthesize + end = 6 节点发 trace, fork_tasks/join_results 是调度节点不发 trace record, `>= 5` 留 mock 抖动 buffer)

#### Scenario: branch 节点 MUST 插入成员 executed_ (per Oracle C2 静态分析 2026-10-12)

- **WHEN** `execute_fork_branches` 主循环每 branch 完成, 调 `execute_single_branch` 返回 `last_node_path`
- **THEN** `executed_.insert(last_node_path)` MUST 在 `update_successors(last_node, last_node_path)` **同 guard 块内** 同步执行 (per design D1 line 47-48 实施点 — 在 caller 侧, 与 `update_successors` 共享 `if (last_node_it != node_map_.end())` 守卫)
- **AND** `finalize_execution` (topo_scheduler.cpp:651-674) 计算 `all_nodes_ - executed_` → branch 节点 (task_a/b/c, 单节点 branch 假设下 `last_node == branch 唯一节点`) 在 executed_ 集合中, **不**算 unexecuted
- **AND** `result.success == true` (而非 "Unmet dependencies or cycles. Unexecuted nodes: [task_a, task_b, task_c]")
- **AND** RED 验证 (tasks 2.1.7): 临时注释 `executed_.insert(last_node_path)` 单行 (D1 update_successors 保留) → Case 6 重新 FAIL, 验证 C2 修复的回归守卫
- **AND** 设计取舍 (per Oracle M6 advisory): 当前 DSL 3 branch 均为单节点 (lib/loop/fork_join.agent.md 实测), last_node == 唯一 branch 节点, fix 完备; 多节点 branch 场景下中间节点不入 executed_ 是已知限制 (per design R3 + Risks R3), 留 future change tracker

#### Scenario: branch 空执行防御 (R4 缓解, per Oracle M4 修正)

- **WHEN** branch 子图**正常**完整执行 (init_branch_state 必 seed branch_root, 不可达"空 ready queue" 状态)
- **THEN** `last_node_path` 非空 (因至少执行 1 个 branch 节点)
- **AND** `execute_fork_branches` 主循环通过 `if (last_node_it != node_map_.end())` 守卫覆盖防御性空路径
- **AND** 显式 **禁止** `if (last_node_path.empty()) continue;` 形式 (per Oracle M2: index 不增 → 死循环 + results 数量错位)
- **AND** `find_branch_start_node` (line 467) 失败时仍抛 `std::runtime_error` (现有行为, 实施时**不**改)

#### Scenario: branch 完成时 in_degree 不变负 (R2 缓解)

- **WHEN** 3 branch 顺序完成, 每 branch 调 `update_successors` 减 `in_degree_[join_results]`
- **THEN** 减后值**永不为负** (initial 7 → fork 后 6 → branch×3 后 0, 后续 if not 0 skip push)
- **AND** `update_successors` 现有实现 (line 802-813) `if (--in_degree_[next_path] == 0)` 守卫保证不 push 负数
- **AND** main loop dispatch_ready_nodes 看到的 `in_degree_` 终态全部 ≥ 0

### Requirement: TopoScheduler::execute_single_branch MUST 返回 (Context, NodePath last_node_path) 元组

为让 `execute_fork_branches` 获取 branch 最后执行节点路径 (用于 D1 调 `update_successors`), `execute_single_branch` MUST 返回 `std::pair<Context, NodePath>` (final_ctx + last_executed_node_path). 内部在 `branch_executed.insert(current_path)` 后记录 `last_node_path = current_path`, 函数末尾 `return {std::move(current_ctx), last_node_path}`.

#### Scenario: 单 branch 完整执行返回正确 last_node_path

- **WHEN** `execute_single_branch("/main/task_a", initial_ctx)` 执行 task_a tool_call (1 节点)
- **THEN** 返回 `pair{Context_final, "/main/task_a"}`
- **AND** `last_node_path == "/main/task_a"` (与 task_a 子图终点一致)
- **AND** 公开 API 改动仅 1 个内部 helper 函数 (1 个调用方 `execute_fork_branches`)

#### Scenario: 多节点 branch 返回最后节点路径

- **WHEN** `execute_single_branch("/main/complex_branch", initial_ctx)` 执行 3 节点序列 (a → b → c)
- **THEN** 返回 `pair{Context_final, "/main/c"}`
- **AND** `last_node_path == "/main/c"` (分支最后执行节点)
- **AND** 中间节点 (a, b) 不被错误地记录为 last

#### Scenario: branch 子图为空防御 (per Oracle M2 修正)

- **WHEN** `execute_single_branch` 正常完成 (init_branch_state 必 seed branch_root, **不可达** "空 ready queue" 状态 per design D2 理由段)
- **THEN** `last_node_path` 永**非**空 (因至少执行 1 个 branch 节点)
- **AND** 调用方 `execute_fork_branches` 通过 `if (last_node_it != node_map_.end())` 守卫覆盖防御性空路径 (per design R4 选定的 canonical guard, **非** `if (last_node_path.empty())` 形式)
- **AND** `find_branch_start_node` (line 467) 失败时仍抛 `std::runtime_error` (现有行为, 实施时**不**改)

### Requirement: lib/loop/fork_join.agent.md 完整 e2e success=true 测试 MUST 通过

`tests/test_fork_join_dsl_real_parallel.cpp` MUST 新增 Case 6 验证完整 fork_join DSL e2e success=true 路径, 覆盖:
- 加载 `lib/loop/fork_join.agent.md` 经 `DSLEngine::from_markdown` + `MockLLMProvider`
- 跑 `engine->run(...)` 返回 `ExecutionResult`
- 断言 `result.success == true` (而非 "Unmet dependencies or cycles" 确定性 FAIL)
- 断言 `engine->get_last_traces().size() >= 5` (per Oracle C3 修正: `ExecutionResult` 无 `steps` 字段; 起算 start + task×3 + synthesize + end = 6 节点发 trace, fork_tasks/join_results 是调度节点不发 trace record, `>= 5` 留 mock 抖动 buffer)
- 断言 `result.final_context` 聚合含 task_a/b/c 输出 (per ContextEngine::merge policy, **非** `result.response` — `response` 字段语义无 merge 聚合)

#### Scenario: Case 6 fork/join 完整 e2e PASS (主场景, per Oracle C2+C3 修正)

- **WHEN** `tests/test_fork_join_dsl_real_parallel.cpp:Case 6` 跑 `engine->run(DSLEngine::from_markdown(lib_loop_fork_join_md, mock))`
- **THEN** `result.success == true` (而非当前 RED 状态的确定性 FAIL "Unmet dependencies or cycles")
- **AND** `engine->get_last_traces().size() >= 5` (trace 记录数: start + task×3 + synthesize + end = 6 节点; fork_tasks/join_results 是调度节点不发 trace record; `>= 5` 留 mock 抖动 buffer)
- **AND** `result.final_context` 聚合含 task_a/b/c 三段输出 (per ContextEngine::merge 顺序合并, output_keys: result_a/result_b/result_c)
- **AND** RED 状态可观察 (per C2 静态分析 2026-10-12): 还原 D1 修复 (注释 `update_successors` 行) → Case 6 FAIL (in_degree 永不为 0 → join_results 永远不可达 → "Unmet dependencies or cycles")
- **AND** RED 状态可观察 (per C2 修复回归守卫): 还原 C2 修复 (注释 `executed_.insert` 单行, D1 保留) → Case 6 FAIL (in_degree 全清零但 finalize_execution 算 unexecuted 包含 task_a/b/c → "Unmet dependencies or cycles")
- **AND** GREEN 状态可观察: 应用 D1+C2 完整修复 → Case 6 PASS

#### Scenario: examples Test 2 锁住 examples 路径不退化 (per Oracle m1 修正)

- **WHEN** `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp:Test 2` (per 2026-10-10 ship commit `21bca13` 升级) 跑 `loop/run fork_join` + MockLLMProvider
- **THEN** `result.success == true` (新增, 此前仅断言 steps)
- **AND** `engine->get_last_traces().size() >= 5` (新增, 表面与 Case 6 一致)
- **AND** **保留** 现有 `output["steps"] >= 7` 断言 (per `run_ret.value("steps", 0)` 走 `loop/run` tool JSON 输出, 与 core tree `engine->get_last_traces()` 表面不同; mock LLM 工具调用多 trace record 让 loop/run steps 通常 ≥ 7, 数字差异源于 mock 工具调用, **不**降级到 6)
- **AND** examples tree `ctest -LE must_realllm` 零回归 (PDK loop_agent plugin 集成后语义)

---

## MODIFIED Requirements

### Requirement: TopoScheduler 内部 helper 函数签名变更 (execute_single_branch)

`TopoScheduler::execute_single_branch` (topo_scheduler.cpp:420) MUST 改返回值为 `std::pair<Context, NodePath>` (final_ctx + last_executed_node_path). 头文件 `src/modules/scheduler/topo_scheduler.h:135` MUST 同步 (per Oracle C1 修正, 实际声明位置). 加 `<utility>` include (std::pair 在 `<utility>` 标准库, 实施时验证是否被 transitive include 覆盖).

**不变更**:
- ❌ `include/agenticdsl/pdk/agent_loops/fork_join_loop.h` (C++ PDK `ForkJoinLoop` class 头, **无** `execute_single_branch` 声明 per Oracle C1 静态分析; per ADR-0089 v1.4 §D8.inv.forkjoin 零变化)
- ❌ `pdk/loop_agent/src/fork_join_loop.cpp` (PDK ForkJoinLoop 实现)
- ❌ `DomainWorkerPool` 4-worker 语义

#### Scenario: 唯一调用方解包正确 (R1 缓解)

- **WHEN** `execute_fork_branches` (topo_scheduler.cpp:405) 调 `execute_single_branch` 解包 `auto [ctx, path] = ...`
- **THEN** 编译通过, structured binding 解包 pair 正确
- **AND** `ctx` 类型 = `Context`, `path` 类型 = `NodePath`
- **AND** 无 std::pair 误用 (e.g. `result.value()` 误调 — per G1 case study lesson a, pair 无 .value())
- **AND** 无 `.get()` 误调 — `node_map_` 是 `unordered_map<NodePath, Node*>`, `last_node_it->second` 已是 `Node*` raw pointer, 直接传 (per Oracle M1 修正)

#### Scenario: 头文件同步路径正确 (D3 协同, per Oracle C1 修正)

- **WHEN** `src/modules/scheduler/topo_scheduler.h:135` 函数签名改 `std::pair<Context, NodePath> execute_single_branch(...)`
- **THEN** `agenticdsl_core` 静态库 rebuild (返回值类型变化, 链接符号 hash 变化)
- **AND** PDK `.so` (14/15) 链接 `libagenticdsl_core.a` 时自动重新链接 (in-tree build, 无需额外步骤)
- **AND** **不**触发 `sync-pdk.sh` (per Oracle m3 修正: `topo_scheduler.h` 是 **agenticdsl_core 内部头**, **非** PDK 契约层 per ADR-0021 §3.5 = `contract/*.h`; 仅 `contract/*.h` 变更需 sync-pdk)
- **AND** PDK 静态库 `POSITION_INDEPENDENT_CODE` 必须 ON (per AGENTS.md 模式 #6 Sprint 28 沉淀, 与本 change 无关)

---

## REMOVED Requirements

(none)

---

## 不变更需求 (RECAP, 与既有 spec 一致)

### Requirement: C++ ForkJoinLoop 行为零变化

ADR-0089 v1.4 §D8.inv.forkjoin: ForkJoinLoop class 不在本 change 范围, 维持现状. DomainWorkerPool 4-worker 不变, ADR-0087 benchmark 不退化.

#### Scenario: test_pdk_fork_join.cpp 5 cases 全 PASS

- **WHEN** 跑 `tests/test_pdk_fork_join.cpp` (C++ ForkJoinLoop 4-worker 路径)
- **THEN** 5 cases 全 PASS, 行为零变化
- **AND** ADR-0087 benchmark 复跑 ≥ 3× 数据点不退化 (post-merge verification)

### Requirement: DomainWorkerPool 4-worker 行为零变化

ADR-0020 §3.2: DomainWorkerPool 4 jthread pool 独立模块, 4-worker 行为是 ADR-0087 benchmark 承载对象. 本 change 不触 DomainWorkerPool.

#### Scenario: DomainWorkerPool 接口零变化

- **WHEN** 跑 `tests/test_domain_worker_pool.cpp` 12 cases (含 B.3 mock 100 串行 + B.4 mock RateLimited)
- **THEN** 12 cases 全 PASS, 行为零变化
- **AND** `DomainWorkerPool(4, bus)` 构造参数 4 不变
- **AND** 并发 4 任务在 4 jthread 上分发, 互不干扰

### Requirement: lib/loop/react.agent.md + lib/loop/plan_execute.agent.md 零变化

lib/loop/react.agent.md + lib/loop/plan_execute.agent.md 已 Phase 2 ship, per 2026-10-09 Pattern #12 v1.3 ship D2.inv.api 不变量. 本 change 仅在 fork/join 调度机制, 不动 React / PlanExecute 路径.

#### Scenario: test_loop_agent_plan_execute_realllm.cpp 3 cases 全 PASS

- **WHEN** 跑 `tests/test_loop_agent_plan_execute_realllm.cpp` (real LLM, must_realllm label)
- **THEN** 3 cases 全 PASS, plan_execute 路径零影响
- **AND** 24h cooling-off 链式 (per ADR-0089 v1.4 ship AC-12) 仍有效
- **AND** lib/loop/plan_execute.agent.md plan prompt + extract 容错 4 fence 仍 ship (per fork-join-dsl-migration partial ship commit `7e81c2f`)