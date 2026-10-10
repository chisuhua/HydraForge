# Spec: fork-join-dsl-completion (fork/join + plan_execute DSL 路径完整化)

**关联 OpenSpec change**: `openspec/changes/2026-10-10-fork-join-dsl-migration/`
**目标**: 修复 BUG-2 (TopoScheduler fork/join routing) + BUG-3 (plan_execute.agent.md prompt 语义对齐), 让 `lib/loop/fork_join.agent.md` + `lib/loop/plan_execute.agent.md` 作为 canonical 生产路径完整可用.

---

## MODIFIED Requirements

### Requirement: DSL fork 节点 working context 快照传递 (BUG-2 真实根因)

DSL fork 节点 (`type: fork`) 在 `TopoScheduler::handle_fork_node` 执行时 **MUST** 快照当前 working context, 各 branch 节点执行时继承 fork 前的 working context (含 `{{user_input}}` 等已注入 template variables).

#### Scenario: fork 后的 branch 节点 template variable 可解析

- **WHEN** DSL `lib/loop/fork_join.agent.md` 经 `loop/run loop_type=fork_join` 执行
- **THEN** fork 节点执行后, 各 branch 节点 (task_a/b/c) 的 working context 含 fork 前的 `user_input` + `system_prompt` + `history` + `context` (per loop/run 注入, BUG-4 fix)
- **AND** `task_a` 的 `prompt_template: "{{user_input}}"` template render 成功 (无 "Template render error: variable 'user_input' not found")
- **AND** branch 节点的 `loop/process_task` 工具收到正确的 user_input (per `task_arguments` field)

#### Scenario: fork 前的 routing bug 假设不成立 (现状记录)

- **WHEN** DSL fork 节点经 `TopoScheduler::run_main_loop` (line 175+) 路由
- **THEN** `handle_fork_branches_block` (line 175) 在 `session_.execute_node` 之前正确触发
- **NOTE**: 2026-10-10 实施阶段实测, `TopoScheduler::run_main_loop` line 209-221 的 fork/join 检查顺序实际正确 (handle_fork_node 在 execute_node 后调用, 但 line 175 handle_fork_branches_block 才是真正的 fork 路由点). 原 BUG-2 假设 "routing bug" 经实测不成立, 真 BUG 是 fork 节点未快照 working context.

#### Scenario: C++ ForkJoinLoop 4-worker 行为不变 (D8.inv.forkjoin-class-unchanged regression guard)

- **WHEN** C++ `ForkJoinLoop::run` (位于 `include/agenticdsl/pdk/agent_loops/fork_join_loop.h:138-340`) 直接调用
- **THEN** `DomainWorkerPool(4)` 4 jthread 行为零变化 (per D1.inv.forkjoin-class-unchanged)
- **AND** `tests/test_pdk_fork_join.cpp` 5 cases 全部 PASS (regression guard)
- **AND** ADR-0087 benchmark 4-worker 3.3× speedup 不退化

### Requirement: TopoScheduler fork/join 单元测试覆盖上下文快照契约 (后续 future change)

`tests/test_topo_scheduler_fork_join.cpp` **MUST** 在 BUG-2 working context 快照传递修复后新增, 验证 fork 节点 working context 快照契约. (本 change 不实施, 留待 fork_join_dsl_migration future change.)

#### Scenario: fork 节点快照 working context (future change 测试)

- **WHEN** DSL 含 fork 节点, working context 有 `{{user_input}}` 等已设 key
- **THEN** fork 节点执行后, 各分支节点的 working context 仍含 `user_input` 等 key (fork 节点快照传递)
- **AND** 各分支节点的 template render 成功 (如 `{{user_input}}`)
- **NOTE**: 本测试留待 BUG-2 修复时一并新增

#### Scenario: YIELD 与 FORK 路由互不干扰

- **WHEN** DSL 子图同时含 llm_call (YIELD 触发点) 与 fork 节点
- **THEN** YIELD 状态机正确保存/恢复 (返回 YIELDED), FORK 路由正确 (execute_node 不被调用 for FORK 节点)
- **AND** 恢复后 (resume) FORK 节点仍按 D1.inv.execute-order 处理### Requirement: DSL plan_execute agent prompt 与 loop_phases::run_plan_phase 单源对齐

`lib/loop/plan_execute.agent.md` plan 节点的 `prompt_template` **MUST** 与 `loop_phases::run_plan_phase` (`include/agenticdsl/pdk/agent_loops/loop_phases.h:39-68`) 的 prompt 模板语义对齐, 引导真实 LLM 产出 ```yaml AgenticDSL block (而非自然语言 plan).

#### Scenario: 真实 LLM 产出 ```yaml AgenticDSL block

