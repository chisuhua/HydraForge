# Design — GEPA/MCTS 进化预算接入

## Context

T3 evolution-budget-cap 提供了预算基础设施 (ExecutionBudget 字段 + IBudgetController 接口 + begin/end_evolution_cycle)，但 GEPALoop/MCTSWorkflowSearch 均未调用 `try_consume_evolution_llm_call()`。Oracle N1 缺口 (Blocker) 判定: 预算闸无人过闸 = 死代码。本 change 将预算检查接入两个进化引擎的 LLM 调用点。

**实装验证**:
- GEPALoop: `reflect_and_commit()` 入口 line 54, 主循环 line 71, `llm_->generate()` line 92
- MCTSWorkflowSearch: `search()` 入口 line 187, 主循环 line 216, `evaluator_->evaluate()` 迭代内

## 决策

### 决策 1 — 注入方式: ctor 注入 shared_ptr<IBudgetController> (nullptr 兼容)

**理由**: 与 GEPALoop (evaluator_/governor_/llm_) 和 MCTSWorkflowSearch (evaluator_/governor_/regression_gate_/bus_) 现有依赖注入风格一致; nullptr 默认值保证 T19/T20 调用方零修改 (不变量 1)。

```cpp
// GEPALoop (gepa_loop.h)
// 原 ctor (保留, 委托新 ctor):
GEPALoop(std::shared_ptr<IEvaluator> evaluator,
         std::shared_ptr<IMutationGovernor> governor,
         std::shared_ptr<ILLMProvider> llm);
// 新 ctor (含 budget_controller):
GEPALoop(std::shared_ptr<IEvaluator> evaluator,
         std::shared_ptr<IMutationGovernor> governor,
         std::shared_ptr<ILLMProvider> llm,
         Config config,
         std::shared_ptr<IInteractionBus> bus,
         std::shared_ptr<IBudgetController> budget_controller);  // 新增: 默认 nullptr

// MCTSWorkflowSearch (mcts_workflow_search.h)
// 新 ctor (含 budget_controller): (evaluator, governor, regression_gate, SearchConfig, bus, budget_controller)
```

**替代方案**: 不做 (预算闸不接) → 违反 N1 闭环, 拒绝。

### 决策 2 — 接入位置: 主循环迭代顶, 每次 LLM/评估前

```cpp
// GEPA 主循环 (gepa_loop.cpp, line 71 for 循环内, 于 propose 之前):
for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
  if (budget_controller_ && !budget_controller_->try_consume_evolution_llm_call()) {
    result.failure_mode = "evolution_budget_exceeded";
    emit_event(bus_, "gepa.reflection.failed",
               {{"reflection_id", make_reflection_id(failed_trace, iteration)},
                {"reason", result.failure_mode}});
    break;  // 终止反思循环
  }
  // ... 原有逻辑 (emit start → propose → llm.generate → evaluate → commit)
}

// MCTS 主循环 (mcts_workflow_search.cpp, line 216 for 循环内):
for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
  if (budget_controller_ && !budget_controller_->try_consume_evolution_llm_call()) {
    result.failure_mode = "evolution_budget_exceeded";
    emit_event(bus_, "mcts.budget_exceeded",
               {{"task_id", spec.task_id}, {"iteration", iteration}});
    break;  // 终止搜索
  }
  // ... 原有 UCB1 逻辑
}
```

**位置理由**: 进化 LLM 调用成本集中在此循环 (GEPA 每次迭代 1 次 generate; MCTS 每次迭代 1 次 evaluate), 循环顶检查最粗粒且不遗漏; 超限 break 保留已提交候选 (governor 已 emit mutation.committed)。

### 决策 3 — 超限语义: fail-closed + graceful break

| 项 | 行为 |
|----|------|
| 超限检测 | `try_consume_evolution_llm_call()` 返回 false (CAS 已满) |
| 动作 | `break` 终止进化循环, **不抛异常** (不变量 2) |
| 结果标记 | `failure_mode = "evolution_budget_exceeded"` (GEPA ReflectionResult / MCTS SearchResult) |
| 事件 | `gepa.reflection.failed` + `mcts.budget_exceeded` (可观测) |
| 已提交候选 | 保留 (governor 已 emit mutation.committed, 回滚由 governor 负责) |

**Graceful 理由**: 进化预算超限是资源约束非逻辑错误, 不应 propagate exception; break 让调用方 (T5 gate / T6 driver) 看到 failure_mode 后决定重试/放弃。

### 决策 4 — 事件主题: 2 个新增 (ADR-0068 Appendix A **v2.0+**)

