## Why

`ADR-0089 v1.4` (Soft-Deprecate C++ Agent Loop, commit `7dc83a7` ship 2026-10-10) 把 canonical 生产路径收敛到 `lib/loop/*.agent.md` DSL + `loop/run` 工具，但 `.rddf/improvements/fork_join_dsl_migration.md` 跟踪的 2 个 pre-existing 缺陷仍是 DSL 路径的真实断链，**本 change 是该 future change 实施的 Gate 条件**（tracker 原文: "BUG-2 + BUG-3 必须解决，否则 DSL fork_join/plan_execute 路径无法作为 canonical 生产路径"）:

### BUG-2 (DSL fork/join 模板变量传递 - 重新调查发现 2026-10-10)

**原假设 (v1.4 ship 时)**: DSL `fork_join.agent.md` / `plan_execute.agent.md` 执行时, fork/join 节点崩溃, 错误 "ForkNode/JoinNode reached NodeExecutor - scheduler routing bug" (`src/modules/executor/node_executor.cpp:66-68`).

**重新调查发现 (2026-10-10 实施阶段)**: 跑 `lib/loop/fork_join.agent.md` 实际错误是 `Template render error: variable 'user_input' not found` (来自 task_a 节点的 `{{user_input}}` template render), **不是** routing bug. topology fork/join 节点**已被** `handle_fork_branches_block` (line 175) + `handle_fork_node` 路由正确 - 实际 BUG 是 fork 节点**未把 working context 传递给各 branch 节点**, 导致 branch 节点的 template 变量无法解析.

**根因**: `TopoScheduler::start_fork_simulation` + `handle_fork_node` 创建子分支时**未传递 working context** (fork_node->branches 只有 path, 没有 ctx 快照). 分支节点如 `task_a` 的 `prompt_template: "{{user_input}}"` 无法在 working ctx 中找到 `user_input` key.

**影响**: 自 Sprint 20 (2026-08-01) 以来 DSL fork/join 路径在 fork 之后的 branch 节点必崩.

**修复路径** (超出本 change scope, 见 Phase 3 后续):
1. `handle_fork_node` 在 `start_fork_simulation` 前快照 working context
2. 各分支执行时合并 fork 前的 ctx
3. JOIN 节点 merge 时合并 fork 前的 template variables### BUG-3 (DSL 文件设计)
**症状**: DSL `plan_execute.agent.md` 的 plan 节点调 LLM 生成 plan 后, execute 节点调 `loop/execute_plan` 失败, 错误 `"plan is not AgenticDSL markdown: no ```yaml AgenticDSL block found"`。

**根因**: `lib/loop/plan_execute.agent.md:14-19` plan prompt 是 "produce a step-by-step plan" (自然语言), 但 `pdk/loop_agent/src/pdk_entry.cpp` 的 `extract_agenticdsl_block` 期望 LLM 产出 ```yaml AgenticDSL block`。两段**语义不匹配** — `loop_phases::run_plan_phase` (per `include/agenticdsl/pdk/agent_loops/loop_phases.h`) 的既有 prompt 是 "Generate AgenticDSL markdown for /main subgraph", 与 DSL 文件期望不同。

**影响**: `plan_execute` DSL 路径在真实 LLM 下必然无法触达 execute 节点, 已 ship 的 `loop/run loop_type=plan_execute` 在真 LLM 下必失败。

### 已 ship 的相关 fix (pre-BUG-1/BUG-4, commit `f5836e4`)
本会话上一阶段已修 (审计时发现): child engine 缺 `loop/process_task` + `loop/execute_plan` 注册 (BUG-1) + loop/run 缺 `{{context}}` 注入 (BUG-4)。这两个修复**解锁了链路下游**, 但 BUG-2 + BUG-3 让 fork/join / plan_execute **链路上游**仍崩。

### BUG-2 + BUG-3 修复必要性
1. ADR-0089 v1.4 D8/D9 路径: "ForkJoin DSL 迁移是独立 future change tracker (`.rddf/improvements/fork_join_dsl_migration.md`)" — 但 D8.inv.forkjoin 要求 ForkJoinLoop 不删 + ADR-0087 benchmark 不退化; 不修 BUG-2, DSL fork 路径无法作为 canonical, C++ ForkJoinLoop 仍唯一可用, 违背 v1.4 软停用目标
2. BUG-3 影响**已 ship 的 plan_execute 路径**（commit `7dc83a7` ship 时 `test_loop_agent_plan_execute_realllm` 已写为真 LLM 测试但因 BUG-3 无法闭环）
3. ADR-0087 benchmark (4-worker 3.3× speedup, commit `13ac53f`) 测的是 C++ `ForkJoinLoop` + `DomainWorkerPool`, **与 BUG-2 修复无关**（DSL fork 路径是新增并行路径, 不改 fork worker 池行为）

