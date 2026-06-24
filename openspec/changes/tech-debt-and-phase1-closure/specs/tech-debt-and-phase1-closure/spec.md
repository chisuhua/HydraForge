# Spec: tech-debt-and-phase1-closure

> **关联 proposal**: `2026-06-24-tech-debt-and-phase1-closure/proposal.md`
> **关联 design**: `2026-06-24-tech-debt-and-phase1-closure/design.md`
> **关联 tasks**: `2026-06-24-tech-debt-and-phase1-closure/tasks.md`
> **关联 superpowers plan**: `docs/superpowers/plans/2026-06-24-tech-debt-full-closure.md`

## ADDED Requirements

### Requirement: workspace-clean-state

工作区 MUST 处于干净状态:`git status` MUST NOT 显示任何 `D`(deleted)或 `??`(untracked)项(除 `.git/` 内部文件与项目忽略的临时文件)。

#### Scenario: P0.A 删除归档后 git status 干净

- **WHEN** 完成 Step 1(P0.A 仅 commit 9 个 D 文件,不含 docs/superpowers)
- **THEN** `git status` MUST 仅显示 `docs/superpowers/` 1 个 untracked(由 Step 3 处理)
- **AND** `git log --oneline -1` MUST 显示 P0.A commit hash

#### Scenario: Step 3 后 git status 完全干净

- **WHEN** 完成 Step 3(P3.A `git mv` docs/superpowers 至 docs/archive/superpowers + edit docs/README.md)
- **THEN** `git status` MUST 完全干净(zero untracked + zero modified + zero deleted)
- **AND** `git log --oneline -1` MUST 显示 P3.A commit hash
- **AND** `docs/README.md` MUST 不再声明 superpowers 已归档与实际状态矛盾

### Requirement: sprint9-backing-change

Sprint 9 step 1 的 3 个 ship commit(`40008a5` `ce4358b` `bd936af`)MUST 有对应的 backing OpenSpec change `2026-06-24-sprint-9-handle-node-completion`,含 proposal.md + tasks.md 引用 3 commit hash。

#### Scenario: Sprint 9 change 存在

- **WHEN** 完成 Step 2
- **THEN** `ls openspec/changes/2026-06-24-sprint-9-handle-node-completion/` MUST 显示 `proposal.md` + `tasks.md`
- **AND** tasks.md MUST 引用 `40008a5` / `ce4358b` / `bd936af` 三个 commit hash
- **AND** `openspec validate 2026-06-24-sprint-9-handle-node-completion` exit 0

### Requirement: phase1-five-adr-approved

5 个 Phase 1 ADR(0019/0020/0021/0022/0023)MUST 状态字段改 ✅ Approved。`docs/roadmap-status.md` MUST 显示 Phase 1 智能体层 100%。`AGENTS.md` Recent Changes MUST 追加 Sprint 5 收官条目。

#### Scenario: 5 ADR 状态字段 Approved

- **WHEN** 完成 Step 5(P1.B)
- **THEN** `head -10 docs/adr/adr-0019-*.md` MUST 显示 `**状态**: ✅ Approved` 或等效标识
- **AND** `head -10 docs/adr/adr-0020-*.md` / `adr-0021-*.md` / `adr-0022-*.md` / `adr-0023-*.md` 同样 MUST ✅ Approved
- **AND** 5 个 ADR MUST 引用本 OpenSpec change 作为变更依据

#### Scenario: docs/roadmap-status.md 100%

- **WHEN** 完成 Step 5
- **THEN** `grep "Phase 1" docs/roadmap-status.md` MUST 显示 100%
- **AND** 进度数字 MUST 与 Sprint 5 S5.T4 验收清单一致

#### Scenario: AGENTS.md Recent Changes 追加

- **WHEN** 完成 Step 5
- **THEN** `grep "Sprint 5" AGENTS.md` MUST 至少 1 命中
- **AND** 5 ADR ✅ Approved 标记 MUST 出现

### Requirement: phase1-plugin-demo-3-modes

`examples/phase1_plugin_demo/main.cpp` MUST 支持 3 种模式:`--mock`(Sprint 0 fallback)/ `--load-plugin=<path>`(单插件)/ `--plugin-path=<dir>`(扫描目录)。3 模式 MUST 实跑通过。

#### Scenario: --mock 模式回退

- **WHEN** 运行 `./phase1_plugin_demo --mock`
- **THEN** MUST 输出 mock 工具调用结果
- **AND** exit code MUST 0

#### Scenario: --load-plugin 单插件加载

- **WHEN** 编译并运行 `./phase1_plugin_demo --load-plugin=./plugins/test_plugin.so`
- **THEN** MUST 通过 `PluginLoader::load_so` 加载该 .so
- **AND** MUST 调用 .so 中 `pdk_register_tools` 注册的工具
- **AND** exit code MUST 0

