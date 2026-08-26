# Tasks: ship-ievaluator-reward-contract

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit

## Phase 0: 契约类声明（估时 0.5 sprint）

- [ ] **T0.1** Write failing test: `tests/test_evaluator.cpp` 骨架（≥ 4 cases 占位，引入 `IEvaluator` + `RewardSignal` 类型）
- [ ] **T0.2** Verify fail: 编译失败（`fatal error: 'agenticdsl/contract/ievaluator.h' file not found`）
- [ ] **T0.3** Implement minimal: `include/agenticdsl/contract/ievaluator.h`（`class IEvaluator` 抽象类 + 2 虚函数）
- [ ] **T0.4** Implement minimal: `include/agenticdsl/types/reward_signal.h`（`struct RewardSignal` 三态 + 工厂方法）
- [ ] **T0.5** Implement minimal: `include/agenticdsl/types/execution_trace.h`（`struct ExecutionTrace`）
- [ ] **T0.6** Verify pass: 编译成功，4 个 TEST_CASE 编译通过（运行时仍 FAIL）
- [ ] **T0.7** Commit: `feat(contract): IEvaluator + RewardSignal + ExecutionTrace types (T0)`

## Phase 1: V1 TaskSuccessEvaluator（估时 0.5 sprint）

- [ ] **T1.1** Write failing test: `test_evaluator.cpp::task_success_returns_excellent_on_ok`（输入 trace.final_result.ok=true → RewardSignal.quality=Excellent）
- [ ] **T1.2** Write failing test: `test_evaluator.cpp::task_success_returns_poor_on_failure`（输入 ok=false → quality=Poor）
- [ ] **T1.3** Write failing test: `test_evaluator.cpp::compare_returns_winner_correctly`（a.ok=true vs b.ok=false → 返回 +1）
- [ ] **T1.4** Verify fail: 4 cases 全部 FAIL（unimplemented）
- [ ] **T1.5** Implement: `src/modules/cognitive/task_success_evaluator.cpp`（3 行实现，per ADR-0083 §决策 5）
- [ ] **T1.6** Implement: `src/modules/cognitive/CMakeLists.txt` 注册新源文件
- [ ] **T1.7** Verify pass: 4 cases PASS, ctest 185/185 零回归
- [ ] **T1.8** Commit: `feat(cognitive): TaskSuccessEvaluator V1 (T1)`

## Phase 2: 事件集成（估时 0.5 sprint）

- [ ] **T2.1** Write failing test: `test_evaluator.cpp::evaluation_emitted_on_event`（ToolCoordinator 调用 evaluator 后 EventLog 含 `evaluation.result` 主题）
- [ ] **T2.2** Verify fail: 事件未发射（无对应代码）
- [ ] **T2.3** Implement: `src/modules/cognitive/cognitive_worker.cpp` + `src/modules/cognitive/domain_worker_pool.cpp` 调用 `IEvaluator::evaluate()` 并 emit `evaluation.result` 事件（per ADR-0083 §决策 4 + ADR-0068 EventBuilder）
- [ ] **T2.4** Verify pass: ctest 通过
- [ ] **T2.5** Commit: `feat(cognitive): IEvaluator event integration (T2)`

## Phase 3: 文档同步 + ship（估时 0.25 sprint）

- [ ] **T3.1** 修改 `docs/adr/adr-0083-evaluator-reward-contract.md` 头部 `## 状态` 章节: `🔍 Proposed` → `✅ Approved (ship 2026-08-XX)`
- [ ] **T3.2** 修改 `docs/architecture/capability-application-map-2026-08.md` §二 G10 行: `✅ Approved (评审通过 + 代码 ship)` + §八.2 闭环 1/2 第 3 环"实现状态"列更新为 `✅ IEvaluator 已 ship`
- [ ] **T3.3** 修改 `docs/architecture/self-evolution-architecture-2026-08.md` §五 评估信号行 + §七"需要继续形成的架构决议"项 #2 (IEvaluator 代码 ship) 标记完成
- [ ] **T3.4** 修改 `docs/active-status.md` §一 OpenSpec active 计数 + G11 跟踪段更新（移除"ADR-0083 自审修正"提示，标记为 ship）
- [ ] **T3.5** `python3 tools/adr_lint.py` + `python3 tools/docs_drift_audit.py` 全部通过
- [ ] **T3.6** ctest 全量 185/185 PASS 零回归
- [ ] **T3.7** `git commit -m "feat(evaluator): ship IEvaluator + RewardSignal contract (closes G10)"`
- [ ] **T3.8** `openspec archive 2026-08-26-ship-ievaluator-reward-contract`

## 总估时

- Phase 0: 0.5 sprint
- Phase 1: 0.5 sprint
- Phase 2: 0.5 sprint
- Phase 3: 0.25 sprint
- **总估时: ~1.75 sprint**