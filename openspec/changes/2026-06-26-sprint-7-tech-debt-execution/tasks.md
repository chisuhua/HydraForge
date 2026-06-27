# Tasks: Sprint 7 Tech-Debt Follow-up Execution (Sprint 11 主体)

> **来源**: `openspec/changes/archive/2026-06-23-sprint-7-tech-debt-followup/tasks.md` 11 sections 计划 (历史设计依据)
> **范围来源**: `tech-debt-cleanup-sprint-6/tasks.md` §6.3 (Oracle 审查 session `ses_112a9f9c5ffesqpYeefOBgMkjH` 决议)
> **总工时**: ~3 周 (Sprint 11) → **~2 周** (2026-06-27 stale-state 修正: engine.cpp include/execute/DagState/factory test 已 ship, 见 master plan §十一.1)
> **关联 change**: `tech-debt-cleanup-sprint-6` (本 change 完成后 archive)
> **关联 ADR**: ADR-0019 §1.4 (engine.cpp include ≤3), ADR-0021 §7 (PDK 同步)
> **创建日期**: 2026-06-26 (active 化执行)
> **前置依赖**: C0 (`2026-06-26-doc-alignment-adr-states`) ✅ archived 2026-06-26

> **⚠️ 2026-06-27 stale-state 修正 (master plan §十一.1 C1 行记录)**: 本 tasks.md 部分任务基于 2026-06-25 `engine-include-decoupling` 闭环前的 stale 代码状态. 当前实际状态 (2026-06-27 验证):
> - ✅ `engine.cpp` 跨模块 include 已 = 3 (≤3 目标达成, 6.4 任务已闭环)
> - ✅ `execute()` 已 54 行 (≤60 目标达成, 4.3 任务已闭环)
> - ✅ `DagState` 结构体已存在 (topo_scheduler.h:89, 4.1 任务已存在)
> - ✅ `test_engine_factory.cpp` 3 测试已存在 (commit `3681ba8`, 7.1 任务已存在)
> - ✅ `dispatch_next_node` 已不存在 (重命名为 `dispatch_ready_nodes`, Day 1.1 fork 重复修复已闭环, `execute_fork_branches` 仅 1 callsite at topo_scheduler.cpp:616)
> - ❌ `scheduler/factory.{h,cpp}` 不存在 (需 Day 8-9 实现)
> - ❌ `IBudgetController` 接口未引入 (需 Day 6.2 实现)
> - ❌ `create_registry()` / `create_mock_provider()` factory 函数未实现 (需 Day 6 实现)
> - 估时校正: 3 周 → **~2 周** (基线超前 ~1 周)

---

## 1. Day 1 - 🔴 Blocker fork 重复修复

> **✅ 2026-06-27 stale-state 闭环**: Day 1 工作已被 2026-06-25 `engine-include-decoupling` change 闭环. 当前代码状态:
> - `dispatch_next_node` 函数**不存在**, 已重命名为 `dispatch_ready_nodes` (topo_scheduler.cpp:484)
> - `execute_fork_branches` 仅 1 真正 callsite (`handle_fork_branches_block:616`), 1 定义 (L240), 1 注释 (L396), **无重复**
> - `execute()` 已 54 行 (≤60 目标), `DagState` 已引入
> - Day 1.1 任务 (删除 fork 死分支) **前提条件已不成立** — 死分支已在 2026-06-25 重构中消除

### 1.1 删除 `dispatch_next_node` 内 fork 处理死分支

- [x] 1.1.1 ~~编辑 `src/modules/scheduler/topo_scheduler.cpp:634-676`, 删除 L636-642 的 `if (is_executing_fork_branches_) { execute_fork_branches(); ... }` 块~~ — **已由 2026-06-25 `engine-include-decoupling` 重构闭环**, 当前代码无该模式
- [x] 1.1.2 ~~在 `dispatch_next_node()` 函数顶部加注释~~ — `dispatch_next_node` 已不存在, fork 分支在 `execute()` L173 `handle_fork_branches_block()` 调用
- [x] 1.1.3 验证: `grep -n "execute_fork_branches" src/modules/scheduler/topo_scheduler.cpp` 仅 1 命中 (execute() 内) — **当前 3 命中 (1 定义 L240, 1 注释 L396, 1 callsite L616), 实际 callsite 仅 1 处, 满足条件**
- [x] 1.1.4 `cmake --build build` + `ctest --output-on-failure` 33/33 PASS 零回归 — 2026-06-25 闭环时已验证
- [x] 1.1.5 提交: `fix(scheduler): remove duplicated fork handling in dispatch_next_node (Sprint 11 Blocker)` — 包含在 `2026-06-24-engine-include-final-decoupling` commits 中
- [ ] 1.1.6 更新 `tech-debt-cleanup-sprint-6/tasks.md` §6.3.1 标 `[x]` — **仍待更新** (Day 16 归档前完成)

