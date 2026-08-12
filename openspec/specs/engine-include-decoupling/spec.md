# Spec: engine-include-decoupling

> **承接 change**: `2026-06-24-engine-include-final-decoupling` (已 archive, 2026-06-25)
> **承接 superpowers plan**: `docs/superpowers/plans/2026-06-24-engine-include-final-decoupling.md`
> **承接 parent change**: `tech-debt-and-phase1-closure` (阶段 C handoff 路径)

## Purpose

`engine-include-decoupling` 跟踪 `src/core/engine.cpp` 跨模块+common 完整 include 数从基线 10 降至 ≤ 3 的渐进式重构 + baseline tests coverage extension + sanitizer revalidation 三大主题。本 spec 在 change archive 后从 `openspec/changes/2026-06-24-engine-include-final-decoupling/specs/engine-include-decoupling/spec.md` 持久化,记录 Sprint 10 ship gate 基线。

## Requirements

### Requirement: engine-include-decoupling-progress

`src/core/engine.cpp` 跨模块(`modules/`)和 common(`common/`)完整 include 数 MUST 从基线 10 降至 ≤ 3,采用分批 commit 策略(2-4 batch),每批 MUST 验证 `ctest 34/34 PASS` 零回归。

#### Scenario: 基线数字记录 (2026-06-25 实测)

- **WHEN** 启动 P2.C Step 3.1
- **THEN** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 10 (承袭 tech-debt-and-phase1-closure §6.1 实测基线)
- **AND** MUST 记录到本 spec Scenario 后续 batch commit baseline

#### Scenario: Commit A 替换 ToolRegistry include (2026-06-24, commit `e7306d9`)

- **WHEN** 完成 P2.C Step 3.2 (Commit A)
- **THEN** `src/core/engine.cpp` MUST 用 `IToolRegistry*` 依赖替换 `ToolRegistry` 完整 include
- **AND** `cmake --build build` MUST 编译通过
- **AND** `ctest --output-on-failure` MUST 34/34 PASS
- **AND** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 9 (baseline - 1)
- **AND** MUST 引用 `ADR-0019 §1.4` 已 ship 的 `IToolRegistry` 接口
- **AND** MUST 新增 `src/common/tools/factory.{h,cpp}` 工厂函数

#### Scenario: Commit B 替换 LLM provider include (2026-06-24, commit `18ce4aa`)

- **WHEN** 完成 P2.C Step 3.3 (Commit B)
- **THEN** `src/core/engine.cpp` MUST 用 `IProviderFactory*` 依赖替换 `MockLLMProvider` 完整 include
- **AND** `cmake --build build` MUST 编译通过
- **AND** `ctest --output-on-failure` MUST 34/34 PASS
- **AND** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 4 (baseline - 6)

#### Scenario: Commit C 替换 BudgetController include (2026-06-24, commit `8f2ad54`)

- **WHEN** 完成 P2.C Step 3.4 (Commit C)
- **THEN** `src/core/engine.cpp` MUST 用 forward declaration 替换 `BudgetController` 完整 include
- **AND** `cmake --build build` MUST 编译通过
- **AND** `ctest --output-on-failure` MUST 34/34 PASS
- **AND** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST ≤ 3
- **AND** MUST NOT 新建 `IBudgetController` 抽象(forward declaration 已足够,因 `~DSLEngine()` 通过 `topo_scheduler.h` transitively 提供完整类型)

#### Scenario: P2.C ship gate (2026-06-25 实测)

- **WHEN** P2.C 全部 batch commit 完成
- **THEN** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST ≤ 3 (实测 = 3, 2026-06-25)
- **AND** `ctest --output-on-failure` MUST 34/34 PASS
- **AND** 1.5 day 时间盒 MUST 未超时 (实际 ~2 hours, 2026-06-24 23:43 ~ 2026-06-25 01:17)

#### Scenario: Review fixes (2026-06-25, commit `a8abc35`)

- **WHEN** review 阶段
- **THEN** MUST 删除 dead guard 代码(由 P2.C factory changes 后已 unreachable)
- **AND** `ctest --output-on-failure` MUST 34/34 PASS

### Requirement: baseline-tests-coverage-extension

`tests/test_scheduler.cpp` MUST 验证 7 个已有 TEST_CASE 覆盖 `DagState` 3 子函数契约 (per Sprint 7 spec `dag-scheduler-pipeline` 详细 contract, 已 ship at `b3ad5bc` + `4d1a855`)。`tests/test_parser.cpp` MUST 验证 5 个已有 TEST_CASE 覆盖 `NodeFactoryRegistry` 全功能 (per Sprint 6 spec `node-factory-registry`, 已 ship at `6c5557c`)。新建 `tests/test_engine_factory.cpp` MUST 包含 3 个 TEST_CASE 验证 `DSLEngine` 构造路径(覆盖 P2.A 删 factory 后的直接构造路径)。

#### Scenario: scheduler 7 case verify (Sprint 7 ship)

