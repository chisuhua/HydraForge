## Why

Sprint 7 Day 5-9 (commit `a7a2edc` / `6a63518` / `b45d049` / `69670ec` / `75ded94` / `a00734f`, merge `b44b486`) ship 4/5 Sprint 7 spec `scheduler-pipeline-tightened` 子项, 零回归 (34/34 ctest + 4/4 fork static contracts PASS). 1/5 spec 子项 (execute() ≤ 60 行) + 2/6 验收项 (hub out_degree < 30 + L275-279 死代码修) 推迟到本 change 闭环.

Oracle 5 次抽查 (`bg_50f6c7e0` / `bg_c12d7e56` / `bg_2ca26c00` / `bg_d619ae69` / `bg_de10c037` / `bg_eabac100`) 揭示 5+ 项技术债:

1. 🔴 **JOIN 死代码 + 死循环** (Oracle Day 8 确认): `topo_scheduler.cpp` L165 `finish_fork_simulation()` 过早调用 + L553 `current_fork_branches_.clear()` (size=0) + L276 `is_executing_fork_branches_ && current_fork_branch_index_ == current_fork_branches_.size()` 条件总 false + L290 `ready_queue_.push(current_path)` JOIN 重新入队死循环. 当前 7 Day 2 tests + 34/34 ctest PASS 因无 fork+join DSL fixture 覆盖此路径 (test_scheduler.cpp:464-466 注释: "NodeExecutor::execute_fork 当前未实现 (throws runtime_error), 无法通过 DSL 触发 is_executing_fork_branches_=true").

2. 🟠 **execute() 203 行 vs ≤ 60 目标** (Oracle Day 7+9 确认): Day 9 commit `a00734f` 提取 2 子函数 (process_jump + update_successors) 后仍 203 行, 差 143 行. 3 内联子逻辑未提取:
   - L185-230 resolve_dynamic_waits (46 行): 中等独立, 可 Day 1 提取
   - L263-293 process_fork_join (30 行): 含死代码, 推迟到 JOIN 死代码修复后
   - L322-363 rebuild_dynamic_graph (42 行): 含 build_dag() L144 耦合, 推迟到 build_dag 迁移

3. 🟠 **build_dag() L144 → state 迁移** (Oracle Day 7 确认): `build_dag()` L144 `if (in_degree_[path] == 0) ready_queue_.push(path)` 仍 push 到 this->ready_queue_, cycle_detection 测试 (test_scheduler.cpp:325) 无 entry_point 依赖此成员队列. Day 7 commit `69670ec` 引入 state.ready_queue 但 build_dag 仍写 this->ready_queue_, 状态分裂.

4. 🟡 **handle_node_completion + NodeResult 类型定义** (Day 6 决议方案 A 推迟): plan Task 2 L155 需 `NodeResult` 类型, 代码库不存在 (`grep "(struct|class|enum) Node(Status|Result)"` 全仓 0 命中). 4 TDD 测试已 Day 2 ship (test_scheduler.cpp:354/385/428/468) 但函数体未实现.

5. 🟡 **Hub out_degree < 30 测量** (Oracle Day 9 未测): execute() 203 行 + inline 9 session_/node_/context 调用 + 3 subfunction 调用, 估计 out_degree ~35-40. 需 execute() ≤ 60 重构后做 `mcp__code-review-graph__get_hub_nodes --top_n 5` 精确测量.

## What Changes

### 🔴 Blocker (Day 1 必做, 1 PR)

- **修 JOIN 死代码 + 死循环**: 重设计 `topo_scheduler.cpp:274-293` JOIN 处理为"派发即处理"模式:
  - 删除 L276 `if (is_executing_fork_branches_ && ...)` 条件
  - 删除 L283-291 else 分支 (re-queue JOIN)
  - 简化为直接 `start_join_simulation + finish_join_simulation + finish_fork_simulation`
  - 同步移 L165 `finish_fork_simulation()` 调用从 L165 (执行 L163-167 fork 主块) 到 JOIN 处理内 (避免过早重置 is_executing_fork_branches_)
  - 新增 `prepare_dag_state_fork_branch` + `handle_node_completion_fork_join` 2 TDD 测试 (test_scheduler.cpp) 覆盖修复后路径

### 🟠 Major (Sprint 8 主体, 2-3 PR)

- **`build_dag()` state 迁移**:
  - 编辑 `topo_scheduler.cpp:build_dag()` 接受 `DagState& state` 参数, push 到 `state.ready_queue` (替代 `this->ready_queue_`)
  - 同步 `execute()` 主 while 条件 `!ready_queue_.empty() || ...` → `!state.ready_queue.empty() || ...`
  - 移除 `prepare_dag_state` 内的 `state.ready_queue.push(entry_point.value())` 双写 (Day 7 commit `69670ec` 临时)
  - 验证 cycle_detection 测试 (test_scheduler.cpp:325) 仍 PASS (无 entry_point 路径用 state.ready_queue 初始化)

- **execute() ≤ 60 重构**:
  - 提取 L185-230 `resolve_dynamic_waits(DagState& state, Node* current_node, Context& context): ExecutionResult` 子函数 (46 行)
  - 提取 L263-293 (含 JOIN 死代码修复) `process_fork_join(DagState& state, Node* current_node, Context& context)` 子函数 (30 行)
  - 提取 L322-363 `rebuild_dynamic_graph(DagState& state)` 子函数 (42 行, 接受 state 引用, 重建后刷新 state.topology 4 字段)
  - execute() 重写为纯编排层: `DagState state;` → `build_dag(state)` → 循环 `dispatch_ready_nodes + handle_node_completion` → 4 子逻辑委派 → `finalize_execution(state)` → 返回
  - 验证: `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l` ≤ 60

