# Tasks: Sprint 8 Scheduler Pipeline Follow-up

> **范围来源**: Sprint 7 Oracle 抽查 6 次 (`bg_50f6c7e0` / `bg_c12d7e56` / `bg_2ca26c00` / `bg_d619ae69` / `bg_de10c037` / `bg_eabac100`) 揭示 5+ 项技术债
> **总工时**: ~2.5 周 (1 Sprint 8)
> **前置依赖**: Sprint 7 Day 5-9 已 ship (commit `a7a2edc` / `6a63518` / `b45d049` / `69670ec` / `75ded94` / `a00734f`, merge `b44b486`)
> **关联 change**: `2026-06-23-sprint-7-tech-debt-followup` (Sprint 7 已 archive)
> **创建日期**: 2026-06-23

---

## 1. Day 1 - 🔴 Blocker JOIN 死代码 + 死循环修复

### 1.1 修 JOIN 处理 (派发即处理模式)

- [ ] 1.1.1 编辑 `src/modules/scheduler/topo_scheduler.cpp:274-293`: 删除 L276 `if (is_executing_fork_branches_ && ...)` 条件 + L283-291 else 分支
- [ ] 1.1.2 简化为直接 `start_join_simulation + finish_join_simulation + finish_fork_simulation`
- [ ] 1.1.3 同步移 L165 `finish_fork_simulation()` 调用: 从 L162-167 fork 主块移到 JOIN 处理内 (或删除, JOIN 派发即处理模式不需要 L165 预清零)
- [ ] 1.1.4 `cmake --build build` 编译通过

### 1.2 2 fork+join TDD 测试

- [ ] 1.2.1 `tests/test_scheduler.cpp` 末尾添加 `TEST_CASE("prepare_dag_state_fork_branch", "[scheduler][sprint8][fork]")`: 2 节点 fork, 验证 `is_executing_fork_branches_` 切换
- [ ] 1.2.2 添加 `TEST_CASE("handle_node_completion_fork_join", "[scheduler][sprint8][join]")`: fork 完成后 JOIN dispatch, 验证不重新入队 (修复后行为)
- [ ] 1.2.3 `ctest -R test_scheduler --output-on-failure` 9 cases (7 Day 2 + 2 Sprint 8) PASS + 既有零回归

### 1.3 提交

- [ ] 1.3.1 `git commit -m "fix(scheduler): repair JOIN dead code + dead loop, dispatch-then-process (Sprint 8 Blocker)"`
- [ ] 1.3.2 跑 `openspec validate 2026-07-30-sprint-8-scheduler-pipeline-followup` 验证变更

---

## 2. Day 2-5 - 🟠 Major execute() ≤ 60 重构

### 2.1 `build_dag()` state 迁移

- [ ] 2.1.1 编辑 `src/modules/scheduler/topo_scheduler.h`: `build_dag()` 声明加 `DagState& state` 参数
- [ ] 2.1.2 编辑 `src/modules/scheduler/topo_scheduler.cpp` `build_dag(DagState& state)`: L144 `ready_queue_.push(path)` → `state.ready_queue.push(path)`
- [ ] 2.1.3 同步 `execute()` L156 调用: `build_dag();` → `build_dag(state);`
- [ ] 2.1.4 同步 `execute()` L158 while 条件: `!ready_queue_.empty()` → `!state.ready_queue.empty()` (移除 `is_executing_fork_branches_` 条件)
- [ ] 2.1.5 同步 `register_node` 无参 build_dag() 路径: 内部默认构造 DagState 仅写 this-> (避免 API 破坏)

### 2.2 Day 7 双写清理

- [ ] 2.2.1 编辑 `topo_scheduler.cpp:prepare_dag_state`: 移除 `state.ready_queue.push(entry_point.value())` 双写 (Day 7 临时, Day 8 移除条件已满足)
- [ ] 2.2.2 验证 cycle_detection 测试 (test_scheduler.cpp:325) 仍 PASS

### 2.3 `resolve_dynamic_waits` 子函数提取

- [ ] 2.3.1 编辑 `topo_scheduler.h`: 新增 `std::optional<ExecutionResult> resolve_dynamic_waits(DagState& state, Node* current_node, const Context& context);` 声明
- [ ] 2.3.2 提取 L185-230 (46 行) 内联逻辑为 `resolve_dynamic_waits` 函数体
- [ ] 2.3.3 execute() 调用方改: 替换 L185-230 内联代码为 `if (auto r = resolve_dynamic_waits(state, current_node, context); r.has_value()) return *r;`

### 2.4 `process_fork_join` 子函数提取 (含死代码修复)

- [ ] 2.4.1 编辑 `topo_scheduler.h`: 新增 `void process_fork_join(DagState& state, Node* current_node, Context& context);` 声明
- [ ] 2.4.2 提取 L263-293 (30 行, 含 Day 8 死代码修复后逻辑) 为 `process_fork_join` 函数体
- [ ] 2.4.3 execute() 调用方改: 替换 L263-293 内联代码为 `process_fork_join(state, current_node, context);`

### 2.5 `rebuild_dynamic_graph` 子函数提取

- [ ] 2.5.1 编辑 `topo_scheduler.h`: 新增 `void rebuild_dynamic_graph(DagState& state);` 声明
- [ ] 2.5.2 提取 L322-363 (42 行) 内联逻辑为 `rebuild_dynamic_graph` 函数体 (接受 state 引用, 重建后刷新 state.topology 4 字段)
- [ ] 2.5.3 execute() 调用方改: 替换 L322-363 内联代码为 `rebuild_dynamic_graph(state);`

### 2.6 `execute()` 编排层重写

