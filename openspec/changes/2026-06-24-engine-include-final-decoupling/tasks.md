# Tasks: Engine Include Final Decoupling

> **变更类型**: 真实实现 (4 阶段, ~3-4 天总工时)
> **承接 change**: `openspec/changes/tech-debt-and-phase1-closure/` (阶段 C handoff 路径)
> **关联 superpowers plan**: `docs/superpowers/plans/2026-06-24-tech-debt-full-closure.md` §3.5.6
> **创建日期**: 2026-06-24 (handoff 自 tech-debt-and-phase1-closure)
> **前置依赖**: tech-debt-and-phase1-closure 阶段 A+B 100% + 6.3.2 P2.A ship (commit 871b62d),ctest 34/34 baseline PASS

---

## 1. P2.B — 15 测试新增 (3 commits, ~1d)

### 1.1 Commit A: 7 scheduler tests (test_scheduler.cpp)

- [ ] 1.1.1 在 `tests/test_scheduler.cpp` 末尾添加 7 个 TEST_CASE 框架:
  - `prepare_dag_state_simple_linear` `[scheduler][stageN]`
  - `prepare_dag_state_diamond` `[scheduler][stageN]`
  - `prepare_dag_state_cycle_detection` `[scheduler][stageN]`
  - `dispatch_ready_nodes_initial` `[scheduler][stageN]`
  - `dispatch_ready_nodes_parallel` `[scheduler][stageN]`
  - `handle_node_completion_success` `[scheduler][stageN]`
  - `handle_node_completion_failure` `[scheduler][stageN]`
- [ ] 1.1.2 实现各 TEST_CASE 主体(参考 Sprint 7 spec `dag-scheduler-pipeline` Requirement 详细 contract)
- [ ] 1.1.3 `cmake --build build` + `ctest -R test_scheduler` MUST 14/14 PASS(7 baseline + 7 新)
- [ ] 1.1.4 `git add tests/test_scheduler.cpp` + `git commit -m "test(scheduler): add 7 test cases for DagState 3 subfunctions (6.3.4 part 1)"`

### 1.2 Commit B: 5 parser tests (test_parser.cpp)

- [ ] 1.2.1 在 `tests/test_parser.cpp` 末尾添加 5 个 TEST_CASE:
  - `factory_registry_registers_all_types`
  - `factory_registry_creates_correct_subtype`
  - `factory_registry_unknown_type_returns_nullptr`
  - `factory_registry_global_singleton`
  - `factory_registry_concurrent_access`(TSan 验证)
- [ ] 1.2.2 实现各 TEST_CASE 主体
- [ ] 1.2.3 `cmake --build build` + `ctest -R test_parser` MUST 5/5 PASS
- [ ] 1.2.4 `cmake --preset tsan && ctest -R factory_registry_concurrent_access` MUST 0 race
- [ ] 1.2.5 `git add tests/test_parser.cpp` + `git commit -m "test(parser): add 5 test cases for NodeFactoryRegistry (6.3.4 part 2)"`

### 1.3 Commit C: 3 engine_factory tests (新建 test_engine_factory.cpp)

- [ ] 1.3.1 新建 `tests/test_engine_factory.cpp`:
  - `test_engine_create_with_default_config` (验证 DSLEngine 默认构造路径)
  - `test_engine_create_with_custom_config` (验证自定义配置)
  - `test_engine_create_with_dependencies` (验证依赖注入)
- [ ] 1.3.2 编辑 `tests/CMakeLists.txt` 注册新测试文件
- [ ] 1.3.3 **测试覆盖 P2.A 删除后的 engine.cpp 构造路径(非已删 factory)**
- [ ] 1.3.4 `cmake --build build` + `ctest -R test_engine_factory` MUST 3/3 PASS
- [ ] 1.3.5 `git add tests/test_engine_factory.cpp tests/CMakeLists.txt` + `git commit -m "test(engine): add 3 test cases for engine construction post-factory-removal (6.3.4 part 3)"`

### 1.4 P2.B ship gate

