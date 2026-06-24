## Why

2026-06-23 状态审计发现 3 类真实可执行债：(1) `2026-07-14-plugin-loader` Sprint 5 仅 ship S5.T1+T2 骨架,Phase 1 收官所需的 S5.T3 demo 扩展 + S5.T4 5 ADR Approved + S5.T5 sync-pdk 全部未做,Phase 1 智能体层进度卡在 80%;(2) `tech-debt-cleanup-sprint-6` STATUS NOTE 标记的 §6.3 follow-up 列表 3/6 项仍未 ship(6.3.2 scheduler factory 死代码、6.3.4 15 个测试、6.3.5 engine.cpp 跨模块 include — 6.3.6 已在 Sprint 7 `75ded94` ship,本 change 仅回归验证),导致 backlog 143 task 持续膨胀污染 Sprint 7+;(3) 工作区脏(`git status` 显示 9 个 `D` + 1 个 untracked `docs/superpowers/`)与 Sprint 9 step 1 3 个 commit 无 backing change 形成的治理债。**Sprint 6 的"ship + STATUS NOTE 留 backlog"反模式已演化为治理危机**,需在 1 个 change 内严格按 Oracle 推荐的 13 步全路径闭环,避免任何"ship-as-is + 留账"再次发生。

## What Changes

- **工作区清场**:commit `openspec/changes/` 下 9 个 D 文件(已 archive 的 Sprint 7/8 change 残留);`git mv docs/superpowers/plans/2026-06-22-*.md` 至 `docs/archive/superpowers/plans/`(避免"声明已归档却留新文件"的自相矛盾);对账 `docs/README.md` superpowers 段落。
- **Sprint 9 回填**:为 `40008a5`/`ce4358b`/`bd936af` 3 个已 ship commit 创建 `2026-06-24-sprint-9-handle-node-completion` 形式 change tracking(治理一致性,任何 sprint 必须配 change)。
- **Phase 1 收官 (Sprint 5)**:扩展 `examples/phase1_plugin_demo/main.cpp` 加 `--load-plugin=<path>` 与 `--plugin-path=<dir>` flag 验证 3 模式;5 个 ADR(0019/0020/0021/0022/0023)状态改 ✅ Approved;`docs/roadmap-status.md` Phase 1 80%→100%;`./scripts/sync-pdk.sh` Sprint 5 ship 后执行 + 验证 standalone `hydraforge-pdk` repo。
- **6.3.2 scheduler factory 死代码**:**删除** `src/modules/scheduler/factory.{h,cpp}`(per STATUS NOTE 零调用 = 死代码,删除而非补 Config 满足 over-engineering discipline);同步改 `src/core/engine.cpp` 直接构造路径。
- **6.3.6 pending_dynamic_deps_ 访问一致**:Sprint 7 Day 8 已 ship (commit `75ded94`),`execution_session.h:70` getter 已存在,`topo_scheduler.cpp` L172/515/518 已使用 `session_.get_pending_dynamic_deps()` 访问器。**本 change 不重复实施,仅 ship gate 阶段做回归验证**(grep scope 限定源代码避免 `req1.md` markdown 误命中)。
- **6.3.4 15 个测试**:`tests/test_scheduler.cpp` +7 case,`tests/test_parser.cpp` +5 case,新建 `tests/test_engine_factory.cpp` +3 case(覆盖 P2.A 删除后的 engine.cpp 构造路径,而非已删 factory)。**TDD 顺序硬约束**:此步必须在 6.3.5 engine includes 之前。
- **6.3.5 engine.cpp 跨模块 include 10→≤3**:**分批提交** (2-4 commit,每批 ctest 验证),利用已存在的 `IProviderFactory` + `IToolRegistry` 接口(per ADR-0019 §1.4 ✅)从"发明接口"降级为"接线",如需 `IBudgetController` 抽象则一并补。
- **P2.F TSan/ASan 复验**:Sprint 6 STATUS NOTE 5.2/5.3 ship gate 自 Sprint 6 ship 后从未重跑,代码变更后必须补做。
- **Archive 闭环**:`openspec archive 2026-07-14-plugin-loader` + `openspec archive tech-debt-cleanup-sprint-6` (后者需先完成 §6.3 全部关闭 + STATUS NOTE 对账)+ Sprint 9 回填 change ship 后 archive。

**无 API breaking change**(仅内部实现重构 + ADR 状态同步,公共 contract 接口保持稳定)。

## Capabilities

### New Capabilities

