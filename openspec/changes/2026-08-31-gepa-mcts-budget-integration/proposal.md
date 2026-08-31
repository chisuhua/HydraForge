# GEPA/MCTS 进化预算接入 (Evolution Budget Integration)

> **Oracle 判定**: 🟡 Conditional-Go (2026-08-31, session ses_facbd3ffbffeUjlJgZsgMWFiM4) — N1 接入方完整(GEPA `llm_->generate()` gepa_loop.cpp:92 + MCTS `evaluator_->evaluate()` mcts_workflow_search.cpp:263); 强依赖 T3 evolution-budget-cap (commit `06ddd13` 已修 W3 IBudgetController 4 新方法 + ADR-0068 v2.0+ 归口); 遗留覆盖空白: MCTS V1 mock evaluator 零 LLM 调用(V2 real evaluator 升级路径需文档化) + SkillCompiler.compile() 间接调用未覆盖 + PlanExecuteLoop 独立 LLM 调用点未覆盖
>
> **状态**: 🔍 Proposed (2026-08-31, Oracle N1 缺口闭环)
> **关联文档**:
> - `docs/architecture/axis6-chain-workflow-architecture-2026-08.md` §六 N1 (进化预算失控, 🔴 Blocker)
> - `openspec/changes/2026-08-31-evolution-budget-cap/` (T3, 预算基础设施 ✅ 已审)
> - `include/agenticdsl/cognitive/gepa_loop.h` / `src/modules/cognitive/gepa_loop.cpp` (GEPA 实装)
> - `include/agenticdsl/cognitive/mcts_workflow_search.h` / `src/modules/cognitive/mcts_workflow_search.cpp` (MCTS 实装)
> **Oracle 评审**: session `ses_facbd3ffbffeUjlJgZsgMWFiM4` (N1 缺口: 进化是正反馈循环, 预算闸需接入方否则 T3 是死代码)
> **最后更新**: 2026-08-31

## Why

### N1 缺口闭环 (Oracle 关键判断)

```
T3 evolution-budget-cap 提供了预算基础设施 (ExecutionBudget 字段 + IBudgetController 接口)
  ↓ 但
GEPALoop / MCTSWorkflowSearch 完全没有调用 try_consume_evolution_llm_call()
  ↓ 所以
T3 的预算闸是"无人过闸"的闸 — 重演 MCTSWorkflowSearch 当前"零消费者"问题的教训
  ↓ 本 change 修复
GEPA/MCTS 接入: 每次 LLM 调用前 try_consume_evolution_llm_call(), 超限即终止进化
```

**Oracle 明确警告**: "N1 接入方应升级为本批次 V1 必备（否则 T3 是死代码，违反本项目'零消费者'自己的教训）"。

### 代码实装验证 (接入点精确定位)

**GEPALoop** (`src/modules/cognitive/gepa_loop.cpp`):
- `reflect_and_commit(failed_trace)` 是入口 (line 54)
- 主循环 `for (iteration < config_.max_iterations)` (line 71)
- **LLM 调用点**: `llm_->generate(request, ...)` (line 92) — 每次迭代 1 次 LLM
- **接入点**: 每次迭代的 `llm_->generate()` 前 `try_consume_evolution_llm_call()`

**MCTSWorkflowSearch** (`src/modules/cognitive/mcts_workflow_search.cpp`):
- `search(spec)` 是入口 (line 187)
- 主循环 `for (iteration < config_.max_iterations)` (line 216)
- **评估点**: `evaluator_->evaluate()` 在每次迭代 (V1 mock, 未来真实 LLM) — 每次迭代 1 次评估
- **接入点**: 每次迭代 `evaluator_->evaluate()` 前 `try_consume_evolution_llm_call()`

### 前置依赖

| 依赖 | 状态 |
|------|------|
| `openspec/changes/2026-08-31-evolution-budget-cap/` (T3) | 🔴 **关键前置** — 本 change 消费 `IBudgetController::try_consume_evolution_llm_call()`, T3 必须先 ship |
| GEPALoop (T19) | ✅ ship |
| MCTSWorkflowSearch (T20) | ✅ ship |

## What Changes

### Phase 0 (本 change, ~0.5 sprint)