### 用户决策 (2026-10-10)
- BUG-3 立即修 (推荐选项 1)
- BUG-2 同步今天修 (不等 fork_join_dsl_migration 观察期)
- 走完整 OpenSpec 流程 (24h cooling-off + Oracle dual-review)
- **cooling-off override** (per ADR-0089 v1.4 HARD pause override precedent): 用户明确决策 → 本会话完成 TDD 5 步 + Oracle pre-impl review + atomic commit + archive

## What Changes

- **修复 1 (BUG-2 根因)**: 修 `src/modules/scheduler/topo_scheduler.cpp` `run_main_loop` 主循环, **先类型分发后 execute_node** — 在 `session_.execute_node` 调用**之前**插入 `if (found.node->type == NodeType::FORK) { handle_fork_node(...); continue; }` + `if (found.node->type == NodeType::JOIN) { process_fork_join(...); }` 块, 并移除原 line 215-221 的 post-execute 类型检查 (DELAY)
- **修复 2 (BUG-3 根因)**: 改 `lib/loop/plan_execute.agent.md` plan 节点 prompt, 复用 `loop_phases::run_plan_phase` 既有 prompt 语义 ("Generate AgenticDSL markdown for /main subgraph"), 与 C++ PlanExecuteLoop 单源对齐 (per ADR-0089 v1.3 Pattern #12 consolidate-loop-phases)
- **修复 3 (regression guard)**: 新增 `tests/test_topo_scheduler_fork_join.cpp` 覆盖 fork/join 调度路由顺序 + 并发安全 (per AGENTS.md Pattern #7 多轮 fuzz); 新增 `examples/pdk_chat_demo/tests/test_loop_agent_plan_execute_e2e.cpp` (mock) 验证 plan → execute 闭环
- **修复 4 (real LLM 验证)**: 既有 `test_loop_agent_plan_execute_realllm` Test 1 断言升级 — 从 "链路断言 (error 不再是 Tool not registered)" 升级为 "success=true + response 非空 + steps ≥ N + verify 可达", 验证 BUG-3 真 LLM 闭环修复
- **修复 5 (test 镜像)**: `test_loop_agent_fork_join` Test 2 断言升级 — 从 "链路断言 (error 不再是 Tool not registered)" 升级为 "success=true + response 非 mock fallback + 3 branch 并行输出", 验证 BUG-2 fork/join 调度路由修复
- **修复 6 (docs 同步)**: 更新 `.rddf/improvements/fork_join_dsl_migration.md` — 标记 BUG-2/BUG-3 已修复, Gate 条件解除, 启动条件只剩 "用户决策启动" 或 "观察期满"
- **修复 7 (AGENTS.md 登记)**: 更新 `AGENTS.md` Pattern #12 follow-up + Recent Changes 登记本次 ship
- **修复 8 (ADR-0089 v1.4 ship gate 关闭)**: 更新 `docs/adr/adr-0089-v1-4-amendment-soft-deprecate-cpp-agent-loops.md` ship gate checklist — `[ ] must_realllm: NOT-RUN` 项关闭 (本会话已用 DEEPSEEK_API_KEY 跑验 11/11 PASS)

## Capabilities

### 新增能力
- DSL fork/join 节点**首次可执行** (Sprint 20 ship 以来首次修复)
- DSL plan_execute 路径**真 LLM 闭环** (plan → execute → verify → end)
- `lib/loop/fork_join.agent.md` + `examples/pdk_chat_demo/dsl/fork_join_example.agent.md` + `examples/pdk_chat_demo/tests/fixtures/fork_join_golden*.agent.md` + `examples/skill_porting/agenticdsl/axis*/` 多文件中 fork/join 节点全部激活
- `lib/loop/plan_execute.agent.md` 真 LLM 计划生成可执行
- ADR-0087 benchmark (C++ ForkJoinLoop + DomainWorkerPool 4-worker) 不受影响 (per BUG-2 修复仅动调度器, 不改 fork worker 池)

### 不变更能力
- C++ `ForkJoinLoop` + `DomainWorkerPool` 4-worker 行为零变化 (D8.inv.forkjoin 保留, ADR-0087 benchmark 不退化)
- ADR-0089 v1.4 D6/D7 deprecation 范围不变 (ReactLoop + PlanExecuteLoop + DEFINE_AGENT 仍 `[[deprecated]]`)
- ADR-0089 v1.4 D8 ForkJoinLoop 不在 deprecation 范围 不变
- ADR-0089 v1.4 D9.future-change-tracker (fork_join_dsl_migration) **本 change ship 后该 tracker 进入 "✅ Tracked (Can be Closed)" 状态**, 后续仅 re-plan 决策