#### Scenario: --plugin-path 扫描目录加载

- **WHEN** 设置 `HYDRAFORGE_PLUGIN_PATH=./plugins/` 后运行 `./phase1_plugin_demo --plugin-path=./plugins/`
- **THEN** MUST 扫描目录中所有 .so 并逐一加载
- **AND** 加载成功的 .so MUST 出现在 `PluginLoader::list_loaded()` 输出
- **AND** exit code MUST 0

#### Scenario: 互斥与 fallback

- **WHEN** 同时指定 `--mock` 与 `--load-plugin`
- **THEN** MUST 输出 error 并 exit non-zero
- **AND** MUST NOT 加载任何 .so

### Requirement: phase1-sync-pdk-executed

`./scripts/sync-pdk.sh` MUST 在 Sprint 5 ship 后执行成功。standalone `hydraforge-pdk` 仓库 MUST 收到 sync commit 并能 `cmake` + build 通过。

#### Scenario: sync-pdk.sh 成功执行

- **WHEN** 完成 Step 6(P1.C)
- **THEN** `./scripts/sync-pdk.sh` exit code MUST 0
- **AND** standalone `github.com/chisuhua/hydraforge-pdk` repo MUST 收到新 commit
- **AND** standalone repo `cmake -B build && cmake --build build` MUST 成功

#### Scenario: 外部阻塞时优雅降级

- **WHEN** standalone repo push 失败(GitHub 组织不存在/网络阻塞)
- **THEN** sync-pdk.sh MUST 记录 STATUS NOTE
- **AND** Step 7 archive MUST 仍可进行(不强依赖 external push 成功)

### Requirement: tech-debt-6-3-2-scheduler-factory-removed

`src/modules/scheduler/factory.{h,cpp}` MUST 被删除(零调用 = 死代码,删除非补 Config 参数)。`src/core/engine.cpp` MUST 改直接构造路径,不再调用已删除 factory。CMake 注册 MUST 同步移除。

#### Scenario: 二次确认 factory 零调用

- **WHEN** Step 8 启动前
- **THEN** `grep -rn "namespace.*scheduler::create\|scheduler::factory" src/ include/` MUST 0 命中
- **AND** 承重假设验证通过(零调用确认)

#### Scenario: factory 文件已删除

- **WHEN** 完成 Step 8(P2.A)
- **THEN** `ls src/modules/scheduler/factory.{h,cpp}` MUST "No such file or directory"
- **AND** `git log --oneline -1 -- src/modules/scheduler/factory.cpp` MUST 显示删除 commit
- **AND** `cmake --build build` MUST 成功
- **AND** `ctest` MUST 仍 34/34 PASS

#### Scenario: engine.cpp 不再调用 factory

- **WHEN** 完成 Step 8
- **THEN** `grep "factory" src/core/engine.cpp` MUST 0 命中
- **AND** engine.cpp 跨模块 include MUST 顺势 -1(为 P2.C 铺路)

### Requirement: tech-debt-6-3-6-pending-dynamic-deps-accessor-regression

`topo_scheduler.cpp` 中所有 `pending_dynamic_deps_` 访问 MUST 通过 `session_.get_pending_dynamic_deps()` 访问器,不再直接 friend 访问 `session_.pending_dynamic_deps_`。本 MUST 在 ship gate 阶段回归验证(实施已在 Sprint 7 ship,本 change 无新工作)。

> **STATUS (2026-06-24, Oracle 审查 ses_108c2a3b0ffe012zA30ujXdHOP)**: 实施工作已于 **Sprint 7 Day 8 ship** (commit `75ded94 refactor(scheduler): use get_pending_dynamic_deps() accessor (Day 8 step 1)`)。`execution_session.h:70` getter 已存在,`topo_scheduler.cpp` L172/L515/L518 已使用访问器。本 change 仅做回归验证。

#### Scenario: 代码中访问器一致 (回归验证)

- **WHEN** 跑本 change 全路径 ship gate 验证
- **THEN** `grep "session_\.pending_dynamic_deps_" src/ --include='*.cpp' --include='*.h'` MUST 0 命中
  **(grep scope 限定 .cpp/.h 文件,避免 `src/modules/exports/req1.md` markdown 文档误命中)**
- **AND** `grep "session_.get_pending_dynamic_deps()" src/modules/scheduler/topo_scheduler.cpp` MUST ≥ 1 命中
- **AND** `cmake --build build` MUST 成功
- **AND** `ctest` MUST 34/34 PASS(零回归)

### Requirement: tech-debt-6-3-4-fifteen-tests

`tests/test_scheduler.cpp` MUST 新增 7 case,`tests/test_parser.cpp` MUST 新增 5 case,新建 `tests/test_engine_factory.cpp` MUST 新增 3 case(覆盖 P2.A 删除后的 engine.cpp 构造路径,而非已删 factory)。`ctest` MUST 升至 ~49/49 全 PASS。