- [ ] 1.4.1 `ctest --output-on-failure` MUST **34/34 PASS**(34 baseline 中 `test_path_resolution` 因测试的 `load_llm_config()` 被删除而移除;新增 `test_engine_factory` 二进制;净计数保持 34)
- [ ] 1.4.2 `ctest` 通过后,运行 `./build/tests/test_scheduler --reporter compact` / `./build/tests/test_parser --reporter compact` / `./build/tests/test_engine_factory --reporter compact`,确认 TEST_CASE PASS 总数 ≥ 13 + 17 + 3 = 33
- [ ] 1.4.3 零回归(34 个既有测试 binary 全部保留,除 `test_path_resolution` 因测试目标函数 `load_llm_config()` 被删除而整体移除)
- [ ] 1.4.4 **P2.C 启动前置检查**:本 step 全 [x] + ctest 34/34 PASS(per plan §7 TDD 硬约束 Decision 3)

---

## 2. P2.F — TSan/ASan 复验 (0 commits, ~1h)

- [ ] 2.1 启动前:`ctest` baseline **34/34 PASS**
- [ ] 2.2 `cmake --preset asan && ctest --output-on-failure` 0 error
- [ ] 2.3 `cmake --preset tsan && ctest --output-on-failure` 0 race
- [ ] 2.4 验证:`factory_registry_concurrent_access` under TSan 0 race
- [ ] 2.5 **优雅降级**:若发现历史 race/leak(非本 change 引入),记录为 pre-existing + 创建独立 OpenSpec change 跟踪(本 change 仍 archive)
- [ ] 2.6 ship gate:ASan + TSan 0 error(本 change 引入)

---

## 3. P2.C — engine.cpp includes 10→≤3 (2-4 commits, ~1.5d 时间盒)

### 3.1 启动前基线

- [ ] 3.1.1 **基线记录**:`grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出当前数字(预计 10,经 P2.A 删除 factory 后未变,实测 = 10)
- [ ] 3.1.2 **记录基线数字到本 tasks.md**(承重假设:实际数字 = 10)
- [ ] 3.1.3 1.5 day 时间盒启动,记录开始时间

### 3.2 Commit A: 替换 ToolRegistry include

- [ ] 3.2.1 编辑 `src/core/engine.cpp`:用 `IToolRegistry*` 依赖替换 `ToolRegistry` 完整 include(per ADR-0019 §1.4 已 ship 接口)
- [ ] 3.2.2 `cmake --build build` 编译通过
- [ ] 3.2.3 `ctest --output-on-failure` MUST 34/34 PASS
- [ ] 3.2.4 `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 **9**
- [ ] 3.2.5 `git add src/core/engine.cpp` + `git commit -m "refactor(core): factory-inject ToolRegistry, reduce includes baseline→baseline-2 (6.3.5 batch 1)"`

### 3.3 Commit B: 替换 MockLLMProvider include

- [ ] 3.3.1 编辑 `src/core/engine.cpp`:用 `IProviderFactory*` 依赖替换 `MockLLMProvider` 完整 include
- [ ] 3.3.2 `cmake --build build` 编译通过
- [ ] 3.3.3 `ctest --output-on-failure` MUST 34/34 PASS
- [ ] 3.3.4 `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST 输出 **4**
- [ ] 3.3.5 `git add src/core/engine.cpp` + `git commit -m "refactor(core): factory-inject MockLLMProvider, reduce includes baseline-2→baseline-5 (6.3.5 batch 2)"`

### 3.4 Commit C: 替换 BudgetController include + IBudgetController 抽象(若需)

- [ ] 3.4.1 **决策点**: 是否需要 `IBudgetController` 抽象?若 BudgetController 是 struct(POD-style)且 engine.cpp 仍依赖完整型 → 引入接口
- [ ] 3.4.2 若需: 新建 `include/agenticdsl/contract/ibudget_controller.h` 纯虚接口
- [ ] 3.4.3 编辑 `src/core/engine.cpp`: 用 `IBudgetController*` 依赖替换 `BudgetController` 完整 include
- [ ] 3.4.4 `cmake --build build` 编译通过
- [ ] 3.4.5 `ctest --output-on-failure` MUST 34/34 PASS
- [ ] 3.4.6 `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST ≤ 3
- [ ] 3.4.7 `git add src/core/engine.cpp include/agenticdsl/contract/ibudget_controller.h` + `git commit -m "refactor(core): introduce IBudgetController + factory-inject, reduce includes baseline-5→≤3 (6.3.5 batch 3)"`

