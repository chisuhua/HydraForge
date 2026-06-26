# Proposal: Sprint 7 Tech-Debt Follow-up Execution (Sprint 11 主体)

> **变更类型**: 实施 (Sprint 6 tech-debt-cleanup LIMFALL 闭环)
> **作者**: Sisyphus
> **创建日期**: 2026-06-26
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C1
> **关联 ADR**: ADR-0019 §1.4 (engine.cpp include ≤3) / ADR-0021 §7 (PDK 同步)
> **关联 change**: `tech-debt-cleanup-sprint-6` (待 archive) / `archive/2026-06-23-sprint-7-tech-debt-followup` (设计依据)
> **前置**: C0 (`2026-06-26-doc-alignment-adr-states`) 完成, 同步 ADR-0019 §1.4 状态
> **后续**: C2 (`2026-06-26-adr-0030-v2-async-runtime`) 依赖本 change engine.cpp decoupling 达成

## Why

2026-06-22 Oracle 深度审查 Sprint 6 `tech-debt-cleanup-sprint-6` 4 个代码 commit (`7cc4239` / `6c5557c` / `9fa0364` / `7923b2a`) 决议: **4 commits 保持合入不回退** (33/33 ctest pass, 行为保持, git status clean), **不 archive** OpenSpec change `tech-debt-cleanup-sprint-6`, 但**验收标准系统性未达**:

- 🔴 scheduler fork 处理逻辑被复制两处 (`topo_scheduler.cpp:161-167` 与 `:636-642` 逐字相同) — 唯一有 bug 风险的代码缺陷
- 🟠 `execute()` 222 行 (spec 要求 ≤ 60 行, 差 3.7 倍)
- 🟠 scheduler 函数命名 2/3 不符 spec; 无 `DagState` 结构体; `handle_node_completion` 未提取
- 🟠 `engine.cpp` 跨模块 include 10 个 (spec 要求 ≤ 3)
- 🟠 scheduler factory 死代码 (零调用点, 签名漏 `Config` 参数)
- 🔴 15 个新测试 (7 scheduler + 5 parser + 3 factory) **0 个交付**
- 🟠 plugin 测试 7 case 名称/范围不符 spec; Loaded 状态零覆盖 (E2E `TEST_PLUGIN_FIXTURE_PATH` 宏未注入)

详细偏离表见 `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` §6.1, Oracle 报告 session `ses_112a9f9c5ffesqpYeefOBgMkjH`.

2026-06-22 同步创建的 `openspec/changes/archive/2026-06-23-sprint-7-tech-debt-followup/` change 完整记录了 10 sections, 14-17 commits 的修复计划 (proposal.md 151 行, design.md 5 个 Decision, tasks.md 281 行 10 sections), 但**未实施 (tasks 0% 完成)**. 该 change 已 archive, spec 合并到 `openspec/specs/sprint-7-tech-debt-followup/spec.md`.

本 change 在**保留原有 10 sections 计划**基础上, 重新 active 化执行, 闭环 `tech-debt-cleanup-sprint-6` 全部偏离项, 让 Sprint 6 最终 archive.

不解决此问题: (a) Sprint 6 永远不能 archive, backlog 持续累积; (b) `engine.cpp` 跨模块 include 计数无法达到 ADR-0019 §1.4 完全退出标准; (c) scheduler hub out_degree 过高, 重构风险持续; (d) 后续 C2 (Phase 2) 启动时无干净的 engine.cpp 基础.

## What Changes

### 🔴 Blocker (Day 1 必做, 1 PR)

- **删除 fork 处理重复块**: 移除 `src/modules/scheduler/topo_scheduler.cpp:636-642` (`dispatch_next_node` 内的 fork 处理, 因 `execute()` 161-167 已置 `is_executing_fork_branches_=false` 故为死分支). 修后 ctest 零回归.

### 🟠 Major (Sprint 7 主体, 5 commits)

- **scheduler 拆分收紧 + `DagState` 结构体引入**:
  - 新增 `struct DagState { unordered_map<NodeId, NodeExecutionStatus> nodes; queue<NodeId> ready_queue; int pending_count; }`
  - 重写 3 子函数为纯函数式: `prepare_dag_state(DagState&, const ParsedGraph&, Context&)` / `dispatch_ready_nodes(DagState&, ExecutionSession&)` / `handle_node_completion(DagState&, const NodeResult&)`
  - 真正提取动态 `wait_for` 解析 (184-229) / jump 处理 (239-254) / fork-join (262-292) / 动态图重建 (321-362) 各自成子函数
  - 收 `execute()` 到 ≤ 60 行
  - 验证 `code-review-graph get_hub_nodes` out_degree < 30

