# Sprint 7 Day 5-9: scheduler-pipeline-tightened Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 闭环 Sprint 6 `topo_scheduler.cpp` 偏离项：引入 `DagState` 结构体使 3 子函数为纯函数式、收 `execute()` ≤ 60 行、修 L275-279 JOIN then-branch 死代码、修 `pending_dynamic_deps_` 访问不一致。

**Architecture:** 渐进式 TDD 重构 — Day 5 brainstorm 决定 DagState 字段 → Day 6 改 3 子函数签名接受 `DagState&`（既有 7 测试零回归）→ Day 7-8 重写 3 子函数体为纯函数式 + TDD 新增 4-5 测试 → Day 9 收 `execute()` ≤ 60 行 + 修 2 处访问不一致 + hub out_degree 验证。Day 5/8/9 三个 commit 后各跑一次 Oracle 抽查防止 Sprint 6 偏离再现。

**Tech Stack:** C++20, Catch2 v3 amalgamated, CMake 3.20+, OpenSpec CLI v1.4.1, git-master atomic commits, Oracle deep review, code-review-graph MCP (hub out_degree 验证).

**前置状态:** Sprint 7 Day 1-4 + Day 5 warmup 已完成（4 commits on main, ctest 34/34 PASS）。本 plan 从 Day 5 起。已修：
- ✅ `dispatch_next_node` 内 fork 重复块（commit `84c4c0a`）
- ✅ `NodeFactoryRegistry::create` throw → nullptr（commit `9cef3f8`）
- ✅ Day 4 spec §3.3.3 修正（commit `680718e`）

---

## File Structure (实施范围)

### Modify
- `src/modules/scheduler/topo_scheduler.h` — 加 `struct DagState` + `struct NodeExecutionStatus`，改 3 子函数签名
- `src/modules/scheduler/topo_scheduler.cpp` — 重写 3 子函数体，收 `execute()` ≤ 60 行，修 L275-279 死代码，修 `pending_dynamic_deps_` 访问

### Create (Day 7-8 测试)
- `tests/test_scheduler_dag_state.cpp` (可选合并到 test_scheduler.cpp) — 4-5 新 TDD 测试

### Reference (实施时查阅)
- `openspec/changes/sprint-7-tech-debt-followup/specs/sprint-7-tech-debt-followup/spec.md` `scheduler-pipeline-tightened` Requirement — 验收标准
- `.omo/plans/2026-06-22-sprint7-tech-debt-day-1-4.md` Task 5+ outline
- `tests/test_scheduler.cpp` 526 行 Day 2 7 测试 (既有 baseline)
- `src/modules/scheduler/execution_session.h` — `get_pending_dynamic_deps()` 访问器

---

## Task 1 (Day 5): Brainstorm + 引入 DagState + NodeExecutionStatus

**Files:**
- Modify: `src/modules/scheduler/topo_scheduler.h:88-95` (在 `private:` 块加 struct)
- Test: `cd build && cmake --build . && ctest -R test_scheduler --output-on-failure` (Day 2 7 测试零回归 = DagState 引入是 pure add)

**Context:** Oracle 审查标记 `scheduler-pipeline-tightened` Requirement 要求 3 子函数为纯函数式（仅通过 `DagState&` 参数通信，不直接修改 `TopoScheduler` 成员）。当前 `topo_scheduler.h:88-95` 已有：

```cpp
std::optional<ExecutionResult> prepare_dag_state();
struct NodeLookupResult { NodePath path; Node* node; };
std::variant<std::monostate, NodeLookupResult, ExecutionResult>
dispatch_next_node(const Context& context);
ExecutionResult finalize_execution(const Context& context);
```

注意：`dispatch_next_node` 当前签名 `const Context&` 但内部修改成员（不纯）。`finalize_execution` 同。Day 5 改 3 子函数签名为 `(DagState&, ...)` 形式，**Task 2 实施**（保持本 Task 0 行为变更，只引入 struct）。

### 子任务

- [ ] **Step 1.1: Day 5 头脑风暴 — 决定 DagState 字段**

