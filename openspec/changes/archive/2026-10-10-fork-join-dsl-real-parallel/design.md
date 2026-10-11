# Design: Fork/Join DSL real parallel semantics repair (path 2)

## 概述

本设计解决 4 个 fork/join 结构性 bug (BUG-2 routing-JOIN + BUG-3 fork no update_successors + BUG-4 branches schema + BUG-5 task next chain)。Oracle + Metis dual-agent review 静态验证 100% 确认 BUG-2/3/4 是 fork/join 机制空转的根因, BUG-5 是症状层设计缺陷。修复 1-5 按依赖关系依次解锁 (D2 依赖 D4, D5 依赖 D4, D6 依赖 D2+D5)。

## D1: JOIN routing 前移位置与对称模式

**决策**: 把 JOIN 类型检查从 `topo_scheduler.cpp:228-230` (post-execute 死代码) 前移到 line 192-196 模式 (与 FORK 对称)。

**实施**:
```cpp
// 当前 line 192-204 (FORK 在 execute_node 之前):
if (found.node->type == NodeType::FORK) {
    handle_fork_node(found.node, context);
    executed_.insert(found.path);
    continue;
}
// 新增 (紧跟其后, JOIN 对称):
if (found.node->type == NodeType::JOIN) {
    process_fork_join(found.node, context);
    executed_.insert(found.path);
    update_successors(found.node, found.path);  // D5 协同
    continue;
}
auto session_result = session_.execute_node(found.node, context);
// 删除原 line 228-230 (死代码 JOIN 检查)
```

**理由**: 与 FORK 完全对称; 消除 `node_executor.cpp:66-68` 的 logic_error 抛出路径; 让 `process_fork_join` 真正被调用。

**反模式**: 只前移 FORK 不前移 JOIN (Day-1 routing fix `b51adea` 的原 bug) — 必现死代码。

## D2: fork path update_successors 调用时机

**决策**: 在 fork 分支 `continue` 前调 `handle_node_completion(state, NodeResult{true, ""}, found.node, found.path)`, 复用现有 handler 推进 fork.next 边。

**实施**:
```cpp
if (found.node->type == NodeType::FORK) {
    handle_fork_node(found.node, context);
    executed_.insert(found.path);
    NodeResult fake_result;  // 成功路径
    fake_result.success = true;
    if (auto err = handle_node_completion(state, fake_result, found.node, found.path); err) return *err;
    continue;
}
```

**理由**:
- `handle_node_completion` (line 711-717) 已实现 `update_successors(current_node, current_path)` 调用, 复用避免代码重复
- 传 NodeResult{true} 表示成功执行 (fork 是调度语义, 无实际执行结果)
- 与普通节点 (TOOL_CALL, LLM_CALL) 在 execute_node 后调 handle_node_completion 的模式对称

**反模式**: 在 fork 处理中直接调 `update_successors` (跳过 handler) — 破坏一致性, 未来加 tracing/重试逻辑会漏掉 fork 路径。

## D3: branches schema 双规范兼容策略

**决策**: make_fork + make_join 支持**双 schema** (向后兼容, 旧 schema 标 deprecated):

```cpp
// make_fork (node_factory.cpp:208-223 替换):
std::unique_ptr<Node> make_fork(const NodePath& path, const nlohmann::json& j) {
  auto ctx = parse_context(j);
  std::vector<NodePath> branches;
  
  // 新规范 (扁平, lib/loop/*.agent.md 使用): branches: [{path: "/main/task_a"}, ...]
  if (j.contains("branches") && j["branches"].is_array()) {
    for (const auto& b : j["branches"]) {
      if (b.contains("path")) branches.push_back(b["path"].get<std::string>());
    }
  }
  // 旧规范 (嵌套字符串数组, deprecated): fork: {branches: ["/main/task_a"]}
  else if (j.contains("fork") && j["fork"].contains("branches")) {
    LOG_WARN("DEPRECATED: nested fork.branches schema, migrate to flat branches:[{path}]");
    for (const auto& b : j["fork"]["branches"]) branches.push_back(b.get<std::string>());
  }
  
  auto node = std::make_unique<ForkNode>(path, std::move(branches), std::move(ctx.next_paths));
  apply_context(*node, ctx);
  return node;
}

// make_join (line 226-247 替换):
// 同样模式: 优先扁平 wait_for, fallback 嵌套 join.wait_for (deprecated warn)
```

**理由**:
- lib/loop/fork_join.agent.md 用扁平 schema (DSL 作者自然写法, 设计意图清晰)
- 旧 fork_join_example.agent.md (C++ ForkJoinLoop 路径) 用嵌套 schema — 不能破坏
- 双兼容避免一次清退所有旧 DSL 文件

**反模式**: 硬切单一 schema — 破坏 1 个 example + 1 个 fixture + 潜在用户 DSL。

## D4: build_dag 防双执行机制

**决策**: build_dag 识别 fork→branch 边, 把 fork_node 算入 branch_path 的 in_degree (防止 seed 阶段把 branch 节点加入 in_degree=0 队列)。

**实施** (build_dag 函数内):
```cpp
// 当前: for each node, if (in_degree[path] == 0) seed_ready_queue
// 新增: 若 node 是 ForkNode, 对每个 branch_path:
//   - 不 seed branch_path (即使 in_degree[branch_path] == 0)
//   - 保留 fork→branch 的 reverse_edges 记录 (execute_single_branch 内部跟踪)
//   - 但 branch_path 在主循环仍可通过 fork→branch 调度 (非 seed, 显式触发)
```