- **scheduler 工厂复活**: 补 `Config` 参数 (`create(const SchedulerConfig&, IToolRegistry&, ILLMProvider*, const vector<ParsedGraph>*)`), 改 `engine.cpp:188` 调用 `agenticdsl::scheduler::create(config, ...)`, 让 budget 注入生效

- **`engine.cpp` 跨模块 include 续推 10 → ≤ 3**:
  - 工厂化 `ToolRegistry` (`make_unique<ToolRegistry>()` → `agenticdsl::tools::create_registry()`)
  - 工厂化 `MockLLMProvider` (`make_unique<MockLLMProvider>()` → `agenticdsl::llm::create_mock_provider()`)
  - 引入 `IBudgetController` 抽象, 让 `budget::create_controller()` 返回接口而非具体类, 解 `engine.cpp` 完整型依赖
  - 验证: `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp` ≤ 3

### 🔴 测试补齐 (15 个新 TEST_CASE, 3 commits)

- **scheduler (7 个, `tests/test_scheduler.cpp`)**:
  - `prepare_dag_state_simple_linear`: 3 节点线性 DAG ready_queue 顺序
  - `prepare_dag_state_diamond`: 4 节点菱形拓扑序
  - `prepare_dag_state_cycle_detection`: A→B→A 循环 runtime_error
  - `dispatch_ready_nodes_initial`: 初始 ready 队列派发
  - `dispatch_ready_nodes_parallel`: 3 独立节点并发派发
  - `handle_node_completion_success`: 完成触发下游
  - `handle_node_completion_failure`: 失败传播, downstream 标 Skipped

- **parser (5 个, `tests/test_parser.cpp`)**:
  - `factory_registry_registers_all_types`: size() == 11 (修 spec 13 → 11)
  - `factory_registry_creates_correct_subtype`: typeid 检查
  - `factory_registry_unknown_type_returns_nullptr`: 验证旧语义
  - `factory_registry_global_singleton`: global() 同地址
  - `factory_registry_concurrent_access`: 4 线程并发 create + 1 线程 register, TSan 0 race

- **factory (3 个, `tests/test_engine_factory.cpp` 新建)**:
  - `test_scheduler_create_default_config`
  - `test_budget_create_default_config`
  - `test_provider_factory_create_llm_config`

- **plugin (7 case 重命名 + 补 E2E, 2 commits)**:
  - 改名: `destructor_safe` → `unload_all_raii_verification` / `multiple_failures` → `dlsym_missing_register_fn` / `load_all_zero_paths` → `load_all_search_paths` / 等
  - 实施推迟的 `load_valid_plugin` / `abi_version_mismatch_strict` / `abi_version_mismatch_non_strict`
  - CMake `target_compile_definitions(tests PRIVATE TEST_PLUGIN_FIXTURE_PATH=...)` 注入宏 + 创建 mock .so fixture
  - 验证 Loaded 状态真覆盖

### 🟡 Minor (顺手做, 2 commits)

- 修 `pending_dynamic_deps_` 访问不一致: `dispatch_next_node()` L669 改用 `session_.get_pending_dynamic_deps()` 访问器
- 修 spec 笔误: `node-factory-registry/spec.md:30,40` 与 `tasks.md:3.2.3/3.4.2` 13 → 11
- 修 `create_node_from_json` `has_factory` 预检逻辑或改 spec §3.3.3
- 删/补 `tests/test_plugin_loader.cpp:206` `#ifdef TEST_PLUGIN_FIXTURE_PATH` 死代码

### 📦 归档

- 验证 `tech-debt-cleanup-sprint-6` 全部验收项达标 (含本 change 引入的 spec 修正)
- `openspec archive tech-debt-cleanup-sprint-6 --yes` + 同步到 `openspec/specs/*`
- 更新 `AGENTS.md` § Recent Changes 标记 Sprint 6 final + Sprint 11 ship
- 同步 PDK 头文件: `./scripts/sync-pdk.sh` (factory.h 涉及, 双仓库同步)

## Capabilities

### Modified Capabilities

