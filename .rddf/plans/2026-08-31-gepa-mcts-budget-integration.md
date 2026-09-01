# gepa-mcts-budget-integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实施 `GEPALoop` 和 `MCTSWorkflowSearch` 的进化预算接入 — 每次 LLM 调用前调用 `try_consume_evolution_llm_call()`, 超限 graceful break (而非 throw)。Oracle 评审 🟡 Conditional-Go (`session ses_facbd3ffbffeUjlJgZsgMWFiM4`), commit `5a566e7` 是最新 commit 体现设计。强依赖 T3 evolution-budget-cap (N1 修复) 必须先 ship。

**Architecture:** 在 GEPALoop ctor 注入 `shared_ptr<IBudgetController>` (默认 nullptr 零回归); 主循环 `for (int iteration = 0; iteration < config_.max_iterations; ++iteration)` 顶插入 2 行预算检查 + `if (!budget_controller_->try_consume_evolution_llm_call()) { break; }`。MCTSWorkflowSearch 同样在 `search()` 主循环顶。fail-closed graceful break (failure_mode='evolution_budget_exceeded')。emit 2 事件: MCTS 新增 `mcts.budget_exceeded`, GEPA 复用 `gepa.reflection.failed`。零实装修改 (`06ddd13` W4 ADR-0068 v2.0+ 归口已 ship)。

**Tech Stack:** C++20, std::shared_ptr<IBudgetController>, std::stop_token, IInteractionBus, ADR-0001 ILLMProvider 接口不变 (T3 已在 BudgetController 层加方法, GEPA/MCTS 仅消费)。

---

## Scope Adjustments vs proposal

**Adopted scope** (Oracle Conditional-Go):
- `GEPALoop` ctor 重载: 加 `shared_ptr<IBudgetController>` 参数 (nullptr 零回归兼容 5 case ctest baseline)
- `GEPALoop::reflect_and_commit` 主循环顶 (`gepa_loop.cpp:71`) 加预算检查 + break
- `MCTSWorkflowSearch` ctor 重载: 加 `shared_ptr<IBudgetController>` 参数
- `MCTSWorkflowSearch::search` 主循环顶 (`mcts_workflow_search.cpp:263`) 加预算检查 + break
- emit MCTS 新事件 `mcts.budget_exceeded` (ADR-0068 v2.0+ 注册)
- emit GEPA 复用 `gepa.reflection.failed` (ADR-0068 已有)
- ≥5 测试 case: ctor nullptr / GEPA 超限 break / MCTS 超限 break / GEPA 事件触发 / MCTS 事件触发

**Deferred to follow-up**:
- MCTS V1 mock evaluator 真实 LLM 接入后的覆盖验证 (当前 mock 不消耗预算, V2 real evaluator 升级路径需文档化)
- PlanExecuteLoop plan_phase LLM 调用预算接入 (非本 change scope, V2 follow-up)
- SkillCompiler.compile() 内部 LLM 调用预算接入 (当前 compile 不调 LLM, V2 follow-up)

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `include/agenticdsl/cognitive/gepa_loop.h` | GEPALoop ctor 重载 + budget_controller_ 成员 |
| `src/modules/cognitive/gepa_loop.cpp` | 主循环顶预算检查 (gepa_loop.cpp:71) + emit `gepa.reflection.failed` |
| `include/agenticdsl/cognitive/mcts_workflow_search.h` | MCTSWorkflowSearch ctor 重载 + budget_controller_ 成员 |
| `src/modules/cognitive/mcts_workflow_search.cpp` | `search()` 主循环顶预算检查 (line 263) + emit `mcts.budget_exceeded` |
| `docs/adr/adr-0068-event-emission-contract.md` Appendix A | 新增 `mcts.budget_exceeded` v2.0+ 主题行 |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_gepa_mcts_budget_integration.cpp` (new, ≥5 cases) | 5 个核心 case (ctor nullptr + GEPA break + MCTS break + 2 事件) |

---

## TDD 5-Step Execution

### Step 1: Write failing test

**File**: `tests/test_gepa_mcts_budget_integration.cpp` (new, ~80 LOC)

```cpp
#include <agenticdsl/cognitive/gepa_loop.h>
#include <agenticdsl/cognitive/mcts_workflow_search.h>
#include <agenticdsl/contract/ibudget_controller.h>
#include <agenticdsl/contract/ievaluator.h>
#include <agenticdsl/llm/tracing_decorator.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace {
class MockBudget : public IBudgetController {
public:
    int max_evo = 3;
    int used = 0;
    int evo_calls = 0;  // 真实 counter
    bool try_consume_evolution_llm_call() override {
        if (used >= max_evo) return false;
        used++; evo_calls++; return true;
    }
    bool evolution_budget_exceeded() const override { return used >= max_evo; }
    void begin_evolution_cycle(const std::string&) override {}
    void end_evolution_cycle(const std::string&, bool) override {}
    void reset_evolution_cycle_counter() override { used = 0; }
    void set_bus(IInteractionBus*) override {}
    IInteractionBus* get_bus() const override { return nullptr; }
    // 其他 11 方法返回默认 (略)
};
}

