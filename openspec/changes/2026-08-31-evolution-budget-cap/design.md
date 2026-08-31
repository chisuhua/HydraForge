# Design — EvolutionBudgetCap

## Context

Oracle 评审 (session `ses_facbd3ffbffeUjlJgZsgMWFiM4`) 发现 N1 缺口 (Blocker): 进化是正反馈循环 (MCTS×IEvaluator×GEPA×嵌套 MCTS 全是 LLM 调用), 但 ExecutionBudget 只管单次执行, 无进化周期预算上限。第一次自进化就可能烧光预算。

本 change 在 `ExecutionBudget` 增加进化周期预算预留字段, 与现有 max_llm_calls 同构 (CAS 原子 + fail-closed)。

## 决策

### 决策 1 — ExecutionBudget 新增 2 字段 + 3 方法

```cpp
struct ExecutionBudget {
    // ... 现有字段不变 ...
    int max_evolution_llm_calls = -1;                 // 新增: 进化周期 LLM 调用上限
    mutable std::atomic<int> evolution_llm_calls_used{0};  // 新增: 已消耗

    // 新增方法 (与 try_consume_llm_call 同构):
    bool try_consume_evolution_llm_call() {
        int current = evolution_llm_calls_used.load();
        if (max_evolution_llm_calls >= 0 && current >= max_evolution_llm_calls) return false;
        int expected = current;
        while (!evolution_llm_calls_used.compare_exchange_weak(expected, expected + 1)) {
            if (max_evolution_llm_calls >= 0 && expected >= max_evolution_llm_calls) return false;
        }
        return true;
    }

    bool evolution_budget_exceeded() const {
        return max_evolution_llm_calls >= 0 &&
               evolution_llm_calls_used.load() > max_evolution_llm_calls;
    }

    void reset_evolution_cycle() {
        evolution_llm_calls_used.store(0);
    }
};
```

**移动构造/赋值同步更新**: 新增字段加入重置列表 (与现有 nodes_used 等同模式)。

### 决策 2 — BudgetController 委托 + 周期边界

```cpp
class BudgetController {
 public:
    // 新增委托:
    bool try_consume_evolution_llm_call();
    bool evolution_budget_exceeded() const;

    // 周期边界 (事件发射):
    void begin_evolution_cycle();   // reset + emit budget.evolution_cycle.start
    void end_evolution_cycle();     // emit budget.evolution_cycle.end {used, max}
};
```

### 决策 3 — 事件发射 (budget.evolution_cycle.*)

3 个新主题 (IInteractionBus, ADR-0068 Appendix A 注册):
- `budget.evolution_cycle.start` — 周期开始
- `budget.evolution_cycle.end` — 周期结束 `{used, max, exceeded}`
- `budget.evolution_cycle.exceeded` — 超限 `{used, max, agent_id}`

### 决策 4 — 独立性 (与总预算隔离)

`evolution_budget_exceeded()` 与 `exceeded()` **完全独立**:
- 进化超限 → 仅影响进化路径 (GEPA/MCTS 调用方负责放弃)
- 总执行超限 → 走现有 exceeded() 路径 (预算耗尽终止)
- 两者不交叉 (不变量 2)

### 决策 5 — 默认无限制 (零回归)

`max_evolution_llm_calls = -1` 默认 → 永不超限 → v1.0 行为完全不变 (不变量 1)。

## 接口

### 修改文件

- `src/core/types/budget.h` (2 字段 + 3 方法)
- `src/modules/budget/budget_controller.h` + `.cpp` (委托 + 周期边界)
- `tests/test_evolution_budget_cap.cpp` (新建, ≥5 cases)

### 零修改

- `include/agenticdsl/contract/` (不变量 4)
- GEPALoop / MCTSWorkflowSearch (本 change 仅提供基础设施, 接入属后续 change)

## 反例 (明确不做)

| 反例 | 拒绝理由 |
|------|----------|
| 按嵌套深度限制 (G6 180 层方案) | Oracle: 按预算而非深度设防, nested_iterations 是迭代数非深度 |
| 进化预算与总预算共享计数器 | 不变量 2: 独立性, 避免混淆 |
| 跨进化周期累计 (持久化) | V2 范围, 需持久化基础设施 |
| 修改 GEPALoop/MCTSWorkflowSearch 接入 try_consume | 本 change 仅基础设施; 接入属对应组件的 change |
| 默认 max_evolution_llm_calls 设为非 -1 | 不变量 1: 零回归 |

## 跨 change 依赖

### 前置 (已 ship)
- ✅ ExecutionBudget (budget.h)
- ✅ BudgetController (budget_controller.{h,cpp})
- ✅ IInteractionBus (事件通道)

### 后续 (接入方, 不在本 change)
- GEPALoop reflect_and_commit 入口接入 try_consume_evolution_llm_call
- MCTSWorkflowSearch search 入口接入
- T5 EvolutionReadinessGate 预算预留检查

### 并行
- T1 workflow-materializer-v1 (独立)
- T4 signature-validation-real-impl (独立)

## ADR 兼容性

| ADR | 兼容性 |
|-----|--------|
| ADR-0019 §6 (ExecutionBudget) | ✅ 增量字段, 同构 CAS 模式 |
| ADR-0084 (MutationGovernance) | ✅ 预算门是治理前置, 不冲突 |
| ADR-0086 (信用分配) | ✅ 预算数据可作为 ConfounderRecord.ResourceChange 来源 |
| ADR-0068 (Event Emission) | 🟡 需 Appendix A 注册 3 个 budget.evolution_cycle.* 主题 |
