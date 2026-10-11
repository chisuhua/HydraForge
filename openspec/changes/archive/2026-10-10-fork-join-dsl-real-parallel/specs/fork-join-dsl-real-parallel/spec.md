# Spec: fork-join-dsl-real-parallel (fork/join 调度机制结构性 bug 全修)

**关联 OpenSpec change**: `openspec/changes/2026-10-10-fork-join-dsl-real-parallel/`
**supersede**: `archive/2026-10-10-fix-fork-join-agent-next-chain-blocked-by-pattern8-dual-agent-review/`
**目标**: 修复 4 个 fork/join 结构性 bug (BUG-2 routing-JOIN + BUG-3 fork no update_successors + BUG-4 branches schema + BUG-5 task next chain), 让 `lib/loop/fork_join.agent.md` 完整 e2e 真 LLM 闭环跑通. 解除 `.rddf/improvements/fork_join_dsl_migration.md` 全部 BUG 状态 Gate.

---

## ADDED Requirements

### Requirement: parser MUST 支持 fork/join 节点扁平 schema (与旧嵌套 schema 向后兼容)

DSL `fork` 节点的 `branches` 字段 MUST 同时支持:
- 新规范 (扁平对象数组): `branches: [{path: "/main/task_a"}, {path: "/main/task_b"}]`
- 旧规范 (嵌套字符串数组, deprecated): `fork: {branches: ["/main/task_a", "/main/task_b"]}`

`make_fork` (node_factory.cpp) MUST 优先扁平 schema, fallback 嵌套 schema + 记录 `LOG_WARN` 标记 deprecated. `make_join` 的 `wait_for` 字段 MUST 同等支持扁平 (`wait_for: [...]`) + 嵌套 (`join: {wait_for: [...]}`) 双 schema.

#### Scenario: DSL 扁平 branches schema 正确解析

- **WHEN** DSL `fork` 节点用扁平 `branches: [{path: "/main/x"}, {path: "/main/y"}]` 写法
- **THEN** `make_fork` 解析后 `ForkNode.branches` 应含 `["/main/x", "/main/y"]`
- **AND** 不触发 deprecated `LOG_WARN`
- **AND** 后续 `start_fork_simulation` 能正确启动 2 个 branch 模拟

#### Scenario: 旧嵌套 branches schema 仍兼容 (向后兼容)

- **WHEN** DSL `fork` 节点用旧嵌套 `fork: {branches: ["/main/x"]}` 写法 (例如 `examples/pdk_chat_demo/dsl/fork_join_example.agent.md`)
- **THEN** `make_fork` 解析后 `ForkNode.branches` 应含 `["/main/x"]`
- **AND** 触发 deprecated `LOG_WARN` 标记 (但不影响执行)
- **AND** `examples/pdk_chat_demo/tests/fixtures/fork_join_golden*.agent.md` 等旧 fixture 零破坏

#### Scenario: DSL 扁平 wait_for schema 正确解析

- **WHEN** DSL `join` 节点用扁平 `wait_for: ["/main/a", "/main/b"]`
- **THEN** `make_join` 解析后 `JoinNode.deps` 含 `["/main/a", "/main/b"]`
- **AND** `start_join_simulation` 正确合并依赖节点输出

### Requirement: TopoScheduler 主循环 MUST 把 JOIN 节点路由到 execute_node 之前 (与 FORK 对称)

JOIN 节点是调度语义 (非节点执行语义), 必须在 `execute_node` 之前路由. 当前 `topo_scheduler.cpp:228-230` 的 post-execute JOIN 检查是死代码 (因为 execute_node 先抛 `logic_error("ForkNode/JoinNode reached NodeExecutor")`), MUST 前移到 line 192-196 模式 (与 FORK 对称).

#### Scenario: JOIN 节点不抛 logic_error (R4 主场景)

- **WHEN** TopoScheduler dispatch 一个 `NodeType::JOIN` 节点
- **THEN** `session_.execute_node(found.node, context)` **不**被调用
- **AND** `NodeExecutor::execute_node` 不被触发, 不抛 `std::logic_error("JoinNode reached NodeExecutor - scheduler routing bug")`
- **AND** `process_fork_join(found.node, context)` 在 execute_node 之前被调用

#### Scenario: JOIN 前移与 FORK 对称

- **WHEN** 比较 FORK (line 192-196) 与 JOIN (新前移块) 处理代码
- **THEN** 两者结构对称: 类型检查 → 调对应 handler → `executed_.insert` → `continue`
- **AND** `update_successors` 在两者处理后都被调 (fork path 加 update_successors 是 R5 范畴)

### Requirement: TopoScheduler 主循环 fork 路径 MUST 调 update_successors 推进 fork.next 边

FORK 节点处理 (`handle_fork_node` + `executed_.insert` + `continue`) MUST 推进 fork.next 边 (例如 fork_tasks → join_results). 当前主循环 fork 路径**不调 update_successors** (`handle_node_completion` 未被调用), fork.next 是死边, in_degree 永不为 0.