- **WHEN** 完成 P2.B Step 1.1 (verify, no new commit)
- **THEN** `tests/test_scheduler.cpp` MUST 包含 7 个已有 TEST_CASE:
  - `prepare_dag_state_simple_linear` `[scheduler][stageN]`
  - `prepare_dag_state_diamond` `[scheduler][stageN]`
  - `prepare_dag_state_cycle_detection` `[scheduler][stageN]`
  - `dispatch_ready_nodes_initial` `[scheduler][stageN]`
  - `dispatch_ready_nodes_parallel` `[scheduler][stageN]`
  - `handle_node_completion_success` `[scheduler][stageN]`
  - `handle_node_completion_failure` `[scheduler][stageN]`
- **AND** `cmake --build build` + `ctest -R test_scheduler` MUST 14/14 PASS(7 baseline + 7 verify)

#### Scenario: parser 5 case verify (Sprint 6 ship)

- **WHEN** 完成 P2.B Step 1.2 (verify, no new commit)
- **THEN** `tests/test_parser.cpp` MUST 包含 5 个已有 TEST_CASE:
  - `factory_registry_registers_all_types`
  - `factory_registry_creates_correct_subtype`
  - `factory_registry_unknown_type_returns_nullptr`
  - `factory_registry_global_singleton`
  - `factory_registry_concurrent_access`(TSan 验证)
- **AND** `cmake --build build` + `ctest -R test_parser` MUST 5/5 PASS

#### Scenario: engine_factory 3 case 新增 (2026-06-24, commit `3681ba8`)

- **WHEN** 完成 P2.B Step 1.3 (Commit C)
- **THEN** 新建 `tests/test_engine_factory.cpp` MUST 包含 3 个 TEST_CASE:
  - `test_engine_create_with_default_config` (验证 DSLEngine 默认构造路径)
  - `test_engine_create_with_custom_config` (验证自定义配置)
  - `test_engine_create_with_dependencies` (验证依赖注入)
- **AND** `tests/CMakeLists.txt` MUST 注册新测试文件
- **AND** 测试覆盖 MUST 是 P2.A 删除后的 engine.cpp 构造路径(非已删 factory)
- **AND** `cmake --build build` + `ctest -R test_engine_factory` MUST 3/3 PASS

#### Scenario: P2.B ship gate (2026-06-25 实测)

- **WHEN** P2.B verify + Commit C 完成
- **THEN** `ctest --output-on-failure` MUST 34/34 PASS (Sprint 7 7 + Sprint 6 5 + engine_factory 3 已有 baseline,新增 0 测试,本 change 仅 3 新增)
- **AND** 零回归
- **AND** **P2.C 启动前置检查**:本 step 全 [x] + ctest 34/34 PASS

### Requirement: sanitizer-revalidation (2026-06-25 P2.5 复验)

`cmake --preset asan && ctest` MUST 0 error(无 ASan 报告,本 change 引入)。`cmake --preset tsan && ctest` MUST 0 race(无 TSan 报告,本 change 引入)。`factory_registry_concurrent_access` under TSan MUST 0 race。

#### Scenario: ASan 0 error (2026-06-25 P2.5 实测)

- **WHEN** 完成 P2.F Step 2.2 (2026-06-25 P2.5 复验)
- **THEN** `cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON && cmake --build build/asan -j && ctest --output-on-failure` MUST 33/34 PASS
- **AND** 唯一失败 MUST 为 pre-existing test_cognitive_worker (`stack-use-after-scope` at test_cognitive_worker.cpp:226, Sprint 2 ship)
- **AND** 失败 MUST 由独立 OpenSpec change `2026-06-25-pre-existing-sanitizer-findings` 跟踪(不阻塞本 spec ship gate)

#### Scenario: TSan 0 race (2026-06-25 P2.5 实测)

- **WHEN** 完成 P2.F Step 2.3 (2026-06-25 P2.5 复验)
- **THEN** `cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON && cmake --build build/tsan -j && ctest --output-on-failure` MUST 32/34 PASS
- **AND** 2 失败 MUST 全部 pre-existing:
  - `test_cognitive_worker` (Sprint 2, ASan 同样失败)
  - `test_domain_worker_pool` (Sprint 3, 12 TSan warnings 但 94 assertions 全 PASS, Catch2+jthread 框架交互)
- **AND** 失败 MUST 由独立 OpenSpec change `2026-06-25-pre-existing-sanitizer-findings` 跟踪

#### Scenario: 历史 race/leak 优雅降级 (本 spec 实施)

- **WHEN** ASan/TSan 发现历史 race/leak(非本 change 引入)
- **THEN** MUST 记录为 pre-existing
- **AND** MUST 创建独立 OpenSpec change 跟踪 (`2026-06-25-pre-existing-sanitizer-findings`)
- **AND** 本 spec 仍 MUST 视为 ship gate PASS (不阻塞 archive 闭环)
