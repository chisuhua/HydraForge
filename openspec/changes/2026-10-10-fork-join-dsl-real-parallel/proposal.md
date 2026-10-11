# Proposal: Fork/Join DSL real parallel semantics repair (path 2)

## Why

`fix-fork-join-agent-next-chain` OpenSpec change 被 Pattern #8 dual-agent review **BLOCK** + zero-cost static verification (2026-10-10) 后 cancelled (supersede 至本 change, 原 change 4 文件归档至 `archive/2026-10-10-fix-fork-join-agent-next-chain-blocked-by-pattern8-dual-agent-review/` + Cancellation Note 详述 4 个结构性 bug)。

**真实根因链** (Oracle 静态验证 100%):

| Bug | 描述 | 静态验证证据 |
|-----|------|---------------|
| **BUG-2** | routing-JOIN 顺序错 (JOIN 走 execute_node 抛 logic_error) | `topo_scheduler.cpp:198` execute_node 在 JOIN check 之前 + `node_executor.cpp:66-68` 抛 `logic_error` + `topo_scheduler.cpp:228-230` JOIN check 是死代码 |
| **BUG-3** | fork 路径不调 update_successors → fork.next 死边 | `topo_scheduler.cpp:192-196` `handle_fork_node(); executed_.insert(); continue;` — 不调 handle_node_completion (对比 `handle_node_completion:711-717` 才会调 update_successors) |
| **BUG-4** | branches schema 不匹配 → ForkNode.branches 恒为空 | `node_factory.cpp:208-223` make_fork 读嵌套 `j["fork"]["branches"]` + `b.get<std::string>()` 期望**字符串数组** vs `lib/loop/fork_join.agent.md:13-17` 扁平 `branches: [{path: ...}]` **对象数组** — 双重不匹配 |
| **BUG-5** | fork_join.agent.md task_a/b/c next → end 而非 join_results | `lib/loop/fork_join.agent.md:26,34,42` |

**3 行 DSL 修复物理无效**: BUG-2 让 JOIN dispatch 必失败, BUG-3 让 fork.next 永不推进 join_results in_degree, BUG-4 让 branches 空转 (fork/join 机制 no-op) — BUG-5 修复 (BLOCKED change 的唯一改动) 在三个前置 bug 未修时无效果。

