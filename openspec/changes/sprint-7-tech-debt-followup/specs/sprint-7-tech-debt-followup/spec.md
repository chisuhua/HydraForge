## ADDED Requirements

### Requirement: scheduler-fork-dedup

`src/modules/scheduler/topo_scheduler.cpp` MUST NOT 含重复的 fork 处理逻辑 — `execute()` L161-167 与 `dispatch_next_node()` L636-642 当前逐字相同。

#### Scenario: fork 处理仅 1 处

- **WHEN** `grep -n "execute_fork_branches" src/modules/scheduler/topo_scheduler.cpp`
- **THEN** MUST 仅 1 个调用点 (位于 `execute()` 内, 排除函数定义与文档注释)
- **AND** `dispatch_next_node()` MUST NOT 调用 `execute_fork_branches`

### Requirement: scheduler-pipeline-tightened

`TopoScheduler::execute()` MUST 进一步拆分为纯函数式 3 子函数, 函数体 MUST ≤ 60 行:

- MUST 新增 `struct DagState { unordered_map<NodeId, NodeExecutionStatus> nodes; queue<NodeId> ready_queue; int pending_count; };`
- MUST 真正提取动态 `wait_for` 解析 / jump 处理 / fork-join / 动态图重建 各自成子函数
- MUST 3 子函数声明为 `private`, 仅通过 `DagState&` 参数通信, 不直接修改 `TopoScheduler` 成员
- MUST 使用 `session_.get_pending_dynamic_deps()` 访问器 (非直接访问 `pending_dynamic_deps_`)

#### Scenario: execute 函数行数上限

- **WHEN** `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l`
- **THEN** MUST ≤ 60 行

#### Scenario: DagState 结构体存在

- **WHEN** `grep "struct DagState" src/modules/scheduler/topo_scheduler.h`
- **THEN** MUST 命中 1 个结构体定义
- **AND** MUST 含 `nodes` / `ready_queue` / `pending_count` 3 个字段

#### Scenario: 3 子函数命名匹配 spec

- **WHEN** `grep "prepare_dag_state\|dispatch_ready_nodes\|handle_node_completion" src/modules/scheduler/topo_scheduler.h`
- **THEN** MUST 命中 3 个函数声明
- **AND** 名称 MUST 严格匹配 (非 `dispatch_next_node` / `finalize_execution` 等偏离命名)

#### Scenario: 访问一致

- **WHEN** `grep "session_\.pending_dynamic_deps_" src/modules/scheduler/ -r`
- **THEN** MUST 返回 0 命中 (统一使用 accessor)

#### Scenario: Hub out_degree

- **WHEN** `mcp__code-review-graph__get_hub_nodes --top_n 5`
- **THEN** `topo_scheduler::execute` out_degree MUST < 30
- **AND** 3 子函数各自 out_degree MUST < 25

### Requirement: scheduler-test-coverage

`tests/test_scheduler.cpp` MUST 新增 ≥ 7 个 Catch2 test case 验证 3 子函数:

- `prepare_dag_state_simple_linear`: 3 节点线性 DAG ready_queue 顺序
- `prepare_dag_state_diamond`: 4 节点菱形拓扑序正确
- `prepare_dag_state_cycle_detection`: A→B→A 循环 runtime_error
- `dispatch_ready_nodes_initial`: 初始 ready 队列派发 2 节点
- `dispatch_ready_nodes_parallel`: 3 独立节点并发派发
- `handle_node_completion_success`: 完成触发下游
- `handle_node_completion_failure`: 失败传播, 2 downstream 标 Skipped

#### Scenario: 7 测试通过

- **WHEN** `cd build && ctest -R test_scheduler --output-on-failure`
- **THEN** MUST 至少 7 个新 TEST_CASE 通过
- **AND** 既有 test_scheduler test MUST 零回归

#### Scenario: TSan 干净

- **WHEN** `cmake --preset tsan && ctest -R test_scheduler --output-on-failure`
- **THEN** MUST 0 data race report

### Requirement: parser-test-coverage

