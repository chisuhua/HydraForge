## Context

`fork-join-dsl-migration` OpenSpec change Phase 3 (commit `469f495`, 2026-10-10) 重新调查 BUG-2 后记录真实根因为 fork 节点**未快照 working context 传递给 branch 节点**, 修复涉及 `handle_fork_node` + `start_fork_simulation` working context 快照传递机制改造. 但本次 fork-join-dsl-migration change 因修复风险中-高 (ctx 生命周期 + 调度器状态机) 未实施, 仅文档化. `.rddf/improvements/fork_join_dsl_migration.md` BUG-2 状态保留为 `⚠️ 未修复 (working context 快照传递)`.

**现状代码路径** (`src/modules/scheduler/`):
1. `TopoScheduler::run_main_loop` (line 175) 调 `handle_fork_branches_block()` (在 `session_.execute_node(found.node, context)` **之前**)
2. `handle_fork_branches_block` → `execute_fork_branches` (line 327)
3. `execute_fork_branches` line 331: `const Context* fork_snapshot = session_.get_context_engine().get_snapshot(current_fork_node_path_.value())`
4. Line 340: `Context branch_initial_ctx = *fork_snapshot` (复制 snapshot)
5. `execute_single_branch(branch_path, branch_initial_ctx)` 用这个 ctx 跑 branch

**BUG-2 现象** (实测 2026-10-10):
- `lib/loop/fork_join.agent.md` 经 `loop/run` + mock provider 调用
- 错误: `Template render error: variable 'user_input' not found` (task_a 节点 `prompt_template: "{{user_input}}"`)
- snapshot 机制**已存在** (`ExecutionSession::execute_node` line 174-178 + `needs_snapshot` line 341), 但实际 `fork_snapshot` 不含 `user_input` (或被覆盖/evict)

**OpenSpec change 立项** (本 change):
- 名称: `fix-fork-dsl-context-propagation`
- 目标: 修 BUG-2 working context 快照传递, 解锁 `lib/loop/fork_join.agent.md` 真 LLM 闭环
- 范围: 3-5 文件改动 (核心 1-2 个 C++ 源 + 1 测试 + docs)
- 风险: 中 (ctx 生命周期 + 调度器状态机 + snapshot 一致性)
- 治理: 完整 OpenSpec 流程 (24h cooling-off + Oracle dual-review) — **与 fork-join-dsl-migration 不同**: 此次**不**用 cooling-off override, 因涉及调度器核心代码 (per AGENTS.md 治理: 调度器改动需完整 review)

## Decisions

### Decision D1 — Issue Diagnosis (RED 测试前置)