**用户决策路径 2** (2026-10-10, per Pattern #8 dual-agent review): 真 fork/join 语义全修 — 覆盖 parser + build_dag + executor + DSL + 测试, **非**诚实降级承认"DSL fork/join = 结构化顺序 DAG"。

## What Changes

### 修复 1 (BUG-2 — routing-JOIN 前移)
`src/modules/scheduler/topo_scheduler.cpp` 主循环: 把 JOIN 类型检查从 line 228-230 (post-execute 死代码) 前移到 execute_node 之前 (与 FORK 对称, line 192-196 模式)。删除原 line 228-230 JOIN 检查。

### 修复 2 (BUG-3 — fork path 加 update_successors)
`src/modules/scheduler/topo_scheduler.cpp:192-196` 主循环 FORK 分支: 在 `continue` 前加 `handle_node_completion(state, NodeResult{true, ""}, found.node, found.path)` — 调用 update_successors 推进 fork.next 边 (fork_tasks → join_results)。

### 修复 3 (BUG-4 — parser 支持扁平 + 对象数组 branches)
`src/modules/parser/node_factory.cpp:208-223` make_fork: 支持两种 schema (向后兼容):
- 新规范: 扁平 `branches: [{path: "/main/task_a"}, ...]` 对象数组 (parse `b["path"].get<std::string>()`)
- 旧规范: 嵌套 `fork: {branches: ["/main/task_a", ...]}` 字符串数组 (existing behavior, mark deprecated)
另 make_join 的 wait_for 也同步支持两种 schema (扁平 `wait_for: [...]` 字符串数组, 现有为嵌套 `join: {wait_for: [...]}`)。

### 修复 4 (BUG-5 — DSL next 链修复)
`lib/loop/fork_join.agent.md` 3 行: task_a (line 26) + task_b (line 34) + task_c (line 42) 的 `next: [/main/end]` → `next: [/main/join_results]`。synthesize (line 56) 的 `next: [/main/end]` 保持不变 (synthesize→end 是正确设计, join_results→synthesize→end 链路)。

### 修复 5 (build_dag 防双执行防护)
`src/modules/scheduler/topo_scheduler.cpp` build_dag: 当 FORK 节点有 branches 时, fork→branch 边计为 in-edge (不把 branch 节点 seed 到 in_degree=0)。这是 BUG-4 修复后的必要保护 — 当前 schema 修通后 task_a/b/c 会双重执行 (build_dag seed 一次 + execute_single_branch 一次)。需 build_dag 识别 fork→branch 边并扣除对应 in_degree。

### 修复 6 (regression guards)
- `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2: 更新 line 79-84 陈旧注释 + 升级断言为严格 success=true + 新增 join_results 节点触发断言
- `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` 新增 Test 4: fork/join 真 e2e (3 branches 并行执行 + join 合并 + synthesize llm_call + end)。断言 `result.value("success") == true` + `result.value("steps") >= 6` (fork/join 是调度语义不产生 trace, 可 trace = start + task×3 + synthesize + end = 6, 而非原 R3 估计 7)
- 新增 `tests/test_fork_join_dsl_real_parallel.cpp` (core tree): 4 cases 覆盖 (1) parser 双 schema 兼容, (2) build_dag 防双执行, (3) routing-FORK/JOIN 前移, (4) update_successors on fork path

### 修复 7 (AGENTS.md + tracker + docs 同步)
- AGENTS.md Recent Changes: 本 change ship 登记 (含 5 commits hash + Reverse Indicator 5 字段)
- `.rddf/improvements/fork_join_dsl_migration.md` BUG-2/3/4/5 状态更新: ❌ → ✅ (本 change ship 后)
- `docs/specs/dsl.md` §5.2 schema 段: 补充 `branches: [{path}]` 扁平对象数组规范 (向后兼容 `fork: {branches: []}`)

## Capabilities

### 新增能力
- `lib/loop/fork_join.agent.md` 完整 e2e 真 LLM 闭环 (fork + 3 branches 真并行 + join 合并 + synthesize llm_call + end)
- `node_factory.cpp` make_fork + make_join 支持双 schema (向后兼容)
- `topo_scheduler.cpp` FORK/JOIN 调度语义完整 (routing 前移 + update_successors + build_dag 防双执行)
- `fork_join_dsl_migration.md` tracker BUG-2/3/4/5 全部 ✅

### 不变更能力
- `examples/pdk_chat_demo/dsl/fork_join_example.agent.md` (用 `type: fork_join` 走 C++ ForkJoinLoop 路径) 零变化 (per ADR-0089 v1.4 D8.inv.forkjoin)
- `examples/pdk_chat_demo/tests/fixtures/fork_join_golden*.agent.md` 零变化 (不同 schema)
- ADR-0087 benchmark 4-worker 3.3× speedup 不退化 (本 change 不触 DomainWorkerPool)
- `tests/test_pdk_fork_join.cpp` 5 cases 零回归 (C++ ForkJoinLoop 4-worker 行为不变)
- `loop/run_plan` + `loop/run_verify` helper 行为零变化
- `extract_agenticdsl_block` 4 fence 容错零变化
- `lib/loop/react.agent.md` + `lib/loop/plan_execute.agent.md` 零变化

## 关联

### 关联 OpenSpec changes
- `archive/2026-10-10-fix-fork-join-agent-next-chain-blocked-by-pattern8-dual-agent-review/`: 本 change supersede 源 (BLOCKED by Pattern #8 dual-agent review)
- `archive/2026-10-10-fork-join-dsl-migration/` (commits `7e81c2f` + `469f495` ship): BUG-3 (plan prompt) 部分修复 + BUG-2 根因重新调查
- `archive/2026-10-10-fix-fork-dsl-context-propagation/` (commits `3dd12a2` + `adb7910` + `83883bc` + `6b85ca7` + `b51adea` + `b706c31` ship): BUG-2-A (routing-FORK 前移) + BUG-2-B (template context defensive fix)

### 关联 tracker
- `.rddf/improvements/fork_join_dsl_migration.md`: 本 change ship 后 BUG-2/3/4/5 全部 ✅, tracker 完全闭合

### 关联 ADR
- ADR-0089 v1.4 §D8.inv.forkjoin: ForkJoinLoop 维持现状 (本 change 不触)
- ADR-0087: 4-worker benchmark 不退化
- ADR-0021 §3.5: PDK 头依赖限制 (本 change 不触 PDK public API)
