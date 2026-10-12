# Proposal: Fork/Join DSL completion feedback (full e2e success=true)

## Why

`2026-10-10-fork-join-dsl-real-parallel` (commits `8b3dffe + 81b44d4 + 5a37f3c + fa4b040 + 21bca13 + dd6237a`) ship 闭合 5 个 fork/join 结构性 BUG (routing-FORK/JOIN 前移 + fork update_successors + parser 双 schema + lib/loop fork_join next 链 + finish_join_simulation 防御性), 但 **完整 fork_join DSL e2e success=true** 仍 deferred per Oracle `ses_ed35f7205ffeCm6OlJTBrbVhtG` Critical-1 Option (a) deferred recommendation.

**真实根因** (Oracle static analysis 100% confirmed, 2026-10-10 ship verdict):

`execute_fork_branches()` (topo_scheduler.cpp:389-419) 在每个 branch 通过 `execute_single_branch` (line 420-459) 完成后, 仅维护本地 `branch_in_degree` 副本 (line 451-454), **不** decrement 主调度器的 `in_degree_[join_results]`。具体表现:

| 阶段 | `in_degree_[join_results]` |
|---|---|
| build_dag 后 (line 802-813) | 7 = fork.next +1 + task×3.next +3 + wait_for +3 |
| fork_tasks 主循环处理后 (`handle_node_completion` via D2 fix) | 6 (减 1: fork.next 边推进) |
| **branch 完成 (execute_fork_branches 内)** | **仍 6 (本 change 修复点)** |
| join_results 进入 ready_queue | 0 |

**branch 完成必须 feedback 主调度器** 才能让 join_results 进入 ready_queue + 被 dispatch。当前架构下 join_results 永远不可达, fork/join e2e 必现 hang. 需 ~15 行 `execute_fork_branches` 加 branch→main scheduler completion feedback.

**用户原始动机** (2026-10-10, per `.rddf/improvements/fork_join_dsl_migration.md`): "C++ 版本的 Agent Loop 可以停用, 只暴露 DSL 路径吗?" → ForkJoinLoop 软停用 v1.5 future change 需先解决 e2e 闭环才能做语义对照.

## What Changes

### 修复 1 (D1 — execute_fork_branches branch→main feedback)

`src/modules/scheduler/topo_scheduler.cpp::execute_fork_branches` (line 389-419) 在每个 branch `execute_single_branch` 返回后, **显式**调 update_successors 推送 consumer (如 join_results) in_degree 递减. 主信号:

```cpp
while (current_fork_branch_index_ < current_fork_branches_.size()) {
    const NodePath& branch_path = current_fork_branches_[current_fork_branch_index_];
    Context branch_initial_ctx = *fork_snapshot;
    try {
        auto [branch_final_ctx, last_node_path] = execute_single_branch(branch_path, branch_initial_ctx);
        
        // D2: execute_single_branch 返回值扩展为 (Context, NodePath last_node_path)
        // 这里用 last_node_path 调 update_successors, 让 consumer (join_results) in_degree 正确递减
        auto last_node_it = node_map_.find(last_node_path);
        if (last_node_it != node_map_.end()) {
            update_successors(last_node_it->second, last_node_path);
        }
        
        current_fork_branch_results_.push_back(std::move(branch_final_ctx));
        current_fork_branch_index_++;
    } catch (const HardEndException& e) {
        throw;
    }
}
```

### 修复 2 (D2 — execute_single_branch 返回值扩展)

`src/modules/scheduler/topo_scheduler.cpp::execute_single_branch` (line 420-459) 返回值从 `Context` 扩展为 `std::pair<Context, NodePath>` (final_ctx + last_node_path). 内部分支循环末尾记录 last_executed_node_path (line 445 `branch_executed.insert(current_path)` 后), 函数返回 pair. 公开 API 是 TopoScheduler 内部 helper, **外部**消费者仅 `execute_fork_branches` 1 个调用点, 影响零扩散.

### 修复 3 (D3 — execute_fork_branches 函数签名 + impl 同步)

`src/modules/scheduler/topo_scheduler.h` 的 `execute_single_branch` (line 135) 签名同步改 `std::pair<Context, NodePath>`. 头文件改动最小化 (1 个内部 helper, 1 个调用方).

**不动 `include/agenticdsl/pdk/agent_loops/fork_join_loop.h`** (C++ PDK `ForkJoinLoop` class, per ADR-0089 v1.4 §D8.inv.forkjoin).

### 修复 4 (D4 — 完整 e2e success=true 测试)

升级 `tests/test_fork_join_dsl_real_parallel.cpp` (commit `21bca13` 新增) 加 Case 6 — 完整 e2e success=true:
- 加载 `lib/loop/fork_join.agent.md` (fork + 3 branches + join + synthesize + end) 经 `DSLEngine::from_markdown` + MockLLMProvider
- 断言 `engine->run(...).success == true` (而非 "不抛 logic_error")
- 断言 `engine->get_last_traces().size() >= 5` (per Oracle C3 修正: `ExecutionResult` 无 `steps` 字段, 用 trace count; 起算节点 start + task×3 + synthesize + end = 6 节点, fork_tasks/join_results 是调度节点不发 trace record, `>= 5` 留 mock 抖动 buffer)
- 断言 result final_context 含 task_a/b/c 输出聚合 (per ContextEngine::merge policy)

