# evolution-budget-cap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实施 `EvolutionBudgetCap` — 在 `IBudgetController` 接口新增 6 个进化周期预算控制方法 + `ExecutionBudget` 新增 `max_evolution_llm_calls` 字段 + BudgetController 实现 + 2 个新事件发射。Oracle 评审 🟢 Go (`session ses_facbd3ffbffeUjlJgZsgMWFiM4`), N1 修复路径对症, commit `06ddd13` 已修 W3 (接口扩展 + ADR-0068 v1.9+ 归口)。

**Architecture:** 在 `src/core/types/budget.h` 增加 `max_evolution_llm_calls` 字段 + try_consume_evolution_llm_call() 方法（与现有 `max_llm_calls` 同构）。在 `src/modules/budget/budget_controller.h` IBudgetController 接口扩展 6 方法（4 try_consume + begin/end_evolution_cycle + evolution_budget_exceeded + set_bus/get_bus）。emit `budget.evolution_cycle.started` / `.exceeded` 事件到 IInteractionBus。设计符合 self-evolution §五 IBudgetController 复用 + ADR-0084 MutationGovernance 串联顺序（预算先于治理）。

**Tech Stack:** C++20, std::atomic<int> (CAS loop), IInteractionBus (27+ 主题已注册), Catch2 v3, Contract 层零修改（接口扩展非签名修改, 仅 IBudgetController 增加纯虚方法）。

---

## Scope Adjustments vs proposal

**Adopted scope** (Oracle Go):
- `ExecutionBudget` 新增 4 字段 + 1 方法（max_evolution_llm_calls / evolution_llm_calls_used / try_consume_evolution_llm_call / evolution_budget_exceeded）
- `IBudgetController` 接口扩展 6 纯虚方法（4 try_consume 系列 + begin/end_evolution_cycle + evolution_budget_exceeded）
- `BudgetController` 实现 + 移动构造重置（commit `06ddd13` W3 警告修复）
- `set_bus` + `get_bus` 注入 + emit 2 事件（`budget.evolution_cycle.started` / `.exceeded`）
- ADR-0068 Appendix A v1.9+ 注册 2 个新主题
- ≥5 测试 case（含移动构造 + reset + 并发 try_consume）

**Deferred to follow-up** (由 T6 触发):
- 并发进化 per-cycle 隔离（per CycleResource 类 / Stack-based scope）— 当前单 atomic 全局共享，N1.2 并发场景见 self-evolution §四 4.4
- 预算超限后手动注入新预算继续（V2 范围）
- ConfounderRecord.ResourceChange 消费 budget 事件（ADR-0086 ship 后）

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `src/core/types/budget.h` | `ExecutionBudget` 新增 4 字段 + 1 方法（CAS atomic） |
| `src/modules/budget/budget_controller.h` | `IBudgetController` 新增 6 纯虚方法 + `set_bus`/`get_bus` |
| `src/modules/budget/budget_controller.cpp` | `BudgetController` 完整实现（commit `06ddd13` 已修移动构造重置） |
| `include/agenticdsl/contract/iinteraction_bus.h` (无修改) | 复用现有 `bus_->emit(BusEvent{...})` |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_budget_evolution_cap.cpp` (new, ≥5 cases) | 默认 -1 / cap=3 第4次失败 / 进化总预算独立 / reset / 移动构造重置 |

### ADR/Docs

| File | Responsibility |
|---|---|
| `docs/adr/adr-0084-mutation-governance-contract.md` | (无修改) 串联顺序注释: 预算闸 → MutationGovernor.evaluate() → approve/deny |
| `docs/adr/adr-0086-credit-assignment-contract.md` | (无修改, 🔍 Proposed) ConfounderRecord::ResourceChange 可消费 budget.evolution_cycle.exceeded 事件 |

---

## TDD 5-Step Execution

### Step 1: Write failing test for evolution budget cap

**File**: `tests/test_budget_evolution_cap.cpp` (new, ~80 LOC, 5 cases)

```cpp
#include <agenticdsl/core/types/budget.h>
#include <agenticdsl/contract/ibudget_controller.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace agenticdsl {
class MockBudgetBus : public IInteractionBus {
public:
    void emit(const BusEvent& e) override { events.push_back(e); }
    size_t subscribe(...) override { return 0; }
    void unsubscribe(size_t) override {}
    std::vector<BusEvent> events;
};
}

TEST_CASE("evolution budget: default unlimited (-1) accepts any number", "[budget][evolution]") {
    ExecutionBudget b;
    b.max_evolution_llm_calls = -1;
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE_FALSE(b.evolution_budget_exceeded());
}