#### Scenario: 7 scheduler test 新增

- **WHEN** 完成 Step 10(P2.B commit A)
- **THEN** `tests/test_scheduler.cpp` MUST 新增 7 个 `TEST_CASE`(per Sprint 7 spec `scheduler-pipeline-tightened` Requirement:prepare_dag_state_simple_linear / diamond / cycle_detection + dispatch_ready_nodes_initial / parallel + handle_node_completion_success / failure)
- **AND** `ctest -R test_scheduler` MUST 14/14 PASS(7 baseline + 7 新)

#### Scenario: 5 parser test 新增

- **WHEN** 完成 Step 10 commit B
- **THEN** `tests/test_parser.cpp` MUST 新增 5 个 `TEST_CASE`(per Sprint 6 spec `node-factory-registry` Requirement:factory_registry_registers_all_types / creates_correct_subtype / unknown_type_returns_nullptr / global_singleton / concurrent_access)
- **AND** `ctest -R test_parser` MUST 5/5 PASS(全 5 新)

#### Scenario: 3 engine_factory test 新建

- **WHEN** 完成 Step 10 commit C
- **THEN** `tests/test_engine_factory.cpp` MUST 存在且 3 个 `TEST_CASE`(test_engine_create_with_default_config / test_engine_create_with_custom_config / test_engine_create_with_dependencies)
- **AND** `ctest -R test_engine_factory` MUST 3/3 PASS
- **AND** 测试 MUST 覆盖 P2.A 删除 factory 后的 engine.cpp 直接构造路径(非已删 factory)

#### Scenario: 全量 ctest 49/49

- **WHEN** 完成 Step 10 全部 3 commit
- **THEN** `ctest --output-on-failure` MUST ~49/49 PASS
- **AND** 零回归(34 baseline + 15 新 case = 49)

#### Scenario: TSan 验证并发测试

- **WHEN** `cmake --preset tsan && ctest -R "concurrent_access|engine_factory" --output-on-failure`
- **THEN** MUST 0 race report
- **AND** `factory_registry_concurrent_access` 测试 MUST pass under TSan

### Requirement: tech-debt-6-3-5-engine-includes-decremented

`src/core/engine.cpp` 跨模块/common include MUST 从 10 降至 ≤ 3。**分批提交**(2-4 commit),每批 `ctest` 验证。利用已存在的 `IProviderFactory` + `IToolRegistry` 接口(per ADR-0019 §1.4 ✅)。若需 `IBudgetController` 抽象则一并补。

#### Scenario: 验证前置(选项 D 验证)

- **WHEN** Step 12 启动前(承重假设二次确认)
- **THEN** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出当前基线数字(预计 10,作为 P2.C 起点)
- **AND** 基线数字 MUST 记录在 tasks.md Step 12 子任务

#### Scenario: P2.C Commit A(2 个 include 去除)

- **WHEN** 完成 P2.C Commit A(替换 ToolRegistry include)
- **THEN** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 baseline-2
- **AND** `ctest --output-on-failure` MUST ~49/49 PASS
- **AND** commit message MUST 包含 reduce 数字(如 "10→8")

#### Scenario: P2.C Commit B(3 个 include 去除)

- **WHEN** 完成 P2.C Commit B(替换 MockLLMProvider include)
- **THEN** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 baseline-5
- **AND** `ctest --output-on-failure` MUST ~49/49 PASS
- **AND** commit message MUST 包含 reduce 数字(如 "8→5")

#### Scenario: P2.C Commit C(2-3 个 include 去除 + IBudgetController 抽象若需)