---

## 2. Day 2-3 - 🔴 scheduler 测试补齐 (7 个)

### 2.1 `tests/test_scheduler.cpp` 增强

- [ ] 2.1.1 打开 `tests/test_scheduler.cpp`, 添加 7 个新 TEST_CASE 框架
- [ ] 2.1.2 `prepare_dag_state_simple_linear`: 3 节点线性 DAG, 验证 ready_queue 顺序
- [ ] 2.1.3 `prepare_dag_state_diamond`: 4 节点菱形, 验证拓扑序正确
- [ ] 2.1.4 `prepare_dag_state_cycle_detection`: A→B→A 循环, 期望 runtime_error("DAG cycle detected")
- [ ] 2.1.5 `dispatch_ready_nodes_initial`: 初始 ready 队列派发 2 节点
- [ ] 2.1.6 `dispatch_ready_nodes_parallel`: 3 个独立节点并发派发
- [ ] 2.1.7 `handle_node_completion_success`: 完成后下游节点入 ready_queue
- [ ] 2.1.8 `handle_node_completion_failure`: 失败传播, 2 个 downstream 标 Skipped
- [ ] 2.1.9 `ctest -R test_scheduler --output-on-failure` ≥ 7 新 case pass + 既有零回归
- [ ] 2.1.10 `cmake --preset tsan && ctest -R test_scheduler` 0 race
- [ ] 2.1.11 提交: `git commit -m "test(scheduler): add 7 state-based test cases for split execute() pipeline (Sprint 11)"`

---

## 3. Day 4 - 🔴 parser 测试补齐 (5 个)

### 3.1 `tests/test_parser.cpp` 增强

- [ ] 3.1.1 打开 `tests/test_parser.cpp`, 添加 5 个新 TEST_CASE
- [ ] 3.1.2 `factory_registry_registers_all_types`: size() == 11 (spec 修正后)
- [ ] 3.1.3 `factory_registry_creates_correct_subtype`: typeid 检查返回正确子类 (LLMNode / ToolNode / 等)
- [ ] 3.1.4 `factory_registry_unknown_type_returns_nullptr`: type="NonExistent", 期望 nullptr (旧行为, 验证 spec §3.3.3 修正)
- [ ] 3.1.5 `factory_registry_global_singleton`: 两次 global() 调用 `&r1 == &r2`
- [ ] 3.1.6 `factory_registry_concurrent_access`: 4 线程并发 create + 1 线程 register (延迟 1ms 后启动), TSan 0 race
- [ ] 3.1.7 `ctest -R test_parser --output-on-failure` ≥ 5 新 case pass + 既有零回归
- [ ] 3.1.8 `cmake --preset tsan && ctest -R test_parser` 0 race
- [ ] 3.1.9 提交: `git commit -m "test(parser): add 5 test cases for NodeFactoryRegistry incl. TSan (Sprint 11)"`

### 3.2 spec 笔误修正

- [ ] 3.2.1 编辑 `tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md`: L30 13 → 11, L40 13 → 11
- [ ] 3.2.2 编辑 `tech-debt-cleanup-sprint-6/tasks.md`: L139 13 → 11, L153 13 → 11
- [ ] 3.2.3 编辑 `tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md`: L61 "throws" → "returns nullptr"
- [ ] 3.2.4 编辑 `tech-debt-cleanup-sprint-6/tasks.md` §3.3.3 "保留 throw" → "保留 nullptr-on-unknown"
- [ ] 3.2.5 提交: `git commit -m "docs(openspec): fix spec typos (13→11 NodeType, throw→nullptr) (Sprint 11)"`

---

## 4. Day 5-7 - 🟠 scheduler 拆分收紧 + DagState + 访问一致

### 4.1 `DagState` 结构体引入