TEST_CASE("evolution budget: cap=3, 4th call returns false", "[budget][evolution]") {
    ExecutionBudget b;
    b.max_evolution_llm_calls = 3;
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE_FALSE(b.try_consume_evolution_llm_call());  // 第 4 次超限
    REQUIRE(b.evolution_budget_exceeded());
}

TEST_CASE("evolution budget independent from total llm_calls", "[budget][evolution]") {
    ExecutionBudget b;
    b.max_llm_calls = 10;
    b.max_evolution_llm_calls = 2;
    b.llm_calls_used = 10;  // 总预算满
    // 进化预算仍可用 (独立计数器)
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE_FALSE(b.try_consume_evolution_llm_call());
}

TEST_CASE("evolution budget reset via begin_evolution_cycle / end_evolution_cycle", "[budget][evolution]") {
    ExecutionBudget b;
    b.max_evolution_llm_calls = 2;
    b.try_consume_evolution_llm_call();
    b.try_consume_evolution_llm_call();
    REQUIRE(b.evolution_budget_exceeded());
    // 新周期: reset
    b.reset_evolution_cycle_counter();
    REQUIRE_FALSE(b.evolution_budget_exceeded());
    REQUIRE(b.try_consume_evolution_llm_call());
}

TEST_CASE("ExecutionBudget move-construct resets evolution counter", "[budget][evolution]") {
    ExecutionBudget a;
    a.max_evolution_llm_calls = 2;
    a.try_consume_evolution_llm_call();
    ExecutionBudget b = std::move(a);
    REQUIRE(b.try_consume_evolution_llm_call());  // b 计数器从 0 开始 (commit 06ddd13 修复)
    REQUIRE_FALSE(b.try_consume_evolution_llm_call());  // 第 3 次超限
}
```

**Verification**:
```bash
cd build && cmake --build . --target test_budget_evolution_cap
ctest -R "budget_evolution_cap" --output-on-failure
# Expected: FAIL (字段尚未添加)
```

---

### Step 2: Implement `ExecutionBudget` evolution fields + methods

**File**: `src/core/types/budget.h`

在 `ExecutionBudget` struct 添加（与现有 `max_llm_calls` 同构）:

```cpp
struct ExecutionBudget {
    int max_nodes = 100;
    int max_llm_calls = 50;
    int max_duration_sec = 60;
    int max_subgraph_depth = 3;
    
    // Evolution cycle budget (N1 修复)
    int max_evolution_llm_calls = -1;  // 默认 -1 (无限制, 零回归)
    int evolution_llm_calls_used = 0;  // atomic, 由 BudgetController 持
    
    // ... 现有 max_snapshots / snapshot_max_size_kb 不变
    
    // 现有 try_consume_llm_call() 不变
    // 新增 evolution cycle 方法
    bool try_consume_evolution_llm_call() {
        if (max_evolution_llm_calls < 0) return true;  // unlimited
        return evolution_llm_calls_used.fetch_add(1, std::memory_order_acq_rel) < max_evolution_llm_calls;
    }
    bool evolution_budget_exceeded() const {
        if (max_evolution_llm_calls < 0) return false;  // unlimited
        return evolution_llm_calls_used.load(std::memory_order_acquire) > max_evolution_llm_calls;
    }
    void reset_evolution_cycle_counter() {
        evolution_llm_calls_used.store(0, std::memory_order_release);
    }
};
```

**Verification**:
```bash
cd build && cmake --build . --target test_budget_evolution_cap
ctest -R "budget_evolution_cap" --output-on-failure
# Expected: PASS (5 cases)
```

---

### Step 3: Extend `IBudgetController` interface + `BudgetController` implementation

**File**: `src/modules/budget/budget_controller.h`

```cpp
class IBudgetController {
public:
    virtual ~IBudgetController() = default;
    // 现有 11 方法保留
    
    // N1 evolution cycle 新增 (commit 06ddd13 已定)
    virtual bool try_consume_evolution_llm_call() = 0;
    virtual bool evolution_budget_exceeded() const = 0;
    virtual void begin_evolution_cycle(const std::string& cycle_id) = 0;
    virtual void end_evolution_cycle(const std::string& cycle_id, bool success) = 0;
    virtual void reset_evolution_cycle_counter() = 0;
    
