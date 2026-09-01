# mcts-axis6-cognitive-domain Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实施 `MCTS Axis6` v1.1 amendment — 在 `WorkflowNode` 新增第 6 轴 `Axis6CognitiveDomain` enum + `CognitiveDomainChainConfig` 配置结构 + commit chain (单主体归因 `source_id: MCTS` 走 MutationGovernor.commit) + 3 个 `axis6.*` 事件主题 (ADR-0068 Appendix A v1.8 独占)。Oracle 评审 🟡 Conditional-Go (`session ses_fa91c94bdffeOraAXCrgkwK05f`), commit `283591f` v2.1 应用 B1 (governor commit API) + B3 (commit-revert 触发统一) + W4 (双发射语义分离)。commit `06ddd13` 应用 B3 依赖声明 + ADR-0068 v1.8 归口。**Phase 1 启动前置 blocker**: ADR-0086 信用分配契约立项 (commit `0f19997` 标注)。

**Architecture:** `MCTSWorkflowSearch` ctor 重载 + `SearchConfig` 扩展 `cognitive_domain_chain` 字段 (CognitiveDomainChainConfig) + 搜索时 UCB1 评估 `axis6` 节点可执行性 + 评估通过后 chain commit 走 `governor_->commit(ctx).approved` 判定 (单主体归因, 不触发多主体 credit assignment) + emit 4 个 axis6.* 事件。`axis6` 节点值为 enum (Reflect/Search/Compile/Meta_Select/Reason/None), Phase 0 实施 Reflect/Search/Compile 三种 (specialist 由 T5 提供), Phase 1 Meta_Select + Reason deferred。

**Tech Stack:** C++20, std::shared_ptr<IMutationGovernor>, IInteractionBus (4 个新 axis6.* 主题), nlohmann::json (CognitiveDomainChainConfig), ADR-0068 Appendix A v1.8 (T2 独占归口, T1/T3 用 v1.9+)。

---

## Scope Adjustments vs proposal

**Adopted scope** (Oracle Conditional-Go, commit `283591f` v2.1):
- `Axis6CognitiveDomain` enum (Reflect/Search/Compile/Meta_Select/Reason/None)
- `WorkflowNode::axis6` 字段 (commit `283591f` v2.1 引入)
- `CognitiveDomainChainConfig` struct (chain 节点枚举 + `max_chain_depth` 截断)
- `SearchConfig` 扩展 `cognitive_domain_chain` 字段
- `MCTSWorkflowSearch` ctor 重载 (commit `283591f` v2.1 B2 ctor 委托 v1.0 默认行为)
- `commit_chain()` 方法: `governor_->commit(ctx).approved` 判定 (单主体归因, source_id='MCTS')
- emit 4 个 axis6.* 事件: `axis6.search.started` / `axis6.commit.committed` / `axis6.commit.reverted` / `axis6.degraded`
- ADR-0068 Appendix A v1.8 (T2 独占) 注册 4 主题
- ≥6 测试 case: axis6 enum 默认 None / chain 截断 / commit approved / commit reverted / 事件序列 / 不变量回归

**Deferred to follow-up** (per ADR-0061-08 v1.1 决策 7):
- Phase 1: Meta_Select + Reason enum 实施 (IPER 未实装, ADR-0061-06 跟进)
- Phase 2: V2 嵌套 MCTS 搜索 (嵌套预算受 max_nested_search_iterations 约束)
- Phase 2: L2+ workflow variants (commit 走 MutationGovernor L2+ 需 ADR-0086 信用分配 ship 后)
- 跨进程 axis6 chain (ADR-0077 gRPC descoped, Phase 7+)

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `include/agenticdsl/cognitive/mcts_workflow_search.h` | `Axis6CognitiveDomain` enum + `WorkflowNode.axis6` + `CognitiveDomainChainConfig` + SearchConfig 扩展 + ctor 重载 |
| `src/modules/cognitive/mcts_workflow_search.cpp` | `commit_chain()` 实现 + 4 个 axis6.* 事件发射 + UCB1 axis6 评估 |
| `include/agenticdsl/contract/imutation_governance.h` | (无修改) `commit(ctx).approved` API 已 ship (commit `283591f` B1) |
| `docs/adr/adr-0068-event-emission-contract.md` Appendix A | 新增 v1.8 段: 4 个 axis6.* 主题 (T2 独占归口) |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_mcts_axis6.cpp` (new, ≥6 cases) | 6 个核心 case (enum 默认 + chain 截断 + commit approved/reverted + 事件序列 + v1.0 baseline) |

---

## TDD 5-Step Execution

### Step 1: Write failing test

**File**: `tests/test_mcts_axis6.cpp` (new, ~100 LOC)

```cpp
#include <agenticdsl/cognitive/mcts_workflow_search.h>
#include <agenticdsl/contract/imutation_governance.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace {
class MockMutGovernor : public IMutationGovernor {
public:
    bool approved = true;
    std::string last_decision;
    MutationDecision commit(const MutationContext& ctx) override {
        last_decision = approved ? "approved" : "denied";
        return MutationDecision{approved, last_decision};
    }
    // 其他方法返回 default (略)
};
class MockBus : public IInteractionBus {
public:
    std::vector<BusEvent> events;
    void emit(const BusEvent& e) override { events.push_back(e); }
    size_t subscribe(...) override { return 0; }
    void unsubscribe(size_t) override {}
};
}

