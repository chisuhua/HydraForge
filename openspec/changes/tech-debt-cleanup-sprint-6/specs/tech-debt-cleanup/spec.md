<!-- STATUS NOTE (2026-06-22 Oracle Code Review 决议)
本 spec 部分验收项 Sprint 6 ship 时未达, 详细偏离项见 `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` §6.1 与 §6.3。

**Sprint 6 实际偏离 (Oracle ses_112a9f9c5ffesqpYeefOBgMkjH 实测)**:
- `src/core/engine.cpp` 跨模块/common include 实测 **10 个** (`grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp` = 10), spec 要求 ≤ 3 — 🔴 严重偏离 (commit `7cc4239` 自述 "2/10 替换" 但仅 swap 头未减计数)
- `agenticdsl::scheduler::create()` 漏 `Config`/`initial_budget` 参数 → `engine.cpp:188` 仍直接 `TopoScheduler` 构造 → factory **零调用点 (死代码)** — 🟠 Major
- budget factory 返回具体类 `unique_ptr<BudgetController>` 而非 `IBudgetController` 抽象, engine.cpp 仍依赖完整型 (传递 include 维持, 脆弱) — 🟡 (RISK-7 已知推迟到 Sprint 7 + design Open Question 1)
- factory 测试 ≥ 3 个要求 → **0 个交付** — 🔴 零交付
- plugin 测试 7 case 名称/范围不符 spec (`load_valid_plugin` / `abi_mismatch_strict` / `dlsym_missing_register_fn` / `unload_all_raii_verification` 等全部推迟; `TEST_PLUGIN_FIXTURE_PATH` 宏 CMake 从未注入, E2E 编译排除, **Loaded 状态零覆盖**) — 🟠 Major
- AGENTS.md 顶部状态日志追加 Sprint 6 → **未追加** (Oracle 实测 0 命中) — 🟡 文档欠
- ship gate `47/47 ctest` → 实际 `33/33` (差 14 个) — 🔴 ship gate 未达
- TSan / ASan / hub out_degree / adr_lint / docs_drift_audit / openspec validate 全部 ship gate 未跑 — 🟡 未验证

**Sprint 6 行为保持**: `LLMProviderFactory` 工厂化 (`common/llm/factory.h` 薄头) PIMPL-lite 价值真实; `BudgetController` 工厂化 (头 swap) 减少 engine.cpp 头依赖; CMake wiring 正确 (3 factory.cpp + node_factory.cpp 全部注册); 33/33 ctest pass。

**Sprint 7 follow-up**: 全部 6.3.x 项推到 OpenSpec change `2026-07-22-sprint-7-tech-debt-followup`。本 change 不 archive。
-->

## ADDED Requirements

### Requirement: engine-cpp-factory-pattern

`src/core/engine.cpp` MUST 通过各模块工厂函数构造依赖,消除 10 个跨模块完整类型 include:

- `src/modules/scheduler/factory.h` 新增 `agenticdsl::scheduler::create(const SchedulerConfig&) -> std::unique_ptr<IScheduler>`
- `src/modules/budget/factory.h` 新增 `agenticdsl::budget::create_controller(const BudgetConfig&) -> std::unique_ptr<BudgetController>`
- `src/common/llm/factory.h` 新增 `agenticdsl::llm::create_provider_factory(const LLMConfig&) -> std::unique_ptr<IProviderFactory>`

engine.cpp 跨模块 include 数 MUST 从 10 降至 ≤ 3（仅 types + 2 contract 头）。

#### Scenario: engine.cpp 跨模块 include 计数

- **WHEN** `grep -c '^#include' src/core/engine.cpp | grep "modules/\|common/"`
- **THEN** MUST ≤ 3（保留必要的 types 与 contract 抽象）

#### Scenario: 3 个工厂函数存在

- **WHEN** `grep -rn "namespace.*scheduler.*create\|namespace.*budget.*create_controller\|namespace.*llm.*create_provider_factory" src/`
- **THEN** MUST 命中 3 个工厂函数定义

#### Scenario: 工厂函数单测

- **WHEN** `cd build && ctest -R "test_factory|test_engine_factory" --output-on-failure`
- **THEN** MUST 至少 3 个新 TEST_CASE 通过（每个工厂 1 个）

### Requirement: adr-status-synchronization

3 个 ADR 文档状态 MUST 同步至代码实际状态:

- `docs/adr/adr-0020-thread-model-isolation.md`: 顶部状态字段 → `✅ Resolved (2026-06-19)` + §2.2.1/§3.2 标 `Resolved`
- `docs/adr/adr-0021-pdk-design.md`: 顶部状态字段 → `🟡 Partial (2026-06-19 Sprint 4 ship)` + §3 实施状态更新
- `docs/adr/adr-0022-plugin-loading.md`: 顶部状态字段 → `🟡 Partial (Sprint 5 in-flight)` + §1.2 标 `In Implementation`

