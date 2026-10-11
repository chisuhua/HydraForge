# Spec: fork-dsl-context-propagation (DSL fork 节点 working context 快照传递修复)

**关联 OpenSpec change**: `openspec/changes/2026-10-10-fix-fork-dsl-context-propagation/`
**目标**: 修复 BUG-2 (fork 节点未快照 working context 传递给各 branch 节点), 让 `lib/loop/fork_join.agent.md` 作为 canonical 生产路径真 LLM 闭环可用. 解除 `.rddf/improvements/fork_join_dsl_migration.md` BUG-2 Gate 条件.

---

## ADDED Requirements

### Requirement: DSL fork 节点 MUST 快照 working context 并传递到各 branch 节点

DSL fork 节点 (`type: fork`) 在 `TopoScheduler::handle_fork_node` / `start_fork_simulation` / `execute_fork_branches` 执行链中 **MUST** 快照 fork 前的 working context (含 `user_input` / `system_prompt` / `history` / `context` 等已注入 template variables), 各 branch 节点执行时继承此 context. branch 节点的 `prompt_template: "{{user_input}}"` (或类似 template variable) **MUST** 渲染成功, 不抛 `Template render error: variable 'user_input' not found`.

#### Scenario: DSL fork_join.agent.md 端到端成功执行 (BUG-2 主场景)

- **WHEN** `lib/loop/fork_join.agent.md` 经 `loop/run loop_type=fork_join` + MockLLMProvider 调用 (per `test_loop_agent_fork_join.cpp:Test 2` 模式)
- **THEN** fork 节点的 3 个 branch (task_a/b/c) `prompt_template: "{{user_input}}"` template render 成功
- **AND** `result.success == true`
- **AND** `result.error` 字段不含 `"Template render error: variable 'user_input' not found"` 或 `"variable 'user_input'"`
- **AND** `result.steps >= 3` (3 branch + join + synthesize)

#### Scenario: fork 节点 working context 完整快照

- **WHEN** DSL 子图 main_loop 起始 working context 含 `{{user_input}}`, `{{system_prompt}}`, `{{history}}`, `{{context}}` 4 个已注入 template variables
- **THEN** fork 节点执行时, 各 branch 节点 working context 仍含这 4 个 keys
- **AND** snapshot map 在 fork 完成后正确清理 (per `enforce_snapshot_budget` 机制)

#### Scenario: snapshot 路径 key 一致性

- **WHEN** `execute_fork_branches` line 331 用 `current_fork_node_path_.value()` 从 `ContextEngine::get_snapshot(key)` 取 snapshot
- **THEN** 该 key 与 `execute_node` line 177 `save_snapshot(node->path, context_with_resources)` 保存的 key **一致** (即 `current_fork_node_path_ == fork_node->path` 全程保持)
- **AND** `get_snapshot` 返回非 nullptr (找到 snapshot)

#### Scenario: fork + join + synthesize 完整图执行

- **WHEN** DSL `fork_join.agent.md` 经 `loop/run` + mock provider
- **THEN** fork 节点展开 3 branch (task_a/b/c 调用 `loop/process_task`)
- **AND** branch 节点 `loop/process_task` 经 `run_process_task` (per fork-join-dsl-migration commit `f5836e4` 修复) 调 mock provider
- **AND** JOIN 节点 `process_fork_join` 合并 3 branch 结果
- **AND** synthesize llm_call 节点调 mock provider, 含 fork 前 working context variables (如 `{{result_a}}`)
- **AND** end 节点终止子图, `result.success == true`

### Requirement: 调度器状态机 YIELD 后 resume fork 仍正确

YIELD 暂停 + resume 后, fork 节点的 working context 快照传递行为 MUST 正确恢复, 不退化.

#### Scenario: YIELD 后 resume fork 节点

- **WHEN** DSL 子图含 llm_call (触发 YIELD) 与 fork 节点, YIELD 后 resume
- **THEN** resume 后 fork 节点 working context 快照传递行为与 YIELD 前一致
- **AND** `current_fork_node_path_` 不被 YIELD resume 错位重置
- **AND** branch 节点的 template render 仍成功 (无 `Template render error`)