- [ ] 4.1.1 编辑 `src/modules/scheduler/topo_scheduler.h` (private 块): 新增 `struct DagState { unordered_map<NodeId, NodeExecutionStatus> nodes; queue<NodeId> ready_queue; int pending_count; vector<ParsedGraph> dynamic_graphs; };`
- [ ] 4.1.2 新增 `struct NodeExecutionStatus { NodeId id; NodeStatus status; NodeResult result; int indegree; vector<NodeId> dependents; };`
- [ ] 4.1.3 `cmake --build build` 编译通过

### 4.2 3 子函数重写为纯函数式

- [ ] 4.2.1 重写 `prepare_dag_state(DagState&, const ParsedGraph&, Context&)`: 解析 entry_point + 计算 indegree + 拓扑排序 (失败 throw) + 填充 ready_queue, 不改外部成员
- [ ] 4.2.2 重写 `dispatch_ready_nodes(DagState&, ExecutionSession&): size_t`: 遍历 ready_queue → execute_node → 更新 DagState.nodes[id].status = Running, 不直接调用 worker
- [ ] 4.2.3 新增 `handle_node_completion(DagState&, const NodeResult&): bool`: 更新 result → 遍历 dependents → 减 indegree → 0 时 push ready_queue + 失败传播 (dependents 标 Skipped), 不抛异常
- [ ] 4.2.4 真正提取 4 内联子逻辑: `resolve_dynamic_waits(DagState&)` / `process_jump(DagState&, NodeId)` / `process_fork_join(DagState&)` / `rebuild_dynamic_graph(DagState&)`, 各自成函数
- [ ] 4.2.5 `cmake --build build` 编译通过

### 4.3 `execute()` 收 ≤ 60 行

- [ ] 4.3.1 重写 `TopoScheduler::execute()` 仅保留: `DagState state;` → `prepare_dag_state(state, graph, ctx)` → 循环 `dispatch_ready_nodes + handle_node_completion` → `finalize_execution(state)` → 返回
- [ ] 4.3.2 验证: `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l` ≤ 60
- [ ] 4.3.3 `execute_single_branch` MUST NOT 修改 (保留 118 行)
- [ ] 4.3.4 `ctest -R test_scheduler --output-on-failure` 既有 + 7 新 case 全 pass

### 4.4 访问一致性

- [ ] 4.4.1 编辑 `src/modules/scheduler/topo_scheduler.cpp:669`: `session_.pending_dynamic_deps_` → `session_.get_pending_dynamic_deps()`
- [ ] 4.4.2 验证: `grep "session_\.pending_dynamic_deps_" src/modules/scheduler/` 0 命中

### 4.5 Hub 出度验证

