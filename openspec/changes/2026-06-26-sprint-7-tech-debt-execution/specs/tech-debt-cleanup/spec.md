# tech-debt-cleanup Specification

> **Purpose**: 闭环 Sprint 6 tech-debt-cleanup LIMFALL 偏离项, 让 Sprint 6 最终 archive

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

`tests/test_scheduler.cpp` MUST 含 ≥ 7 个新 TEST_CASE 覆盖 DagState 子函数行为

#### Scenario: scheduler 7 测试通过

- **WHEN** `ctest -R test_scheduler --output-on-failure`
- **THEN** MUST ≥ 7 新 case pass
- **AND** 既有测试 MUST 零回归

### Requirement: parser-test-coverage

`tests/test_parser.cpp` MUST 含 ≥ 5 个新 TEST_CASE 覆盖 NodeFactoryRegistry

#### Scenario: parser 5 测试通过

- **WHEN** `ctest -R test_parser --output-on-failure`
- **THEN** MUST ≥ 5 新 case pass
- **AND** 既有测试 MUST 零回归

#### Scenario: parser 5 测试 TSan 干净

- **WHEN** `cmake --preset tsan && ctest -R test_parser`
- **THEN** MUST 0 race (尤其 `factory_registry_concurrent_access` case)

### Requirement: node-factory-registry-count-corrected

`openspec/changes/tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md` MUST 修正 NodeType 计数 13 → 11

#### Scenario: spec 笔误修正

- **WHEN** `grep -E "13 (types|Nodes)|types.*13" tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md`
- **THEN** MUST 返回 0 命中 (全部替换为 11)

#### Scenario: spec throw → nullptr 修正

- **WHEN** `grep -E "throw.*unknown.*type|unknown.*throw" tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md`
- **THEN** MUST 返回 0 命中
- **AND** MUST 含 "returns nullptr" 字样

### Requirement: engine-cpp-include-le-3

`src/core/engine.cpp` 跨模块 include 计数 MUST ≤ 3 (ADR-0019 §1.4 退出标准)

#### Scenario: include 计数验证

- **WHEN** `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp`
- **THEN** MUST ≤ 3

#### Scenario: ToolRegistry factory 化

- **WHEN** `grep "make_unique<ToolRegistry>" src/core/engine.cpp`
- **THEN** MUST 返回 0 命中
- **AND** MUST 改用 `agenticdsl::tools::create_registry()`

#### Scenario: MockLLMProvider factory 化

- **WHEN** `grep "make_unique<MockLLMProvider>" src/core/engine.cpp`
- **THEN** MUST 返回 0 命中
- **AND** MUST 改用 `agenticdsl::llm::create_mock_provider()`

#### Scenario: IBudgetController 抽象注入

- **WHEN** `grep "BudgetController" src/core/engine.cpp`
- **THEN** MUST 仅 1 命中 (类型声明, 完整型不再使用)
- **AND** MUST 改用 `IBudgetController` 接口

### Requirement: scheduler-factory-resurrected

`src/modules/scheduler/factory.{h,cpp}` MUST 补 Config 参数并被 engine.cpp 调用

#### Scenario: factory 签名带 Config

- **WHEN** `grep "create.*SchedulerConfig" src/modules/scheduler/factory.h`
- **THEN** MUST 命中 ≥ 1
- **AND** 签名 MUST 含 `SchedulerConfig` + `IToolRegistry&` + `ILLMProvider*` + `vector<ParsedGraph>*` 4 参数

#### Scenario: engine.cpp 调用 factory

- **WHEN** `grep "agenticdsl::scheduler::create" src/core/engine.cpp`
- **THEN** MUST 命中 ≥ 1
- **AND** 替换 `make_unique<TopoScheduler>(...)` 调用

### Requirement: engine-factory-test-coverage

`tests/test_engine_factory.cpp` MUST 新建并含 3 个 TEST_CASE

#### Scenario: factory 3 测试通过

- **WHEN** `ctest -R test_engine_factory --output-on-failure`
- **THEN** MUST 3 case pass
- **AND** 覆盖: scheduler create / budget create / provider factory create

### Requirement: plugin-test-rename-and-e2e

`tests/test_plugin_loader.cpp` MUST 改名 7 case 匹配 spec + 实施 mock .so fixture E2E 测试

#### Scenario: 7 case 改名匹配 spec

- **WHEN** `grep -E "load_valid_plugin|abi_version_mismatch_strict|abi_version_mismatch_non_strict|dlsym_missing_register_fn|dlopen_failure_invalid_path|unload_all_raii_verification|load_all_search_paths" tests/test_plugin_loader.cpp`
- **THEN** MUST 命中 7

#### Scenario: mock .so fixture 存在

- **WHEN** `find build -name "mock_plugin*.so"`
- **THEN** MUST ≥ 3 文件存在 (mock_plugin + mock_plugin_v0 + mock_plugin_no_register)

#### Scenario: Loaded 状态覆盖

- **WHEN** `grep "state.*==.*Loaded\|set_loaded\|mark_loaded" tests/test_plugin_loader.cpp`
- **THEN** MUST ≥ 1 命中

#### Scenario: TEST_PLUGIN_FIXTURE_PATH 宏注入

- **WHEN** `grep "TEST_PLUGIN_FIXTURE_PATH" tests/CMakeLists.txt`
- **THEN** MUST 命中 `target_compile_definitions(test_plugin_loader PRIVATE TEST_PLUGIN_FIXTURE_PATH=...)`

### Requirement: ship-gate-all-pass

C1 ship gate 全部验证 MUST pass

#### Scenario: ctest 全绿

- **WHEN** `cd build && ctest --output-on-failure`
- **THEN** MUST ≥ 47/47 PASS

#### Scenario: TSan 全绿

- **WHEN** `cmake --preset tsan && ctest --output-on-failure`
- **THEN** MUST 0 race / 0 warning

#### Scenario: ASan 全绿

- **WHEN** `cmake --preset asan && ctest --output-on-failure`
- **THEN** MUST 0 leak

#### Scenario: docs audit 干净

- **WHEN** `python3 tools/adr_lint.py docs/adr/`
- **THEN** MUST exit 0
- **WHEN** `python3 tools/docs_drift_audit.py`
- **THEN** MUST 返回 0 critical drift

#### Scenario: openspec validate 成功

- **WHEN** `openspec validate 2026-06-26-sprint-7-tech-debt-execution`
- **THEN** MUST exit 0

#### Scenario: tech-debt-cleanup-sprint-6 archive 成功

- **WHEN** `openspec archive tech-debt-cleanup-sprint-6 --yes`
- **THEN** MUST exit 0
- **AND** `openspec list --specs` MUST 显示 `tech-debt-cleanup` spec 已合并

#### Scenario: PDK 同步无 error

- **WHEN** `./scripts/sync-pdk.sh --dry-run`
- **THEN** MUST exit 0, 0 error
