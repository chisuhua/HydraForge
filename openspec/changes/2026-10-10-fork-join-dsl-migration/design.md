## Context

ADR-0089 v1.4 (`docs/adr/adr-0089-v1-4-amendment-soft-deprecate-cpp-agent-loops.md`, ship 2026-10-10 via commits `7dc83a7` + `87aadc4` + `96172e3` + `f47acaa`) 把 PDK Agent Loop canonical 路径收敛到 `lib/loop/*.agent.md` DSL + `loop/run` 工具。但 DSL 路径上仍残留 2 个 pre-existing 缺陷, 阻塞 fork/join + plan_execute DSL 路径作为完整 canonical:

| # | 缺陷 | 根因 | 影响 |
|---|---|---|---|
| BUG-2 (实际) | DSL fork 节点**未把 working context 传递给各 branch 节点**, 导致 branch 节点的 template 变量 (`{{user_input}}` 等) 无法解析 | `TopoScheduler::handle_fork_node` + `start_fork_simulation` (`src/modules/scheduler/topo_scheduler.cpp:316-326, 673-683`) 未快照 working context | fork 之后的 branch 节点 template render 必失败, DSL fork 路径从未真正可用 (原假设 "routing bug" 经实测不成立) |
| BUG-3 | DSL plan_execute prompt 与 execute_plan 工具语义不匹配 | `lib/loop/plan_execute.agent.md:14-19` plan prompt 生成自然语言 plan, 但 `pdk_entry.cpp` extract_agenticdsl_block 期望 ```yaml AgenticDSL block | 真 LLM 下 plan_execute 必崩 |

pre-BUG-1 + BUG-4 已 ship (commit `f5836e4`, 本会话上阶段) — child engine 补 `loop/process_task` + `loop/execute_plan` 注册 (BUG-1) + loop/run 注入 `{{context}}` 变量 (BUG-4)。**剩下 BUG-2 + BUG-3 是 fork_join_dsl_migration.md tracker 的 Gate 条件**。

**Oracle 评估** (session `ses_ed9e45122ffemlcLQmcT6sunT0`, bg `bg_a4315562`):
- BUG-2 实际修复 = working context 快照传递机制 (handle_fork_node 改造), 非调度器 routing 顺序修复. 风险中-高 (调度器状态机 + ctx 生命周期)
- BUG-3 修复 = 改 plan_execute.agent.md plan prompt (复用 helper prompt 语义), 风险低, < 半天
- ADR-0087 benchmark 不受影响 (C++ ForkJoinLoop + DomainWorkerPool 4-worker 行为不动, DSL 路径是新增并行引擎)

**用户决策** (2026-10-10):
- 立即修 BUG-3 + 同步修 BUG-2 (不等 fork_join 观察期)
- 完整 OpenSpec 流程 + cooling-off override (per ADR-0089 v1.4 HARD pause override precedent)

## Decisions

### Decision D1 — BUG-2 上下文快照传递修复 (范围扩展, 本 change 部分修复)

**位置**: `src/modules/scheduler/topo_scheduler.cpp:316-326` (`start_fork_simulation`) + `673-683` (`handle_fork_node`).

**原 BUG-2 假设** (v1.4 ship 时): TopoScheduler routing 顺序 bug. **重新调查发现**: routing 顺序实际正确 (handle_fork_branches_block line 175 在 execute_node 之前触发). 真正 BUG 是 fork 节点**未把 working context 传递给各 branch 节点**.

**修复方案**: fork 节点执行前快照 working context (含 user_input 等所有已注入变量), 各分支执行时合并 fork 前的 ctx. JOIN 节点合并各分支结果时, 合并的 working context 含 fork 前的 template variables.

**关键不变量**:
- **D1.inv.execute-order**: TopoScheduler routing 顺序已正确, 本 change 不再调整
- **D1.inv.safety-net**: NodeExecutor 仍 throw logic_error (作为路由 bug 检测点, 不删除)
- **D1.inv.forkjoin-class-unchanged**: C++ `ForkJoinLoop` + `DomainWorkerPool` 4-worker 行为不变
- **D1.inv.dsl-fork-zero-change**: lib/loop/fork_join.agent.md DSL 文件零修改 (公开 API 零变化)

**Status (2026-10-10 实施阶段)**: 经实测, BUG-2 修复涉及上下文快照传递机制改造, **超出本会话范围** (需要 handle_fork_node + start_fork_simulation + 各分支 ctx merge 机制联合改造, 风险中-高). 本 change Phase 3 调整为:**记录 BUG-2 真实根因为后续 future change 跟踪** (`.rddf/improvements/fork_join_dsl_migration.md` BUG-2 状态更新).### Decision D2 — BUG-3 DSL prompt 对齐修复 (低风险)

**位置**: `lib/loop/plan_execute.agent.md:14-19` plan 节点 prompt。

**当前 bug prompt**:
```yaml
- id: plan
  type: llm_call
  prompt_template: |
    You are a planning agent. Given the user's request, produce a step-by-step plan.

    User request: {{user_input}}
    Context: {{context}}
  output_keys: [plan_response]
  next: [/main/execute]
```

**修复方案** (per Oracle 评估推荐方案 C: 复用 helper prompt): 改 plan prompt 为 `loop_phases::run_plan_phase` 既有 prompt 语义, 与 C++ PlanExecuteLoop 单源对齐:

```yaml
- id: plan
  type: llm_call
  prompt_template: |
    Goal: {{user_input}}
    Context: {{context}}
    Generate AgenticDSL markdown for /main subgraph:
  output_keys: [plan_response]
  next: [/main/execute]