- [ ] 4.5.1 实施 `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 `topo_scheduler::execute` out_degree < 30
- [ ] 4.5.2 验证 3 子函数 out_degree < 25
- [ ] 4.5.3 `create_node_from_json` out_degree < 30

### 4.6 提交

- [ ] 4.6.1 提交 1: `git commit -m "refactor(scheduler): introduce DagState + pure-function subfunctions + execute ≤ 60 lines (Sprint 11)"`
- [ ] 4.6.2 提交 2: `git commit -m "fix(scheduler): use get_pending_dynamic_deps() accessor consistently (Sprint 11)"`
- [ ] 4.6.3 更新 `tech-debt-cleanup-sprint-6/tasks.md` §6.3.3 + §6.3.6 标 `[x]`

---

## 5. Day 8-9 - 🟠 scheduler factory 复活 + engine.cpp 调用迁移

### 5.1 scheduler factory 补 `Config` 参数 (Decision 2 方案 A)

- [ ] 5.1.1 编辑 `src/modules/scheduler/factory.h`: 签名 `create(const SchedulerConfig&, IToolRegistry&, ILLMProvider*, const vector<ParsedGraph>*) -> unique_ptr<IScheduler>`
- [ ] 5.1.2 编辑 `src/modules/scheduler/factory.cpp`: 实现 body 用 cfg.initial_budget + tools + provider 构造 `TopoScheduler`
- [ ] 5.1.3 验证 `engine.cpp:188` 调用 `agenticdsl::scheduler::create(config, *tools_, provider_.get(), &dynamic_graphs_)` 替换 `make_unique<TopoScheduler>(...)`
- [ ] 5.1.4 `cmake --build build` 编译通过
- [ ] 5.1.5 `ctest --output-on-failure` 33/33 PASS
- [ ] 5.1.6 提交: `git commit -m "refactor(scheduler): add Config param to factory + integrate engine.cpp call (Sprint 11)"`

---

## 6. Day 10-11 - 🟠 engine.cpp include ≤ 3 续推

### 6.1 `ToolRegistry` factory 化

- [ ] 6.1.1 编辑 `src/common/tools/registry.h`: 新增 `namespace agenticdsl::tools { unique_ptr<ToolRegistry> create_registry(); }`
- [ ] 6.1.2 实现 body: `make_unique<ToolRegistry>()` (等价于 engine.cpp 原调用)
- [ ] 6.1.3 编辑 `src/core/engine.cpp`: `make_unique<ToolRegistry>()` → `agenticdsl::tools::create_registry()`
- [ ] 6.1.4 CMake: `common/tools/registry.cpp` 已存在, 无需新文件 (声明放 header)
- [ ] 6.1.5 验证: `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp` 计数应降 1

### 6.2 `IBudgetController` 抽象引入 (Decision 3 方案 A)

- [ ] 6.2.1 编辑 `src/modules/budget/budget_controller.h`: 在文件顶部加 `class IBudgetController { virtual ~IBudgetController()=default; virtual bool try_consume(double)=0; virtual void record_llm_call(const string&, double)=0; virtual double remaining() const=0; virtual void reset()=0; };`
- [ ] 6.2.2 改 `class BudgetController : public IBudgetController { ... };` 现有 public 方法 override (加 `override` 关键字)
- [ ] 6.2.3 编辑 `src/modules/budget/factory.h`: 返回 `unique_ptr<IBudgetController>`
- [ ] 6.2.4 编辑 `src/modules/budget/factory.cpp`: `return make_unique<BudgetController>();` (隐式转换 IBudgetController*)
- [ ] 6.2.5 编辑 `src/core/engine.cpp`: `budget_controller_` 类型改 `unique_ptr<IBudgetController>`, `get_budget_controller()` 返回 `IBudgetController&`
- [ ] 6.2.6 验证: `grep "IBudgetController" src/core/engine.cpp` 仅 1 命中 (类型声明)
- [ ] 6.2.7 `cmake --build build` + `ctest` 33/33 PASS

### 6.3 `MockLLMProvider` factory 化

- [ ] 6.3.1 编辑 `src/common/llm/factory.h`: 新增 `namespace agenticdsl::llm { unique_ptr<ILLMProvider> create_mock_provider(); }`
- [ ] 6.3.2 编辑 `src/common/llm/factory.cpp`: 实现 body
- [ ] 6.3.3 编辑 `src/core/engine.cpp:127`: `make_unique<MockLLMProvider>()` → `agenticdsl::llm::create_mock_provider()`
- [ ] 6.3.4 `cmake --build build` 编译通过

### 6.4 include 计数验证

- [ ] 6.4.1 `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp` ≤ 3
- [ ] 6.4.2 若 > 3: 继续工厂化 (`IBudgetController` 已降 1, `MockLLMProvider` 已降 1, `ToolRegistry` 已降 1, 总降 3, 应达 ≤ 3)
- [ ] 6.4.3 `ctest --output-on-failure` 33/33 PASS

### 6.5 提交

- [ ] 6.5.1 提交 1: `git commit -m "refactor(tools): add create_registry factory + integrate engine.cpp (Sprint 11)"`
- [ ] 6.5.2 提交 2: `git commit -m "refactor(budget): introduce IBudgetController abstraction + factory returns interface (Sprint 11)"`
- [ ] 6.5.3 提交 3: `git commit -m "refactor(llm): add create_mock_provider factory + engine.cpp include ≤ 3 (Sprint 11)"`
- [ ] 6.5.4 更新 `tech-debt-cleanup-sprint-6/tasks.md` §6.3.5 标 `[x]`

---

## 7. Day 12-13 - 🔴 factory 测试新建 (3 个)

### 7.1 新建 `tests/test_engine_factory.cpp`

- [ ] 7.1.1 新建文件, 文件头注释 + `#include` 头
- [ ] 7.1.2 `TEST_CASE("test_scheduler_create_default_config")`: 验证 `agenticdsl::scheduler::create(SchedulerConfig{}, ...)` 返回非空 + 后续 execute 行为正确
- [ ] 7.1.3 `TEST_CASE("test_budget_create_default_config")`: 验证 `agenticdsl::budget::create_controller()` 返回非空 + `try_consume(0)` 行为
- [ ] 7.1.4 `TEST_CASE("test_provider_factory_create_llm_config")`: 验证 `agenticdsl::llm::create_provider_factory()` 返回非空
- [ ] 7.1.5 编辑 `tests/CMakeLists.txt`: 添加 `add_executable(test_engine_factory tests/test_engine_factory.cpp)` + `target_link_libraries(test_engine_factory ...)`
- [ ] 7.1.6 `ctest -R test_engine_factory --output-on-failure` ≥ 3 pass
- [ ] 7.1.7 提交: `git commit -m "test(engine_factory): add 3 test cases for 3 factory functions (Sprint 11)"`
- [ ] 7.1.8 更新 `tech-debt-cleanup-sprint-6/tasks.md` §6.3.4 (factory 部分) 标 `[x]`

