# Tasks — EvolutionBudgetCap

> **关键不变量**: 默认 -1 无限制 (零回归), 进化预算与总预算独立, fail-closed
> **估时**: 0.25 sprint (最小任务)
> **前置依赖**: 全部 ✅ ship (ExecutionBudget + BudgetController + IInteractionBus)
> **Oracle 评审**: session `ses_facbd3ffbffeUjlJgZsgMWFiM4` (N1 Blocker)

## 1. Pre-flight Verification

- [ ] 1.1 验证 ExecutionBudget 结构 (budget.h)
  - 命令: `grep -c "try_consume_llm_call\|exceeded" src/core/types/budget.h`
  - 预期: ≥2
- [ ] 1.2 验证 BudgetController 委托路径
  - 命令: `grep -c "try_consume\|exceeded" src/modules/budget/budget_controller.cpp`
  - 预期: ≥3
- [ ] 1.3 验证 ctest baseline
  - 命令: `ctest 2>&1 | tail -3`
  - 预期: 0 failures

## 2. Phase 0 — ExecutionBudget 字段 + 方法

- [ ] 2.1 修改 `src/core/types/budget.h`:
  - 新增 `int max_evolution_llm_calls = -1;`
  - 新增 `mutable std::atomic<int> evolution_llm_calls_used{0};`
  - 新增 `try_consume_evolution_llm_call()` (CAS, 与 try_consume_llm_call 同构)
  - 新增 `evolution_budget_exceeded()`
  - 新增 `reset_evolution_cycle()`
  - 移动构造/赋值同步更新新字段重置列表
- [ ] 2.2 修改 `src/modules/budget/budget_controller.h` + `.cpp`:
  - 新增 `try_consume_evolution_llm_call()` / `evolution_budget_exceeded()` 委托
  - 新增 `begin_evolution_cycle()` / `end_evolution_cycle()` (reset + 事件发射)
- [ ] 2.3 新建 `tests/test_evolution_budget_cap.cpp` (≥5 cases):
  - `default_unlimited_never_exceeded`
  - `cap_3_third_call_fails`
  - `evolution_exceeded_independent_of_total_exceeded`
  - `reset_evolution_cycle_resets_counter`
  - `budget_controller_delegation_works`
- [ ] 2.4 编译 + 测试通过
  - 命令: `cmake --build build --target test_evolution_budget_cap && ./build/tests/test_evolution_budget_cap --reporter compact`
  - 预期: 5 cases / 15+ assertions all pass

## 3. Phase 0 — 事件注册

- [ ] 3.1 ADR-0068 Appendix A 注册 3 个 `budget.evolution_cycle.*` 主题 (**v1.9+**, W4 归口: Axis6 change 独占 v1.8, 本 change 用 v1.9+ 紧随, 不与其竞争版本号)
  - `budget.evolution_cycle.start` / `.end` / `.exceeded`
  - owner=BudgetController
- [ ] 3.2 验证注册
  - 命令: `grep -c "budget.evolution_cycle" docs/adr/adr-0068-event-emission-contract.md`
  - 预期: ≥3

## 4. Ship Gate

- [ ] 4.1 `openspec validate 2026-08-31-evolution-budget-cap --strict` PASS
- [ ] 4.2 `python3 tools/adr_lint.py` 0 errors
- [ ] 4.3 `python3 tools/docs_drift_audit.py` 0 CRITICAL
- [ ] 4.4 `git diff --stat HEAD -- include/agenticdsl/contract/` = 0 行
- [ ] 4.5 ctest 全量零回归

## 5. Commit

- [ ] 5.1 git add:
  - `src/core/types/budget.h`
  - `src/modules/budget/budget_controller.h` + `.cpp`
  - `tests/test_evolution_budget_cap.cpp`
  - `docs/adr/adr-0068-event-emission-contract.md`
- [ ] 5.2 commit message:
  ```
  feat(budget): EvolutionBudgetCap — 进化周期预算上限 (N1 修复)

  Oracle 评审 (session ses_facbd3ffbffeUjlJgZsgMWFiM4) 发现 N1 Blocker:
  进化是正反馈循环 (MCTS×IEvaluator×GEPA×嵌套 MCTS 全是 LLM 调用),
  ExecutionBudget 只管单次执行, 无进化周期预算上限 — 第一次自进化就可能烧光预算。

  ExecutionBudget 新增 max_evolution_llm_calls (-1 默认无限制, 零回归) +
  evolution_llm_calls_used (CAS 原子) + try_consume_evolution_llm_call /
  evolution_budget_exceeded / reset_evolution_cycle 3 方法 (与现有
  try_consume_* 同构)。BudgetController 委托 + begin/end_evolution_cycle
  周期边界。3 个 budget.evolution_cycle.* 事件 (start/end/exceeded)。

  不变量: 默认无限制零回归 / 进化预算与总预算独立 / fail-closed /
  contract 零修改 / CAS 线程安全。5 新测试全 pass。

  Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)
  Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>
  ```

## 6. 后续追踪 (接入方, 不在本 change)

- GEPALoop reflect_and_commit 入口接入 try_consume_evolution_llm_call
- MCTSWorkflowSearch search 入口接入
- T5 EvolutionReadinessGate 预算预留检查 (依赖 ADR-0086 ship)

## 7. 工时估算

| Phase | 估时 |
|-------|------|
| budget.h + budget_controller + 5 tests | 0.15 sprint |
| 事件注册 + ship gate | 0.1 sprint |
| **总计** | **0.25 sprint** |