`tests/test_parser.cpp` MUST 新增 ≥ 5 个 Catch2 test case 验证 NodeFactoryRegistry:

- `factory_registry_registers_all_types`: size() == 11
- `factory_registry_creates_correct_subtype`: typeid 检查
- `factory_registry_unknown_type_returns_nullptr`: 未注册 type 返回 nullptr (NOT throw)
- `factory_registry_global_singleton`: 两次 `global()` 返回同一地址
- `factory_registry_concurrent_access`: 4 线程并发 create + 1 线程 register (延迟 1ms), TSan 0 race

#### Scenario: 5 测试通过

- **WHEN** `cd build && ctest -R test_parser --output-on-failure`
- **THEN** MUST 至少 5 个新 TEST_CASE 通过
- **AND** 既有 test_parser test MUST 零回归

#### Scenario: TSan 并发干净

- **WHEN** `cmake --preset tsan && ctest -R test_parser --output-on-failure`
- **THEN** MUST 0 data race report

### Requirement: parser-spec-typo-fix

`tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md` 与 `tasks.md` MUST 修正 13 → 11 NodeType 笔误, MUST 改 `throw` → `nullptr` 描述 (旧行为本就 nullptr-on-unknown):

- spec.md L30, L40 "13" → "11"
- spec.md L61 "throws" → "returns nullptr"
- tasks.md L139, L153 "13" → "11"
- tasks.md §3.3.3 "throw" → "nullptr"

#### Scenario: spec 修正验证

- **WHEN** `grep -nE "13|\bthrow\b" tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md tech-debt-cleanup-sprint-6/tasks.md`
- **THEN** MUST 不含上述 13 / throw 笔误 (除 spec 故意保留 throw 路径说明的例外)

### Requirement: scheduler-factory-integration

`src/modules/scheduler/factory.h::create` MUST 接收 `SchedulerConfig` 参数, `engine.cpp` MUST 调用此 factory 替换直接 `make_unique<TopoScheduler>()`:

- factory 签名: `create(const SchedulerConfig&, IToolRegistry&, ILLMProvider*, const vector<ParsedGraph>*) -> unique_ptr<IScheduler>`
- `engine.cpp:188` MUST 改用 `agenticdsl::scheduler::create(config, *tools_, provider_.get(), &dynamic_graphs_)`

#### Scenario: factory 调用点存在

- **WHEN** `grep -n "scheduler::create" src/core/engine.cpp`
- **THEN** MUST 命中 1 处调用
- **AND** `grep -n "make_unique<TopoScheduler>" src/core/engine.cpp` MUST 返回 0 命中

#### Scenario: factory 签名正确

- **WHEN** `grep "SchedulerConfig" src/modules/scheduler/factory.h`
- **THEN** MUST 命中参数声明

### Requirement: engine-cpp-include-reduction

`src/core/engine.cpp` 跨模块/common include 数 MUST ≤ 3 (Sprint 6 ship 时实测 10):

- `make_unique<ToolRegistry>()` → `agenticdsl::tools::create_registry()` (factory 化, 头依赖降)
- `make_unique<MockLLMProvider>()` → `agenticdsl::llm::create_mock_provider()` (factory 化, 头依赖降)
- 引入 `IBudgetController` 接口, `budget::create_controller()` 返回 `unique_ptr<IBudgetController>`, `engine.cpp` 切换完整型 → 接口引用

#### Scenario: engine.cpp include 计数

- **WHEN** `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp`
- **THEN** MUST ≤ 3

#### Scenario: 直接构造消除

- **WHEN** `grep -nE "make_unique<ToolRegistry>|make_unique<MockLLMProvider>|make_unique<BudgetController>" src/core/engine.cpp`
- **THEN** MUST 返回 0 命中 (全部通过 factory)

### Requirement: factory-test-coverage

`tests/test_engine_factory.cpp` MUST 新建并含 ≥ 3 个 Catch2 test case:

- `test_scheduler_create_default_config`: `scheduler::create(SchedulerConfig{}, ...)` 返回非空
- `test_budget_create_default_config`: `budget::create_controller()` 返回非空
- `test_provider_factory_create_llm_config`: `llm::create_provider_factory()` 返回非空