TEST_CASE("Axis6CognitiveDomain enum defaults to None (v1.0 backward compat)", "[mcts][axis6]") {
    WorkflowNode node;
    REQUIRE(node.axis6 == Axis6CognitiveDomain::None);
}

TEST_CASE("MCTSWorkflowSearch with axis6=Reflect produces cognitive chain", "[mcts][axis6]") {
    auto eval = std::make_shared<MockEvaluator>();
    auto gov = std::make_shared<MockMutGovernor>();
    auto reg = std::make_shared<MockRegressionGate>();
    SearchConfig cfg;
    cfg.cognitive_domain_chain = CognitiveDomainChainConfig{
        .specialists = {Axis6CognitiveDomain::Reflect, Axis6CognitiveDomain::Compile},
        .max_chain_depth = 2
    };
    MCTSWorkflowSearch searcher(eval, gov, reg, cfg);
    WorkflowNode node;
    node.axis6 = Axis6CognitiveDomain::Reflect;
    REQUIRE(searcher.can_execute(node));
}

TEST_CASE("axis6 chain commits to governor with source_id=MCTS (单主体归因)", "[mcts][axis6][governance]") {
    auto gov = std::make_shared<MockMutGovernor>();
    gov->approved = true;
    auto searcher = make_searcher_with(gov);
    searcher.commit_chain({Axis6CognitiveDomain::Reflect});
    REQUIRE(gov->last_decision == "approved");
    REQUIRE(gov->last_source_id == "MCTS");
}

TEST_CASE("axis6 chain reverts when governor denies", "[mcts][axis6][governance]") {
    auto gov = std::make_shared<MockMutGovernor>();
    gov->approved = false;
    auto searcher = make_searcher_with(gov);
    searcher.commit_chain({Axis6CognitiveDomain::Reflect});
    REQUIRE(gov->last_decision == "denied");
    // 验证 emit axis6.commit.reverted
}

TEST_CASE("axis6 chain depth > max_chain_depth is rejected (degraded event)", "[mcts][axis6][safety]") {
    SearchConfig cfg;
    cfg.cognitive_domain_chain.max_chain_depth = 3;
    auto searcher = make_searcher_with(cfg);
    auto result = searcher.commit_chain({Reflect, Search, Compile, Meta_Select});  // 4 > 3
    REQUIRE(result.status == "degraded");
}

TEST_CASE("axis6 emits 4 events in order: started → committed/reverted → degraded (if any)", "[mcts][axis6][bus]") {
    auto bus = std::make_shared<MockBus>();
    auto searcher = make_searcher_with(bus);
    searcher.commit_chain({Reflect});
    REQUIRE(bus->events.size() >= 2);
    REQUIRE(bus->events[0].topic == "axis6.search.started");
    REQUIRE(bus->events[1].topic == "axis6.commit.committed");
}

TEST_CASE("axis6 chain preserves v1.0 baseline (no axis6 → MCTS V1 behavior unchanged)", "[mcts][axis6][regression]") {
    WorkflowNode node;
    node.axis1 = Axis1Template::Linear;
    node.axis6 = Axis6CognitiveDomain::None;  // 默认 None
    auto searcher = make_searcher_v1_baseline();
    // v1.0 17 cases baseline 必须不变 (由 cap-map §一 #29 ship 验证)
}
```

**Verification**:
```bash
cmake --build build --target test_mcts_axis6
ctest -R "mcts_axis6" --output-on-failure
# Expected: FAIL (axis6 字段未加)
```

---

### Step 2: Add `Axis6CognitiveDomain` enum + `WorkflowNode.axis6` field

**File**: `include/agenticdsl/cognitive/mcts_workflow_search.h`

```cpp
// v1.1 amendment 第 6 轴 (commit 283591f v2.1)
enum class Axis6CognitiveDomain {
    None,
    Reflect,         // GEPA Loop reflect_and_commit (T5 提供 specialist)
    Search,          // 嵌套 MCTS (V2)
    Compile,         // SkillCompiler (T5 提供 specialist)
    Meta_Select,     // Phase 1 (IPER, ADR-0061-06 跟进)
    Reason           // Phase 1 (IPER, ADR-0061-06 跟进)
};

