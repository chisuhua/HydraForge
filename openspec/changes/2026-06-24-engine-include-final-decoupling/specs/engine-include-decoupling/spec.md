# Spec: engine-include-decoupling

> **关联 proposal**: `2026-06-24-engine-include-final-decoupling/proposal.md`
> **关联 tasks**: `2026-06-24-engine-include-final-decoupling/tasks.md`
> **承接 change**: `tech-debt-and-phase1-closure` (阶段 C handoff 路径)
> **关联 superpowers plan**: `docs/superpowers/plans/2026-06-24-tech-debt-full-closure.md` §3.5.6

## ADDED Requirements

### Requirement: engine-include-decoupling-progress

`src/core/engine.cpp` 跨模块(`modules/`)和 common(`common/`)完整 include 数 MUST 从基线 10 降至 ≤ 3,采用分批 commit 策略(2-4 batch),每批 MUST 验证 `ctest ~49/49 PASS` 零回归。

#### Scenario: 基线数字记录

- **WHEN** 启动 P2.C Step 3.1
- **THEN** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 10 (承袭 tech-debt-and-phase1-closure §6.1 实测基线)
- **AND** MUST 记录到本 change tasks.md §3.1.1

#### Scenario: Commit A 替换 ToolRegistry include

- **WHEN** 完成 P2.C Step 3.2 (Commit A)
- **THEN** `src/core/engine.cpp` MUST 用 `IToolRegistry*` 依赖替换 `ToolRegistry` 完整 include
- **AND** `cmake --build build` MUST 编译通过
- **AND** `ctest --output-on-failure` MUST **34/34 PASS**
- **AND** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 9
- **AND** MUST 引用 `ADR-0019 §1.4` 已 ship 的 `IToolRegistry` 接口

#### Scenario: Commit B 替换 MockLLMProvider include

- **WHEN** 完成 P2.C Step 3.3 (Commit B)
- **THEN** `src/core/engine.cpp` MUST 用 `IProviderFactory*` 依赖替换 `MockLLMProvider` 完整 include
- **AND** `cmake --build build` MUST 编译通过
- **AND** `ctest --output-on-failure` MUST ~49/49 PASS
- **AND** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 4

#### Scenario: Commit C 替换 BudgetController include

- **WHEN** 完成 P2.C Step 3.4 (Commit C)
- **THEN** `src/core/engine.cpp` MUST 用 `IBudgetController*` 依赖替换 `BudgetController` 完整 include
- **AND** 若 BudgetController 是 struct(POD-style)且 engine.cpp 仍依赖完整型 → MUST 新建 `include/agenticdsl/contract/ibudget_controller.h` 纯虚接口
- **AND** `cmake --build build` MUST 编译通过
- **AND** `ctest --output-on-failure` MUST ~49/49 PASS
- **AND** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST ≤ 3

#### Scenario: P2.C ship gate

- **WHEN** P2.C 全部 batch commit 完成
- **THEN** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST ≤ 3
- **AND** `ctest --output-on-failure` MUST ~49/49 PASS
- **AND** `mcp__code-review-graph__get_hub_nodes --top_n 5` MUST 验证 `topo_scheduler::execute` out_degree < 30
- **AND** 1.5 day 时间盒 MUST 未超时

#### Scenario: P2.C 二次 handoff 变体

- **WHEN** 1.5 day 时间盒超时仍未达 ≤ 3
- **THEN** MUST 创建新 OpenSpec change `2026-07-xx-engine-include-final-decoupling-v2`
- **AND** MUST 引用本 change + 继续 P2.C 未完成部分
- **AND** `openspec validate 2026-07-xx-engine-include-final-decoupling-v2` exit 0
- **AND** MUST 更新本 tasks.md §3.5 状态为 `⏳ Handoff to 2026-07-xx-engine-include-final-decoupling-v2`

### Requirement: baseline-tests-coverage-extension

`tests/test_scheduler.cpp` MUST 新增 7 个 TEST_CASE 覆盖 `DagState` 3 子函数契约。`tests/test_parser.cpp` MUST 新增 5 个 TEST_CASE 覆盖 `NodeFactoryRegistry` 全功能。新建 `tests/test_engine_factory.cpp` MUST 包含 3 个 TEST_CASE 验证 `DSLEngine` 构造路径(覆盖 P2.A 删 factory 后的直接构造路径)。

