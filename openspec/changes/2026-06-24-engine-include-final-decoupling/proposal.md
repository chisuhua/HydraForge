## Why

`tech-debt-and-phase1-closure` change (13 步全路径) 在 2026-06-24 完成阶段 A+B 100% (Task 0-3 + Step 4-7) + 6.3.2 P2.A (删 scheduler factory 死代码) 实际关闭,但 6.3.4 P2.B (15 测试) + 6.3.5 P2.C (engine.cpp includes 10→≤3) + P2.F (TSan/ASan 复验) + 6.3.6 回归验证 — 这些 6.3.x 剩余项因工作量与时间盒限制(per plan §3.5.6 设计的 1.5 day 时间盒),**显式 handoff** 至本独立 change 跟踪,避免重蹈 Sprint 6 ship-as-is 留账反模式。

本次 handoff 是**主动 handoff**(有 change proposal + tasks + spec),**非 ship-as-is 留账**(无 change 仅 tasks.md 0/164 勾选)。本 change 接收所有 6.3.x 剩余工作,提供独立的 spec 跟踪 + ship gate + 验收清单。

## What Changes

- **P2.B — 15 测试新增**(~1d,3 commits):
  - `tests/test_scheduler.cpp` +7 case (per spec `dag-scheduler-pipeline` 详细 contract)
  - `tests/test_parser.cpp` +5 case (NodeFactoryRegistry 并发安全 + 全 NodeType 覆盖)
  - 新建 `tests/test_engine_factory.cpp` +3 case (DSLEngine 默认/自定义/依赖注入构造)
- **P2.C — engine.cpp includes 10→≤3 分批重构**(~1.5d 时间盒,2-4 commits):
  - Commit A: ToolRegistry 完整 include → `IToolRegistry*` 依赖(per ADR-0019 §1.4 已 ship 接口)
  - Commit B: MockLLMProvider 完整 include → `IProviderFactory*` 依赖
  - Commit C: BudgetController 完整 include → `IBudgetController*` 抽象(若需)
- **P2.F — TSan/ASan 复验**(~1h,0 commits):
  - `cmake --preset asan && ctest` 0 error
  - `cmake --preset tsan && ctest` 0 race
  - `factory_registry_concurrent_access` under TSan 0 race
- **6.3.6 — pending_dynamic_deps_ 访问一致回归验证**(Sprint 7 `75ded94` 已 ship,本 change 仅 grep 验证源代码 0 命中)
- **Archive 闭环**:
  - ship 后 archive 本 change
  - 同步 archive `tech-debt-cleanup-sprint-6` + `sprint-9-handle-node-completion` + `tech-debt-and-phase1-closure`
  - 达到 `openspec list` 0 active change,Sprint 10 干净起点

**无 API breaking change**(仅内部重构 + 新增测试,公共 contract 接口保持稳定)。

## Capabilities

### New Capabilities

- `engine-include-decoupling`: engine.cpp 跨模块 include 10→≤3 渐进式重构(分批 commit + ctest 验证)

### Modified Capabilities

- `tech-debt-cleanup`: 扩展现有 spec,关闭 §6.3 follow-up 列表 6.3.4/6.3.5/6.3.6(本 change 实施)+ 6.3.1/6.3.2(`tech-debt-and-phase1-closure` 关闭)

## Impact

**修改文件**:
- `src/core/engine.cpp` (P2.C 3 batch commit)
- `include/agenticdsl/contract/ibudget_controller.h` (P2.C Commit C,可选若 BudgetController 是 POD-style)
- `tests/test_scheduler.cpp` (P2.B Commit A,+7 case)
- `tests/test_parser.cpp` (P2.B Commit B,+5 case)
- `tests/test_engine_factory.cpp` (P2.B Commit C,新建 +3 case)
- `tests/CMakeLists.txt` (注册 test_engine_factory)
- `openspec/changes/tech-debt-and-phase1-closure/tasks.md` (本 change 引用,handoff 闭环)
- `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` (本 change 引用,§6.3 对账)
- `AGENTS.md` (Recent Changes 追加本 change ship)

**API 稳定性**:
- `DSLEngine` 公共 API 零变化
- `TopoScheduler` / `NodeExecutor` 公共 API 零变化
- `IToolRegistry` / `IProviderFactory` / `IBudgetController`(若引入) 公共契约稳定

**依赖变更**:
- 无新第三方依赖
- `IBudgetController` 抽象若引入,仅在 `include/agenticdsl/contract/` 下新增接口头

**测试影响**:
- baseline 34/34 ctest pass → 目标 49/49 (新增 7+5+3 = 15 test case)
- TSan + ASan 全矩阵复验
- `code-review-graph` hub out_degree 复验:`TopoScheduler::execute` < 30 + 3 subfunction < 25

**风险域**:
- 🔴 P2.C 是 Sprint 6 limfall 重灾区,本 change 需 1.5 day 时间盒 + 分批提交 + 失败时启动二次 handoff
- 🟠 P2.B TDD 硬约束:必须 P2.B 全部 100% [x] 才能启动 P2.C(per plan §7 Decision 3)
- 🟡 TSan/ASan 优雅降级:若发现历史 race/leak(非本 change 引入),记录 pre-existing + 独立 change 跟踪(本 change 仍 archive)

**Non-goals**:
- **不重做** Sprint 6/7/8/9 已 ship 的 commit
- **不修改** PDK / CognitiveWorker / DomainWorkerPool 公共 API
- **不引入** 新第三方依赖
- **不实现** ADR-0007 LLM 压缩 / ADR-0031/0033 实质化(属 P3 长期)
- **不重构** `external/` vendor 代码
- **不创建** `2026-07-30-sprint-8-...` 或 `sprint-7-tech-debt-followup` 续接 change(此二已 archive)
