# Tasks — GEPA/MCTS 进化预算接入

> **关键不变量**: nullptr budget_controller → 行为 100% 等同 T19/T20 (零回归); 超限 → graceful break 非抛异常
> **估时**: 0.5 sprint
> **预期 commit 数**: 1
> **前置依赖**: `openspec/changes/2026-08-31-evolution-budget-cap` (T3) **必须先 ship** (本 change 消费其 IBudgetController 接口)

## 1. Pre-flight Verification (Setup)

- [ ] 1.1 验证 T3 evolution-budget-cap 已 ship (IBudgetController 有 try_consume_evolution_llm_call)
  - 命令: `grep "try_consume_evolution_llm_call" src/modules/budget/budget_controller.h`
  - 预期: ≥1 行 (接口层 + 实现层)
- [ ] 1.2 验证 GEPA/MCTS 基线测试通过 (17 + 65 assertions)
  - 命令: `./build/tests/test_gepa_loop --reporter compact 2>/dev/null; ./build/tests/test_mcts_workflow_search --reporter compact`
  - 预期: 0 failures
- [ ] 1.3 记录 GEPA 主循环当前针位 (gepa_loop.cpp line ~71 for 循环)
- [ ] 1.4 记录 MCTS 主循环当前针位 (mcts_workflow_search.cpp line ~216 for 循环)

## 2. Phase 0 — GEPALoop 预算接入

- [ ] 2.1 `include/agenticdsl/cognitive/gepa_loop.h` 新增 ctor 重载:
  - `GEPALoop(evaluator, governor, llm, Config, bus, std::shared_ptr<IBudgetController> budget_controller)`
  - 原 ctor 委托新 ctor (`budget_controller = nullptr`)
  - 新增成员 `std::shared_ptr<IBudgetController> budget_controller_`
- [ ] 2.2 `src/modules/cognitive/gepa_loop.cpp` 主循环顶接入:
  ```cpp
  for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
    if (budget_controller_ && !budget_controller_->try_consume_evolution_llm_call()) {
      result.failure_mode = "evolution_budget_exceeded";
      emit_event(bus_, "gepa.reflection.failed",
                 {{"reflection_id", make_reflection_id(failed_trace, iteration)},
                  {"reason", result.failure_mode}});
      break;
    }
    // ... 原有逻辑不变
  }
  ```
- [ ] 2.3 写 GEPA 预算测试 (tests/test_gepa_mcts_budget_integration.cpp 前 2 cases):
  - `gepa_nullptr_budget_unchanged` — nullptr 时行为等同 T19 (mock 迭代正常完成)
  - `gepa_budget_exceeded_breaks` — 预算上限=1, 2 次迭代 → 第 2 次 break + failure_mode="evolution_budget_exceeded"

## 3. Phase 0 — MCTSWorkflowSearch 预算接入

- [ ] 3.1 `include/agenticdsl/cognitive/mcts_workflow_search.h` 新增 ctor 重载:
  - `MCTSWorkflowSearch(evaluator, governor, regression_gate, SearchConfig, bus, std::shared_ptr<IBudgetController> budget_controller)`
  - 原 ctor 委托新 ctor (`budget_controller = nullptr`)
  - 新增成员 `std::shared_ptr<IBudgetController> budget_controller_`
- [ ] 3.2 `src/modules/cognitive/mcts_workflow_search.cpp` 主循环顶接入:
  ```cpp
  for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
    if (budget_controller_ && !budget_controller_->try_consume_evolution_llm_call()) {
      result.failure_mode = "evolution_budget_exceeded";
      emit_event(bus_, "mcts.budget_exceeded",
                 {{"task_id", spec.task_id}, {"iteration", iteration}});
      break;
    }
    // ... 原有逻辑不变
  }
  ```
- [ ] 3.3 写 MCTS 预算测试 (后 3 cases):
  - `mcts_nullptr_budget_unchanged` — nullptr 时行为等同 T20 (mock 搜索正常完成)
  - `mcts_budget_exceeded_breaks` — 预算上限=1, 搜索 break + failure_mode="evolution_budget_exceeded"
  - `mcts_budget_adequate_completes` — 预算充足时 max_iterations 内正常完成

## 4. Phase 0 — 事件注册 (ADR-0068 Appendix A v2.0+)

- [ ] 4.1 ADR-0068 Appendix A 注册 `mcts.budget_exceeded` 主题 (**v2.0+**, W4 归口: Axis6 owns v1.8, T1/T2/T3 用 v1.9+, 本 change 用 v2.0+ 紧随)
  - payload: `{task_id, iteration}`
- [ ] 4.2 验证注册
  - 命令: `grep -c "mcts.budget_exceeded" docs/adr/adr-0068-event-emission-contract.md`
  - 预期: ≥1 行

## 5. Phase 0 — Ship Gate

- [ ] 5.1 `tools/adr_lint.py` 0 errors
- [ ] 5.2 `tools/docs_drift_audit.py` 0 CRITICAL
- [ ] 5.3 `openspec validate 2026-08-31-gepa-mcts-budget-integration --strict` valid
- [ ] 5.4 不变量 4 验证 (contract 零修改): `git diff --stat HEAD -- include/agenticdsl/contract/` = 0 行
- [ ] 5.5 不变量 5 验证 (基线 0 回归): GEPA 17 cases + MCTS 65 assertions 全 pass
- [ ] 5.6 全量 ctest 0 回归

## 6. Phase 0 — Commit

- [ ] 6.1 Git add: ge_loop.h/cpp + mcts_workflow_search.h/cpp + test + adr-0068
- [ ] 6.2 Commit message (conventional):
  ```
  feat(cognitive): GEPA/MCTS 进化预算接入 (N1 闭环, Oracle Blocker)
  
  T3 evolution-budget-cap 提供预算基础设施后, 本 change 将 try_consume_evolution_llm_call()
  接入两个进化引擎主循环 (N1 缺口闭环): GEPA reflect_and_commit + MCTS search,
  每次迭代顶预算检查, 超限 graceful break (failure_mode="evolution_budget_exceeded"), 不抛异常。
  
  ctor 重载注入 shared_ptr<IBudgetController> (默认 nullptr 零回归, T19/T20 调用方零修改):
  GEPA(e, g, llm, config, bus, budget_controller) / MCTS(e, g, rg, config, bus, budget_controller)
  
  新增 1 个 mcts.budget_exceeded 事件 (ADR-0068 Appendix A v2.0+); GEPA 复用 gpa.reflection.failed。
  5 新测试 (test_gepa_mcts_budget_integration.cpp): nullptr 不变 ×2 / 超限 break ×2 / 充足完成 ×1。
  
  Oracle N1: "预算闸需接入方否则 T3 死代码" — 本 change 闭环。
  ```

## 7. 后续追踪 (不在本 change)

- [ ] 7.1 T5 evolution-readiness-gate-v1 (消费 budget_controller begin/end_evolution_cycle)
- [ ] 7.2 T6 chain-evolution-driver-v1 (串联 GEPA/MCTS + budget)
- [ ] 7.3 N2 in-flight 一致性 / N3 并发互斥 (后续 change)
- [ ] 7.4 axis6-chain-workflow 文档 N1 状态标注 "已修复 (接入 closed)"