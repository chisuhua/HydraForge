## Why

`fork-join-dsl-migration` OpenSpec change (`openspec/changes/archive/2026-10-10-fork-join-dsl-migration/`, commits `7e81c2f` + `469f495` ship 2026-10-10) 重新调查 BUG-2 后确认真实根因:

**BUG-2 现象** (2026-10-10 实测): 跑 `lib/loop/fork_join.agent.md` 实际错误是 `Node execution failed: Template render error: variable 'user_input' not found` (来自 task_a 节点的 `prompt_template: "{{user_input}}"` template render). 不是 BUG-2 原假设 ("ForkNode/JoinNode reached NodeExecutor" routing bug).

**BUG-2 根因** (经 commit `469f495` 文档化):
1. `TopoScheduler::run_main_loop` (line 175) 在 `execute_node` 之前调 `handle_fork_branches_block()`
2. `handle_fork_branches_block()` → `execute_fork_branches()` (line 327-353)
3. `execute_fork_branches` line 331: `const Context* fork_snapshot = session_.get_context_engine().get_snapshot(current_fork_node_path_.value())` — 从 snapshot map 拿 fork 前的 context
4. `execute_fork_branches` line 340: `Context branch_initial_ctx = *fork_snapshot` — 复制 snapshot
5. `execute_single_branch(branch_path, branch_initial_ctx)` 用这个 ctx 跑 branch 节点
6. branch 节点的 `prompt_template: "{{user_input}}"` template render 失败 — **branch_initial_ctx 应含 user_input 但实际不含**

**根因调查** (本次 change 实施阶段应继续深入):
- 假设 1: snapshot 保存的 `context_with_resources` 缺少 `user_input` (main_loop 起始未传入)
- 假设 2: snapshot 保存正确, branch 执行时被覆盖或 evict
- 假设 3: `get_snapshot` key mismatch (`current_fork_node_path_` ≠ snapshot 保存时的 key)
- 假设 4: snapshot 保存的 ctx 序列化/反序列化丢失 user_input 等顶层 scalar

**OpenSpec change 立项必要**: `fork-join-dsl-migration.md` tracker 明确"BUG-2 是 Gate 条件, 不修 DSL fork_join 路径无法作为 canonical 生产路径". 当前 self-evolve `fork-join-dsl-migration` partial 已 ship (BUG-3 修, BUG-2 文档化). 本 change 是其 Phase 3 续作 — 实施 BUG-2 实际修复.

## What Changes

- **修复 1 (BUG-2 根因 RED)**: 写最小复现 test — 直接调 `engine->run(fork_join.agent.md)` 加 mock provider, 断言 success=true + response 含 task_a 输出 (经 `{{user_input}}` 替换后). 当前 RED 应失败 (`Template render error`)
- **修复 2 (BUG-2 实际修复)**: 根据 RED 失败堆栈定位根因 (假设 1-4 之一), 修相关 TopoScheduler/ExecutionSession/ContextEngine 代码. 候选:
  - 候选 A: `handle_fork_branches_block` (或 `execute_fork_branches`) 直接传 main_loop 的 `context` 给 `execute_single_branch` 而非从 snapshot 取 — **绕过 snapshot key mismatch 问题**
  - 候选 B: 修 `current_fork_node_path_` 在 fork/branch 期间被某处错误重置, 导致后续 `get_snapshot` 返回 nullptr
  - 候选 C: `save_snapshot` key 与 `current_fork_node_path_` 不一致, 改用统一 key 生成器
  - 候选 D: `execute_with_snapshot` 先保存后执行的语义修复 (注释说 "before" 但代码先 execute_func)
- **修复 3 (regression guard)**: `tests/test_pdk_fork_join.cpp` 5 cases 全过 (C++ `ForkJoinLoop` 4-worker 行为不变, `D8.inv.forkjoin-class-unchanged`)
- **修复 4 (test 升级)**: `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2 升级为完整 e2e (success=true + response 非 mock fallback + 3 branch 输出). `test_loop_agent_plan_execute_realllm.cpp` Test 1 链路断言保留 (per fork-join-dsl-migration Phase 4)
- **修复 5 (docs 同步)**: 更新 `.rddf/improvements/fork_join_dsl_migration.md` BUG-2 状态 `⚠️ 未修复` → `✅ 已修复 (commit <hash>)`. 更新 AGENTS.md Pattern #12 follow-up 段记录 BUG-2 修复 ship
- **修复 6 (benchmark)**: 复跑 ADR-0087 benchmark 4-worker 3.3× speedup (C++ ForkJoinLoop, 不退化)

## Capabilities

### 新增能力
- DSL fork_join.agent.md **真 LLM 闭环**: fork → 3 branch (task_a/b/c 经 `{{user_input}}` template render 成功) → join → synthesize (llm_call) → end 全部可执行
- `lib/loop/fork_join.agent.md` + `examples/pdk_chat_demo/dsl/fork_join_example.agent.md` + `examples/pdk_chat_demo/tests/fixtures/fork_join_golden*.agent.md` + `examples/skill_porting/agenticdsl/axis*/` 多文件中 fork/join 节点全部激活
- DSL plan_execute.agent.md 中 **plan_context 内嵌 USE fork/join** 可行 (per fork-join-dsl-migration BUG-3 fix 链路)
- `fork_join_dsl_migration.md` tracker BUG-2 Gate 条件解除, 启动条件更新

### 不变更能力
- C++ `ForkJoinLoop` + `DomainWorkerPool` 4-worker 行为零变化 (D8.inv.forkjoin-class-unchanged, ADR-0087 benchmark 不退化)
- ADR-0089 v1.4 D6/D7/D8 不变量保留 (ReactLoop/PlanExecuteLoop/DEFINE_AGENT 仍 `[[deprecated]]`, ForkJoinLoop 不在 deprecation 范围)
- `fork-join-dsl-migration.md` tracker Phase 1+2+3 状态保留 (per commits `7e81c2f` + `469f495`)
- snapshot 机制契约 (其他场景用 snapshot 的代码不变)
- `loop_phases::run_plan_phase` helper 与 ADR-0089 v1.3 Pattern #12 不变量保留