### 3.5 P2.C ship gate

- [ ] 3.5.1 `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST ≤ 3
- [ ] 3.5.2 `ctest --output-on-failure` 34/34 PASS
- [ ] 3.5.3 `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 `topo_scheduler::execute` out_degree < 30
- [ ] 3.5.4 1.5 day 时间盒未超时

### 3.6 P2.C handoff 变体 (1.5 day 超时)

- [ ] 3.6.1 **触发条件**: 1.5 day 时间盒超时仍未达 ≤ 3
- [ ] 3.6.2 创建新 OpenSpec change `2026-07-xx-engine-include-final-decoupling-v2`:
  - `proposal.md` (Why: 6.3.5 超出本 change 时间盒,正式 handoff)
  - `tasks.md` (引用本 change + 继续 P2.C 未完成部分)
  - `specs/engine-include-decoupling/spec.md` (新增 capability spec)
- [ ] 3.6.3 `openspec validate 2026-07-xx-engine-include-final-decoupling-v2` exit 0
- [ ] 3.6.4 **更新本 tasks.md §3.5 状态**: `⏳ Handoff to 2026-07-xx-engine-include-final-decoupling-v2`
- [ ] 3.6.5 `git add openspec/changes/2026-07-xx-engine-include-final-decoupling-v2/ openspec/changes/2026-06-24-engine-include-final-decoupling/tasks.md` + `git commit -m "chore(openspec): handoff 6.3.5 to engine-include-final-decoupling-v2 (timebox overflow)"`
- [ ] 3.6.6 **本 change 仍 archive** (非 ship-as-is 留账)

---

## 4. 6.3.6 — pending_dynamic_deps_ 回归验证 (Sprint 7 已 ship)

- [ ] 4.1 验证:`grep "session_.pending_dynamic_deps_" src/ --include='*.cpp' --include='*.h'` MUST 0 命中
  - **历史基线 (Sprint 7 `75ded94` ship)**: 0 命中,源代码全用 `session_.get_pending_dynamic_deps()` 访问器
  - **唯一 2 处出现在 `src/modules/exports/req1.md`**(markdown 文档代码块,非源代码)
- [ ] 4.2 若 grep 命中源代码:识别未迁移调用点,改用 `session_.get_pending_dynamic_deps()` 访问器
- [ ] 4.3 `ctest --output-on-failure` MUST 34/34 PASS(零回归)
- [ ] 4.4 若 4.2 触发: `git add src/...` + `git commit -m "refactor(scheduler): use get_pending_dynamic_deps() accessor (6.3.6 final regression)"`

---

## 5. Archive 闭环 (跨 Sprint 10 起点)

### 5.1 STATUS NOTE 对账 + archive chain

- [ ] 5.1.1 编辑 `openspec/changes/tech-debt-and-phase1-closure/tasks.md` §8.1 选项 C 决议: 标 "本 change ship 成功,本 change 已 archive" + 引用本 change commit hash
- [ ] 5.1.2 编辑 `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` §6.1 表格: 6.3.4/5 标 ✅(本 change ship) + 6.3.6 标 ✅(Sprint 7 `75ded94`) + 引用本 change
- [ ] 5.1.3 `openspec validate tech-debt-cleanup-sprint-6` exit 0
- [ ] 5.1.4 `openspec archive tech-debt-cleanup-sprint-6 --yes`
- [ ] 5.1.5 验证: `ls openspec/changes/tech-debt-cleanup-sprint-6/` MUST "No such file or directory"
- [ ] 5.1.6 `git add -A` + `git commit -m "chore(openspec): archive tech-debt-cleanup-sprint-6 (6.3.x all closed via handoff chain)"`

### 5.2 Sprint 9 archive

- [ ] 5.2.1 验证: `openspec/changes/sprint-9-handle-node-completion/tasks.md` 全部 [x]
- [ ] 5.2.2 `openspec archive sprint-9-handle-node-completion --yes`
- [ ] 5.2.3 验证: `ls openspec/changes/sprint-9-handle-node-completion/` MUST "No such file or directory"
- [ ] 5.2.4 `git add -A` + `git commit -m "chore(openspec): archive sprint-9-handle-node-completion"`

