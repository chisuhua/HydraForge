# Tasks: ship-ievaluator-reward-contract

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit

## Phase 0: 契约类型声明（估时 0.5 sprint）

- [x] **T0.1** Write failing test: `tests/test_evaluator.cpp` 骨架（≥ 4 cases 占位，引入 `IEvaluator` + `RewardSignal` + `ExecutionTrace` 类型）
- [x] **T0.2** Verify fail: 编译失败（`fatal error: 'agenticdsl/contract/ievaluator.h' file not found`）
- [x] **T0.3** Implement minimal: `include/agenticdsl/contract/ievaluator.h`（`class IEvaluator` 抽象类 + 2 虚函数：`evaluate(const ExecutionTrace&)` + `compare(a, b)`）
- [x] **T0.4** Implement minimal: `include/agenticdsl/types/reward_signal.h`（`struct RewardSignal` 三态 + 工厂方法 + scalar 范围验证）
- [x] **T0.5** Implement minimal: `include/agenticdsl/types/execution_trace.h`（`struct ExecutionTrace` 含 `final_result` + `trace_id`）
- [x] **T0.6** Verify pass: 编译成功，4 个 TEST_CASE 编译通过（运行时仍 FAIL）
- [x] **T0.7** Commit: `feat(contract): IEvaluator + RewardSignal + ExecutionTrace types (T0)`

## Phase 1: V1 TaskSuccessEvaluator（估时 0.5 sprint）

- [x] **T1.1** Write failing test: `test_evaluator.cpp::task_success_returns_excellent_on_ok`（输入 trace.final_result.ok=true → RewardSignal.quality=Excellent）
- [x] **T1.2** Write failing test: `test_evaluator.cpp::task_success_returns_poor_on_failure`（输入 ok=false → quality=Poor）
- [x] **T1.3** Write failing test: `test_evaluator.cpp::compare_returns_winner_correctly`（a.ok=true vs b.ok=false → 返回 +1）
- [x] **T1.4** Write failing test: `test_evaluator.cpp::scalar_range_validation`（scalar < -1.0 或 > 1.0 时抛 std::out_of_range）
- [x] **T1.5** Verify fail: 上述全部 4 cases FAIL（unimplemented）
- [x] **T1.6** Implement: `src/modules/cognitive/task_success_evaluator.cpp`（3 行实现，per ADR-0083 §决策 5）
- [x] **T1.7** Implement: `src/modules/cognitive/CMakeLists.txt` 注册新源文件
- [x] **T1.8** Verify pass: 4 cases PASS
- [x] **T1.9** Commit: `feat(cognitive): TaskSuccessEvaluator V1 (T1)`

## Phase 2: Setter 注入集成（估时 0.5 sprint）

- [x] **T2.1** Write failing test: `test_evaluator.cpp::cognitive_worker_set_evaluator`（setter 注入后 evaluate 正常调用）
- [x] **T2.2** Write failing test: `test_evaluator.cpp::domain_worker_pool_set_evaluator`（setter 注入后 evaluate 正常调用）
- [x] **T2.3** Write failing test: `test_evaluator.cpp::null_evaluator_no_crash`（evaluator==nullptr 时不崩溃，条件判断）
- [x] **T2.4** Verify fail: 3 cases FAIL（setter 未实现）
- [x] **T2.5** Implement: `include/agenticdsl/cognitive/cognitive_worker.h` 新增 `void set_evaluator(std::shared_ptr<IEvaluator>)`
- [x] **T2.6** Implement: `src/modules/cognitive/cognitive_worker.cpp` 实现 setter + 任务完成时条件调用 evaluate()
- [x] **T2.7** Implement: `include/agenticdsl/cognitive/domain_worker_pool.h` 新增 `void set_evaluator(std::shared_ptr<IEvaluator>)`
- [x] **T2.8** Implement: `src/modules/cognitive/domain_worker_pool.cpp` 实现 setter + 任务完成时条件调用 evaluate()
- [x] **T2.9** Verify pass: 3 cases PASS + 现有测试全部 PASS
- [x] **T2.10** Commit: `feat(cognitive): IEvaluator setter injection in workers (T2)`

## Phase 3: 事件发射集成（估时 0.5 sprint）

- [x] **T3.1** Write failing test: `test_evaluator.cpp::evaluation_result_event_emitted`（CognitiveWorker 任务完成且有 evaluator 时发射 evaluation.result）
- [x] **T3.2** Write failing test: `test_evaluator.cpp::evaluation_result_contains_required_fields`（事件 payload 含 evaluation_id/scalar/quality/schema_version）
- [x] **T3.3** Verify fail: 2 cases FAIL（事件发射未实现）
- [x] **T3.4** Implement: CognitiveWorker worker_loop 在任务完成后发射 `evaluation.result`（使用 EventBuilder）
- [x] **T3.5** Implement: DomainWorkerPool process_task 在任务完成后发射 `evaluation.result`（使用 EventBuilder）
- [x] **T3.6** Verify pass: 2 cases PASS + 现有测试全部 PASS
- [x] **T3.7** Commit: `feat(cognitive): evaluation.result event emission from workers (T3)`

## Phase 4: 文档同步 + ship（估时 0.25 sprint）

- [x] **T4.1** 修改 `docs/adr/adr-0083-evaluator-reward-contract.md` 头部 `## 状态` 章节: `🔍 Proposed` → `✅ Approved (ship 2026-08-XX)`（条件：上述所有验证通过）
- [x] **T4.2** 修改 `docs/architecture/capability-application-map-2026-08.md` §二 G10 行: `✅ Approved (评审通过 + 代码 ship)` + §八.2 闭环 1/2 第 3 环"实现状态"列更新为 `✅ IEvaluator 已 ship`（条件：上述验证通过）
- [x] **T4.3** 修改 `docs/architecture/self-evolution-architecture-2026-08.md` §五 评估信号行 + §七"需要继续形成的架构决议"项 #2 (IEvaluator 代码 ship) 标记完成（条件：上述验证通过）
- [x] **T4.4** `python3 tools/adr_lint.py` 通过
- [x] **T4.5** `python3 tools/docs_drift_audit.py` 通过
- [x] **T4.6** ctest 全量当前总数 PASS 零回归（动态计数，非固定 185）
- [x] **T4.7** Commit: `feat(evaluator): ship IEvaluator + RewardSignal contract`（条件：T4.4-T4.6 通过）
- [x] **T4.8** `openspec archive 2026-08-26-ship-ievaluator-reward-contract`（条件：T4.7 完成）

## 总估时

- Phase 0: 0.5 sprint
- Phase 1: 0.5 sprint
- Phase 2: 0.5 sprint
- Phase 3: 0.5 sprint
- Phase 4: 0.25 sprint
- **总估时: ~2.25 sprint**

## 明确 out of scope 的内容

- BehavioralEquivalenceEvaluator（V2 follow-up）
- CompositeEvaluator（V2 follow-up）
- IEvaluator 构造注入（ constructors 保持不变）
- evaluation.result 在 ADR-0068 Canonical Topic Registry 中注册（本 change 独立引入）