- **WHEN** `loop/run loop_type=plan_execute` 经真实 DeepSeek LLM (DEEPSEEK_API_KEY 注入) 执行
- **THEN** plan 节点 LLM 响应含 ```yaml\n ... ``` fenced block (可被 `extract_agenticdsl_block` 解析)
- **AND** execute 节点调 `loop/execute_plan` (经 `run_execute_plan`) 成功提取 AgenticDSL markdown, 创建子 DSLEngine 解析
- **AND** 子 DSLEngine 执行成功 (含 llm_call 节点), execute_plan 返回 `success=true`
- **AND** verify 节点调 `loop/run_verify` (经 `run_verify_phase`) 评估子图结果
- **AND** 主图 result.success == true, steps ≥ 4 (plan → execute → verify → end)

#### Scenario: plan prompt 文本与 helper prompt 单源对齐

- **WHEN** 比较 `lib/loop/plan_execute.agent.md` plan 节点 prompt 与 `loop_phases::run_plan_phase` 的 prompt 模板 (`loop_phases.h:43-49`)
- **THEN** 两者 prompt 结构对齐: 含 `Goal:` + `Context:` + `Generate AgenticDSL markdown for /main subgraph:` 三大段
- **AND** `{{user_input}}` 模板变量对应 `Goal: ` 后的用户目标
- **AND** `{{context}}` 模板变量对应 `Context: ` 后的 working context (loop/run 已注入, per BUG-4 fix)

#### Scenario: plan_execute DSL 文件零行为变化 (除 prompt 文本)

- **WHEN** 修改 `lib/loop/plan_execute.agent.md` plan 节点的 `prompt_template` 文本
- **THEN** plan 节点 `id/next/output_keys/type` 字段零变化 (D2.inv.dsl-public-api)
- **AND** `lib/loop/react.agent.md` + `lib/loop/fork_join.agent.md` 零变化 (D2.inv.dsl-react-agents)

## ADDED Requirements

### Requirement: DSL plan_execute 真 LLM 闭环验证

`examples/pdk_chat_demo/tests/test_loop_agent_plan_execute_realllm.cpp` Test 1 (`loop/run plan_execute: real DeepSeek end-to-end`) **MUST** 验证完整闭环, 不再是 "链路断言 (error 不再是 Tool not registered)" 兼容模式.

#### Scenario: 真 LLM 闭环完整断言

- **WHEN** DEEPSEEK_API_KEY 已注入 (CI skip 模式被覆盖) 且 `loop/run loop_type=plan_execute` 被调用
- **THEN** `result.success == true` (非兼容 PASS)
- **AND** `result.response` 非空 (LLM 真产出, 非 mock fallback)
- **AND** `result.steps >= 4` (plan → execute → verify → end)
- **AND** `result.error` 字段为空 (无 error)
- **AND** `loop/run_plan` + `loop/run_verify` helper 工具返回 ok (per GAP-5 验证)

### Requirement: DSL fork_join DSL e2e 完整验证

`examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2 (`loop/run fork_join: loop/process_task registered in child engine`) **MUST** 升级为完整 e2e 验证, 不再是 "链路断言 (error 不再是 Tool not registered)" 兼容模式.

#### Scenario: DSL fork_join 完整 e2e 断言

- **WHEN** `loop/run loop_type=fork_join` 经 MockLLMProvider 调用 (Test 2 增强模式)
- **THEN** `result.success == true` (非兼容 PASS)
- **AND** `result.response` 含 LLM 合成文本 (synthesize llm_call 节点), 非 mock fallback 前缀
- **AND** `result.steps >= 3` (fork 分支数, 含 task_a/b/c + join + synthesize)
- **AND** `result.error` 字段为空 (无 error, BUG-2 routing 已修)

### Requirement: TopoScheduler fork/join 单元测试 MUST 覆盖路由契约

`tests/test_topo_scheduler_fork_join.cpp` 新增, **MUST** 覆盖 TopoScheduler fork/join 路由顺序契约 (D1.inv.execute-order).

#### Scenario: FORK 节点不被送到 NodeExecutor

- **WHEN** DSL 含 fork 节点 (`/main/fork` → `/main/task_a`, `/main/task_b`, `/main/task_c`) 经 TopoScheduler 调度
- **THEN** `NodeExecutor::execute_node` 调用次数 = 0 (仅 3 个 `loop/process_task` 分支 + `synthesize` llm_call + `end`, 共 5 个非 fork 节点)
- **AND** `TopoScheduler::handle_fork_node` 被调用 1 次 (FORK 展开)
- **AND** `TopoScheduler::process_fork_join` 被调用 1 次 (JOIN 汇聚)

#### Scenario: FORK/JOIN 并发 fuzz (Pattern #7)

- **WHEN** 50 iter × 4 thread 并发执行 fork/join DAG
- **THEN** 所有 iter 成功 (success=true), 无 race condition (TDirty CheckSanitizer Report = 0)
- **NOTE**: per AGENTS.md Pattern #7 多轮竞争检测, 验证调度器状态机并发安全

#### Scenario: YIELD 与 FORK 互不干扰

- **WHEN** DSL 子图同时含 llm_call (YIELD 触发点) 与 fork 节点
- **THEN** YIELD 状态机正确保存/恢复 (返回 YIELDED), FORK 路由正确 (execute_node 不被调用 for FORK 节点)
- **AND** 恢复后 (resume) FORK 节点仍按 D1.inv.execute-order 处理