### Requirement: C++ ForkJoinLoop 4-worker 行为不变 (regression guard)

C++ `ForkJoinLoop` (`include/agenticdsl/pdk/agent_loops/fork_join_loop.h`) + `DomainWorkerPool` 4 jthread 行为 MUST 零变化 (per fork-join-dsl-migration D8.inv.forkjoin-class-unchanged).

#### Scenario: C++ ForkJoinLoop 单元测试全过

- **WHEN** `tests/test_pdk_fork_join.cpp` 5 cases 直接调用 `ForkJoinLoop::run`
- **THEN** 所有 5 cases PASS (3-branch并发 / 1-branch fail-fast / degenerate / 4-branch merge顺序 / exception isolation)
- **AND** `DomainWorkerPool(4)` 4 jthread 行为不变

#### Scenario: ADR-0087 benchmark 4-worker 3.3× speedup 不退化

- **WHEN** 跑 ADR-0087 benchmark 真实 LLM 4-worker 并发 vs 串行 baseline
- **THEN** speedup 数据点 ≈ 3.3× (per ADR-0087 step 5.1 commit `13ac53f`)
- **AND** 4-worker 并发总耗时 ≤ 4 × 串行基线 (per `DomainWorkerPool` 约束)

### Requirement: snapshot 机制不破坏其他使用场景

DSL fork 上下文快照修复 MUST 不破坏其他使用 `ContextEngine::save_snapshot` / `get_snapshot` 的场景 (e.g., DSL_CALL / GENERATE_SUBGRAPH / ASSERT).

#### Scenario: DSL_CALL / GENERATE_SUBGRAPH / ASSERT snapshot 仍正确

- **WHEN** DSL 子图含 fork + 其他需要 snapshot 的节点类型 (DSL_CALL / GENERATE_SUBGRAPH / ASSERT)
- **THEN** fork 节点 snapshot 与其他节点 snapshot 互不干扰
- **AND** `enforce_snapshot_budget` 仍正确清理过期 snapshot (无 memory leak)

#### Scenario: 全量 ctest 零回归

- **WHEN** 跑 `ctest --test-dir build -LE must_realllm` (baseline 265 tests)
- **THEN** 100% PASS (零回归)
- **AND** `ctest --test-dir build/examples/pdk_chat_demo/tests -R loop_agent` 100% PASS (含升级后的 Test 2)
- **AND** `ctest --test-dir build -L must_realllm` 真 LLM 12/12 PASS

## MODIFIED Requirements

### Requirement: TopoScheduler fork 节点处理契约更新

`TopoScheduler::run_main_loop` 与 `handle_fork_node` / `start_fork_simulation` / `execute_fork_branches` 的工作流程 MUST 修正 working context 快照传递机制 (per OpenSpec design.md D2 选择方案).

#### Scenario: fork 节点执行时 working context snapshot 语义

- **WHEN** DSL fork 节点经 `handle_fork_branches_block` 进入 `execute_fork_branches`
- **THEN** fork 节点的 working context 在各 branch 节点 `execute_single_branch` 执行时**继承 fork 前的内容** (含 `{{user_input}}` 等 template variables)
- **NOTE**: 实施阶段需先 RED 失败堆栈分析, 然后选 D2 候选方案 (A: 绕过 snapshot / B: 修 path / C: 修 key / D: 修语义)

#### Scenario: `current_fork_node_path_` 全局一致性

- **WHEN** fork 节点执行链中 (从 `handle_fork_node` 到 `execute_fork_branches` 到 `process_fork_join`)
- **THEN** `current_fork_node_path_` 全程保持 `fork_node->path` 值, 不被中间步骤重置或修改

#### Scenario: YIELD 与 fork 路由互不干扰 (per fork-join-dsl-migration Phase 3 spec)

- **WHEN** DSL 子图含 llm_call (YIELD 触发点) 与 fork 节点
- **THEN** YIELD 状态机正确保存/恢复 (返回 YIELDED), FORK 路由正确 (execute_node 不被调用 for FORK 节点)
- **AND** 恢复后 (resume) FORK 节点仍按 D1.inv.execute-order 处理 + working context 快照正确