// CognitiveDomainChainConfig (commit 283591f v2.1 B2)
struct CognitiveDomainChainConfig {
    std::vector<Axis6CognitiveDomain> specialists;
    int max_chain_depth = 3;  // 硬截断 (避免嵌套 cost 爆炸)
    // Phase 0: 实施 None/Reflect/Search/Compile
};

struct WorkflowNode {
    Axis1Template axis1 = Axis1Template::Linear;
    Axis2Param axis2 = Axis2Param::Temperature;
    Axis3Tool axis3 = Axis3Tool::None;
    Axis4Control axis4 = Axis4Control::Sequential;
    Axis5Error axis5 = Axis5Error::Retry;
    Axis6CognitiveDomain axis6 = Axis6CognitiveDomain::None;  // v1.1 新增
};
```

---

### Step 3: Extend `SearchConfig` + `MCTSWorkflowSearch` ctor overload

**File**: `include/agenticdsl/cognitive/mcts_workflow_search.h`

```cpp
struct SearchConfig {
    // v1.0 字段保留
    int max_iterations = 100;
    double ucb1_c = 1.414;
    // v1.1 新增
    CognitiveDomainChainConfig cognitive_domain_chain = {};
    int max_nested_search_iterations = 30;  // 决策 5: 嵌套预算 30³ ≈ 2.7万
};

class MCTSWorkflowSearch {
public:
    // v1.0 ctor 保留 (向后兼容, 默认 axis6=None)
    MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                       std::shared_ptr<IMutationGovernor> governor,
                       std::shared_ptr<IRegressionGate> regression_gate,
                       SearchConfig config = SearchConfig{},
                       std::shared_ptr<IInteractionBus> bus = nullptr);
    // v1.1 ctor overload (axis6 显式接入)
    MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                       std::shared_ptr<IMutationGovernor> governor,
                       std::shared_ptr<IRegressionGate> regression_gate,
                       SearchConfig config,
                       std::shared_ptr<IInteractionBus> bus,
                       std::shared_ptr<IBudgetController> budget);  // 依赖 T6
    
    SearchResult search(const TaskSpec& spec);
    
    // v1.1 新增 (commit 283591f v2.1)
    void commit_chain(const std::vector<Axis6CognitiveDomain>& chain);
    bool can_execute(const WorkflowNode& node) const;
    
private:
    std::shared_ptr<IBudgetController> budget_controller_;  // T6 注入
    std::shared_ptr<IInteractionBus> bus_;
};
```

---

### Step 4: Implement `commit_chain()` + 4 events

**File**: `src/modules/cognitive/mcts_workflow_search.cpp`

```cpp
void MCTSWorkflowSearch::commit_chain(const std::vector<Axis6CognitiveDomain>& chain) {
    // 决策 5: 兜底 (chain 为空或 None → degraded)
    if (chain.empty() || (chain.size() == 1 && chain[0] == Axis6CognitiveDomain::None)) {
        if (bus_) {
            bus_->emit(BusEvent{"axis6.degraded", {
                {"reason", "empty_chain_or_all_none"},
                {"chain_size", chain.size()}
            }});
        }
        return;
    }
    
    // 决策 5: max_chain_depth 硬截断
    if (static_cast<int>(chain.size()) > config_.cognitive_domain_chain.max_chain_depth) {
        if (bus_) {
            bus_->emit(BusEvent{"axis6.degraded", {
                {"reason", "chain_depth_exceeded"},
                {"requested_depth", chain.size()},
                {"max_depth", config_.cognitive_domain_chain.max_chain_depth}
            }});
        }
        LOG_WARN("axis6 chain depth " << chain.size() << " > max " << config_.cognitive_domain_chain.max_chain_depth);
        return;
    }
    
    // emit started
    if (bus_) {
        bus_->emit(BusEvent{"axis6.search.started", {
            {"chain", nlohmann::json(chain)},
            {"source_id", "MCTS"}
        }});
    }
    
    // B1 修复: governor_->commit(ctx).approved 判定 (替换虚构 authorize)
    MutationContext ctx{
        .source_id = "MCTS",
        .subject_version = "v1.1",
        .parent_version = "v1.0",
        .mutation_kind = "workflow_variants",
        .resource_cost = {"chain_size", static_cast<int>(chain.size())}
    };
    auto decision = governor_->commit(ctx);
    
    // emit committed or reverted
    if (decision.approved) {
        if (bus_) {
            bus_->emit(BusEvent{"axis6.commit.committed", {
                {"mutation_id", ctx.mutation_id},
                {"chain", nlohmann::json(chain)}
            }});
        }
    } else {
        if (bus_) {
            bus_->emit(BusEvent{"axis6.commit.reverted", {
                {"mutation_id", ctx.mutation_id},
                {"reason", decision.message}
            }});
        }
    }
}