- **`handle_node_completion` 函数体实施 + NodeResult 类型定义**:
  - 在 `core/types/node.h` 新增 `struct NodeResult { bool success; nlohmann::json output; std::string error_message; };`
  - 实施 `topo_scheduler.cpp:handle_node_completion(DagState& state, const NodeResult& result)` 函数体: 更新 `state.executed.insert(path)` + 遍历 `state.wait_for_dependents[path]` + 减 `state.in_degree[dependent]` + 0 时 `state.ready_queue.push(dependent)` + 失败传播 (downstream 标 Skipped)
  - 验证: `grep "handle_node_completion" src/modules/scheduler/topo_scheduler.cpp` 仅 1 调用 (execute() 主循环)

- **Hub out_degree 测量 + 验证**:
  - `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 `topo_scheduler::execute` out_degree < 30
  - 验证 3 子函数 (`prepare_dag_state` / `dispatch_ready_nodes` / `handle_node_completion`) out_degree < 25
  - 验证 `create_node_from_json` out_degree < 30

### 🟡 Minor (顺手做, 1 PR)

- 验证 Sprint 6+7+8 ship gate: ctest ≥ 34/34 (含 7+2=9 Day 2+8 tests) + TSan 0 race + ASan 0 leak
- `python3 tools/adr_lint.py docs/adr/` exit 0
- `python3 tools/docs_drift_audit.py` 0 critical drift
- `openspec validate 2026-07-30-sprint-8-scheduler-pipeline-followup` exit 0
- 同步 ADR-0019 §1.4 状态: DagState 实施完成 + execute() ≤ 60 达成

### 📦 归档

- `openspec archive 2026-07-30-sprint-8-scheduler-pipeline-followup --yes` (5/5 spec 子项 100% 达成)
- 同步到 `openspec/specs/dag-scheduler-pipeline/`
- 更新 `AGENTS.md` § Recent Changes 标记 Sprint 8 ship

## Capabilities

### Modified Capabilities

- `dag-scheduler-pipeline`: 修正 execute ≤ 60 行 ✅ (Sprint 8 闭环) + JOIN 死代码修 ✅ + hub out_degree < 30 ✅ + handle_node_completion 实施 ✅
- `node-types`: 新增 `struct NodeResult` 节点结果类型
- `dag-state`: build_dag → state 迁移, DagState 完整生命周期

## Impact

**修改文件**:
- `src/modules/scheduler/topo_scheduler.{h,cpp}` (execute 重构 + JOIN 死代码修 + handle_node_completion 实施 + 3 子函数提取)
- `src/core/types/node.h` (新增 NodeResult struct)
- `tests/test_scheduler.cpp` (新增 2 fork+join TDD tests)
- `docs/adr/adr-0019-*.md` §1.4 状态更新
- `AGENTS.md` § Recent Changes 追加 Sprint 8 总结

**API 稳定性**:
- `DSLEngine` 公共 API 零变化
- `TopoScheduler` 公共 API 零变化 (`DagState` 仍私有内部结构, `handle_node_completion` 仍私有成员)
- `NodeResult` 为新增 data-only struct, 兼容旧 `NodeExecutionStatus` 模式 (后者已推迟)

**测试影响**:
- 34/34 → ≥ 36/36 (+2 fork+join TDD tests)
- TSan: scheduler worker race + JOIN dispatch 路径 0 race
- ASan: 0 leak

**风险域**:
- 🔴 JOIN 死代码修复涉及行为变更 (当前死代码, 修复后激活), 需 2 fork+join TDD 测试覆盖
- 🟠 execute() ≤ 60 重构涉及大量代码移动, 行为保持靠 34/34 ctest + 4/4 fork static contracts
- 🟠 build_dag → state 迁移涉及 cycle_detection 测试路径, 需 0 behavior diff
- 🟡 handle_node_completion 实施涉及 9 处 failed propagation 路径, 需 4 existing TDD tests 覆盖

## Non-goals

- **不改** Sprint 7 已 ship 子项 (DagState 7 字段契约 / 3 子函数签名 / 渐进纯函数化 / accessor 修复)
- **不改** CognitiveWorker / DomainWorkerPool (Sprint 2/3 ship)
- **不改** PDK 公共 API (ADR-0021 T4b 治理锁定)
- **不引入** 新第三方依赖
- **不重做** Sprint 6+7 已 ship 6 commits
- **不修改** Sprint 8 范围外的 refactor
- **不实质化** ADR-0007 / ADR-0031 / ADR-0033 (P3 长期项)
- **不重新 base** ADR-0019 / ADR-0020 / ADR-0021 / ADR-0022 (仅追加状态标记)

## Estimated Effort

- 🔴 Blocker (JOIN 死代码修 + 2 fork+join tests): 1 天
- 🟠 Major 主体 (build_dag state 迁移 + execute ≤ 60 + handle_node_completion 实施 + hub out_degree 测量): 2 周
- 🟡 Minor (ship gate 验证 + ADR 同步 + 归档): 0.5 天

**总计**: ~2.5 周 (1 Sprint 8 全职), 比 Sprint 7 缩短 0.5 周 (Day 5-7 基础已 ship).