**实际方案**: 在 `seed_initial_ready_queue` (line 139-148) 加 ForkNode 检查 — 是 ForkNode 的 branches 不 seed。

```cpp
void TopoScheduler::seed_initial_ready_queue() {
  for (const auto& node_ptr : all_nodes_) {
    NodePath path = node_ptr->path;
    if (in_degree_[path] == 0) {
      // 防双执行: ForkNode 的 branches 不 seed (由 execute_fork_branches 显式触发)
      if (auto* fork_node = dynamic_cast<const ForkNode*>(node_ptr.get())) {
        for (const auto& branch_path : fork_node->branches) {
          if (branch_path == path) continue;  // 跳过 branches
        }
      }
      ready_queue_.push(path);
    }
  }
}
```

**理由**: 当前 BUG-4 状态下 branches 恒为空, fork/join 机制空转, task_a/b/c 作为普通节点串行执行 — BUG-4 修通后会暴露双执行 (seed 一次 + execute_single_branch 一次)。本 D4 是必要防护。

**反模式**: 让 branches 既 seed 又在 execute_single_branch 中执行 (无防护) — 真 LLM 下 2× token 消耗 + 非幂等工具副作用翻倍。

## D5: Test 断言 steps >= 6 (非原 R3 估计 7)

**决策**: 修正 design.md R3 风险 (原 estimate `steps >= 7` 物理不可达) — `steps >= 6`。

**理由**:
- fork/join 是调度语义, 不产生 trace (`trace_exporter.cpp:9` on_node_start 由 ExecutionSession::execute_node 调, 但 FORK/JOIN 不走 execute_node, D1 后更不会)
- 可 trace 节点 = start + task×3 + synthesize + end = 6
- R3 原 estimate 7 包含 fork 或 join 各 1 个, 物理不可达

**实施**:
```cpp
// test_loop_agent_fork_join.cpp Test 4:
REQUIRE(run_ret.value("success", false) == true);
REQUIRE(run_ret.value("steps", 0) >= 6);  // start + task×3 + synthesize + end
```

**反模式**: 坚持 `steps >= 7` 断言 (R3 误判) — 物理必 FAIL, 不可调试。

## D6: 向后兼容策略

**决策**: 不破坏任何现有 example / fixture / test binary。

**清单**:
- `examples/pdk_chat_demo/dsl/fork_join_example.agent.md` (用 `type: fork_join`, 走 C++ ForkJoinLoop) 零变化
- `examples/pdk_chat_demo/tests/fixtures/fork_join_golden*.agent.md` 零变化
- `tests/test_pdk_fork_join.cpp` 5 cases 零回归
- `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 1 + Test 3 零退化

**验证**: 全量 ctest -LE must_realllm 零回归 (per AGENTS.md Pattern #11 ship gate)。

## D7: 回归守卫覆盖范围

**决策**: 双层守卫 (核心 tree + examples tree):

**核心 tree** (`tests/test_fork_join_dsl_real_parallel.cpp` 新增 binary):
- Case 1: parser 双 schema 兼容 (新扁平 / 旧嵌套)
- Case 2: build_dag 防双执行 (fork→branch 不 seed)
- Case 3: routing-FORK 前移 (FORK 不走 execute_node)
- Case 4: routing-JOIN 前移 (JOIN 不走 execute_node)
- Case 5: update_successors on fork path (post-fix in_degree 正确递减)

**Examples tree** (`examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 4 新增):
- 完整 e2e: 3 branch 真并行 + join 合并 + synthesize llm_call + end
- 断言: success=true + steps>=6

## D8: 不变更 ADR-0089 v1.4 ForkJoinLoop 范围

**决策**: 本 change 不触 C++ ForkJoinLoop / DomainWorkerPool / ADR-0087 benchmark。

**理由**:
- ADR-0089 v1.4 §D8.inv.forkjoin: ForkJoinLoop 维持现状
- ADR-0087 4-worker 3.3× speedup 不退化 (本 change 只改 TopoScheduler 调度 + parser + DSL 文件)
- DomainWorkerPool 4 jthread 不变 (per ADR-0020 §3.2)

**验证**: test_pdk_fork_join.cpp 5 cases PASS + ADR-0087 benchmark 复跑 ≥ 3× 数据点不退化 (post-merge verification)。

## Risks

| ID | 风险 | 缓解 |
|----|------|------|
| R1 | build_dag 防双执行改动可能影响其他 fork/join 用户 | 全量 ctest + 回归守卫 Case 2 |
| R2 | JOIN 前移 + update_successors 可能改变 join_results 触发顺序 | 回归守卫 Case 4+5 |
| R3 | branches 双 schema 兼容可能让 parser 行为不可预测 | 双 schema 互斥 (新优先, 旧 fallback warn) |
| R4 | steps >= 6 断言可能因 LLM mock 行为变化 FAIL | Test 4 用 MockLLMProvider 稳定 canned response |
| R5 | 真 LLM 下 branches 并行可能暴露 PlanExecute 类似的并发 race | must_realllm test 验证 (本期覆盖 mock, 真 LLM 留 follow-up) |