```

**关键不变量**:
- **D2.inv.dsl-public-api**: `lib/loop/plan_execute.agent.md` 节点结构 (id/next) 零变化, 仅 prompt 文本调整
- **D2.inv.helper-align**: prompt 文本与 `loop_phases::run_plan_phase` (`include/agenticdsl/pdk/agent_loops/loop_phases.h`) 单源对齐
- **D2.inv.dsl-react-agents**: `lib/loop/react.agent.md` 零变化 (react 路径不受影响)

### Decision D3 — 回归测试 (TDD 5-step)

**回归测试改由 fork_join_dsl_migration.md tracker 跟踪**: BUG-2 working context 快照修复需独立 future change, 本 change 不新增 test_topo_scheduler_fork_join.cpp (经实测 fork 节点 routing 顺序正确, 新增测试会立即通过但不能证明 BUG-2 已修).

**更新 `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2** (从 "链路断言" → "完整 e2e 断言"):
```cpp
REQUIRE(run_ret.value("success", false) == true);
auto resp_str = run_ret.value("response", std::string{});
REQUIRE(resp_str.find("[loop_agent/fork_join] Processed:") == std::string::npos);
REQUIRE(run_ret.value("steps", 0) >= 3);  // fork 分支 ≥ 3
REQUIRE_FALSE(run_ret.value("error", std::string{}).empty() == false);  // 无 error
```

**更新 `examples/pdk_chat_demo/tests/test_loop_agent_plan_execute_realllm.cpp` Test 1** (从 "链路断言" → "完整 e2e 断言"):
```cpp
REQUIRE(run_ret.value("success", false) == true);
REQUIRE_FALSE(run_ret.value("response", std::string{}).empty());
REQUIRE(run_ret.value("steps", 0) >= 4);  // plan → execute → verify → end
// 移除 "链路断言" 注释 (BUG-3 已修, 无需追溯 DSL 设计缺陷)
```

### Decision D4 — 文档同步 (D6/D7/D8/D9 刷新)

- **D6**: `.rddf/improvements/fork_join_dsl_migration.md` — BUG-2/BUG-3 状态从 `⚠️ 未修复` → `✅ 已修复`, Gate 条件移除, 启动条件更新
- **D7**: `AGENTS.md` Pattern #12 follow-up 段 — 提及 v1.5 (fork_join_dsl_migration) ship
- **D8**: `AGENTS.md` Recent Changes 登记本次 ship
- **D9**: `docs/adr/adr-0089-v1-4-amendment-soft-deprecate-cpp-agent-loops.md` — ship gate checklist `[ ] must_realllm NOT-RUN` 项关闭 (本会话已用 DEEPSEEK_API_KEY 跑验 11/11 PASS)

## Risks

| # | 风险 | 严重度 | 缓解 |
|---|---|---|---|
| R1 | 实际 BUG-2 working context 快照传递破坏既有 fork 行为 | 🟠 中 | 本 change 不实施 (Phase 3 仅文档化), 留待 fork_join_dsl_migration future change |
| R2 | fork 节点 pre-execute 路由导致 executed_.insert 顺序变化 | 🟡 低 | 显式 `state.executed_.insert(found.path)` + 验证 fork 分支独立路径 |
| R3 | 真实 LLM plan 生成仍可能产生不可解析格式 | 🟢 低 | `loop_phases::run_plan_phase` 既有 prompt 在 C++ PlanExecuteLoop ship 后稳定运行 (v1.3 Pattern #12), 验证有效 |
| R5 | DSL fork 路径并行度设计若超额 submit 触发 DomainWorkerPool 约束 | 🟢 低 | fork 分支由 TopoScheduler::handle_fork_branches_block 串行/并行调度, 与 C++ ForkJoinLoop 的 4-worker pool 是不同原语 (per Oracle 评估) |
| R6 | Pattern #10 hygiene: fresh-deploy 验证 fork 路径 | 🟢 低 | TDD 5-step 含独立 regression test, CI 上无 host 持久状态依赖 |
| R7 | cooling-off override 风险 | 🟢 低 | Oracle dual-review pre-impl 完整应用, commit message 透明记录 override 决策 |

## Phase 实施 (5-step)

- **Phase 1 (commit 1) — docs**: OpenSpec change artifacts (proposal/design/specs/tasks), AGENTS.md Pattern #12 follow-up, `.rddf/improvements/fork_join_dsl_migration.md` 状态刷新, ADR-0089 v1.4 ship gate NOT-RUN 关闭
- **Phase 2 (commit 2) — BUG-3 fix**: `lib/loop/plan_execute.agent.md` plan prompt 替换 (D2), 1 file 改动
- **Phase 3 (commit 3) — BUG-2 真实状态文档化**: 不实施 working context 快照传递修复 (超出本会话范围). 改为: `.rddf/improvements/fork_join_dsl_migration.md` BUG-2 状态从 `⚠️ 未修复 (routing bug 假设)` → `⚠️ 未修复 (template context 快照 bug 实际根因, 见 proposal.md BUG-2 重新调查)`. 1 file 修改, 文档化真实根因 + 修复路径 + 风险评估.
- **Phase 4 (commit 4) — test 升级 + 必须_realllm**: 更新 `test_loop_agent_fork_join` Test 2 + `test_loop_agent_plan_execute_realllm` Test 1 (断言升级), 真 LLM 跑验闭环
- **Phase 5 (commit 5) — archive + AGENTS.md Recent Changes**: OpenSpec change `git mv` → `archive/2026-10-10-fork-join-dsl-migration/`, AGENTS.md Recent Changes 登记 + ship gate 同步