bool MCTSWorkflowSearch::can_execute(const WorkflowNode& node) const {
    return node.axis6 != Axis6CognitiveDomain::None;
}
```

---

### Step 5: Register axis6.* 4 events in ADR-0068 Appendix A v1.8

**File**: `docs/adr/adr-0068-event-emission-contract.md` Appendix A

新增 v1.8 段 (T2 独占归口, commit `06ddd13` W4 + commit `283591f`):
```
### v1.8 amendment (2026-08-31, T2 独占归口)
| 主题 | 描述 | 发射点 | 状态 |
|------|------|--------|------|
| `axis6.search.started` | cognitive domain chain 评估开始 | MCTSWorkflowSearch.commit_chain | ✅ Shipped (T2 2026-08-31) |
| `axis6.commit.committed` | chain 评估通过 + governor approve | MCTSWorkflowSearch.commit_chain | ✅ Shipped (T2 2026-08-31) |
| `axis6.commit.reverted` | chain 评估拒绝 (governor deny 或 depth 超限) | MCTSWorkflowSearch.commit_chain | ✅ Shipped (T2 2026-08-31) |
| `axis6.degraded` | chain 为空 / 深度超限 / specialist 未注册 | MCTSWorkflowSearch.commit_chain | ✅ Shipped (T2 2026-08-31) |
```

---

### Step 6: Commit

```bash
git add include/agenticdsl/cognitive/mcts_workflow_search.h \
        src/modules/cognitive/mcts_workflow_search.cpp \
        tests/test_mcts_axis6.cpp \
        tests/CMakeLists.txt \
        docs/adr/adr-0068-event-emission-contract.md
git commit -m "feat(mcts): Axis6 cognitive_domain composition chain + commit API (T2 v1.1 amendment)"
```

---

## Ship Gate Validation

```bash
# 1. openspec validate (T3 + T6 已 ship 是前提)
openspec validate 2026-08-31-mcts-axis6-cognitive-domain --strict

# 2. compile + tests
cmake --build build && ctest -R "mcts_axis6" --output-on-failure
# Expected: PASS (6+ cases)

# 3. Baseline regression (T3 + T6 已 ship, 214 tests + 6 new = 220)
ctest --output-on-failure  # 220

# 4. ADR lint + drift
python3 tools/adr_lint.py  # ✓
python3 tools/docs_drift_audit.py  # 0 DRIFT

# 5. v1.0 baseline (17 cases)
ctest -R "test_mcts_workflow_search" --output-on-failure  # 17 cases / 65 assertions PASS

# 6. ADR-0068 v1.8 独占验证 (4 主题归 T2 独占)
grep -c "axis6\." docs/adr/adr-0068-event-emission-contract.md  # ≥ 4 (T2 独占, T1/T3 v1.9+)
```

---

## Risk Assessment

| 风险 | 缓解 |
|------|------|
| T3/T6 未 ship (接口不存在) | ship 顺序: T3 → T6 → T2, ship gate grep T3 + T6 接口 |
| Meta-Agent 概念复活 (违反 ADR-0085 §决策 5) | 决策 8 显式声明: 本 change 是 cognitive 编排的 ADR-0061-08 amendment, 不是 cross-cutting Meta-Agent |
| 单主体归因 vs 多主体 (self-evolution §五 红线) | commit `source_id: "MCTS"` 单主体, 不触发多主体 credit assignment; ADR-0086 ship 前 commit 路径单主体归因 |
| L1/L2+ commit 授权混淆 (R5) | 决策 6: Phase 0 commit = L1 搜索审计, L2+ workflow variants V2 |
| specialists 未实装 (R6) | 决策 5 兜底: chain 中 axis6=None → degraded 事件, 等同 v1.0 |
| T1 axis6 测试依赖 (compile-time) | T1 design.md §B3 已声明 T2 必须先 ship; ship gate grep axis6 字段存在 |
| 嵌套 cost 爆炸 (R3, 100³=百万迭代) | 决策 5 `max_nested_search_iterations=30` 嵌套预算 (30³ ≈ 2.7万) |
| ADR-0086 未 ship 触发 Phase 1 启动 (R7) | commit `0f19997` 标注; Phase 1 启动前置 blocker 显式声明; 当前仅 Phase 0 ship |

---

## 后续触发条件

| 触发条件 | 后续 OpenSpec change |
|----------|---------------------|
| T2 ship 立即 | T5 cognitive-specialists-as-tools 启动 (Reflect/Search/Compile specialist 提供) |
| T2 + T5 ship | T1 workflow-materializer-v1 启动 (axis6 节点可执行) |
| ADR-0086 ✅ Shipped | T2 Phase 1 启动: Meta_Select + Reason enum + L2+ workflow variants |
| T2 + Phase 6a Wave 2 | Integration with BehavioralEquivalenceEvaluator (ADR-0083 V2) |