---

## 8. Day 14-15 - 🟠 plugin 测试改名 + E2E fixture

### 8.1 plugin 测试改名匹配 spec

- [ ] 8.1.1 编辑 `tests/test_plugin_loader.cpp`:
  - `destructor_safe` → `unload_all_raii_verification`
  - `multiple_failures` → `dlsym_missing_register_fn` (扩展实现, 用 mock dlopen)
  - `load_all_zero_paths` → `load_all_search_paths` (扩展: 加 2 个 mock path)
  - `path_traversal` → `dlopen_failure_invalid_path`
  - `empty_path_validation` → `load_valid_plugin` (E2E 实施)
  - `idempotent_unload` → `abi_version_mismatch_strict`
  - `list_loaded_copy` → `abi_version_mismatch_non_strict`
- [ ] 8.1.2 验证: `grep -E "load_valid_plugin|abi_version_mismatch_strict|abi_version_mismatch_non_strict|dlsym_missing_register_fn|dlopen_failure_invalid_path|unload_all_raii_verification|load_all_search_paths" tests/test_plugin_loader.cpp` 7 命中

### 8.2 mock .so fixture (Decision 5 方案 A)

- [ ] 8.2.1 新建 `tests/fixtures/mock_plugin.cpp` (PDK_ABI_VERSION=1, 提供 pdk_register_tools)
- [ ] 8.2.2 新建 `tests/fixtures/mock_plugin_v0.cpp` (PDK_ABI_VERSION=0, 触发 strict mismatch)
- [ ] 8.2.3 新建 `tests/fixtures/mock_plugin_no_register.cpp` (无 pdk_register_tools 符号)
- [ ] 8.2.4 编辑 `tests/CMakeLists.txt`: `add_custom_target(plugin_fixtures ALL ...)` + `add_dependencies(test_plugin_loader plugin_fixtures)` + `target_compile_definitions(test_plugin_loader PRIVATE TEST_PLUGIN_FIXTURE_PATH="${CMAKE_BINARY_DIR}")`
- [ ] 8.2.5 `cmake --build build` + 验证 `build/mock_plugin*.so` 3 个文件存在

### 8.3 E2E 测试启用

- [ ] 8.3.1 编辑 `tests/test_plugin_loader.cpp:206`: 删除 `#ifdef TEST_PLUGIN_FIXTURE_PATH` 守卫, 改用 `#ifndef` 或运行时检查
- [ ] 8.3.2 实施 spec 推迟的 7 case (load_valid / abi_mismatch_strict / abi_mismatch_non_strict / dlsym_missing / dlopen_invalid / unload_raii / load_all_search_paths)
- [ ] 8.3.3 `ctest -R test_plugin_loader --output-on-failure` ≥ 7 pass (含 E2E)
- [ ] 8.3.4 验证: Loaded 状态真覆盖 (grep "state==Loaded\|set_loaded\|mark_loaded" 至少 1 命中)

### 8.4 提交

- [ ] 8.4.1 提交 1: `git commit -m "test(plugin): rename 7 test cases to match spec + implement mock .so fixture (Sprint 11)"`
- [ ] 8.4.2 更新 `tech-debt-cleanup-sprint-6/tasks.md` §6.3.8 + §6.3.9 标 `[x]`

---

## 9. Day 16 - 🟡 Minor (ship gate 验证)

### 9.1 各种 ship gate 全跑