#### Scenario: 3 factory 测试通过

- **WHEN** `cd build && ctest -R test_engine_factory --output-on-failure`
- **THEN** MUST 至少 3 个 TEST_CASE 通过
- **AND** test_engine_factory MUST 已注册到 `tests/CMakeLists.txt`

### Requirement: plugin-loader-test-rename-and-e2e

`tests/test_plugin_loader.cpp` MUST 重命名 7 case 匹配 spec, MUST 实施推迟的 E2E mock .so fixture 测试:

- `destructor_safe` → `unload_all_raii_verification`
- `multiple_failures` → `dlsym_missing_register_fn` (用 mock dlopen)
- `load_all_zero_paths` → `load_all_search_paths` (扩展: 2 mock path)
- `path_traversal` → `dlopen_failure_invalid_path`
- `empty_path_validation` → `load_valid_plugin` (E2E)
- `idempotent_unload` → `abi_version_mismatch_strict`
- `list_loaded_copy` → `abi_version_mismatch_non_strict`

`tests/CMakeLists.txt` MUST 注入 `TEST_PLUGIN_FIXTURE_PATH` 宏定义, 启用 E2E 测试编译。

#### Scenario: 7 case 命名匹配 spec

- **WHEN** `grep -E "load_valid_plugin|abi_version_mismatch_strict|abi_version_mismatch_non_strict|dlsym_missing_register_fn|dlopen_failure_invalid_path|unload_all_raii_verification|load_all_search_paths" tests/test_plugin_loader.cpp`
- **THEN** MUST 命中 7 个 TEST_CASE

#### Scenario: TEST_PLUGIN_FIXTURE_PATH 注入

- **WHEN** `grep "TEST_PLUGIN_FIXTURE_PATH" tests/CMakeLists.txt`
- **THEN** MUST 含 `target_compile_definitions` 行

#### Scenario: mock .so fixture 存在

- **WHEN** `ls build/mock_plugin*.so`
- **THEN** MUST ≥ 1 个 .so 文件存在 (至少 valid 变体)

#### Scenario: E2E Loaded 状态覆盖

- **WHEN** `grep "state.*Loaded\|set_loaded\|mark_loaded" tests/test_plugin_loader.cpp`
- **THEN** MUST ≥ 1 命中 (Loaded 状态真覆盖)

### Requirement: sprint-7-ship-gate

Sprint 7 完成 MUST 满足 Sprint 6 全部 ship gate 项:

- `ctest --output-on-failure` ≥ 47/47 PASS (33 baseline + 7 scheduler + 5 parser + 3 factory - 1 plugin = 47; plugin 7 case 改名保留)
- TSan + ASan 全 pass, 0 race / 0 leak
- `python3 tools/adr_lint.py docs/adr/` exit 0
- `python3 tools/docs_drift_audit.py` 0 critical drift
- `openspec validate tech-debt-cleanup-sprint-6` exit 0 (含本 change 引入的 spec 修正)
- `openspec archive tech-debt-cleanup-sprint-6 --yes` 成功
- `git status` clean, 14+ commits 按 Day 分组
- AGENTS.md § Recent Changes 含 Sprint 6 final + Sprint 7 ship 标记
- PDK 同步脚本无 error (`./scripts/sync-pdk.sh --dry-run`)

#### Scenario: 47/47 ctest

- **WHEN** `cd build && ctest --output-on-failure`
- **THEN** MUST ≥ 47 测试通过, 0 failed

#### Scenario: TSan + ASan 矩阵

- **WHEN** `cmake --preset tsan && ctest --output-on-failure`
- **THEN** MUST 0 race report
- **AND** `cmake --preset asan && ctest --output-on-failure` MUST 0 leak report

#### Scenario: 归档成功

- **WHEN** `openspec archive tech-debt-cleanup-sprint-6 --yes`
- **THEN** MUST exit 0
- **AND** `openspec list` MUST NOT 含 `tech-debt-cleanup-sprint-6` (已转 archived)