- `dag-scheduler-pipeline`: 修正 execute ≤ 60 行 / DagState 结构体 / 3 子函数命名 (dispatch_ready_nodes / handle_node_completion) / 7 测试 / hub out_degree < 30
- `node-factory-registry`: 修正 NodeType 数 13 → 11 + 改 throw → nullptr + 补 5 测试含 TSan
- `tech-debt-cleanup`: 修 engine.cpp include ≤ 3 + 补 3 factory 测试 + plugin 测试重命名 + Loaded 状态覆盖

## Impact

**修改文件**:
- `src/modules/scheduler/topo_scheduler.{h,cpp}` (拆分收紧 + DagState + 修 fork 重复)
- `src/modules/parser/markdown_parser.cpp` (修 has_factory 逻辑)
- `src/core/engine.cpp` (工厂调用 ToolRegistry / MockLLMProvider, IBudgetController 注入)
- `src/common/tools/registry.{h,cpp}` (新增 create_registry factory + IToolRegistry 抽象)
- `src/common/llm/mock_provider.{h,cpp}` (新增 create_mock_provider factory)
- `src/modules/budget/budget_controller.h` (引入 IBudgetController 接口)
- `src/modules/scheduler/factory.{h,cpp}` (补 Config 参数)
- `src/modules/budget/factory.{h,cpp}` (改返回 IBudgetController)
- `tests/test_scheduler.cpp` (新增 7)
- `tests/test_parser.cpp` (新增 5)
- `tests/test_engine_factory.cpp` (新建, 3)
- `tests/test_plugin_loader.cpp` (改名 + 实施 mock fixture + Loaded 覆盖)
- `tests/CMakeLists.txt` (注入 TEST_PLUGIN_FIXTURE_PATH)
- `openspec/changes/tech-debt-cleanup-sprint-6/specs/*/spec.md` (修正 13 → 11, throw → nullptr)
- `AGENTS.md` (Sprint 11 ship + Sprint 6 final 标记)
- `docs/adr/adr-0019-*.md` (更新 §1.4 状态: engine.cpp 跨模块 ≤ 3 达成)

**API 稳定性**:
- `DSLEngine` 公共 API 零变化
- `TopoScheduler` 公共 API 零变化 (`DagState` 为私有内部结构)
- `NodeExecutor` / `MarkdownParser` / `ToolRegistry` 公共 API 零变化 (factory 函数为新增, 非替换)
- 新增 `IBudgetController` 接口, 但 `BudgetController` 继承实现, 兼容

**依赖变更**: 无新外部依赖

**测试影响**:
- baseline 33/33 → 目标 ≥ 47/47 (+14 测试: 7 scheduler + 5 parser + 3 factory - 1 plugin = 47; plugin 7 case 改名保留)
- TSan: factory 并发 / scheduler worker race / plugin 共享注册
- ASan: scheduler / parser / factory 全 pass, 0 leak

**风险域**:
- 🟠 `IBudgetController` 引入需评估现有 `BudgetController` 公开 API 完整覆盖 (可能漏 virtual 函数)
- 🟠 `ToolRegistry` factory 化需保证构造函数参数等价 (避免 silent behavior diff)
- 🟠 plugin E2E mock .so 涉及 dlsym 实际加载, 需 CI 兼容 (Linux only)

## Non-goals

- **不改** CognitiveWorker / DomainWorkerPool (Sprint 2/3 ship)
- **不改** PDK 公共 API (ADR-0021 T4b 治理锁定)
- **不引入** 新第三方依赖
- **不重做** Sprint 6 4 commits (保持现状)
- **不修改** Sprint 7 范围外的 refactor (如 CognitiveOrchestrator / LibraryLoader / TraceExporter)
- **不实质化** ADR-0007 / ADR-0031 / ADR-0033 (后续 Sprint)
- **不重新 base** ADR-0019 / ADR-0020 / ADR-0021 / ADR-0022 (仅追加状态标记)

## Estimated Effort

参考 `openspec/changes/archive/2026-06-23-sprint-7-tech-debt-followup/tasks.md` 11 sections 计划:

- 🔴 Blocker (fork 修复): 0.5 天
- 🔴 scheduler 7 测试 + parser 5 测试: 2 天
- 🟠 Major 主体 (scheduler 拆分 + DagState + factory + engine.cpp): 1.5 周
- 🔴 factory 3 测试 + plugin 改名 + E2E fixture: 1 周
- 🟡 Minor (访问一致 + spec 笔误 + ship gate 验证): 0.5 天
- 📦 归档 (openspec archive + AGENTS.md + PDK sync): 0.5 天

**总计**: ~3 周 (Sprint 11 主体)

**总提交数**: 14-17 commits (按 Day 分组)
