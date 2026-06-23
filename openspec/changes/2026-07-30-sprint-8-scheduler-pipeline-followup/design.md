# Design: Sprint 8 Scheduler Pipeline Follow-up

> **关联 proposal**: `2026-07-30-sprint-8-scheduler-pipeline-followup/proposal.md`
> **关联 spec**: `openspec/specs/dag-scheduler-pipeline/spec.md` (Sprint 7 推进 4/5 后存档)
> **来源**: Sprint 7 Oracle 抽查 6 次 (`bg_50f6c7e0` / `bg_c12d7e56` / `bg_2ca26c00` / `bg_d619ae69` / `bg_de10c037` / `bg_eabac100`)

## 1. JOIN 死代码 + 死循环根因分析 (Oracle Day 8 确认)

### 1.1 当前执行流程

```
L158: while (... || is_executing_fork_branches_) {
L162:     if (is_executing_fork_branches_) {
L163:         execute_fork_branches();
L164:         if (current_fork_branch_index_ == current_fork_branches_.size()) {
L165:             finish_fork_simulation();  ← L550: is_executing_fork_branches_=false
                                              L553: current_fork_branches_.clear() (size=0)
L170:     auto dispatch_result = dispatch_ready_nodes(...);  ← JOIN 在此派发
L274:     if (current_node->type == NodeType::JOIN) {
L276:         if (is_executing_fork_branches_ &&              ← 总是 FALSE (L165 已清零)
              current_fork_branch_index_ == current_fork_branches_.size()) {
L278-282:        [死分支 — 永不执行]
L283:         } else {
L290:             ready_queue_.push(current_path);           ← JOIN 重新入队
L291:             continue;                                  ← 无限循环
L292:         }
L293:     }
```

### 1.2 修复方案 (Day 8 Oracle 推荐: 派发即处理模式)

```cpp
if (current_node->type == NodeType::JOIN) {
    // Sprint 8 fix: 派发即处理 (Day 8 决议)
    // 旧 L276-292 死代码 + 死循环修复
    start_join_simulation(dynamic_cast<const JoinNode*>(current_node));
    finish_join_simulation(context);
    finish_fork_simulation();
    LOG_DEBUG("Join completed, merged context.");
}
```

**为什么这样修复**:
- L165 移走 (移到 L162 块外, 只在 `is_executing_fork_branches_` 为 false 时调用 — 但其实 L165 现在已无意义, 因为 JOIN 派发即处理)
- L165 实际可删除: `is_executing_fork_branches_` 标志仅用于 `execute_fork_branches()` 调用, 处理完即可清零, 无需在 JOIN 派发前
- JOIN dispatch 时 = 准备好处理 (finish_fork_simulation 早已在 L165 调用, fork 状态已清零)
- 删除 if-else, 简化为直接处理

### 1.3 2 新 TDD 测试

1. `prepare_dag_state_fork_branch`: 2 节点 fork → 验证 is_executing_fork_branches_ 正确切换
2. `handle_node_completion_fork_join`: fork 完成后 JOIN dispatch → 验证不重新入队 (修复后行为)

## 2. build_dag → state 迁移 (Oracle Day 7 确认阻碍)

### 2.1 当前阻碍

- `build_dag()` L144: `if (in_degree_[path] == 0) ready_queue_.push(path);` — 写入 this->ready_queue_
- cycle_detection 测试 (test_scheduler.cpp:325) 无 entry_point metadata, 依赖 build_dag() 填充 ready_queue_
- Day 7 commit `69670ec` 引入 `state.ready_queue` 但 build_dag 仍写 this->ready_queue_, 状态分裂

### 2.2 迁移方案

```cpp
void TopoScheduler::build_dag(DagState& state) {
    for (const auto& node : all_nodes_) {
        // ... existing build_dag 逻辑 (计算 in_degree_ / reverse_edges_ / wait_for_dependents_)
        if (in_degree_[path] == 0) {
            state.ready_queue.push(path);  // ← 改写 state
        }
    }
}
```

调用方:
- `execute()` L156: `build_dag(state);` 替换 `build_dag();` (`.h` 声明加 DagState& 参数)
- `TopoScheduler::register_node` 仍调 `build_dag()` 无参版本 (内部默认构造 DagState, 仅写 this->)

### 2.3 execute() while 条件迁移