- [ ] 9.1.1 `cd build && ctest --output-on-failure` ≥ 47/47 (33 baseline + 7 scheduler + 5 parser + 3 factory - 1 plugin = 47; plugin 7 case 改名保留)
- [ ] 9.1.2 `cmake --preset tsan && ctest --output-on-failure` 0 race
- [ ] 9.1.3 `cmake --preset asan && ctest --output-on-failure` 0 leak
- [ ] 9.1.4 `python3 tools/adr_lint.py docs/adr/` exit 0
- [ ] 9.1.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 9.1.6 `openspec validate 2026-06-26-sprint-7-tech-debt-execution` exit 0 (含 spec 修正后)
- [ ] 9.1.7 `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 execute / create_node_from_json out_degree < 30
- [ ] 9.1.8 `git status` clean, 14+ commits 按 Day 分组

### 9.2 文档同步

- [ ] 9.2.1 编辑 `docs/adr/adr-0019-*.md` §1.4 状态: engine.cpp include ≤ 3 达成 (本次 Sprint 11)
- [ ] 9.2.2 编辑 `docs/adr/adr-0020-thread-model-isolation.md` (无需改, Sprint 3 已 Resolved)
- [ ] 9.2.3 编辑 `docs/README.md` ADR-0019 状态行同步
- [ ] 9.2.4 编辑 `AGENTS.md` § Recent Changes 追加 Sprint 11 总结 + Sprint 6 final 标记
- [ ] 9.2.5 更新 `tech-debt-cleanup-sprint-6/tasks.md` §5 全部 `[x]` + §6.3 全部 `[x]`
- [ ] 9.2.6 更新 `tech-debt-cleanup-sprint-6/tasks.md` §4 全部 `[x]`
- [ ] 9.2.7 提交: `git commit -m "docs(adr+status): Sprint 11 ship + ADR-0019 §1.4 final Resolved (engine.cpp ≤3)"`

---

## 10. Day 17 - 📦 归档 `tech-debt-cleanup-sprint-6`

### 10.1 最终验证

- [ ] 10.1.1 跑 §9.1 全部 ship gate 一次, 全 pass
- [ ] 10.1.2 跑 `openspec show tech-debt-cleanup-sprint-6` 确认所有 task 100% 完成

### 10.2 归档

- [ ] 10.2.1 `openspec archive tech-debt-cleanup-sprint-6 --yes` (archive 到 `openspec/specs/`)
- [ ] 10.2.2 `openspec list --specs` 确认 `dag-scheduler-pipeline` / `node-factory-registry` / `tech-debt-cleanup` spec 已合并
- [ ] 10.2.3 编辑本 change `tasks.md` §10.2.1 标 `[x]`

### 10.3 PDK 同步

- [ ] 10.3.1 `./scripts/sync-pdk.sh --dry-run` 无 error
- [ ] 10.3.2 `./scripts/sync-pdk.sh` 实际同步 (factory.h, IBudgetController.h, node_factory.h 涉及)

### 10.4 最终提交

- [ ] 10.4.1 提交: `git commit -m "chore(openspec): archive tech-debt-cleanup-sprint-6 + Sprint 11 final (Sprint 11 ship)"`
- [ ] 10.4.2 `git tag sprint-11-tech-debt-followup-ship` + `git push --tags`
- [ ] 10.4.3 更新 `AGENTS.md` 顶部状态: Sprint 11 ship 标记

### 10.5 同步 master plan

- [ ] 10.5.1 编辑 `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C1 行:
  - 状态: `🟡 准备中` → `✅ archived (2026-07-17)`
  - 追加 archive 链接

---

## 11. 验证检查清单 (C1 ship gate)

- [ ] 11.1 Sprint 6 + Sprint 11 全部 task 100% 完成
- [ ] 11.2 `cd build && ctest --output-on-failure` ≥ 47/47 PASS
- [ ] 11.3 `cmake --preset tsan && ctest --output-on-failure` 0 race
- [ ] 11.4 `cmake --preset asan && ctest --output-on-failure` 0 leak
- [ ] 11.5 `python3 tools/adr_lint.py docs/adr/` exit 0
- [ ] 11.6 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 11.7 `openspec validate 2026-06-26-sprint-7-tech-debt-execution` exit 0
- [ ] 11.8 `openspec archive tech-debt-cleanup-sprint-6` 成功
- [ ] 11.9 `code-review-graph get_hub_nodes` 2 目标函数 out_degree < 30
- [ ] 11.10 `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp` ≤ 3
- [ ] 11.11 `git status` clean, 14-17 commits 按 Day 分组
- [ ] 11.12 AGENTS.md § Recent Changes 含 Sprint 6 final + Sprint 11 ship 标记
- [ ] 11.13 PDK 同步脚本无 error (`./scripts/sync-pdk.sh --dry-run`)
- [ ] 11.14 `git log --oneline -1` 显示 Sprint 11 final commit
- [ ] 11.15 master plan C1 行状态更新