TEST_CASE("GEPA ctor nullptr budget keeps baseline behavior (零回归)", "[gepa][budget]") {
    auto llm = std::make_shared<MockLLMProvider>();
    GEPALoop loop(nullptr, llm);  // budget = nullptr, ctest 5 case baseline 兼容
    // 现有 GEPALoop 行为不变
}

TEST_CASE("GEPA budget exhausted breaks loop gracefully (failure_mode=evolution_budget_exceeded)", "[gepa][budget]") {
    auto llm = std::make_shared<MockLLMProvider>();
    auto budget = std::make_shared<MockBudget>();
    budget->max_evo = 2;  // 2 次后超限
    GEPALoop loop(budget, llm);
    // reflect_and_commit 应在第 3 次迭代 break (failure_mode='evolution_budget_exceeded')
    ExecutionTrace trace;  // mock failed trace
    auto result = loop.reflect_and_commit(trace);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.failure_mode == "evolution_budget_exceeded");
    REQUIRE(budget->used <= 2);  // 不超过 budget
}

TEST_CASE("MCTS budget exhausted breaks search gracefully", "[mcts][budget]") {
    auto evaluator = std::make_shared<MockEvaluator>();
    auto governor = std::make_shared<MockMutationGovernor>();
    auto regression = std::make_shared<MockRegressionGate>();
    auto budget = std::make_shared<MockBudget>();
    budget->max_evo = 5;
    MCTSWorkflowSearch searcher(evaluator, governor, regression, SearchConfig{}, nullptr, budget);
    // search() 在第 6 次 evaluator 调用时 break
    TaskSpec spec;  // mock spec
    auto result = searcher.search(spec);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.failure_mode == "evolution_budget_exceeded");
    REQUIRE(budget->used <= 5);
}

TEST_CASE("GEPA emits gepa.reflection.failed on budget exhausted", "[gepa][bus][budget]") {
    auto bus = std::make_shared<InMemoryBus>();
    auto llm = std::make_shared<MockLLMProvider>();
    auto budget = std::make_shared<MockBudget>();
    budget->max_evo = 1;
    GEPALoop loop(budget, llm);
    loop.set_bus(bus.get());
    ExecutionTrace trace;
    loop.reflect_and_commit(trace);
    // 验证 bus 收到 gepa.reflection.failed 事件
    REQUIRE(bus->events.size() >= 1);
    auto& last = bus->events.back();
    REQUIRE(last.topic == "gepa.reflection.failed");
}