#### Scenario: fork_tasks 处理后 in_degree 正确递减 (R5 主场景)

- **WHEN** `lib/loop/fork_join.agent.md` 加载后 build_dag 完成, `in_degree["/main/join_results"]` 初始值 = 7 (fork.next +1 + task×3.next +3 + wait_for +3)
- **AND** 处理 fork_tasks 节点, 调 `handle_node_completion(state, NodeResult{true}, fork_node, fork_path)`
- **THEN** `in_degree["/main/join_results"]` 减 1 (从 7 → 6, 因 fork.next 边被推进)
- **AND** 后续 task_a/b/c 完成时各减 2 (next + wait_for), join_results 最终 in_degree = 6 - 6 = 0
- **AND** join_results 进入 `ready_queue_` 并被 dispatch

### Requirement: lib/loop/fork_join.agent.md task_a/b/c next 字段 MUST 指向 join_results

DSL 文件中 task_a/b/c 三个 tool_call 节点的 `next:` 字段 MUST 指向 `/main/join_results` (而非 `/main/end`). synthesize 节点的 `next: [/main/end]` 保持不变 (synthesize→end 是正确设计).

#### Scenario: fork_join.agent.md next 链 DAG 拓扑正确

- **WHEN** 加载 `lib/loop/fork_join.agent.md` 后 build_dag 完成
- **THEN** `task_a.next[0] == "/main/join_results"` (line 26 修复)
- **AND** `task_b.next[0] == "/main/join_results"` (line 34 修复)
- **AND** `task_c.next[0] == "/main/join_results"` (line 42 修复)
- **AND** `synthesize.next[0] == "/main/end"` 保持不变 (line 56 不变)
- **AND** `end.next` 为空数组 (终止节点)

### Requirement: build_dag MUST 防止 fork branches 双重执行

当 fork 节点有 non-empty branches 时, branches 节点 MUST NOT 被 seed 到 `ready_queue_` 作为 in_degree=0 节点 (避免主循环先执行一次, `execute_fork_branches` 再执行一次的双执行问题).

#### Scenario: fork branches 不在主循环 ready_queue_ (R7 主场景)

- **WHEN** `lib/loop/fork_join.agent.md` 加载 + build_dag + seed_initial_ready_queue 完成
- **THEN** `ready_queue_` 在初始 seed 阶段**不**包含 `task_a`, `task_b`, `task_c`
- **AND** task_a/b/c 只在 fork_tasks 处理后, `execute_single_branch` 显式调度时被执行
- **AND** 每个 branch 节点在 `result.steps` 中只被 trace 1 次 (不重复计数)

### Requirement: examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp Test 4 (新) MUST 验证 fork/join 真 e2e

Test 4 (新增) MUST 验证完整 fork/join e2e:
- 加载 `lib/loop/fork_join.agent.md` 经 `loop/run loop_type=fork_join` + MockLLMProvider
- 断言 `result.success == true`
- 断言 `result.steps >= 6` (start + task×3 + synthesize + end = 6; fork/join 是调度语义不产生 trace, 可 trace = 6 而非原 R3 估计的 7)
- 断言 result.response 含 task_a/b/c 输出

#### Scenario: Test 4 fork/join 完整 e2e PASS (R8 主场景)

- **WHEN** `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp:Test 4` 跑 `loop/run fork_join` + MockLLMProvider
- **THEN** `result.success == true`
- **AND** `result.steps >= 6`
- **AND** result 含 3 branch (task_a/b/c) 输出聚合
- **AND** Test 1 (mock fallback) + Test 2 (loop/process_task 注册) + Test 3 (DSL schema) 零退化
- **AND** UNMET dependencies / `logic_error` / `Template render error` 错误均不出现

---

## MODIFIED Requirements

### Requirement: j-9 parser schema 一致性约束 (增强)

DSL parser 对 `fork` / `join` 节点的 schema MUST 支持扁平 + 嵌套双规范.

#### Scenario: 双 schema 互斥 (新优先, 旧 fallback)

- **WHEN** DSL 文件同时含 `branches: [...]` 和 `fork: {branches: [...]}` (schema 冲突)
- **THEN** parser MUST 优先扁平 schema, 忽略嵌套 schema
- **AND** 不抛 parse error, 不触发 `LOG_WARN` (因为新规范胜出)

---

## REMOVED Requirements

(none)

---

## 不变更需求 (RECAP, 与既有 spec 一致)

### Requirement: C++ ForkJoinLoop 行为零变化

ADR-0089 v1.4 §D8.inv.forkjoin: ForkJoinLoop class 不在本 deprecation 范围, 维持现状. DomainWorkerPool 4-worker 不变, ADR-0087 benchmark 不退化.

#### Scenario: test_pdk_fork_join.cpp 5 cases 全 PASS

- **WHEN** 跑 `tests/test_pdk_fork_join.cpp` (C++ ForkJoinLoop 4-worker 路径)
- **THEN** 5 cases 全 PASS, 行为零变化
- **AND** ADR-0087 benchmark 复跑 ≥ 3× 数据点不退化