- `tech-debt-and-phase1-closure`:本 change 整体 spec,跟踪 Phase 1 收官 + 6.3.x 全部 4 项关闭 + workspace 卫生 + Sprint 9 回填的 13 步全路径。

### Modified Capabilities

- `tech-debt-cleanup`:delta spec,§6.3 follow-up 列表关闭(6.3.2/6.3.4/6.3.5/6.3.6 — 6.3.6 已 ship,本 change 仅对账+回归验证)→ 在 `tech-debt-cleanup-sprint-6` archive 前更新,变更 Reason + Requirement level 状态。

## Impact

**修改文件**:
- `openspec/changes/2026-07-14-plugin-loader/tasks.md` (S5.T3+S5.T4+S5.T5 全部 [ ])
- `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` (STATUS NOTE §6.3 全部对账)
- `examples/phase1_plugin_demo/main.cpp` (S5.T3 新增 CLI flags)
- `src/core/engine.cpp` (factory 移除 + direct 构造)
- `src/modules/scheduler/factory.{h,cpp}` (删除)
- `src/modules/scheduler/topo_scheduler.cpp` (Step 9 REGRESSION-ONLY, 6.3.6 ship 后验证无退化)
- `src/core/types/node.h` (若新增 IBudgetController 抽象)
- `tests/test_scheduler.cpp` (+7)
- `tests/test_parser.cpp` (+5)
- `tests/test_engine_factory.cpp` (新建 +3)
- `docs/adr/adr-0019-*.md` / `adr-0020-*.md` / `adr-0021-*.md` / `adr-0022-*.md` / `adr-0023-*.md` (状态 → ✅ Approved)
- `docs/roadmap-status.md` (Phase 1 80% → 100%)
- `docs/README.md` (superpowers 段落对账)
- `AGENTS.md` (Recent Changes 追加 Sprint 5+6+9 收官)
- `docs/superpowers/plans/` → `docs/archive/superpowers/plans/` (git mv)
- `openspec/changes/{2026-07-14-plugin-loader,sprint-7-tech-debt-followup,...}` (archive 操作)

**API 稳定性**:
- `DSLEngine` 公共 API 零变化
- `TopoScheduler` / `NodeExecutor` / `BudgetController` 公共 API 零变化(仅内部访问器替换)
- `PluginLoader` 公共 API 零变化(S5.T3 仅扩展 demo,非 API 变更)
- 删除 `scheduler::factory.*` 是内部 API 变更,**非公共 contract 破坏** (per `agenticdsl::scheduler::create` 已在 Sprint 6 commit 引入但零调用)

**依赖变更**:
- 无新第三方依赖
- `IBudgetController` 抽象若引入,仅在 `include/agenticdsl/contract/` 下新增接口头

**测试影响**:
- baseline 34/34 ctest pass → 目标 ~49/49 (新增 7+5+3 = 15 test case)
- TSan + ASan 全矩阵复验(自 Sprint 6 ship 后首次)
- `code-review-graph` hub out_degree 复验:execute < 30 + 3 subfunction < 25
- `openspec validate tech-debt-and-phase1-closure` exit 0
- `python3 tools/adr_lint.py docs/adr/` exit 0

**风险域**:
- 🔴 P2.C (6.3.5 engine.cpp includes) 是 Sprint 6 limfall 重灾区,本 change 需 1.5 day 时间盒 + 分批提交 + 失败时启动 handoff 变体
- 🟠 P2.A 删 factory.h/cpp 前需 `grep -rn` 二次确认零调用(承重假设,若不成立改补 Config 路径)
- 🟡 P2.B 15 测试 → P2.C 重构顺序硬约束(任何颠倒都必 limfall)

**Non-goals**:
- **不重写** CognitiveWorker / DomainWorkerPool / PDK(Sprint 2/3/4 已 ship)
- **不修改** 已 ship 的 PDK 公共 API(per ADR-0021 T4b 治理节奏)
- **不引入** 新第三方依赖(仅标准库 + 已存在 vendor)
- **不实现** ADR-0007 LLM 压缩 / ADR-0031/0033 实质化(属 P3 长期)
- **不重构** `external/` vendor 代码
- **不修改** `docs/proposals/` 内容(语言演进提案独立治理)
- **不创建** `2026-07-30-sprint-8-...` 或 `sprint-7-tech-debt-followup` 续接 change — 此二 change 已 archive,本 change 是新独立 change
- **不重做** Sprint 6 已被 Oracle 决议 ship 的 4 commit (`7cc4239`/`6c5557c`/`9fa0364`/`7923b2a`)