    // bus 注入 (commit 06ddd13 已加, 非新)
    virtual void set_bus(IInteractionBus* bus) = 0;
    virtual IInteractionBus* get_bus() const = 0;
};
```

**File**: `src/modules/budget/budget_controller.cpp`

完整实现 BudgetController (commit `06ddd13` 已 commit 设计):

```cpp
bool BudgetController::try_consume_evolution_llm_call() {
    return budget_.try_consume_evolution_llm_call();
}
bool BudgetController::evolution_budget_exceeded() const {
    return budget_.evolution_budget_exceeded();
}
void BudgetController::begin_evolution_cycle(const std::string& cycle_id) {
    if (bus_) {
        bus_->emit(BusEvent{"budget.evolution_cycle.started", {{"cycle_id", cycle_id}}});
    }
}
void BudgetController::end_evolution_cycle(const std::string& cycle_id, bool success) {
    if (bus_) {
        bus_->emit(BusEvent{"budget.evolution_cycle.ended", {
            {"cycle_id", cycle_id}, {"success", success}
        }});
    }
}
void BudgetController::reset_evolution_cycle_counter() {
    budget_.reset_evolution_cycle_counter();
}
// set_bus/get_bus 已在 commit 06ddd13 实现
```

---

### Step 4: Register `budget.evolution_cycle.*` in ADR-0068 Appendix A

**File**: `docs/adr/adr-0068-event-emission-contract.md` Appendix A

新增 2 行 (commit `06ddd13` W4 已声明归口):
```
| `budget.evolution_cycle.started` | evolution cycle 进入 (M1 emit audit) | BudgetController.begin_evolution_cycle | ✅ Shipped (T3 2026-08-31) |
| `budget.evolution_cycle.ended` | evolution cycle 退出 (success/fail) | BudgetController.end_evolution_cycle | ✅ Shipped (T3 2026-08-31) |
```

**Verification**:
```bash
python3 tools/adr_lint.py  # ✓ 通过
```

---

### Step 5: Commit

```bash
git add src/core/types/budget.h \
        src/modules/budget/budget_controller.h \
        src/modules/budget/budget_controller.cpp \
        tests/test_budget_evolution_cap.cpp \
        tests/CMakeLists.txt \
        docs/adr/adr-0068-event-emission-contract.md
git commit -m "feat(budget): EvolutionBudgetCap - N1 修复 (max_evolution_llm_calls + 2 events)"
```

---

## Ship Gate Validation

```bash
# 1. openspec validate
openspec validate 2026-08-31-evolution-budget-cap --strict

# 2. compile + tests
cmake --build build && ctest -R "budget_evolution_cap" --output-on-failure
# Expected: PASS (5 cases / 20+ assertions)

# 3. Baseline regression (无 opt-in)
ctest --output-on-failure  # 204 unchanged, + 5 new = 209

# 4. ADR lint
python3 tools/adr_lint.py  # ✓ 通过

# 5. Docs drift audit
python3 tools/docs_drift_audit.py  # 0 DRIFT

# 6. LSP discipline
./scripts/check-lsp-discipline.sh --quick  # ✓

# 7. ADR-0068 Appendix A 验证
grep -c "budget.evolution_cycle" docs/adr/adr-0068-event-emission-contract.md  # 2
```

Expected: all PASS, 0 regression, ctest baseline 204 + 5 new = 209.

---

## Risk Assessment

| 风险 | 缓解 |
|------|------|
| 4 个新纯虚方法破坏其他 IBudgetController 实现者 | 当前唯一实现者是 BudgetController (commit `06ddd13` 验证无其他子类); commit 时 grep 确认 |
| evolution_llm_calls_used 非 atomic 跨线程 | std::atomic<int> + memory_order_acq_rel CAS; 单 atomic 跨 BudgetController 实例共享 (设计假定 per-Engine 单实例) |
| 并发进化 (N1.2) 共享计数器冲突 | 当前设计文档明确: Engine 级别单实例; per-cycle 隔离留 V2 |
| commit `06ddd13` 已修 W3 但 spec.md scenario 验收路径仍需复核 | Step 4 ADR-0068 验证同步; ship gate 跑 adr_lint |
| 移动构造重置 (Oracle P0) 行为预期 | test_5 显式验证 move-construct 后计数器归零; commit `06ddd13` 已落实 |
| ADR-0086 credit-assignment 联动 | Step 4 emit 事件已含 cycle_id + success; ADR-0086 ship 后 ConfounderRecord 可消费 |
| Phase 1 启动 (T2 Axis6) 需 ADR-0086 ship | Step 5 ship 后不阻塞 T6 (T6 仅需 BudgetController 接口扩展); T2 Phase 1 阻塞 ADR-0086 |

---

## 后续触发条件

| 触发条件 | 后续 OpenSpec change |
|----------|---------------------|
| T3 ship 立即 | T6 gepa-mcts-budget-integration 启动 (强依赖) |
| N1.2 并发进化需求出现 | evolution-budget-cap-v2 (per-cycle 隔离) |
| ADR-0086 ship | ConfounderRecord.ResourceChange 集成 (consume budget events) |
| Per-engine 多 instance 需求 | per-engine evolution budget scope |