**位置**: 新增 `tests/test_fork_dsl_context_propagation.cpp` (核心 RED 测试) + 复用 `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2 (集成测试).

**RED 测试设计**:
```cpp
// tests/test_fork_dsl_context_propagation.cpp: Test 1 (fork_join.agent.md 端到端)
TEST_CASE("DSL fork_join.agent.md: branch 节点 working context 传递",
          "[scheduler][fork_join][context-propagation]") {
    std::ifstream f("lib/loop/fork_join.agent.md");
    REQUIRE(f.is_open());
    std::stringstream ss; ss << f.rdbuf();
    auto engine = agenticdsl::DSLEngine::from_markdown(ss.str());
    REQUIRE(engine != nullptr);

    // 加载 loop_agent plugin + mock provider (per examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp:Test 2)
    fs::path so_path = "build/pdk/loop_agent/libLoopAgent.so";
    auto loader = std::make_unique<agenticdsl::PluginLoader>();
    REQUIRE(loader->load_so(fs::canonical(so_path).string(), engine->get_tool_registry()));

    auto provider = std::make_unique<agenticdsl::MockLLMProvider>();
    engine->get_tool_registry().call_tool("loop/set_parent_provider",
        {{"provider_ptr", std::to_string(reinterpret_cast<uintptr_t>(provider.get()))}});

    // 调用 loop/run fork_join
    auto run_ret = engine->get_tool_registry().call_tool("loop/run", {
        {"loop_type", "fork_join"},
        {"prompt", "TEST_USER_INPUT"},  // ← 测试用 user_input
        {"system_prompt", "test"},
        {"history", "[]"},
        {"tools", "[]"},
        {"max_steps", "10"}
    });

    // 修复后应 success=true + error 不含 "Template render error"
    REQUIRE(run_ret.value("success", false) == true);
    REQUIRE(run_ret.value("error", std::string{}).find("Template render error") == std::string::npos);
}
```

**关键不变量**:
- **D1.inv.red-first**: RED 测试在修复前必须 FAIL (`Template render error`). 修复后 PASS.
- **D1.inv.minimal-mock**: 用 `MockLLMProvider` 模拟 LLM 调用, 避免依赖真实 API key
- **D1.inv.lib-fork-join-agent-md**: 测试用项目自带 `lib/loop/fork_join.agent.md` (per fork-join-dsl-migration 调研, 此文件已 ship, 是 BUG-2 实际触发点)

### Decision D2 — BUG-2 双 bug 修复 (扩展范围)

**位置**: 
- 根因 A (routing): `src/modules/scheduler/topo_scheduler.cpp:209-221` (主循环) + `execution_session.cpp:174-178` (snapshot key)
- 根因 B (template context): `src/modules/scheduler/topo_scheduler.cpp:316-326` (`start_fork_simulation`) + `673-683` (`handle_fork_node`) + `327-353` (`execute_fork_branches`)

**双 bug 修复策略** (本 change 一次修两 bug):

**A. routing bug 修复** (相对简单):
- `TopoScheduler::run_main_loop` line 209-221: 在 `session_.execute_node(found.node, context)` 调用**之前**插入类型分发块
- 移除原 line 215-221 的 post-execute 类型检查 (避免重复)
- 修复后: FORK 节点在 `handle_fork_branches_block` (line 175) 之前先被 `handle_fork_node` 路由, `execute_node` 不会被调用 for FORK 节点
- 风险: 中 (调度器核心但 ~10 行改动)

**B. template context bug 修复** (相对复杂):
- `execute_fork_branches` line 331: `const Context* fork_snapshot = session_.get_context_engine().get_snapshot(current_fork_node_path_.value())` — 当前 key 与保存 key 一致, 但 snapshot 内容可能缺
- **候选方案** (RED 失败堆栈定位后选):
  - 候选 A: `handle_fork_branches_block` 直接传 main_loop 的 `context` 给 `execute_single_branch`, 跳过 `get_snapshot` 路径 — **绕过 snapshot key mismatch 问题**
  - 候选 B: 修 `current_fork_node_path_` 在 fork 期间被错误重置 (e.g., YIELD 后 resume 错位)
  - 候选 C: `save_snapshot` + `get_snapshot` key 一致性保证 (统一 key 生成器)
  - 候选 D: `execute_with_snapshot` 语义修复 (先 save 后 execute_func, 注释说 "before" 但代码先 execute_func)
- 风险: 中-高 (ctx 生命周期 + 调度器状态机)

**关键不变量** (无论选哪个方案):
- **D2.inv.user_input-propagation**: 修复后 `task_a` 的 `{{user_input}}` template render 成功
- **D2.inv.routing-fix**: 修复后 FORK 节点不被 NodeExecutor 收到 (routing bug 不再触发)
- **D2.inv.forkjoin-class-unchanged**: C++ `ForkJoinLoop` + `DomainWorkerPool` 4-worker 行为不变
- **D2.inv.snapshot-no-leak**: snapshot map 不累积
- **D2.inv.yield-resume-correct**: YIELD 后 resume fork 仍正确### Decision D3 — 回归测试 (TDD 5-step)

**新增 `tests/test_fork_dsl_context_propagation.cpp`** (~200 行):
- Test 1: `lib/loop/fork_join.agent.md` 端到端 RED (per D1)
- Test 2: 3-branch 并行执行 (state.executed_ 顺序断言)
- Test 3: YIELD 后 resume fork 仍正确 (state machine 契约)
- Test 4: fork + join + synthesize 完整图执行 (e2e)
- Test 5: 并发 fuzz (per AGENTS.md Pattern #7 多轮竞争检测, 50 iter × 4 thread)

**升级 `examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp` Test 2**:
```cpp
REQUIRE(run_ret.value("success", false) == true);  // BUG-2 修复后应 PASS
auto resp_str = run_ret.value("response", std::string{});
REQUIRE(resp_str.find("[loop_agent/fork_join] Processed:") == std::string::npos);  // 非 mock fallback
REQUIRE(run_ret.value("steps", 0) >= 3);  // 3 branch + join + synthesize
REQUIRE(run_ret.value("error", std::string{}).empty());
```

### Decision D4 — 文档同步 + ADR-0087 benchmark

- **D4a**: `.rddf/improvements/fork_join_dsl_migration.md` BUG-2 状态: `⚠️ 未修复` → `✅ 已修复 (commit <hash>)`. Gate 条件移除.
- **D4b**: AGENTS.md Pattern #12 follow-up 段: 记录本 change ship
- **D4c**: ADR-0087 benchmark 复跑: 跑 `tests/test_adr_0087_step5_1_benchmark` 或等价, 验证 C++ `ForkJoinLoop` 4-worker 3.3× speedup 不退化 (per `D2.inv.forkjoin-class-unchanged`)
- **D4d**: 更新 ADR-0089 v1.4 amendment ship gate checklist `[x]` 项 (本次 ship 后)

## Risks

| # | 风险 | 严重度 | 缓解 |
|---|---|---|---|
| R1 | BUG-2 修复方案选错 (4 候选之一), 实施后回归测试不通过 | 🟠 中 | RED 测试前置 (D1) + Oracle dual-review pre-impl + 多候选并行实验 |
| R2 | snapshot map 在 fork 期间被 evict 或 key mismatch | 🟠 中 | `enforce_snapshot_budget` 现有机制保留 + 验证 `current_fork_node_path_` 一致性 |
| R3 | C++ `ForkJoinLoop` 4-worker 行为受影响 (per D8.inv.forkjoin-class-unchanged) | 🟡 低 | `tests/test_pdk_fork_join.cpp` 5 cases + ADR-0087 benchmark 复跑 |
| R4 | YIELD 暂停与 fork 路由互不干扰 | 🟡 低 | 现有 test_scheduler 7 cases 基线守护 |
| R5 | 其他场景使用 snapshot 机制被破坏 | 🟡 低 | 全量 ctest 零回归 (`ctest -LE must_realllm 100% PASS`) |
| R6 | Pattern #10: fresh-deploy 验证 | 🟢 低 | 测试无 host 持久状态依赖 |
| R7 | 24h cooling-off + Oracle dual-review 时间成本 | 🟡 中 | 严格走完整 OpenSpec 流程 (与 fork-join-dsl-migration 不同, 不跳过) |

## Phase 实施 (TDD 5-step)

- **Phase 1 (commit 1) — RED tests**: 新增 `tests/test_fork_dsl_context_propagation.cpp` (5 cases, RED), 升级 `test_loop_agent_fork_join.cpp` Test 2 严格断言, **不**修 BUG-2 修复代码
- **Phase 2 (commit 2) — Oracle dual-review**: dual-agent review RED 测试 + 修复方案候选分析. Oracle 评审输出完整覆盖 RED 失败堆栈分析, 推荐修复方案 (D2 选 A/B/C/D)
- **Phase 3 (commit 3) — BUG-2 fix**: 按 Oracle 推荐方案修 TopoScheduler/ExecutionSession/ContextEngine. 单 atomic commit, 含详细 commit message 说明修复方案 + RED → GREEN 验证
- **Phase 4 (commit 4) — 回归验证**: 跑全量 ctest + ADR-0087 benchmark + examples loop_agent 5/5 + must_realllm 真 LLM 12/12. 零回归验证
- **Phase 5 (commit 5) — docs 同步**: 更新 tracker + AGENTS.md + ADR-0089 v1.4 ship gate. OpenSpec change archive. AGENTS.md Recent Changes 登记

## Acceptance Criteria

1. **BUG-2 修复** (D2): `lib/loop/fork_join.agent.md` 经 `loop/run loop_type=fork_join` 成功执行, task_a/b/c 节点的 `{{user_input}}` template render 成功
2. **零回归**: `ctest -LE must_realllm` 100% PASS (baseline 265/265)
3. **C++ ForkJoinLoop 4-worker 行为不变** (D8.inv.forkjoin-class-unchanged): `tests/test_pdk_fork_join.cpp` 5 cases 全过
4. **fork DSL e2e 完整**: `test_loop_agent_fork_join` Test 2 升级后 success=true
5. **ADR-0087 benchmark 不退化**: 4-worker 3.3× speedup 数据点不退化
6. **OpenSpec archive**: 5 文件 artifacts (proposal/design/spec/tasks/specs/*/spec.md) 完整, archive Day-5 4-file integrity (per AGENTS.md §Reverse Indicator)

## Ship Gate (per ADR-0089 v1.4 §Reverse Indicator Rule 5 字段)

每个 commit message 含:
- `+ new_up`: 新增 capability (fork context propagation 真 LLM 闭环)
- `- old_down`: drop_ratio (本 change 预期 ≤ 5%, 实际 0% — 修复是 additive, 既有路径不变)
- `failure_traces`: BUG-2 修复前失败样本 → constraint/hook + ship commit hash
- `ablation`: 3 段对照 (fork_join 修复前/后 × 同任务/同 Harness × fork branch 拦截保留率)
- `context_ids`: N/A (本 change 不改 context schema)