```cpp
while (!state.ready_queue.empty() || !session_.get_pending_dynamic_deps().empty()) {
    // 移除 is_executing_fork_branches_ 条件 (L161 死代码已修, JOIN 派发即处理)
    // ...
}
```

## 3. execute() ≤ 60 重构 (Day 9 Oracle 确认 gap)

### 3.1 当前 203 行 → 目标 ≤ 60 行 (差 143 行)

### 3.2 3 子函数提取 (Day 9 仅 2/4, Sprint 8 补 3/4)

| 子函数 | 范围 | 行数 | 依赖 |
|---|---|---|---|
| `resolve_dynamic_waits` | L185-230 | 46 | 中等独立, Day 1 提取 |
| `process_fork_join` | L263-293 (含死代码修) | 30 | 依赖 JOIN 死代码修复, Day 2-3 |
| `rebuild_dynamic_graph` | L322-363 | 42 | 依赖 build_dag 迁移, Day 4-5 |

### 3.3 execute() 编排层 (≤ 60 行目标)

```cpp
ExecutionResult TopoScheduler::execute(const Context& initial_context) {
    Context context = initial_context;
    DagState state;
    if (auto early = prepare_dag_state(state)) return *early;
    build_dag(state);  // Day 1 迁移
    
    while (!state.ready_queue.empty() || !session_.get_pending_dynamic_deps().empty()) {
        if (auto dispatch_result = dispatch_ready_nodes(state, context);
            std::holds_alternative<ExecutionResult>(dispatch_result)) {
            return std::get<ExecutionResult>(dispatch_result);
        }
        if (std::holds_alternative<std::monostate>(dispatch_result)) break;
        auto& found = std::get<NodeLookupResult>(dispatch_result);
        
        if (auto jump = resolve_dynamic_waits(state, found.node, context); jump.has_value()) {
            return *jump;
        }
        if (found.node->type == NodeType::FORK) {
            start_fork_simulation(...);
            execute_fork_branches();
            process_fork_join(state, found.node, context);
        }
        if (found.node->type == NodeType::JOIN) { /* Day 8 修复: 派发即处理 */ }
        
        if (!state.dynamic_graphs.empty()) {
            rebuild_dynamic_graph(state);
        }
    }
    return finalize_execution(state, context);
}
```

## 4. handle_node_completion 实施 (Day 6 决议方案 A 推迟)

### 4.1 NodeResult 类型 (新增 data-only struct)

```cpp
// core/types/node.h 新增
struct NodeResult {
    bool success = true;
    nlohmann::json output;
    std::string error_message;
};
```

### 4.2 函数体实施

```cpp
std::optional<ExecutionResult> TopoScheduler::handle_node_completion(
    DagState& state, const NodeResult& result) {
    if (!result.success) {
        // 失败传播: downstream 标 Skipped
        for (const auto& dependent : state.wait_for_dependents[/*current_path*/]) {
            state.in_degree[dependent] = -1;  // Skipped 标记
        }
    }
    // 更新 successors via next field
    for (const auto& next_path : /*current_node*/->next) {
        if (--state.in_degree[next_path] == 0) {
            state.ready_queue.push(next_path);
        }
    }
    for (const auto& dependent : state.wait_for_dependents[/*current_path*/]) {
        if (--state.in_degree[dependent] == 0) {
            state.ready_queue.push(dependent);
        }
    }
    state.executed.insert(/*current_path*/);
    return std::nullopt;
}
```

## 5. Hub out_degree 测量 (Day 9 未测)

execute() 重构后用 `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证:
- `topo_scheduler::execute` out_degree < 30
- 3 子函数 out_degree < 25
- `create_node_from_json` out_degree < 30

## 6. 风险评估

| 风险 | 等级 | 缓解 |
|---|---|---|
| JOIN 死代码修行为变更 | 🟠 Major | 2 新 fork+join TDD 测试覆盖 |
| build_dag → state 迁移 | 🟠 Major | cycle_detection 测试已存在, 36/36 ctest 验证 |
| execute() ≤ 60 重构 | 🟠 Major | 34/34 + 2 fork+join = 36/36 ctest 保持 |
| handle_node_completion 实施 | 🟡 Minor | 4 existing TDD tests 覆盖, 9 处失败传播 |
| Hub out_degree 测量 | 🟢 Low | 静态分析 + MCP 工具 |