- [ ] 2.6.1 重写 `execute()` 主体为纯编排层, 调用 4 子函数 + build_dag + dispatch_ready_nodes + handle_node_completion + finalize_execution
- [ ] 2.6.2 验证: `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l` ≤ 60
- [ ] 2.6.3 `cmake --build build` 编译通过
- [ ] 2.6.4 `ctest --output-on-failure` 36/36 PASS (7 Day 2 + 2 Sprint 8 + 33 baseline)

### 2.7 提交

- [ ] 2.7.1 `git commit -m "refactor(scheduler): execute() ≤ 60 lines + build_dag → state + extract 3 subfunctions (Sprint 8)"`
- [ ] 2.7.2 跑 `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 out_degree < 30

---

## 3. Day 6-7 - 🟠 Major handle_node_completion 实施 + NodeResult 定义

### 3.1 NodeResult 类型定义

- [ ] 3.1.1 编辑 `src/core/types/node.h`: 新增 `struct NodeResult { bool success = true; nlohmann::json output; std::string error_message; };`
- [ ] 3.1.2 验证: `grep "struct NodeResult" src/core/types/node.h` 1 命中
- [ ] 3.1.3 `cmake --build build` 编译通过

### 3.2 `handle_node_completion` 函数体实施

- [ ] 3.2.1 编辑 `topo_scheduler.h`: `handle_node_completion` 声明从 `// 推迟到 Day 7-8` 改为正式 `std::optional<ExecutionResult> handle_node_completion(DagState& state, const NodeResult& result);` (接受 NodeResult&)
- [ ] 3.2.2 编辑 `topo_scheduler.cpp`: 实施 `handle_node_completion` 函数体 (按 design.md §4.2 模板)
- [ ] 3.2.3 同步 `execute()` 调用方: 移除 Day 7 临时双写, 改用 handle_node_completion 统一处理 success/failure 传播
- [ ] 3.2.4 验证: `grep "handle_node_completion" src/modules/scheduler/topo_scheduler.cpp` 仅 1 调用 (execute() 主循环)
- [ ] 3.2.5 `ctest -R test_scheduler --output-on-failure` 9 cases PASS

### 3.3 提交

- [ ] 3.3.1 `git commit -m "feat(scheduler): implement handle_node_completion + add NodeResult type (Sprint 8)"`

---

## 4. Day 8 - 🟡 Minor ship gate 验证 + ADR 同步

### 4.1 Ship gate 全跑

- [ ] 4.1.1 `cd build && ctest --output-on-failure` 36/36 PASS
- [ ] 4.1.2 `cmake --preset tsan && ctest --output-on-failure` 0 race
- [ ] 4.1.3 `cmake --preset asan && ctest --output-on-failure` 0 leak
- [ ] 4.1.4 `python3 tools/adr_lint.py docs/adr/` exit 0
- [ ] 4.1.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 4.1.6 `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 execute out_degree < 30 + 3 子函数 out_degree < 25
- [ ] 4.1.7 `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l` ≤ 60
- [ ] 4.1.8 `git status` clean, 4-5 commits 按 Day 分组 (Day 1 JOIN + Day 2-5 execute + Day 6-7 handle + Day 8 ship gate)

### 4.2 ADR 同步

- [ ] 4.2.1 编辑 `docs/adr/adr-0019-*.md` §1.4 状态: DagState 实施完成 + execute ≤ 60 达成 + handle_node_completion 实施
- [ ] 4.2.2 编辑 `AGENTS.md` § Recent Changes 追加 Sprint 8 总结
- [ ] 4.2.3 `git commit -m "docs(adr+status): Sprint 8 ship + execute ≤ 60 + handle_node_completion"`

---

## 5. Day 9 - 📦 归档 `2026-07-30-sprint-8-scheduler-pipeline-followup`

### 5.1 最终验证

- [ ] 5.1.1 跑 §4.1 全部 ship gate 一次, 全 pass
- [ ] 5.1.2 跑 `openspec show 2026-07-30-sprint-8-scheduler-pipeline-followup` 确认所有 task 100% 完成

### 5.2 归档

- [ ] 5.2.1 `openspec archive 2026-07-30-sprint-8-scheduler-pipeline-followup --yes`
- [ ] 5.2.2 `openspec list --specs` 确认 `dag-scheduler-pipeline` spec 已更新 (execute ≤ 60 + JOIN 修复 + handle_node_completion 实施)

### 5.3 最终提交

- [ ] 5.3.1 `git commit -m "chore(openspec): archive sprint-8-scheduler-pipeline-followup + Sprint 8 final ship"`
- [ ] 5.3.2 `git tag sprint-8-scheduler-pipeline-followup-ship`
- [ ] 5.3.3 更新 `AGENTS.md` 顶部状态: Sprint 8 ship 标记

---

## 6. 验证检查清单 (Sprint 8 ship gate)

- [ ] 6.1 Sprint 7+8 全部 task 100% 完成
- [ ] 6.2 `cd build && ctest --output-on-failure` 36/36 PASS
- [ ] 6.3 `cmake --preset tsan && ctest --output-on-failure` 0 race
- [ ] 6.4 `cmake --preset asan && ctest --output-on-failure` 0 leak
- [ ] 6.5 `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l` ≤ 60
- [ ] 6.6 `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 execute out_degree < 30 + 3 子函数 out_degree < 25
- [ ] 6.7 `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp` ≤ 3 (Sprint 7 残留)
- [ ] 6.8 `git status` clean, 4-5 commits 按 Day 分组
- [ ] 6.9 AGENTS.md § Recent Changes 含 Sprint 7 + Sprint 8 ship 标记
- [ ] 6.10 `openspec archive 2026-07-30-sprint-8-scheduler-pipeline-followup` 成功