TEST_CASE("MCTS emits mcts.budget_exceeded on budget exhausted", "[mcts][bus][budget]") {
    auto bus = std::make_shared<InMemoryBus>();
    auto evaluator = std::make_shared<MockEvaluator>();
    auto governor = std::make_shared<MockMutationGovernor>();
    auto regression = std::make_shared<MockRegressionGate>();
    auto budget = std::make_shared<MockBudget>();
    budget->max_evo = 3;
    MCTSWorkflowSearch searcher(evaluator, governor, regression, SearchConfig{}, bus.get(), budget);
    TaskSpec spec;
    searcher.search(spec);
    auto& last = bus->events.back();
    REQUIRE(last.topic == "mcts.budget_exceeded");
}
```

**Verification**:
```bash
cmake --build build --target test_gepa_mcts_budget_integration
ctest -R "gepa_mcts_budget_integration" --output-on-failure
# Expected: FAIL (T6 尚未接入)
```

---

### Step 2: GEPA ctor overload + budget check

**File**: `include/agenticdsl/cognitive/gepa_loop.h`

新增 ctor 重载 (commit `06ddd13` 已定):
```cpp
class GEPALoop {
public:
    // 现有 ctor: backward compat
    GEPALoop(std::shared_ptr<IEvaluator> evaluator,
             std::shared_ptr<IMutationGovernor> governor,
             std::shared_ptr<ILLMProvider> llm);
    // 现有 ctor with bus + config
    GEPALoop(std::shared_ptr<IEvaluator> evaluator,
             std::shared_ptr<IMutationGovernor> governor,
             std::shared_ptr<ILLMProvider> llm,
             Config config,
             std::shared_ptr<IInteractionBus> bus = nullptr);
    // 新增 ctor overload: T6 budget 接入
    GEPALoop(std::shared_ptr<IEvaluator> evaluator,
             std::shared_ptr<IMutationGovernor> governor,
             std::shared_ptr<IBudgetController> budget,
             std::shared_ptr<ILLMProvider> llm,
             Config config = Config{},
             std::shared_ptr<IInteractionBus> bus = nullptr);
    
    void set_bus(std::shared_ptr<IInteractionBus> bus) { bus_ = bus; }  // commit 06ddd13 已加
    ReflectionResult reflect_and_commit(const ExecutionTrace& failed_trace);
    
private:
    std::shared_ptr<IBudgetController> budget_controller_;  // 新增成员
    std::shared_ptr<IInteractionBus> bus_;
    std::shared_ptr<IEvaluator> evaluator_;
    std::shared_ptr<IMutationGovernor> governor_;
    std::shared_ptr<ILLMProvider> llm_;
    Config config_;
};
```

**File**: `src/modules/cognitive/gepa_loop.cpp`

`reflect_and_commit` 主循环顶 (line 71 附近) 加 2 行 (commit `5a566e7` 是 commit template):
```cpp
ReflectionResult GEPALoop::reflect_and_commit(const ExecutionTrace& failed_trace) {
    ReflectionResult result;
    result.success = false;
    result.failure_mode = "unknown";
    try {
        for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
            // N1 budget 闸 (T6 接入)
            if (budget_controller_ && !budget_controller_->try_consume_evolution_llm_call()) {
                result.failure_mode = "evolution_budget_exceeded";
                if (bus_) {
                    bus_->emit(BusEvent{"gepa.reflection.failed", {
                        {"reason", "evolution_budget_exceeded"},
                        {"iterations_used", iteration}
                    }});
                }
                LOG_WARN("GEPALoop: evolution budget exhausted at iteration " << iteration);
                break;  // graceful break
            }
            // 现有 LLM call + mutation logic
            // ...
        }
    } catch (const std::exception& e) {
        // 现有 catch
    }
    return result;
}
```

---

### Step 3: MCTS ctor overload + budget check

**File**: `include/agenticdsl/cognitive/mcts_workflow_search.h`

新增 ctor 重载 (commit `06ddd13` 已定):
```cpp
class MCTSWorkflowSearch {
public:
    // 现有 ctor
    MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                       std::shared_ptr<IMutationGovernor> governor,
                       std::shared_ptr<IRegressionGate> regression_gate,
                       SearchConfig config,
                       std::shared_ptr<IInteractionBus> bus = nullptr);
    // 新增 ctor overload: T6 budget 接入
    MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                       std::shared_ptr<IMutationGovernor> governor,
                       std::shared_ptr<IRegressionGate> regression_gate,
                       SearchConfig config,
                       std::shared_ptr<IInteractionBus> bus,
                       std::shared_ptr<IBudgetController> budget);
    
    SearchResult search(const TaskSpec& spec);
    