- **WHEN** 完成 P2.C Commit C(替换 BudgetController include + 引入 IBudgetController 抽象若需)
- **THEN** `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST ≤ 3
- **AND** `ctest --output-on-failure` MUST ~49/49 PASS
- **AND** 若引入 IBudgetController:`include/agenticdsl/contract/ibudget_controller.h` MUST 存在

#### Scenario: 1.5 day 时间盒超时 → handoff 变体

- **WHEN** P2.C 步骤实施超过 1.5 day 仍未达 ≤ 3
- **THEN** MUST 触发 handoff 变体:创建新 OpenSpec change `2026-07-xx-engine-include-final-decoupling`,含完整 proposal + tasks,把 6.3.5 正式移交
- **AND** 本 change 仍 archive `tech-debt-cleanup-sprint-6`(非 ship-as-is 留账)

### Requirement: tech-debt-p2-f-tsan-asan-reverified

Sprint 6 STATUS NOTE 5.2/5.3 ship gate 自 Sprint 6 ship 后从未重跑,本 change MUST 在 Step 8/9/10/12 代码变更后复验 `cmake --preset asan` + `cmake --preset tsan` 全矩阵 0 error。

#### Scenario: ASan 0 error

- **WHEN** 完成 Step 11
- **THEN** `cmake --preset asan && ctest --output-on-failure` MUST 0 error
- **AND** ASan 报告 MUST 无 leak

#### Scenario: TSan 0 race

- **WHEN** 完成 Step 11
- **THEN** `cmake --preset tsan && ctest --output-on-failure` MUST 0 race report
- **AND** `factory_registry_concurrent_access` 测试 MUST pass under TSan

#### Scenario: 历史 race/leak 优雅降级

- **WHEN** TSan/ASan 复验发现历史 race/leak(非本 change 引入,pre-existing)
- **THEN** MUST 记录为 pre-existing(非本 change 引入)
- **AND** MUST 创建独立 OpenSpec change 跟踪修复
- **AND** 本 change 仍 archive(ship gate 不阻塞)

### Requirement: tech-debt-cleanup-sprint6-archive

`tech-debt-cleanup-sprint-6` MUST 干净 archive:§6.3 follow-up 列表 6 项全部 ✅(6.3.1/6.3.2/6.3.3/6.3.4/6.3.5/6.3.6),STATUS NOTE 对账引用本 change + Sprint 7/8/9 全部 ship commit。`openspec list` MUST ≤ 1 active change(Sprint 9 回填可保留)。

#### Scenario: STATUS NOTE §6.1 表格对账

- **WHEN** 完成 Step 13(P2.E)
- **THEN** `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` §6.1 表格 MUST 6.3.1/2/3/4/5/6 全部标 ✅
- **AND** STATUS NOTE MUST 引用本 change (`2026-06-24-tech-debt-and-phase1-closure`) + Sprint 7/8/9 全部 ship commit hash

#### Scenario: archive 成功

- **WHEN** Step 13 完成
- **THEN** `openspec archive tech-debt-cleanup-sprint-6 --yes` exit code MUST 0
- **AND** `ls openspec/changes/tech-debt-cleanup-sprint-6/` MUST "No such file or directory"
- **AND** `ls openspec/changes/archive/` MUST 含 `2026-06-21-tech-debt-cleanup-sprint-6/`

#### Scenario: openspec list ≤ 1

- **WHEN** Step 7 + Step 13 archive 都完成
- **THEN** `openspec list` MUST 输出 ≤ 1 active change
- **AND** 残留 active change 仅可能是 Sprint 9 回填(ship 后立即 archive)

### Requirement: plugin-loader-archived

`2026-07-14-plugin-loader` MUST 干净 archive:S5.T1+T2 实施 + S5.T3 demo 3 模式 + S5.T4 5 ADR Approved + S5.T5 sync-pdk 全部 [x]。`openspec list` MUST 不再显示此 change。

#### Scenario: tasks.md 全部 [x]

- **WHEN** Step 7(P1.D)完成前
- **THEN** `grep -c "^- \[ \]" openspec/changes/2026-07-14-plugin-loader/tasks.md` MUST 0 命中
- **AND** §Sprint 5 全部子任务 MUST [x]

#### Scenario: archive 成功

- **WHEN** Step 7 完成
- **THEN** `openspec archive 2026-07-14-plugin-loader --yes` exit code MUST 0
- **AND** `ls openspec/changes/archive/` MUST 含 `2026-06-21-2026-07-14-plugin-loader/`

### Requirement: hub-out-degree-verified

`TopoScheduler::execute` out_degree MUST < 30(per Sprint 8 spec),3 subfunction out_degree MUST < 25,`mcp__code-review-graph__get_hub_nodes --top_n 5` 验证。

#### Scenario: hub out_degree 测量

- **WHEN** 全路径 ship gate
- **THEN** `mcp__code-review-graph__get_hub_nodes --top_n 5` MUST 显示 `topo_scheduler::execute` out_degree < 30
- **AND** 3 subfunction (`prepare_dag_state` / `dispatch_ready_nodes` / `handle_node_completion`) out_degree MUST < 25

### Requirement: adr-lint-and-docs-drift-clean

`python3 tools/adr_lint.py docs/adr/` MUST exit 0。`python3 tools/docs_drift_audit.py` MUST 0 critical drift。

#### Scenario: adr_lint 0 error

- **WHEN** 全路径 ship gate
- **THEN** `python3 tools/adr_lint.py docs/adr/` exit code MUST 0
- **AND** 5 ADR (0019/0020/0021/0022/0023) MUST linter 干净

#### Scenario: docs_drift_audit 0 critical

- **WHEN** 全路径 ship gate
- **THEN** `python3 tools/docs_drift_audit.py` exit code MUST 0
- **AND** 0 critical drift(代码与文档一致)