- `mcts.budget_exceeded` — payload `{task_id, iteration}` (MCTS 搜索预算超限)
- (GEPA 复用现有 `gepa.reflection.failed` 主题, reason 字段区分 — 不新增)

**W4 归口**: Axis6 owns v1.8, T1/T2/T3 用 v1.9+, 本 change 用 **v2.0+** (若 T4 用 v1.9 则本用 v2.0; 版本顺序: axis6 v1.8 → T1/T2/T3 v1.9 → 本 v2.0)。

### 决策 5 — 零实装修改 (specialist 本体不改逻辑)

本 change 只加预算检查, **不修改**:
- GEPALoop 反思逻辑 / MCTS UCB1 逻辑 (保持 T19/T20 行为)
- evaluator_/governor_/regression_gate_ 调用链
- 只新增 budget_controller_ 成员 + 循环顶 2 行检查

## 接口

### 修改文件

- `include/agenticdsl/cognitive/gepa_loop.h` (ctor 重载 + budget_controller_ 成员)
- `src/modules/cognitive/gepa_loop.cpp` (主循环 2 行检查)
- `include/agenticdsl/cognitive/mcts_workflow_search.h` (ctor 重载 + budget_controller_ 成员)
- `src/modules/cognitive/mcts_workflow_search.cpp` (主循环 2 行检查)
- `tests/test_gepa_mcts_budget_integration.cpp` (新建, ≥5 cases)

### 零修改

- `include/agenticdsl/contract/` (不变量 4)
- `src/core/types/budget.h` / `src/modules/budget/budget_controller.h` (T3 已提供, 不动)
- GEPA 17 cases / MCTS 65 assertions 基线 (不变量 5)

### 依赖注入链

```
调用方 (T5 gate / T6 driver / CognitiveWorker)
  └─ 构造 BudgetController(initial_budget, bus) (T3, 含 budget_controller)
      └─ 构造 GEPALoop(e, g, llm, config, bus, budget_controller)
           └─ reflect_and_commit() → 每次迭代 try_consume_evolution_llm_call()
      └─ 构造 MCTSWorkflowSearch(e, g, rg, config, bus, budget_controller)
           └─ search(spec) → 每次迭代 try_consume_evolution_llm_call()
```

## 反例 (明确不做)

| 反例 | 拒绝理由 |
|------|----------|
| 不做预算接入 (T3 死代码) | Oracle N1 Blocker: 违反"零消费者"教训 |
| 超限抛异常 | 不变量 2: 资源约束非逻辑错误, graceful break |
| 只接 GEPA 不接 MCTS | 不对称, MCTS 搜索更贵 (嵌套), 必须都接 |
| 修改 T3 的 IBudgetController | T3 已提供, 本 change 只消费 |
| 在 propose/commit 内检查预算 | 那是 governor 的职责 (治理门), 本 change 管 LLM 调用成本 |
| 新增 budget 检查到 DomainWorkerPool | 进化循环才需要, 普通工具调用走总预算 | 

## 跨 change 依赖

### 前置 (必须 ship)
- 🔴 `openspec/changes/2026-08-31-evolution-budget-cap/` (T3) — 提供 IBudgetController::try_consume_evolution_llm_call + begin/end_evolution_cycle
- ✅ GEPALoop (T19)
- ✅ MCTSWorkflowSearch (T20)

### 后续 (不在本 change)
- T5 `evolution-readiness-gate-v1` — 消费 budget_controller (begin/end_evolution_cycle) 做预算预留
- T6 `chain-evolution-driver-v1` — 串联 GEPA/MCTS + budget
- N2 in-flight 一致性 / N3 并发互斥 — 后续 change

### 并行
- T1 workflow-materializer-v1 (独立)
- T2 cognitive-specialists-as-tools (独立)
- T4 signature-validation-real-impl (独立)

## ADR 兼容性

| ADR | 兼容性 |
|-----|--------|
| ADR-0019 §6 (ExecutionBudget) | ✅ 消费 T3 提供的接口, 不修改 |
| ADR-0084 (MutationGovernance) | ✅ 预算检查独立于 governor 治理链 (propose/commit 不受影响) |
| ADR-0086 (信用分配) | ✅ 预算数据可作为 ConfounderRecord.ResourceChange 来源 |
| ADR-0068 (Event Emission) | 🟡 新增 1 个 `mcts.budget_exceeded` 主题 (注册 v2.0+) |
| ADR-0083 (IEvaluator) | ✅ MCTS evaluator_ 调用链不变, 仅前置预算检查 |