`docs/README.md` ADR 表格对应 3 行 MUST 同步更新。

#### Scenario: ADR-0020 顶部状态

- **WHEN** `head -10 docs/adr/adr-0020-thread-model-isolation.md`
- **THEN** MUST 含 `**状态**: ✅ Resolved` 或等效标识
- **AND** MUST 引用本 OpenSpec change (`tech-debt-cleanup-sprint-6`)

#### Scenario: ADR-0021 顶部状态

- **WHEN** `head -10 docs/adr/adr-0021-pdk-design.md`
- **THEN** MUST 含 `**状态**: 🟡 Partial`
- **AND** MUST 含 "Sprint 4 ship" 时间戳

#### Scenario: ADR-0022 顶部状态

- **WHEN** `head -10 docs/adr/adr-0022-plugin-loading.md`
- **THEN** MUST 含 `**状态**: 🟡 Partial`
- **AND** MUST 含 "Sprint 5 in-flight" 标记

#### Scenario: docs/README.md 表格同步

- **WHEN** `grep "adr-0020\|adr-0021\|adr-0022" docs/README.md`
- **THEN** 3 行 MUST 状态字段与对应 ADR 文件一致
- **AND** MUST 链接至本 OpenSpec change

### Requirement: plugin-loader-wip-commit

`openspec/changes/2026-07-14-plugin-loader/` 当前 7 个 untracked 文件 MUST 提交为 WIP commit:

- `include/agenticdsl/plugin/plugin_info.h` (untracked)
- `include/agenticdsl/plugin/plugin_loader.h` (untracked)
- `src/modules/plugin/plugin_loader.cpp` (untracked)
- `src/modules/plugin/CMakeLists.txt` (untracked)
- `tests/test_plugin_loader.cpp` (untracked)
- `openspec/changes/2026-07-14-plugin-loader/design.md` (untracked)
- `openspec/changes/2026-07-14-plugin-loader/specs/plugin-loader/spec.md` (untracked)

`openspec/changes/2026-07-14-plugin-loader/tasks.md` MUST 更新,14/59 子任务标记为 `[x]`。

#### Scenario: 工作树 clean

- **WHEN** `git status`
- **THEN** MUST NOT 含上述 7 个 untracked 文件
- **AND** MUST NOT 含 modified 状态（除本 change 相关）

#### Scenario: WIP commit 存在

- **WHEN** `git log --oneline -3`
- **THEN** 最新 commit MUST 含 `wip(plugin):` 前缀
- **AND** MUST 引用 Sprint 5 子任务编号（S5.T1 / S5.T2）

#### Scenario: tasks.md 进度更新

- **WHEN** `grep -c "^\s*-\s*\[x\]" openspec/changes/2026-07-14-plugin-loader/tasks.md`
- **THEN** MUST ≥ 14
- **AND** `grep -c "^\s*-\s*\[ \]" openspec/changes/2026-07-14-plugin-loader/tasks.md` MUST ≤ 45

### Requirement: yaml-json-debug-removed-cleanup

`src/common/utils/yaml_json.cpp` line 72 与 line 75 的 2 处 `[DEBUG-removed]` 注释 MUST 删除,闭合 2026-06-09 审计 P1-2 残留。

#### Scenario: 死注释清除

- **WHEN** `grep "DEBUG-removed" src/common/utils/yaml_json.cpp`
- **THEN** MUST 返回 0 命中

#### Scenario: ctest 零回归

- **WHEN** `cd build && cmake --build . && ctest --output-on-failure`
- **THEN** MUST ≥ 32/32 PASS (与 baseline 一致)

### Requirement: plugin-loader-test-coverage

`tests/test_plugin_loader.cpp` MUST 新增 ≥ 7 个 Catch2 test case:

- `load_valid_plugin`: 加载有效 .so plugin
- `abi_version_mismatch_strict`: strict 模式下 ABI 不匹配返回 false
- `abi_version_mismatch_non_strict`: non-strict 模式下仅警告
- `dlsym_missing_register_fn`: `pdk_register_tools` 符号缺失
- `dlopen_failure_invalid_path`: 无效路径返回 false
- `unload_all_raii_verification`: 析构时自动 dlclose
- `load_all_search_paths`: 扫描多个搜索路径

#### Scenario: 7 个新 test case 通过

- **WHEN** `cd build && ctest -R test_plugin_loader --output-on-failure`
- **THEN** MUST ≥ 7 个 TEST_CASE 通过
- **AND** 既有 plugin_loader test MUST 零回归