### 5.3 tech-debt-and-phase1-closure archive

- [ ] 5.3.1 验证: `openspec/changes/tech-debt-and-phase1-closure/tasks.md` 全部 ship task [x] + handoff task 标 ⏳ 已 ship 至本 change
- [ ] 5.3.2 `openspec archive tech-debt-and-phase1-closure --yes`
- [ ] 5.3.3 验证: `ls openspec/changes/tech-debt-and-phase1-closure/` MUST "No such file or directory"
- [ ] 5.3.4 `git add -A` + `git commit -m "chore(openspec): archive tech-debt-and-phase1-closure (Sprint 10 ready)"`

### 5.4 本 change archive

- [ ] 5.4.1 `openspec validate 2026-06-24-engine-include-final-decoupling` exit 0
- [ ] 5.4.2 全部 5 阶段 [x] + ship gate 验证通过后
- [ ] 5.4.3 `openspec archive 2026-06-24-engine-include-final-decoupling --yes`
- [ ] 5.4.4 验证: `openspec list` MUST 0 active change
- [ ] 5.4.5 `git add -A` + `git commit -m "chore(openspec): archive 2026-06-24-engine-include-final-decoupling (Sprint 10 0 backlog)"`

---

## 6. ship gate 验证清单 (本 change 全部完成时)

- [ ] 6.1 `git status` MUST 完全干净
- [ ] 6.2 `cd build && ctest --output-on-failure` MUST 34/34 PASS
- [ ] 6.3 `cmake --preset asan && ctest --output-on-failure` MUST 0 error
- [ ] 6.4 `cmake --preset tsan && ctest --output-on-failure` MUST 0 race (本 change 引入)
- [ ] 6.5 `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` MUST ≤ 3
- [ ] 6.6 `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l` MUST ≤ 60
- [ ] 6.7 `mcp__code-review-graph__get_hub_nodes --top_n 5` MUST execute out_degree < 30 + 3 subfunction out_degree < 25
- [ ] 6.8 `python3 tools/adr_lint.py docs/adr/` MUST exit 0
- [ ] 6.9 `python3 tools/docs_drift_audit.py` MUST 0 critical drift
- [ ] 6.10 `openspec list` MUST 0 active change(本 change archive 后)
- [ ] 6.11 `git log --oneline -30` MUST 包含本 change 全部 commit(按 Step 顺序)
- [ ] 6.12 零回归(34 baseline 全部保留,新增 15 测试)

---

## 7. 验证与回归策略

- **回归策略**: 每 Step commit 前 MUST `ctest` 验证 baseline 不破(34 → 49 渐进);P2.C 分批每批验证
- **TDD 硬约束**: P2.B (Step 1) 全部 [x] 之前禁止启动 P2.C (Step 3) — 承袭 plan §7 Decision 3
- **时间盒**: P2.C 1.5 day,超时触发二次 handoff(Step 3.6),仍非 ship-as-is
- **优雅降级**: TSan/ASan 历史 race/leak 不阻塞 archive,记录 pre-existing + 独立 change 跟踪
- **SHALL/MUST 验证**: 1 个 spec Requirement 全部 [x] 方可 archive

---

## 8. STATUS NOTE (本 change 承接 tech-debt-and-phase1-closure)

> **承接关系**: 本 change 是 `tech-debt-and-phase1-closure` 阶段 C (Step 8-12) 的显式 handoff 路径
> **治理模式**: 严格全路径(承袭 plan §8 治理模式) — 不重蹈 Sprint 6 limfall 模式
> **TDD 硬约束**: P2.B 必须在 P2.C 之前(承袭 plan §7 Decision 3)
> **分批提交**: P2.C 2-4 commit(承袭 plan §8 Decision 4)避免 Sprint 6 一次性大改 limfall
> **时间盒 + 二次 handoff**: P2.C 1.5 day 超时触发 §3.6 二次 handoff,仍非 ship-as-is
> **优雅降级**: TSan/ASan 历史 race 不阻塞 archive
> **archive 闭环**: ship 后 archive chain 4 步(tech-debt-cleanup-sprint-6 → sprint-9 → tech-debt-and-phase1-closure → 本 change)
> **承重假设**: 二次 handoff 触发条件仅当 P2.C 1.5 day 超时,否则正常 ship