使用 superpowers:brainstorming skill（推荐 30 分钟）决定 DagState 应含哪些字段。**必讨论**：
- 哪些当前 `TopoScheduler` 私有成员应迁入 `DagState`？（候选：`ready_queue_` / `in_degree_` / `executed_` / `current_fork_*` 字段 / `pending_dynamic_deps_` 等）
- DagState 应是 plain struct（默认构造 + 公开字段）还是 PIMPL？
- 哪些 fork/join 状态变量保留在 `TopoScheduler` 成员，哪些移到 `DagState`？（关键：fork 是执行模式状态，可能留在 `TopoScheduler`；DAG 拓扑状态进 `DagState`）

**记录**: 在本 plan 末尾 "## Brainstorm 决议记录" 段填入 DagState 字段表。**Day 6 实施前必须决议**。

- [ ] **Step 1.2: 引入 struct DagState + struct NodeExecutionStatus**

编辑 `src/modules/scheduler/topo_scheduler.h:88` 之前插入：

```cpp
// --- Sprint 7 Day 5: DagState 纯函数式契约 ---
// 3 子函数 prepare_dag_state / dispatch_ready_nodes / handle_node_completion
// 共享此 struct, 不直接修改 TopoScheduler 成员。
struct DagState {
    std::unordered_map<NodePath, std::unique_ptr<Node>> nodes;  // DAG 节点副本
    std::unordered_map<NodePath, std::vector<NodePath>> reverse_edges;
    std::unordered_map<NodePath, std::vector<NodePath>> wait_for_dependents;
    std::unordered_map<NodePath, int> in_degree;
    std::queue<NodePath> ready_queue;
    std::unordered_set<NodePath> executed;
    int pending_count = 0;
    // --- 动态图支持 ---
    std::vector<ParsedGraph> dynamic_graphs;
    std::vector<NodePath> pending_dynamic_deps;  // 解析中的 dynamic wait_for 列表
};
struct NodeExecutionStatus {
    NodePath path;
    NodeStatus status = NodeStatus::PENDING;
    NodeResult result;
    int indegree = 0;
    std::vector<NodePath> dependents;
};
// --- END Sprint 7 Day 5 ---
```

**注**: 上述是基线 schema, **Day 5 brainstorm 决议可能调整字段名/增减**。本步骤按 brainstorm 结果定稿。

- [ ] **Step 1.3: 验证编译通过 + Day 2 7 测试零回归**

```bash
cd build && cmake --build . -j$(nproc) 2>&1 | tail -5
ctest -R test_scheduler --output-on-failure 2>&1 | tail -8
```

Expected: `[100%] Built target ...` + `100% tests passed, 0 tests failed out of 7` (test_scheduler 7 cases)

- [ ] **Step 1.4: Commit**

```bash
git add src/modules/scheduler/topo_scheduler.h
git commit -m "refactor(scheduler): introduce DagState struct (Sprint 7 Day 5)

Day 5 头脑风暴决议: DagState 含 nodes / reverse_edges / wait_for_dependents /
in_degree / ready_queue / executed / pending_count / dynamic_graphs /
pending_dynamic_deps 9 字段, 支持 3 子函数纯函数式契约。

[Day 5 brainstorm 调整如字段名/增删]

现有 7 Day 2 scheduler tests 零回归 (DagState 引入是 pure add, 行为未变)。
为 Day 6 改 3 子函数签名 (DagState& 形式) 准备。

Closes Sprint 7 spec scheduler-pipeline-tightened 进度 1/4."
```

**风险**: 🟢 低 — pure struct add, 无行为变更。
**回滚**: `git revert HEAD`。

---

## Task 2 (Day 6): 改 3 子函数签名为 (DagState&, ...) 形式

**Files:**
- Modify: `src/modules/scheduler/topo_scheduler.h:88-95` (3 子函数签名)
- Modify: `src/modules/scheduler/topo_scheduler.cpp:148-370` (`execute()` 调用方同步改) + 子函数体（暂保持原逻辑，先迁移到 DagState& 参数）
- Test: ctest 34/34 + test_scheduler 7/7