#### Scenario: scheduler 7 case 通过

- **WHEN** 完成 P2.B Step 1.1 (Commit A)
- **THEN** `tests/test_scheduler.cpp` MUST 新增 7 个 TEST_CASE:
  - `prepare_dag_state_simple_linear` `[scheduler][stageN]`
  - `prepare_dag_state_diamond` `[scheduler][stageN]`
  - `prepare_dag_state_cycle_detection` `[scheduler][stageN]`
  - `dispatch_ready_nodes_initial` `[scheduler][stageN]`
  - `dispatch_ready_nodes_parallel` `[scheduler][stageN]`
  - `handle_node_completion_success` `[scheduler][stageN]`
  - `handle_node_completion_failure` `[scheduler][stageN]`
- **AND** `cmake --build build` + `ctest -R test_scheduler` MUST 14/14 PASS(7 baseline + 7 新)

#### Scenario: parser 5 case 通过

- **WHEN** 完成 P2.B Step 1.2 (Commit B)
- **THEN** `tests/test_parser.cpp` MUST 新增 5 个 TEST_CASE:
  - `factory_registry_registers_all_types`
  - `factory_registry_creates_correct_subtype`
  - `factory_registry_unknown_type_returns_nullptr`
  - `factory_registry_global_singleton`
  - `factory_registry_concurrent_access`(TSan 验证)
- **AND** `cmake --build build` + `ctest -R test_parser` MUST 5/5 PASS
- **AND** `cmake --preset tsan && ctest -R factory_registry_concurrent_access` MUST 0 race

#### Scenario: engine_factory 3 case 通过

- **WHEN** 完成 P2.B Step 1.3 (Commit C)
- **THEN** 新建 `tests/test_engine_factory.cpp` MUST 包含 3 个 TEST_CASE:
  - `test_engine_create_with_default_config` (验证 DSLEngine 默认构造路径)
  - `test_engine_create_with_custom_config` (验证自定义配置)
  - `test_engine_create_with_dependencies` (验证依赖注入)
- **AND** `tests/CMakeLists.txt` MUST 注册新测试文件
- **AND** 测试覆盖 MUST 是 P2.A 删除后的 engine.cpp 构造路径(非已删 factory)
- **AND** `cmake --build build` + `ctest -R test_engine_factory` MUST 3/3 PASS

#### Scenario: P2.B ship gate

- **WHEN** P2.B 全部 3 commit 完成
- **THEN** `ctest --output-on-failure` MUST ~49/49 PASS(34 baseline + 7 scheduler + 5 parser + 3 engine_factory)
- **AND** 零回归
- **AND** **P2.C 启动前置检查**:本 step 全 [x] + ctest 49/49 PASS

### Requirement: sanitizer-revalidation

`cmake --preset asan && ctest` MUST 0 error(无 ASan 报告)。`cmake --preset tsan && ctest` MUST 0 race(无 TSan 报告,自本 change 引入)。`factory_registry_concurrent_access` under TSan MUST 0 race。

#### Scenario: ASan 0 error

- **WHEN** 完成 P2.F Step 2.2
- **THEN** `cmake --preset asan && ctest --output-on-failure` MUST 0 error
- **AND** 输出 MUST 无 memory leak / use-after-free / buffer overflow 报告

#### Scenario: TSan 0 race

- **WHEN** 完成 P2.F Step 2.3
- **THEN** `cmake --preset tsan && ctest --output-on-failure` MUST 0 race
- **AND** `factory_registry_concurrent_access` under TSan MUST 0 race(无 data race 报告)
- **AND** 输出 MUST 无 thread leak / data race 报告

#### Scenario: 历史 race/leak 优雅降级

- **WHEN** ASan/TSan 发现历史 race/leak(非本 change 引入)
- **THEN** MUST 记录为 pre-existing
- **AND** MUST 创建独立 OpenSpec change 跟踪
- **AND** 本 change 仍 MUST archive(不阻塞 ship gate)
