# EvolutionBudgetCap — 进化周期预算上限 (N1 修复)

> **Oracle 判定**: 🟢 Go (2026-08-31, session ses_facbd3ffbffeUjlJgZsgMWFiM4) — N1 修复路径对症,与现有 ExecutionBudget 同构,零回归保证合理;commit `06ddd13` 已修 Oracle W3 警告(spec.md 路径错误 + IBudgetController 4 新方法接口扩展 + 移动构造测试 + ADR-0068 v1.9+ 归口); 遗留 N1.2 并发进化覆盖空白 (设计文档中说明 per-cycle 隔离要求即可); ADR-0086 信用分配 ship 后 ConfounderRecord.ResourceChange 可消费 budget 事件
>
> **状态**: 🔍 Proposed (2026-08-31, Oracle 评审发现 N1 缺口: 进化预算失控为 Blocker)
> **关联文档**:
> - `docs/architecture/axis6-chain-workflow-architecture-2026-08.md` §六 N1 (进化预算失控缺口)
> - `docs/architecture/agent-orchestration-architecture-2026-08.md` §十六 (自进化全景)
> - `docs/adr/adr-0084-mutation-governance-contract.md` (MutationGovernance 治理门)
> - `docs/adr/adr-0086-credit-assignment-contract.md` (信用分配, 🔍 Proposed)
> **Oracle 评审**: session `ses_facbd3ffbffeUjlJgZsgMWFiM4` (N1 进化预算失控为 Blocker — 进化是正反馈循环, 第一次自进化就可能烧光预算)
> **最后更新**: 2026-08-31

## Why

### 缺口 (Oracle N1, Blocker 级)

**进化是正反馈循环**: MCTS 搜索 (IEvaluator 评估×N 迭代) → GEPA 反思 (LLM 调用) → 嵌套 MCTS (axis6=Search) → 每次评估/反思/搜索都烧 LLM token → 评估结果可能触发新一轮进化。

**当前防护空白**:
- `ExecutionBudget` (budget.h:15) 管**单次执行**: max_nodes / max_llm_calls / max_duration_sec / max_subgraph_depth
- **无"每进化周期成本上限"**: 一个进化周期 = MCTS 搜索 (100 迭代 × IEvaluator 调用) + GEPA 反思 (≥1 LLM 调用) + 可能的嵌套 MCTS — LLM 成本无界
- **EvolutionReadinessGate** (axis6-chain-workflow §三) 的预算检查只查**余额**, 不**预留** — 进化循环可烧光预算后才被 exceeded() 拦截

**Oracle 原话**: "不修则第一次自进化就可能烧光预算, fail-closed 原则要求先有钱闸"。

### 最小修复 (0.25 sprint)

在 `ExecutionBudget` 增加**进化周期预算预留**字段 + fail-closed 检查, 与现有 max_llm_calls 等字段同构。

## What Changes

### Phase 0 (本 change 立即, 0.25 sprint)

1. **`src/core/types/budget.h`** 修改:
   - `ExecutionBudget` 新增字段:
     - `int max_evolution_llm_calls = -1;` — 单进化周期 LLM 调用上限 (-1 = 无限制, 默认)
     - `mutable std::atomic<int> evolution_llm_calls_used{0};` — 进化周期已消耗
   - 新增方法:
     - `bool try_consume_evolution_llm_call()` — CAS 原子消耗 (与 try_consume_llm_call 同构)
     - `bool evolution_budget_exceeded() const` — 单独检查进化预算
     - `void reset_evolution_cycle()` — 进化周期结束重置计数器

2. **`src/modules/budget/budget_controller.cpp`** 修改:
   - `BudgetController` 新增:
     - `bool try_consume_evolution_llm_call()` — 委托 ExecutionBudget
     - `bool evolution_budget_exceeded() const` — 委托
     - `void begin_evolution_cycle()` / `void end_evolution_cycle()` — 周期边界 (reset + 事件发射)

3. **事件发射**: `budget.evolution_cycle.exceeded` (IInteractionBus, ADR-0068 附录 A 注册):
   - payload: `{evolution_llm_calls_used, max_evolution_llm_calls, agent_id}`
   - 触发: try_consume_evolution_llm_call() 返回 false 时

4. **`tests/test_evolution_budget_cap.cpp`** (新建, ≥5 cases):
   - 默认 -1 无限制 → 永不超限
   - max_evolution_llm_calls=3, 消耗 3 次后第 4 次返回 false
   - evolution_budget_exceeded() 与 exceeded() 独立 (进化超限不触发总超限)
   - reset_evolution_cycle() 重置计数器
   - BudgetController 委托路径工作

### 集成点 (后续 change, 不在本 change 范围)

- GEPALoop.reflect_and_commit 入口: `try_consume_evolution_llm_call()` 前置
- MCTSWorkflowSearch.search 入口: 同上
- EvolutionReadinessGate (T5): 预算预留检查 = `evolution_budget_exceeded()`

### 明确不做

- ❌ 嵌套深度限制 (Oracle 修正: 按预算而非深度设防, G6 并入本方案)
- ❌ 进化调度策略 (何时进化属 EvolutionReadinessGate, T5)
- ❌ 跨进化周期累计预算 (V2, 需持久化)
- ❌ LLM token 计费精度调整 (复用现有 cost_tracker)

## 不变量

- **不变量 1**: 默认 `max_evolution_llm_calls = -1` (无限制) — v1.0 行为完全不变, 零回归
- **不变量 2**: 进化预算与总预算 (max_llm_calls) 独立 — 进化超限不触发 exceeded(), 反之亦然
- **不变量 3**: fail-closed — try_consume 失败 → 调用方必须放弃该次 LLM 调用
- **不变量 4**: 无 contract 头文件修改 (`include/agenticdsl/contract/`)
- **不变量 5**: CAS 原子操作与现有 try_consume_* 同构 (线程安全)

## 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| **R1 默认值误改** | 默认非 -1 导致现有行为变化 | 不变量 1 + 测试 case 1 锁定默认值 |
| **R2 进化预算与总预算混淆** | 两个计数器混淆导致错误拦截 | 不变量 2 + 独立方法名 (evolution_ 前缀) + 测试 case 3 |
| **R3 调用方忘记前置检查** | 进化路径未接入 try_consume | 本 change 仅提供基础设施; 接入属 GEPA/MCTS change (tasks §后续追踪) |
| **R4 周期边界泄漏** | 忘记 end_evolution_cycle → 计数器累积 | begin/end 配对文档 + BudgetController RAII guard (V2) |