**Context:** 当前 3 子函数是私有成员方法，直接修改 `TopoScheduler` 成员 (`ready_queue_` 等)。Day 6 改签名为 `(DagState& state, ...)` 形式 — **子函数体仍暂时修改 `TopoScheduler` 成员**（pure refactor，零行为变更），Day 7-8 实施真实纯函数化。

### 子任务

- [ ] **Step 2.1: 改 .h 3 子函数签名**

将:
```cpp
std::optional<ExecutionResult> prepare_dag_state();
std::variant<std::monostate, NodeLookupResult, ExecutionResult>
dispatch_next_node(const Context& context);
ExecutionResult finalize_execution(const Context& context);
```

改为:
```cpp
std::optional<ExecutionResult> prepare_dag_state(DagState& state);
std::variant<std::monostate, NodeLookupResult, ExecutionResult>
dispatch_ready_nodes(DagState& state, const Context& context);  // Day 6 重命名为 dispatch_ready_nodes (spec 要求)
ExecutionResult finalize_execution(DagState& state, const Context& context);
std::optional<ExecutionResult> handle_node_completion(DagState& state, const NodeResult& result);  // Day 6 新增 handle_node_completion 声明 (spec 要求)
```

- [ ] **Step 2.2: 改 .cpp 子函数定义同步签名**

编辑 `src/modules/scheduler/topo_scheduler.cpp:608-700` 4 个子函数定义（L608 `prepare_dag_state` / L634 `dispatch_next_node` → `dispatch_ready_nodes` / L678 `finalize_execution`），每个加 `DagState& state` 参数。

**注意**: 子函数体**暂不修改内部成员访问**（仍 `ready_queue_.push(...)` 等），Day 7-8 才迁移到 `state.ready_queue`。本步骤仅是签名变更 — `state` 参数暂时未使用（编译器 `-Wunused-parameter` 警告可接受，commit 时注解 TODO）。

- [ ] **Step 2.3: 更新 `execute()` 调用方**

`topo_scheduler.cpp:148-170` (execute() 主 while 循环入口):
```cpp
// 旧:
auto prepare_result = prepare_dag_state();
auto dispatch_result = dispatch_next_node(context);
return finalize_execution(context);

// 新:
DagState state;
auto prepare_result = prepare_dag_state(state);
auto dispatch_result = dispatch_ready_nodes(state, context);
auto completion = handle_node_completion(state, /*last_result*/ {});
return finalize_execution(state, context);
```

**注**: 实际调用点可能多于 1 处（动态图循环、JUMP 处理等），需逐处同步。每处调用用 grep `prepare_dag_state\|dispatch_next_node\|finalize_execution` 找全。

- [ ] **Step 2.4: 验证编译 + Day 2 7 测试零回归**

```bash
cd build && cmake --build . -j$(nproc) 2>&1 | tail -10
ctest -R test_scheduler --output-on-failure 2>&1 | tail -5
```

Expected: 编译通过（可能 unused parameter warning）+ test_scheduler 7/7 PASS。

- [ ] **Step 2.5: Commit**

```bash
git add src/modules/scheduler/topo_scheduler.h src/modules/scheduler/topo_scheduler.cpp
git commit -m "refactor(scheduler): change 3 subfunction signatures to (DagState&, ...) form (Day 6)

改 3 子函数签名 (prepare_dag_state / dispatch_ready_nodes / finalize_execution)
+ 新增 handle_node_completion 声明, 接受 DagState& 参数。

dispatch_next_node → dispatch_ready_nodes (spec 命名要求)。
子函数体暂保持原逻辑 (仍直接改 TopoScheduler 成员), Day 7-8 实施
真实纯函数化迁移到 state.* 成员。

现有 7 Day 2 scheduler tests 零回归 (签名变更, 行为未变)。

Closes Sprint 7 spec scheduler-pipeline-tightened 进度 2/4 (命名 + 签名)."
```

**风险**: 🟡 中 — 签名变更涉及 4 子函数 + 多处 execute() 调用点, 需全面同步。编译失败可能性高。
**回滚**: `git revert HEAD`。

---

## Task 3 (Day 7): 重写 prepare_dag_state 为纯函数式 + 3 TDD 测试

