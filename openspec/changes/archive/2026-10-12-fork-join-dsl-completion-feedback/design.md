# Design: Fork/Join DSL completion feedback (full e2e success=true)

## 概述

本设计解决 2026-10-10-fork-join-dsl-real-parallel ship 后遗留的**唯一** e2e 闭环缺口: `execute_fork_branches` 内 branch 完成未反馈主调度器, `in_degree_[join_results]` 永不为 0, join_results 永远不可达. Oracle `ses_ed35f7205ffeCm6OlJTBrbVhtG` Critical-1 Option (a) deferred, 现以独立 change 闭合. 总实施量 ~17 行 + 1 个完整 e2e 测试.

**2026-10-12 修正** (per Pattern #8 dual-agent review bg_05d4e505 + bg_6ebeb899):
- **C1** (cross-validated 🔴): D3 头文件路径从 `fork_join_loop.h` (C++ ForkJoinLoop PDK class) 修正为 `src/modules/scheduler/topo_scheduler.h:135` (TopoScheduler 内部 helper 实际声明位置)
- **C2** (Oracle 🔴): 新增 `executed_.insert` 修复 — `execute_single_branch` 内每个 branch 节点完成时**必须**插入成员 `executed_`, 否则 `finalize_execution` (topo_scheduler.cpp:651-674) 计算 `all_nodes_ - executed_` → task_a/b/c 永远 unexecuted → `result.success = false` 即便 D1+D2 正确实施
- **C3** (Oracle 🔴): 测试断言 `result.steps >= 6` 编译失败 (`ExecutionResult` 无 `steps` 字段 per `src/core/types/execution_result.h`), 改用 `engine->get_last_traces().size() >= 5`
- **M1** (cross-validated 🟠): D1 代码片段 `last_node_it->second.get()` 编译失败 (`node_map_` 是 `unordered_map<NodePath, Node*>`, 已是 `Node*` 无 `.get()`), 删 `.get()`
- **M2** (Oracle 🟠): R4 缓解 `if (last_node_path.empty()) continue;` 形式 (tasks.md:2.1.3 原提案) = 死循环 (index 不增) + results 数量错位, **禁止**; 只保留 `find()` 守卫
- **M4** (Oracle 🟠): Spec Scenario 2 "不抛 std::runtime_error" 与 `find_branch_start_node` (line 467) 实际抛 runtime_error 矛盾, 改 Scenario 措辞
- **M6** (Oracle 🟠): "last-executed-node-as-exit-node" 假设对当前单节点 branch 偶然过, 多节点 branch exit-edge 不在最后节点则漏 decrement — D1 实施时加 DEBUG assert 守卫

## D1: branch→main completion feedback 实施位置

**决策**: 在 `execute_fork_branches` (topo_scheduler.cpp:389-419) 主循环 `execute_single_branch` 返回后, **立即**调 `update_successors(last_node, last_node_path)`, 让 consumer (join_results) in_degree 正确递减 + (如果 in_degree 减到 0) push 到 `ready_queue_`.

**实施**:
```cpp
void TopoScheduler::execute_fork_branches() {
    if (!is_executing_fork_branches_ || current_fork_branches_.empty()) return;

    const Context* fork_snapshot = session_.get_context_engine().get_snapshot(current_fork_node_path_.value());
    if (!fork_snapshot) {
        throw std::runtime_error("Snapshot for fork node not found: " + current_fork_node_path_.value());
    }

    while (current_fork_branch_index_ < current_fork_branches_.size()) {
        const NodePath& branch_path = current_fork_branches_[current_fork_branch_index_];
        LOG_DEBUG("Executing fork branch: " << branch_path);

        Context branch_initial_ctx = *fork_snapshot;

        try {
            // D2: 返回值从 Context 扩展为 pair<Context, NodePath>
            auto [branch_final_ctx, last_node_path] = execute_single_branch(branch_path, branch_initial_ctx);
            
            // D1: branch→main completion feedback — 用 last_node_path 在主调度器推进 consumer
            auto last_node_it = node_map_.find(last_node_path);
            if (last_node_it != node_map_.end()) {
                // 修复 M1: node_map_ 是 unordered_map<NodePath, Node*>, .second 已是 Node*, 无 .get()
                update_successors(last_node_it->second, last_node_path);
                // 修复 C2: branch 节点必须插入成员 executed_, 否则 finalize_execution
                //   (topo_scheduler.cpp:651-674) 计算 unexecuted 时把 task_a/b/c 误判未执行
                //   → result.success = false 即便 in_degree_ 全部清零
                executed_.insert(last_node_path);
            }
            
            current_fork_branch_results_.push_back(std::move(branch_final_ctx));
            current_fork_branch_index_++;
        } catch (const HardEndException& e) {
            throw;
        }

        LOG_DEBUG("Branch " << branch_path << " completed. Result stored. Branch " << current_fork_branch_index_ << " / " << current_fork_branches_.size() << " done.");
    }

    LOG_DEBUG("All fork branches completed. Ready for join.");
}
```

**理由**:
- `update_successors` (line 802-813) 已是 TopoScheduler 既有 helper, 处理 `node->next` 边的 in_degree 递减 + push ready_queue. 复用保证一致性.
- 实施点在 branch 完成**之后**, 是真实"完成信号", 时序与 main loop `handle_node_completion` 对称 (line 268 + line 238).
- 位置在 `execute_fork_branches` 主循环内, 不污染 `execute_single_branch` 的 branch-local in_degree 副本语义 (line 451-454 仍维护本地副本用于分支内节点调度).
- **executed_.insert(last_node_path) 与 update_successors 同步** (per C2 修复): finalize_execution (line 651-674) 通过 `all_nodes_ - executed_` 算 unexecuted → branch 节点必须写入成员 `executed_` 才能让 `result.success = true`.

**反模式**:
- 在 `execute_single_branch` 内部直接调主调度器 `update_successors` (line 451-454 之后) — 破坏分支隔离, branch-local 状态污染主调度器.
- 跳过 `update_successors` 独立实现 (直接 decrement `in_degree_[join_results]`) — 失去 ready_queue 自动 push, 漏掉 wait_for_dependents_ 推进 (line 808-812).
- **跳过 `executed_.insert(last_node_path)`** (per C2 静态分析) — 即便 in_degree_ 全部清零, finalize_execution 仍把 task_a/b/c 算 unexecuted → `result.success = false`. 这是 C1/C2 之外 D1 实施时**最容易漏的 1 行**.

## D2: execute_single_branch 返回值扩展 (Context → pair<Context, NodePath>)

**决策**: `execute_single_branch` 返回值从 `Context` 扩展为 `std::pair<Context, NodePath>` (final_ctx + last_executed_node_path). 内部在 `branch_executed.insert(current_path)` (line 445) 之后记录 `last_node_path = current_path`, 函数末尾 `return {std::move(current_ctx), last_node_path}`.

**实施**:
```cpp
std::pair<Context, NodePath> TopoScheduler::execute_single_branch(
    const NodePath& branch_path, const Context& initial_context) {
    NodePath start_node_path = find_branch_start_node(branch_path);
    LOG_DEBUG("Found start node for branch " << branch_path << ": " << start_node_path);

    auto [branch_ready_queue, branch_executed, branch_in_degree, branch_reverse_edges] =
        init_branch_state(branch_path);

    Context current_ctx = initial_context;
    NodePath last_node_path;  // D2: 跟踪最后执行的节点路径

    while (!branch_ready_queue.empty()) {
        NodePath current_path = branch_ready_queue.front();
        branch_ready_queue.pop();

        if (branch_executed.count(current_path) > 0) continue;

        auto node_it = node_map_.find(current_path);
        if (node_it == node_map_.end()) {
            throw std::runtime_error("Node not found in map during branch execution: " + current_path);
        }
        Node* node = node_it->second;

        auto session_result = session_.execute_node(node, current_ctx);
        if (!session_result.success) {
            throw std::runtime_error("Branch execution failed at " + current_path + ": " + session_result.message);
        }
        current_ctx = std::move(session_result.new_context);
        branch_executed.insert(current_path);
        last_node_path = current_path;  // D2: 记录最后执行的节点

        if (process_branch_end_node(node, current_path, branch_path, current_ctx)) {
            break;
        }

        for (const auto& next_path : node->next) {
            if (node_map_.count(next_path) > 0 && --branch_in_degree[next_path] == 0) {
                branch_ready_queue.push(next_path);
            }
        }
    }

    return {std::move(current_ctx), last_node_path};
}
```

**理由**:
- 公开 API 改动: 仅 1 个内部 helper 函数 (1 个调用方 `execute_fork_branches`), 影响面 0. 头文件同步 1 行 (`src/modules/scheduler/topo_scheduler.h:135`, per Oracle C1 修正 — **非** `fork_join_loop.h`).
- `last_node_path` 通过"循环末尾跟踪"自然获取, 不需额外查询 `branch_executed` 最大值或类似 O(N) 操作.
- `last_node_path` 可能在分支**未走任何节点** (start_node 缺失) 时为空, 但 `execute_fork_branches` 调用前已经 `is_executing_fork_branches_ = true` + `current_fork_branches_` 非空, 加 `find_branch_start_node` 必然非空, 防御性 `if (last_node_it != node_map_.end())` 守卫缺位路径.

**反模式**:
- 通过 `current_fork_branch_results_` 反查 last_node (新依赖 state pollution)
- 通过 `branch_executed` unordered_set 大小反查 last_node (O(N) 误用)
- 暴露 `branch_executed_` 给 `execute_fork_branches` (破坏封装, 扩大改动)

## D3: 头文件同步 (1 行)

**决策**: `src/modules/scheduler/topo_scheduler.h` (line 135) `Context execute_single_branch(...)` 签名同步改 `std::pair<Context, NodePath> execute_single_branch(...)`. 加 `<utility>` include (std::pair). 验证: 实际实施时检查 `<utility>` 是否被 transitive include 已覆盖 (现代 stdlib 通常通过 `<tuple>` `<utility>` 链入), 若重复 include 编译 warning `-Winclude-sort` 可加 pragma 抑制.

**理由**: 1 个函数签名 + 0~1 个头 include. `topo_scheduler.h` 是 **agenticdsl_core 内部头** (非 PDK 契约层 per ADR-0021 §3.5 = `contract/*.h`), 修改不触发 `sync-pdk.sh`. ABI 影响: PDK .so 链接 `libagenticdsl_core.a` 时符号 hash 变化需 rebuild — 已在 14/15 PDK .so rebuild 验证机制内.

**重要不变项** (per ADR-0089 v1.4 §D8.inv.forkjoin):
- ❌ 不动 `include/agenticdsl/pdk/agent_loops/fork_join_loop.h` (C++ PDK `ForkJoinLoop` class)
- ❌ 不动 `pdk/loop_agent/src/fork_join_loop.cpp` (PDK ForkJoinLoop 实现)
- ❌ 不动 `DomainWorkerPool` 4-worker 语义 (per ADR-0020 §3.2 + ADR-0087 benchmark)

**反模式**:
- 用 struct 返回 (over-engineering 2 字段, pair 已足够)
- 编辑 `include/agenticdsl/pdk/agent_loops/fork_join_loop.h` (违反 D8.inv.forkjoin + 改错文件位置 per C1)

## D4: 测试策略 — TDD 5-step 双层守卫

**决策**: 升级 `tests/test_fork_join_dsl_real_parallel.cpp` (per commit `21bca13` 已 ship) 加 Case 6, 锁住完整 e2e success=true 路径. **不**新建 binary, 不打散测试组织.

**核心 tree 测试** (`tests/test_fork_join_dsl_real_parallel.cpp` Case 6 新增):
```cpp
// ============================================================
// Case 6 — branch→main completion feedback (Oracle Critical-1 fix)
// ============================================================
// RED (pre-fix): execute_fork_branches 不调 update_successors
//                in_degree[join_results] 永不为 0, join_results 永远不可达
//                e2e 必现确定性 FAIL ("Unmet dependencies or cycles" — 非 hang)
// GREEN (post-fix D1+D2+C2): 每 branch 完成 decrement consumer in_degree
//                   + executed_.insert 修复让 finalize_execution 不再误判
//                   join_results 终可入 ready_queue_, e2e success=true
//
// 验证策略: register_node 完整 fork_join DAG → 触发 build_dag → execute
//           断言 result.success == true
//           断言 engine->get_last_traces().size() >= 5
//           断言 result.final_context 含 task_a/b/c 输出聚合
//
// RED 验证 2 步 (per Pattern #1 步骤 3, 防 mock 假阳性):
//   Step 1: 跑 Case 6 确认 FAIL (in_degree 永不为 0 → join_results 永远不可达)
//   Step 2: 临时还原 D1+C2 (注释 update_successors + executed_.insert) → Case 6 重新 FAIL
TEST_CASE("Fork/join full e2e success=true path completes join_results dispatch",
          "[scheduler][fork-join-dsl-completion-feedback][e2e]") {
    // 加载 lib/loop/fork_join.agent.md (完整 fork+3branches+join+synth+end)
    // DSLEngine::from_markdown + MockLLMProvider
    // 跑 engine->run(...)
    // REQUIRE(result.success == true)
    // REQUIRE(engine->get_last_traces().size() >= 5)  // start + task×3 + synthesize + end = 6, fork_tasks/join_results 是调度节点不发 trace, >=5 留 buffer
    // REQUIRE(result.final_context 含 result_a/result_b/result_c 聚合)
}
```

**Examples tree 测试** (`examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2 强化):
- 升级已 ship GREEN-only Test 2 (per commit `21bca13`) 严格 e2e 断言 (`success == true` + `engine->get_last_traces().size() >= 5`)
- 锁住 examples 路径不退化 (PDK loop_agent plugin 集成后语义)
- **保留** Test 2 现有 `output["steps"] >= 7` 断言 (per `run_ret.value("steps", 0)` 表面, 走 `loop/run` tool JSON 输出, 与 core tree `engine->get_last_traces()` 表面不同) — 数字差异源于 mock LLM 工具调用多 trace record, **不**降级到 6

**反模式**:
- 拆出独立 test binary (小改动 1 个 file 就够, 拆 binary 制造 cmake link complexity)
- 仅 mock test 守门 (无 e2e success=true 真实断言 = 测试通过但 e2e 仍不可达)
- 必须_realllm test (本期 mock 完成 e2e, 真 LLM 留 follow-up per pattern)
- 断言 `result.steps` (`ExecutionResult` 无此字段 per C3, 编译失败)
- 仅断言 `result.success == true` 不验 `executed_` 修复 (C2: 仅 D1+D2 修完仍 FAIL)

## D5: 反向兼容性 (与 ADR-0089 v1.4 不变量一致)

**决策**: 不破坏任何现有 example / fixture / test binary / benchmark.

**清单**:
- `tests/test_pdk_fork_join.cpp` 5 cases 零回归 (C++ ForkJoinLoop 4-worker 行为不变)
- `examples/pdk_chat_demo/dsl/fork_join_example.agent.md` 零变化 (走 `type: fork_join` C++ ForkJoinLoop 路径)
- `examples/pdk_chat_demo/tests/fixtures/fork_join_golden*.agent.md` 零变化 (不同 schema)
- ADR-0087 benchmark 4-worker 3.3× speedup 不退化 (本 change 不触 DomainWorkerPool)
- `lib/loop/react.agent.md` + `lib/loop/plan_execute.agent.md` 零变化 (Phase 2 ship 不变量)
- `extract_agenticdsl_block` 4 fence 容错 零变化
- `loop_phases.h` 3 函数 零变化 (per Pattern #12 v1.3 ship D1.inv.model)
- `tests/test_fork_join_dsl_real_parallel.cpp` Case 1-5 零回归 (per commit `21bca13`)

**验证**: 全量 ctest `-LE must_realllm` 零回归 (per AGENTS.md Pattern #11 ship gate)

## D6: 不变更 ADR-0089 v1.4 ForkJoinLoop 范围

**决策**: 本 change 不触 C++ `ForkJoinLoop` / `DomainWorkerPool` / ADR-0087 benchmark.

**理由**:
- ADR-0089 v1.4 §D8.inv.forkjoin: ForkJoinLoop 维持现状, `DomainWorkerPool` 4-worker 是 ADR-0087 资产承载对象
- 本 change 只改 TopoScheduler 调度逻辑 (`execute_fork_branches` + `execute_single_branch` 函数体内), 不动 ForkJoinLoop / DomainWorkerPool
- 完整 fork-join-dsl-migration (ForkJoinLoop 内部逻辑迁出到 DSL `loop/run_fork_join` 工具) 仍是 future change tracker (`.rddf/improvements/fork_join_dsl_migration.md`), 等 v1.4 观察期满 1-2 月 + 用户决策启动

**验证**: test_pdk_fork_join.cpp 5 cases PASS + ADR-0087 benchmark 复跑 ≥ 3× 数据点不退化 (post-merge verification)

## Risks

| ID | 风险 | 缓解 |
|----|------|------|
| R1 | `execute_single_branch` 返回值扩展破坏唯一调用方 `execute_fork_branches` (per G1 case study lesson a: Result<T,E> 解包) | 显式 `auto [ctx, path] = ...` 解包 pair + 编译期静态检查 + focused ctest |
| R1.5 | **executed_.insert 漏写** (per C2 静态分析) — 即便 D1+D2 正确实施, finalize_execution 仍因 task_a/b/c ∉ executed_ 判定 `success=false` | Case 6 断言 `result.success == true` (单一断言) + 还原性 RED 验证 (Step 2: 注释 executed_.insert → Case 6 重新 FAIL) |
| R2 | `update_successors` 在 `execute_fork_branches` 主循环中调可能与 main loop `handle_node_completion` 重复推进 in_degree (double-decrement) | Case 6 断言 `result.success == true` + 还原性 RED 验证 + `engine->get_last_traces().size() >= 5` (per mock-trace 实测留 buffer) |
| R3 | branch 内最后节点不是 consumer (e.g. synthesize 节点 next 不指向 join_results), update_successors 推进错的 in_degree | 锁住: last_node 必须是 branch 子图终点 (find_branch_start_node + branch_in_degree 本地副本) + DEBUG assert 守卫 |
| R4 | `last_node_path` 在分支空执行时为空, `update_successors` 调不到 `node_map_` | **`if (last_node_it != node_map_.end())` 防御性** (足够). **禁止** `if (last_node_path.empty()) continue;` 形式 (per M2: index 不增 → 死循环 + results 数量错位) |
| R5 | `in_degree_[join_results]` 减到 0 后 push ready_queue_, 但 main loop 当前轮次可能已 dispatch 完, 下轮才拿到 — 时序是否正确 | per design D1, `update_successors` 直接 push ready_queue_ (非延迟到下轮), main loop `while (!ready_queue_.empty() || ...)` 自然重入 |
| R6 | **M1 `.get()` 编译错误** — design 原代码片段 `last_node_it->second.get()` 对 `Node*` 调用 `.get()` 编译失败 | 实施时直接用 `last_node_it->second` (已是 `Node*` per `unordered_map<NodePath, Node*>`) |
| R7 | **M4 spec Scenario 2 自相矛盾** — 声称 "branch 空执行时不抛 std::runtime_error", 但 `find_branch_start_node` (line 467) 找不到 start 节点时**会抛** | spec Scenario 2 措辞改: 区分 (a) find_branch_start_node 失败 (抛 runtime_error, 现有行为) vs (b) 整 branch 子图为空 (不可达, init_branch_state 必 seed branch_root) |