1. **GEPALoop 构造签名扩展** (`include/agenticdsl/cognitive/gepa_loop.h`):
   - 新增 `std::shared_ptr<IBudgetController> budget_controller` 参数 (默认 nullptr, 向后兼容)
   - 新增 ctor 重载 (含 budget_controller); 原 ctor 委托
   - 新增 `bool evolution_budget_exceeded() const` helper (委托 budget_controller_)

2. **GEPALoop 主循环接入** (`src/modules/cognitive/gepa_loop.cpp`):
   ```cpp
   for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
     // N1 接入: 预算检查 (nullptr → 跳过, 行为不变)
     if (budget_controller_ && !budget_controller_->try_consume_evolution_llm_call()) {
       result.failure_mode = "evolution_budget_exceeded";
       emit_event(bus_, "gepa.reflection.failed",
                  {{"reflection_id", make_reflection_id(failed_trace, iteration)},
                   {"reason", result.failure_mode}});
       break;  // 终止整个反思循环
     }
     // ... 原有逻辑 (propose → llm_.generate → evaluate → commit)
   }
   ```

3. **MCTSWorkflowSearch 构造签名扩展** (`include/agenticdsl/cognitive/mcts_workflow_search.h`):
   - 新增 `std::shared_ptr<IBudgetController> budget_controller` 参数 (默认 nullptr, 向后兼容)
   - 新增 ctor 重载; 原 ctor 委托

4. **MCTSWorkflowSearch 主循环接入** (`src/modules/cognitive/mcts_workflow_search.cpp`):
   ```cpp
   for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
     // N1 接入: 预算检查 (nullptr → 跳过, 行为不变)
     if (budget_controller_ && !budget_controller_->try_consume_evolution_llm_call()) {
       result.failure_mode = "evolution_budget_exceeded";
       emit_event(bus_, "mcts.budget_exceeded",
                  {{"task_id", spec.task_id}, {"iteration", iteration}});
       break;  // 终止搜索
     }
     // ... 原有 UCB1 选择/扩展/模拟/反向传播
   }
   ```

5. **测试** (`tests/test_gepa_mcts_budget_integration.cpp`, 新建 ≥5 cases):
   - GEPA 无 budget_controller (nullptr) → 行为不变 (零回归)
   - GEPA 预算超限 → 提前 break + failure_mode="evolution_budget_exceeded"
   - GEPA 预算充足 → 正常完成 (max_iterations 内)
   - MCTS 无 budget_controller (nullptr) → 行为不变 (零回归)
   - MCTS 预算超限 → 提前 break + failure_mode="evolution_budget_exceeded"

### 明确不做

- ❌ 不修改 T3 的 ExecutionBudget / IBudgetController (预算基础设施不动)
- ❌ 不引入真实 LLM 评估 (MCTS V1 mock evaluator 不变, 接入点仍存在)
- ❌ 不实现 T5 EvolutionReadinessGate (独立 change)
- ❌ 不实现连续监控触发 (G3, 独立 change)

## 不变量

- **不变量 1**: `budget_controller_ == nullptr` → 行为 100% 等同 T19/T20 ship 版本 (零回归)
- **不变量 2**: 预算超限 → 提前终止进化循环, **不抛异常** (fail-closed + graceful)
- **不变量 3**: 超限时 failure_mode 明确为 "evolution_budget_exceeded" (可观测)
- **不变量 4**: 5 contract 头文件零修改 (`include/agenticdsl/contract/`)
- **不变量 5**: GEPA 17 cases / MCTS 65 assertions 基线 0 回归

## 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| **R1 预算检查误触发** | nullptr 调用崩溃 | 不变量 1: `budget_controller_ &&` 短路检查 |
| **R2 超限终止丢失进度** | 中途 break 丢已生成候选 | 预期行为 (fail-closed), GEPA 已 commit 的候选保留 (governor 已 emit mutation.committed) |
| **R3 T3 未 ship 依赖** | 本 change 无法编译 | 前置依赖显式声明, T3 先 ship |
| **R4 事件主题未注册** | mcts.budget_exceeded 幻影主题 | ADR-0068 Appendix A 注册 (v2.0+, 随本 change ship) |

## Out of Scope

- ❌ T5 EvolutionReadinessGate (进化触发门禁, 独立)
- ❌ G3 连续监控触发 (独立)
- ❌ G6 联合深度上限 (并入预算方案, 本 change 的预算即深度防护的替代)
- ❌ N2 in-flight 一致性 / N3 并发互斥 (后续 change)