**Files:**
- Modify: `src/modules/scheduler/topo_scheduler.cpp:608-633` (`prepare_dag_state` 函数体)
- Modify: `tests/test_scheduler.cpp` (Day 2 文件末尾加 3 新 TDD 测试)

**Context:** 当前 `prepare_dag_state` 是从 `execute()` L156-158 提取的小函数（仅解析 entry_point + push ready_queue），**未做**拓扑排序/入度计算（`build_dag()` 已做）。Day 7 实施真实纯函数化 + 完整 TDD 覆盖。

按 spec `prepare_dag_state_*: simple_linear / diamond / cycle_detection` 3 测试契约（Day 2 spec 已定义）。

### 子任务

- [ ] **Step 3.1: 写 `prepare_dag_state_simple_linear` 失败测试**

在 `tests/test_scheduler.cpp` 末尾追加（保持 Day 2 7 测试不变）:

```cpp
// Sprint 7 Day 7: 3 subfunction TDD tests
TEST_CASE("prepare_dag_state_simple_linear", "[scheduler][day7]") {
    // 3 节点线性 A→B→C, 验证 DagState.ready_queue 初始 = [A]
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
graph_type: subgraph
nodes:
  - id: A
    type: start
    next: ["/main/B"]
  - id: B
    type: assign
    assign: {x: "1"}
    output_keys: ["x"]
    next: ["/main/C"]
  - id: C
    type: end
```
)";
    agenticdsl::TopoScheduler scheduler({}, /*tools*/..., /*llm*/nullptr);
    scheduler.build_dag();
    // 验证通过执行: run_dsl 期望 success + x=1
    auto result = run_dsl_raw(markdown);
    REQUIRE(result.success);
    REQUIRE(result.final_context.contains("x"));
    REQUIRE(result.final_context["x"] == 1);
}
```

**注**: 完整 ParsedGraph JSON fixture 在实施时根据现有 `run_dsl` helper 调整（参考 Day 2 7 测试风格）。

- [ ] **Step 3.2: 跑测试确认 fail**

```bash
cd build && cmake --build . -j$(nproc) && ctest -R "prepare_dag_state_simple_linear" --output-on-failure
```

Expected: 新测试可能因 fixture JSON 不全 pass（与 baseline 同）或 fail。如果 pass（fixture 复用 OK），说明 baseline 已正确；继续 Step 3.3 添加更多断言。

- [ ] **Step 3.3: 写 `prepare_dag_state_diamond` 测试**

参考 simple_linear 模板，写 4 节点菱形 A→{B,C}→D：

```cpp
TEST_CASE("prepare_dag_state_diamond", "[scheduler][day7]") {
    // 4 节点菱形 A→{B,C}→D, 验证 ready_queue 初始 = [A]
    // ...
}
```

- [ ] **Step 3.4: 写 `prepare_dag_state_cycle_detection` 测试**

```cpp
TEST_CASE("prepare_dag_state_cycle_detection", "[scheduler][day7]") {
    // A→B→A 循环, 期望 runtime_error
    // 注: build_dag() 当前 throw cycle error (topo_scheduler.cpp L165-170), 不是 prepare_dag_state
    // 如果 build_dag 已检测, 本测试验证 build_dag throw, prepare_dag_state 委派
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
graph_type: subgraph
nodes:
  - id: A
    type: start
    next: ["/main/B"]
  - id: B
    type: tool_call
    tool: mock_cycle
    output_keys: ["b"]
    next: ["/main/A"]  # 循环
```
)";
    agenticdsl::TopoScheduler scheduler({}, ...);
    REQUIRE_THROWS(scheduler.build_dag());  // 期望 throw
}
```

- [ ] **Step 3.5: 重写 `prepare_dag_state` 函数体为纯函数式**

编辑 `topo_scheduler.cpp:608-633` 改为:

```cpp
std::optional<ExecutionResult> TopoScheduler::prepare_dag_state(DagState& state) {
    // Pure function: 仅读 TopoScheduler 不可变数据, 写入 state, 不修改外部成员
    if (state.ready_queue.empty()) {
        return ExecutionResult{false, "DAG state not initialized", {}, std::nullopt};
    }
    state.pending_count = static_cast<int>(state.ready_queue.size());
    return std::nullopt;
}
```

**注**: 真实纯函数化需要 DagState 含所有必要数据 (在 Task 1 brainstorm 决定)。此为示意。Day 7 实施时根据 DagState schema 调整。

- [ ] **Step 3.6: 跑 3 测试验证 PASS**

```bash
cd build && cmake --build . -j$(nproc) && ctest -R "prepare_dag_state_" --output-on-failure
```

Expected: 3/3 PASS。

- [ ] **Step 3.7: 跑 Day 2 7 测试 + 全量 34/34 零回归**

```bash
ctest --output-on-failure 2>&1 | tail -5
```

Expected: 34/34 PASS。

- [ ] **Step 3.8: Commit**

```bash
git add src/modules/scheduler/topo_scheduler.cpp tests/test_scheduler.cpp
git commit -m "refactor(scheduler): rewrite prepare_dag_state as pure function + 3 TDD tests (Day 7)

实施 spec scheduler-pipeline-tightened 3 测试:
- prepare_dag_state_simple_linear (3 节点线性, ready_queue=[A])
- prepare_dag_state_diamond (4 节点菱形, 拓扑序)
- prepare_dag_state_cycle_detection (A→B→A, build_dag throw)

prepare_dag_state 改为纯函数式: 仅读 immutable 数据 + 写 DagState,
不直接修改 TopoScheduler 成员。

现有 7 Day 2 tests + 34/34 ctest 零回归。

Closes Sprint 7 spec scheduler-pipeline-tightened 进度 3/4 (3 测试子集)."
```

**风险**: 🟠 Major — 纯函数化可能暴露隐藏的 `this` 依赖, 编译/链接失败可能性高。Day 5/8 Oracle 抽查。
**回滚**: `git revert HEAD`。

---

## Task 4 (Day 8): dispatch_ready_nodes + handle_node_completion 重写 + 修 L275-279 死代码 + 修访问一致

**Files:**
- Modify: `src/modules/scheduler/topo_scheduler.cpp:262-292` (L275-279 JOIN then-branch 死代码)
- Modify: `src/modules/scheduler/topo_scheduler.cpp` `dispatch_ready_nodes` + `handle_node_completion` 函数体重写
- Modify: `src/modules/scheduler/topo_scheduler.cpp:669` (修 `pending_dynamic_deps_` 访问)
- Modify: `tests/test_scheduler.cpp` (加 4 新测试)

**Context:**
- L275-279 死代码：`is_executing_fork_branches_` 在 L164 已置 false, 但 L275 仍检查该 flag
- L669 直接访问私有成员 `session_.pending_dynamic_deps_`, 应改用 `get_pending_dynamic_deps()` 访问器

### 子任务

- [ ] **Step 4.1: 修 L275-279 JOIN then-branch 死代码**

编辑 `topo_scheduler.cpp:273-292` 改为（删除 `is_executing_fork_branches_` 检查，因 L164 已清零）:

```cpp
if (current_node->type == NodeType::JOIN) {
    // 注: is_executing_fork_branches_ 在 execute() L164 finish_fork_simulation 时已清零
    // 故原 L275 if 条件总为 false, 是死代码。Day 8 直接处理 JOIN
    if (current_fork_branch_index_ == current_fork_branches_.size()) {
        // 所有分支完成, 处理 JOIN
        start_join_simulation(dynamic_cast<const JoinNode*>(current_node));
        finish_join_simulation(context);
        finish_fork_simulation();
        LOG_DEBUG("Join completed, merged context.");
    } else {
        // 分支未完, 重新入队等待
        ready_queue_.push(current_path);
        LOG_DEBUG("JoinNode " << current_path << " waiting for branches to finish.");
    }
    continue;
}
```

- [ ] **Step 4.2: 修 L669 `pending_dynamic_deps_` 访问一致**

编辑 `topo_scheduler.cpp:669` (dispatch_ready_nodes 内):
```cpp
// 旧:
if (session_.pending_dynamic_deps_.count(current_path) > 0) {
// 新:
if (session_.get_pending_dynamic_deps().count(current_path) > 0) {
```

验证: `grep "session_\.pending_dynamic_deps_" src/modules/scheduler/` 返回 0 命中。

- [ ] **Step 4.3: 写 `dispatch_ready_nodes_initial` 失败测试**

参考 Task 3 测试模板:
```cpp
TEST_CASE("dispatch_ready_nodes_initial", "[scheduler][day8]") {
    // 3 节点线性, 初始 ready=[A], 派发 1 节点
    // ... (同 simple_linear fixture)
    auto result = run_dsl_raw(markdown);
    REQUIRE(result.success);
}
```

- [ ] **Step 4.4: 写 `dispatch_ready_nodes_parallel` 测试**

```cpp
TEST_CASE("dispatch_ready_nodes_parallel", "[scheduler][day8]") {
    // 3 个独立节点（无依赖）
    // 验证 3 节点全被派发
}
```

- [ ] **Step 4.5: 写 `handle_node_completion_success` 测试**

```cpp
TEST_CASE("handle_node_completion_success", "[scheduler][day8]") {
    // 节点 A 完成 → B ready
    // 验证 A.status=Completed, B 加入 ready_queue
}
```

- [ ] **Step 4.6: 写 `handle_node_completion_failure` 测试**

```cpp
TEST_CASE("handle_node_completion_failure", "[scheduler][day8]") {
    // A 失败 → B,C 应 Skipped
    // 验证 A.status=Failed, B/C.status=Skipped
}
```

- [ ] **Step 4.7: 重写 `dispatch_ready_nodes` + `handle_node_completion` 函数体**

按 DagState 字段实施。`dispatch_ready_nodes(DagState& state, ...)` 遍历 `state.ready_queue` 派发；`handle_node_completion(DagState& state, const NodeResult&)` 更新 `state.executed` / `state.in_degree` / 失败传播。

**注**: 具体函数体由 Day 7 后的 DagState schema 决定, 标 `[Day 7 后细化]`。

- [ ] **Step 4.8: 跑 4 新测试 + 34/34 全量 + TSan 验证**

```bash
cd build && cmake --build . -j$(nproc) && ctest --output-on-failure 2>&1 | tail -5
cmake --preset tsan && ctest --output-on-failure 2>&1 | tail -5
```

Expected: 34/34 (含 4 新 day8) + TSan 0 race。

- [ ] **Step 4.9: Commit**

```bash
git add src/modules/scheduler/topo_scheduler.cpp tests/test_scheduler.cpp
git commit -m "refactor(scheduler): dispatch_ready_nodes + handle_node_completion pure functions + fix L275-279 + access consistency (Day 8)

实施 spec scheduler-pipeline-tightened 后 4 测试:
- dispatch_ready_nodes_initial (初始 ready 派发 1 节点)
- dispatch_ready_nodes_parallel (3 独立节点并发)
- handle_node_completion_success (完成触发下游)
- handle_node_completion_failure (失败传播, downstream Skipped)

修 L275-279 JOIN then-branch 死代码 (is_executing_fork_branches_ 已被 L164
finish_fork_simulation 清零, 原条件总 false, 改用 current_fork_branch_index_
== current_fork_branches_.size() 直接判断)

修 L669 pending_dynamic_deps_ 访问不一致 (直接访问私有成员 → get_pending_dynamic_deps()
访问器, 与 execute() 风格统一)

现有 7 Day 2 + 3 Day 7 + 4 Day 8 = 14 scheduler tests, 34/34 ctest + TSan 0 race 零回归。

Closes Sprint 7 spec scheduler-pipeline-tightened 全部 4/4 子项."
```

**风险**: 🟠 Major — 改 L275-279 涉及 JOIN 节点语义, 影响 14 测试。
**回滚**: `git revert HEAD`。

---

## Task 5 (Day 9): 收 `execute()` ≤ 60 行 + hub out_degree 验证 + Oracle 终审

**Files:**
- Modify: `src/modules/scheduler/topo_scheduler.cpp:149-370` (execute() 函数体)
- Test: `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l` ≤ 60
- Test: `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 out_degree < 30

### 子任务

- [ ] **Step 5.1: 测量当前 `execute()` 行数**

```bash
awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l
```

Expected: 当前 222 行。需收至 ≤ 60。

- [ ] **Step 5.2: 提取 4 内联子逻辑为子函数**

按 spec 要求提取:
- `resolve_dynamic_waits(DagState&)` — 当前 L184-229 动态 wait_for 解析
- `process_jump(DagState&, NodePath)` — 当前 L239-254 jump 处理
- `process_fork_join(DagState&)` — 当前 L262-292 fork/join
- `rebuild_dynamic_graph(DagState&)` — 当前 L321-362 动态图重建

每个子函数 ≤ 30 行, 接受 `DagState&` 参数。

- [ ] **Step 5.3: 重写 `execute()` 编排层**

```cpp
ExecutionResult TopoScheduler::execute(const Context& initial_context) {
    Context context = initial_context;
    DagState state;
    if (auto early = prepare_dag_state(state)) return *early;
    while (!state.ready_queue.empty() || !state.dynamic_graphs.empty()) {
        if (auto dispatch_result = dispatch_ready_nodes(state, context);
            std::holds_alternative<ExecutionResult>(dispatch_result)) {
            return std::get<ExecutionResult>(dispatch_result);
        }
        if (std::holds_alternative<std::monostate>(dispatch_result)) break;
        // ... 4 个子逻辑委派
    }
    return finalize_execution(state, context);
}
```

**目标**: ≤ 60 行。

- [ ] **Step 5.4: 验证 34/34 零回归 + 行数 ≤ 60 + hub out_degree < 30**

```bash
cd build && cmake --build . -j$(nproc) && ctest --output-on-failure 2>&1 | tail -5
awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l
```

用 mcp__code-review-graph__get_hub_nodes --top_n 5 验证 execute out_degree < 30 + 3 子函数 < 25。

- [ ] **Step 5.5: Oracle 终审 (session 必跑)**

委派 Oracle 跑 Day 9 抽查:
- 重点: execute 行数 / 3 子函数纯函数式 / L275-279 死代码消除 / pending_dynamic_deps_ 访问一致 / 14 测试覆盖
- 输出: PASS / FAIL + 偏离项

- [ ] **Step 5.6: Commit**

```bash
git add src/modules/scheduler/topo_scheduler.cpp
git commit -m "refactor(scheduler): extract 4 inline subfunctions + execute ≤ 60 lines (Day 9)

按 spec scheduler-pipeline-tightened 收 execute():
- 提取 4 内联子逻辑: resolve_dynamic_waits / process_jump / process_fork_join / rebuild_dynamic_graph
- execute() 从 222 行收至 ≤ 60 行 (纯编排层)
- 3 子函数 (prepare_dag_state / dispatch_ready_nodes / handle_node_completion) 纯函数式

验证:
- ctest 34/34 PASS (7 Day 2 + 3 Day 7 + 4 Day 8 + 33 baseline)
- execute 行数 ≤ 60 (实测)
- code-review-graph get_hub_nodes: execute out_degree < 30 + 3 子函数 < 25
- Oracle Day 9 抽查 PASS

Closes Sprint 7 spec scheduler-pipeline-tightened 全部 5/5 子项 + sprint-7 follow-up 进度 50%."
```

**风险**: 🟠 Major — execute 收 60 行需大改, 可能破坏 dynamic graph / jump / fork 边界 case。
**回滚**: `git revert HEAD`。

---

## Day 5-9 验证清单 (ship gate)

- [ ] V1: `awk` 验证 `execute()` ≤ 60 行
- [ ] V2: `ctest --output-on-failure` 34/34 PASS (含 Day 7+8 新测试)
- [ ] V3: `cmake --preset tsan && ctest --output-on-failure` 0 race
- [ ] V4: `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 execute out_degree < 30
- [ ] V5: `grep "session_\.pending_dynamic_deps_" src/modules/scheduler/ -r` 0 命中
- [ ] V6: L275-279 死代码已消除（grep "is_executing_fork_branches_" execute() 内仅 L161-167 一处使用）
- [ ] V7: `git log --oneline -5` 显示 5 个 Sprint 7 Day 5-9 commit
- [ ] V8: Oracle Day 5/8/9 三次抽查全 PASS

**Day 5-9 完成后**: Sprint 7 spec `scheduler-pipeline-tightened` Requirement 100% 达成。Day 10+ 继续 Phase 3 engine.cpp include 续推 + Phase 4 factory 测试 + Phase 5 plugin E2E + Day 16-17 ship gate 收尾 + archive。

---

## Self-Review (writing-plans skill 自检)

### 1. Spec 覆盖
- ✅ `scheduler-pipeline-tightened` 5/5 子项 (DagState / 命名 / 纯函数式 / execute ≤ 60 / hub out_degree < 30) — Task 1-5 一一覆盖
- ✅ 修 L275-279 死代码 + pending_dynamic_deps_ 访问一致 — Task 4

### 2. 占位符扫描
- ⚠️ Task 1 Step 1.1 标 `[Day 5 brainstorm 调整如字段名/增删]` — 这是透明标记非占位符
- ⚠️ Task 3 Step 3.5 标 `[Day 5 brainstorm 决定]` 示意代码 — 实施时根据 DagState schema 定稿
- ⚠️ Task 4 Step 4.7 标 `[Day 7 后细化]` — 因 DagState schema 决定后再实施
- 其他步骤无占位符

### 3. 类型一致性
- `DagState` 字段 (Task 1) 与 `prepare_dag_state(DagState&)` (Task 2-3) / `dispatch_ready_nodes(DagState&)` (Task 2+4) / `handle_node_completion(DagState&)` (Task 2+4) 一致
- `NodeExecutionStatus` 字段 (Task 1) 与 `handle_node_completion` (Task 4) 使用一致
- `get_pending_dynamic_deps()` 访问器 (Task 4 Step 4.2) 与现有 `execution_session.h` 一致

### 4. 回滚策略
- 每个 Task 单 commit, `git revert HEAD` 即回滚
- Task 1-2 纯重构, 回滚零风险
- Task 3-5 含行为变更, 回滚需重新跑 ctest 验证

---

## Brainstorm 决议记录 (Task 1 Step 1.1 填充)

> **Day 5 头脑风暴后填写, Day 6 实施前定稿**

| 字段 | 决策 | 理由 |
|---|---|---|
| DagState 字段 | [ ] | [ ] |
| Fork/Join 状态位置 | [ ] | [ ] |
| pending_dynamic_deps_ 去向 | [ ] | [ ] |
| [其他 Day 5 brainstorm 议题] | [ ] | [ ] |

---

## Execution Handoff

按 writing-plans skill 标准结尾，**2 个执行选项**:

### 选项 1: Subagent-Driven (推荐)

**REQUIRED SUB-SKILL**: `superpowers/subagent-driven-development`
- 每个 Task 一个 fresh subagent（隔离上下文）
- 两阶段 review (Task 完成后 review, Reviewer 通过后下一个)
- 适合多 Task 连续实施，节奏快
- Day 5 Task 1 必含 brainstorming 会议（推荐用 superpowers:brainstorming skill）

### 选项 2: Inline Execution

**REQUIRED SUB-SKILL**: `superpowers/executing-plans`
- 当前 session 顺序执行
- 适合小范围 (1-2 Task) 一次性完成
- 检查点暂停让用户 review

**我的推荐**: 选项 1 — Day 5-9 是 5 个独立 Task（含 brainstorming），subagent 隔离可避免上下文污染，Task 1（brainstorm + DagState）与 Task 2-5（实施）改动文件部分重叠但时间错开, 适合 fresh subagent。

**附加建议**:
- 准备 git worktree 隔离（`superpowers/using-git-worktrees` skill）
- Day 5/8/9 三个 commit 后各跑一次 Oracle 抽查（避免 Sprint 6 偏离再现）
- Day 5 启动时先跑 `superpowers:brainstorming` 决定 DagState 字段，再委派 Task 1 subagent

**问**: 选哪个？

- 选 1 → 我用 git worktree 准备 Day 5-9 隔离分支, 委派 5 个 subagent 实施
- 选 2 → 我直接开始 Task 1 brainstorm + 实施