升级 `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2 (per 2026-10-10 ship commit `21bca13` 已升级的 GREEN-only case) 同样断言 `success == true` + `engine->get_last_traces().size() >= 5` (锁住 e2e 路径不退化).

### 修复 5 (D5 — AGENTS.md + tracker + docs 同步)

- AGENTS.md Recent Changes: 本 change ship 登记 (含 commit hash + Reverse Indicator 5 字段)
- `.rddf/improvements/fork_join_dsl_migration.md` status update: e2e gap **前置闭合** (本 change ship 后 e2e success=true ✅), **完整 ForkJoinLoop→DSL `loop/run_fork_join` 工具迁移仍 pending** (1 Sprint 工作, 启动条件不变 per ADR-0089 v1.4 D9 tracker)
- `docs/specs/dsl.md` §5.5 fork/join DSL 段: 补充 branch→main completion feedback 设计意图 (设计意图明确)

## Capabilities

### 新增能力
- `lib/loop/fork_join.agent.md` 完整 e2e success=true (per Oracle Critical-1 收尾)
- `TopoScheduler::execute_fork_branches` branch→main completion feedback 机制 (~15 行)
- `TopoScheduler::execute_single_branch` 返回值扩展 (Context + last_node_path)
- **`TopoScheduler::execute_single_branch` 内 `executed_.insert` 修复** (per Oracle C2 静态分析 2026-10-12: `finalize_execution` 检查 `all_nodes_ - executed_` 算 unexecuted, branch 节点必须写入成员 `executed_` 才能让 `result.success == true`)
- `fork_join_dsl_migration.md` tracker e2e gap 前置 ✅ (完整 ForkJoinLoop→DSL 工具迁移仍 pending)

### 不变更能力 (与 ADR-0089 v1.4 一致)
- ❌ C++ `ForkJoinLoop` class 零变化 (per ADR-0089 v1.4 §D8.inv.forkjoin)
- ❌ `DomainWorkerPool` 4-worker 零变化 (per ADR-0020 §3.2 + ADR-0087 benchmark)
- ❌ `tests/test_pdk_fork_join.cpp` 5 cases 零回归 (C++ ForkJoinLoop 4-worker 行为不变)
- ❌ ADR-0087 benchmark 不退化 (本 change 不触 DomainWorkerPool)
- ❌ `examples/pdk_chat_demo/dsl/fork_join_example.agent.md` (走 `type: fork_join` C++ ForkJoinLoop 路径) 零变化
- ❌ `extract_agenticdsl_block` 4 fence 容错 零变化
- ❌ `lib/loop/react.agent.md` + `lib/loop/plan_execute.agent.md` 零变化
- ❌ `loop_phases.h` 3 函数 零变化 (per Pattern #12 v1.3 ship 不变量 D1.inv.model)
- ❌ TopoScheduler fork/join DAG 节点类型零变化 (本 change 不引入新 NodeType)

## 关联

### 关联 OpenSpec changes (已 ship)
- `archive/2026-10-10-fork-join-dsl-real-parallel/` (commits `8b3dffe + 81b44d4 + 5a37f3c + fa4b040 + 21bca13 + dd6237a`): 闭合 BUG-2/3/4/5 + Critical-2/Major-2, 留 Critical-1 Option (a) deferred = 本 change
- `archive/2026-10-10-fork-join-dsl-migration/` (commits `7e81c2f + 469f495`): BUG-3 (plan prompt) 部分修复
- `archive/2026-10-10-fix-fork-dsl-context-propagation/` (commits `3dd12a2 + adb7910 + 83883bc + 6b85ca7 + b51adea + b706c31`): BUG-2-A (routing-FORK 前移) + BUG-2-B (template context defensive fix)
- `archive/2026-10-10-fix-fork-join-agent-next-chain-blocked-by-pattern8-dual-agent-review/`: BLOCKED by Pattern #8 dual-agent review, supersede 至 fork-join-dsl-real-parallel

### Pre-implementation dual-agent review (2026-10-12)
- **Metis `bg_05d4e505`** + **Oracle `bg_6ebeb899`** 并行审查 per AGENTS.md Pattern #8
- 收敛信号 (cross-validated): **C1 D3 错误文件路径** (Metis 🔴 + Oracle 🔴) + **M1 `.get()` 编译错误** (Metis 🟠 + Oracle 🟠)
- Oracle-only 独立发现 (高置信度): **C2 `executed_` 未插** (🔴) + **C3 `ExecutionResult` 无 `steps`** (🔴, compile-blocker) + **M2 R4 `continue` 死循环** (🟠) + **M4 spec Scenario 2 自相矛盾** (🟠) + **M6 last-node-as-exit-node 假设未强制** (🟠)
- Metis-only 独立发现: **M3 `==6` vs `>=6` 矛盾** (🟠) + **M5 tracker 闭合语义错配** (🟠) + **m1 examples Test 2 `>=7` vs `>=6` 数字矛盾** (🟡)
- **所有 Critical/Major 在本 change ship 前必须修尽** (per Pattern #8)

### 关联 tracker
- `.rddf/improvements/fork_join_dsl_migration.md`: 本 change ship 后 e2e gap **前置闭合** (per design D1+D2 + C2 修复), tracker status 保持 "🔍 Future Change Tracker" (per ADR-0089 v1.4 D9, 完整 ForkJoinLoop→DSL `loop/run_fork_join` 工具迁移仍 pending 1 Sprint, 启动条件 v1.4 观察期满 ≥1-2 月 + 用户决策 + ADR-0087 benchmark 不退化)

### 关联 ADR
- ADR-0089 v1.4 §D8.inv.forkjoin: ForkJoinLoop 维持现状 (本 change 不触)
- ADR-0087: 4-worker benchmark 不退化 (本 change 不触 DomainWorkerPool)
- ADR-0021 §3.5: PDK 头依赖限制 (本 change 仅改 1 个 PDK 头函数返回值, 零原生 ABI 破坏)
- ADR-0020 §3.2: DomainWorkerPool 模式不变