private:
    std::shared_ptr<IBudgetController> budget_controller_;  // 新增
};
```

**File**: `src/modules/cognitive/mcts_workflow_search.cpp`

`search()` 主循环顶 (line 263 附近, evaluator_->evaluate() 调用前) 加 2 行:
```cpp
SearchResult MCTSWorkflowSearch::search(const TaskSpec& spec) {
    SearchResult result;
    // ... 初始化
    while (/* UCB1 loop */) {
        // N1 budget 闸 (T6 接入)
        if (budget_controller_ && !budget_controller_->try_consume_evolution_llm_call()) {
            result.failure_mode = "evolution_budget_exceeded";
            if (bus_) {
                bus_->emit(BusEvent{"mcts.budget_exceeded", {
                    {"iterations_used", current_iteration},
                    {"reason", "evolution_budget_exceeded"}
                }});
            }
            LOG_WARN("MCTSWorkflowSearch: evolution budget exhausted");
            break;
        }
        // 现有 evaluator_->evaluate() 调用 (line 263)
        auto eval_result = evaluator_->evaluate(...);
        // ...
    }
    return result;
}
```

---

### Step 4: Register `mcts.budget_exceeded` in ADR-0068 Appendix A

**File**: `docs/adr/adr-0068-event-emission-contract.md`

新增 1 行 (commit `06ddd13` W4 已声明归口):
```
| `mcts.budget_exceeded` | 进化周期预算超限 (T6 接入) | MCTSWorkflowSearch.search 预算检查 | ✅ Shipped (T6 2026-08-31) |
```

---

### Step 5: Commit

```bash
git add include/agenticdsl/cognitive/gepa_loop.h \
        src/modules/cognitive/gepa_loop.cpp \
        include/agenticdsl/cognitive/mcts_workflow_search.h \
        src/modules/cognitive/mcts_workflow_search.cpp \
        tests/test_gepa_mcts_budget_integration.cpp \
        tests/CMakeLists.txt \
        docs/adr/adr-0068-event-emission-contract.md
git commit -m "feat(gepa-mcts): 进化预算闸接入 GEPALoop/MCTSWorkflowSearch 主循环 (T6 N1 闭环)"
```

---

## Ship Gate Validation

```bash
# 1. openspec validate (T3 依赖必须已 ship)
openspec validate 2026-08-31-gepa-mcts-budget-integration --strict

# 2. compile + tests
cmake --build build && ctest -R "gepa_mcts_budget_integration" --output-on-failure
# Expected: PASS (5 cases)

# 3. Baseline regression (T3 已 ship, 209 tests)
ctest --output-on-failure  # 209 + 5 = 214

# 4. ADR lint + drift
python3 tools/adr_lint.py  # ✓
python3 tools/docs_drift_audit.py  # 0 DRIFT

# 5. T3 接口验证 (ship 顺序 enforce)
grep -c "try_consume_evolution_llm_call" include/agenticdsl/contract/ibudget_controller.h  # ≥1
grep -c "max_evolution_llm_calls" src/core/types/budget.h  # ≥1
```

---

## Risk Assessment

| 风险 | 缓解 |
|------|------|
| T3 未 ship (接口不存在) | ship 顺序 enforce: T3 先 ship, T6 依赖 T3 接口; ship gate grep T3 字段 |
| ctor 重载破坏现有 ctest 5 case baseline | 现有 ctor 保留, 新 ctor overload; nullptr budget 零回归 |
| MCTS V1 mock evaluator 零 LLM 消耗 (预算闸 dead code) | 文档化 V2 real evaluator 升级路径; Step 4 emit 事件验证预算闸被调用 |
| PlanExecuteLoop plan_phase / SkillCompiler.compile() 间接 LLM 未覆盖 | V2 follow-up: 各自一个 budget-integration 子 change; 当前非 ship-blocker |
| ADR-0068 v2.0+ 主题注册 (commit `06ddd13` 已 ship, 但 W4 行未写入) | Step 4 同步; ship gate grep 验证 |
| 并发进化 budget 共享 (N1.2) | 由 T3 N1.2 follow-up 处理; 当前设计假定 per-Engine 单实例 |
| 强类型 commit `5a566e7` 已 ship, commit `06ddd13` 已修 W4; ship gate 跑完整 5 件套 |

---

## 后续触发条件

| 触发条件 | 后续 OpenSpec change |
|----------|---------------------|
| T6 ship 立即 | T2 mcts-axis6-cognitive-domain 启动 (Axis6 Phase 1 依赖 budget 闸存在) |
| MCTS V2 real evaluator 实装 | MCTS V2 upgrade plan (real LLM + 验证 budget 实际超限) |
| PlanExecuteLoop plan_phase LLM | plan-execute-budget-integration 子 change |
| SkillCompiler.compile() 内 LLM (future) | skill-compiler-budget 子 change |