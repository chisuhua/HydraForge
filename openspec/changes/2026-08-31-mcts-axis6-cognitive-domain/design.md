# Design — MCTS Axis6 cognitive_domain composition chain

> **v2 (Oracle 评审 5 blocker 修复后)**: B1 实装 API 对齐 / B2 chain 语义澄清 / B3 commit-revert 触发统一 / B4 ADR-0068 v1.8 主题注册 / B5 兜底 Scenario + 口径统一

## Context

ADR-0061-08 v1.0 (T20 MCTSWorkflowSearch V1, 2026-08-28 ship) 定义了 5 轴 MCTS 搜索空间, 用于在 workflow 结构空间中自动发现最优 workflow。

`orchestration-architecture-2026-08.md` v1.5 §十四 M5 + §18.10.1 提出 cognitive domain 显式注册 + 链式 cognitive domain composition, 需要 MCTS 节点模板新增第 6 个属性维度以枚举 cognitive_domain composition chain。

本 change 是 ADR-0061-08 v1.1 amendment 的实施载体, 含 Axis6 enum + CognitiveDomainChainConfig + ctor 重载, 阶段 0 范围 1-1.5 sprint。

## 决策

### 决策 1 — Axis6CognitiveDomain enum (6 值, 节点级属性)

**Oracle B2 核心修复**: Axis6 是 **WorkflowNode 的节点级属性**, 与 axis1-5 同一抽象层 — 不是独立的 chain 搜索算法。

| 值 | 对应实装 | 状态 |
|----|---------|------|
| `None` | 该节点不绑定 cognitive_domain specialist (退化为 v1.0) | ✅ |
| `Reflect` | 该节点绑定 GEPALoop.reflect_and_commit (T19) | ✅ ship 2026-08-27 |
| `Search` | 该节点绑定 MCTSWorkflowSearch (嵌套! 需决策 5 预算上限) | ✅ ship 2026-08-28 |
| `Compile` | 该节点绑定 SkillCompiler (T17) | ✅ ship 2026-08-27 |
| `Reason` | 该节点绑定 IPER (ADR-0015, 未实装) | 🔍 Phase 1 预留 |
| `Meta_Select` | 该节点是"选择哪个 specialist"的决策点 | V2 阶段 |

**chain 的图级形态**: MCTS 树中一条根→叶路径上, axis6≠None 节点的最大连续段 = 一条候选 chain。不存在独立的 `search_chain` 算法 — 搜索完全复用 v1.0 UCB1 树策略 (per-child `q + C·sqrt(ln N / n)`)。

**替代方案**:
- ❌ enum 值硬编码 specialist 名 (GEPA/MCTS/SkillCompiler) — 与不变量 5 冲突
- ✅ enum 值用语义角色 (Reflect/Search/Compile/Reason/Meta_Select) — 抽象, 通过 `available_specialists` 注入具体值

### 决策 2 — CognitiveDomainChainConfig 通过 ctor 注入 (对齐实装风格)

**Oracle B1 修复**: v1.0 实装签名是:
```cpp
MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                   std::shared_ptr<IMutationGovernor> governor,
                   std::shared_ptr<BehavioralRegressionGate> regression_gate,
                   SearchConfig config,
                   std::shared_ptr<IInteractionBus> bus = nullptr);
SearchResult search(const TaskSpec& spec);
```

v1.1 遵循同一风格 — **配置走 ctor 注入, search() 签名不变**:

```cpp
// v1.1 新增: chain 配置 (ctor 注入, 与 SearchConfig 同级)
struct CognitiveDomainChainConfig {
  bool enable_axis6 = false;
  std::vector<Axis6CognitiveDomain> available_specialists;
  int max_chain_depth = 3;
  double min_eval_improvement = 0.05;      // 绝对值, 非百分比 (Oracle B5)
  int max_nested_search_iterations = 30;   // R3 嵌套预算 (决策 5)
};

// v1.1 ctor 重载 (原 ctor 保留委托, 默认 CognitiveDomainChainConfig{})
MCTSWorkflowSearch(evaluator, governor, regression_gate,
                   SearchConfig config,
                   CognitiveDomainChainConfig chain_config,   // 新增参数
                   bus = nullptr);

// search() 签名不变: SearchResult search(const TaskSpec& spec);
```

**5 字段对应**:
- `enable_axis6` → 决策 4 触发条件 opt-in
- `available_specialists` → §十四 M5 注册项 (不变量 5)
- `max_chain_depth` → 不变量 4 (默认 3)
- `min_eval_improvement` → 决策 5 (**绝对差**, 不再称 "5%")
- `max_nested_search_iterations` → 决策 5 (R3: 30³ ≈ 2.7万 而非 100³)

### 决策 3 — chain 语义与评分 (Oracle B2 核心修复)

**chain materialize 规则**:
- MCTS 树一条根→叶路径 = 候选 WorkflowGraph
- 图中 axis6≠None 节点的最大连续段 = chain, 长度 ≤ max_chain_depth

**chain eval 语义 (确定性)**:
- Phase 0 沿用 v1.0 模拟层: `IEvaluator::evaluate(workflow)` 评估整个 WorkflowGraph
- 含有效 chain 的图 eval 加权重 1.0 (无额外加成 — 纯结构搜索)
- **改进停机**: `best_reward` 绝对改进 < 0.05 (绝对值) 即停
- **测试确定性**: 复用 v1.0 Mock evaluator, 不执行真实 specialist

### 决策 4 — 行为不变性 (不变量 2)

| 调用方 | 调用方式 | 行为 |
|--------|---------|------|
| v1.0 调用方 | 原 ctor (4 参数 + bus) | 内部委托新 ctor + `CognitiveDomainChainConfig{}` (enable_axis6=false) → 完全等同 v1.0 |
| v1.1 opt-out | 新 ctor + `chain_config.enable_axis6=false` | 等同 v1.0 |
| v1.1 opt-in | 新 ctor + `enable_axis6=true, available_specialists=[...]` | 子节点扩展采样 axis6, chain 截断/停机生效 |

**测试保证**: v1.0 `tests/test_mcts_workflow_search.cpp` 17 cases / 65 assertions 必须全 pass (不变量 1)。

### 决策 5 — 停机判据 (4 项 Phase 0 + 1 项 Phase 1)

| 触发 | 判据 | 阶段 |
|------|------|------|
| `max_iterations` 耗尽 | SearchConfig.max_iterations = 100 | Phase 0 (v1.0) |
| chain 深度截断 | 连续 axis6≠None 节点数 ≥ max_chain_depth (默认 3) | Phase 0 |
| 嵌套 Search 预算 | axis6=Search 节点嵌套迭代 ≤ max_nested_search_iterations (默认 30) | Phase 0 (R3) |
| 改进停机 | best_reward 绝对改进 < min_eval_improvement (0.05 绝对值) | Phase 0 |
| understanding_complete | BehavioralEquivalence Match 连续 N 次 | Phase 1 (§18.10.1 A2) |

### 决策 6 — commit/revert/degraded 事件语义统一 (Oracle B3 修复)

| 事件 | 触发 (唯一定义) | payload |
|------|----------------|---------|
| `axis6.commit` | 新 chain 通过改进阈值 **且** `governor_->commit()` 返回 `MutationDecision{approved=true}` (L1) | `{chain, eval_score, depth, iterations_used}` |
| `axis6.revert` | `governor_->commit()` 返回 `MutationDecision{approved=false}` (L1, denial_reason 非空) | `{chain, prev_eval_score, denial_reason}` |
| `axis6.degraded` | `available_specialists` 为空/仅 None (R6 兜底) | `{reason: "empty_specialists"}` |

**governor 调用是 commit 的前置** (不变量 3): 伪代码必须 `if (governor_->commit(ctx).approved) emit(commit); else emit(revert);`。`governor` 内部已 emit `mutation.committed`/`mutation.denied`, MCTS 层只追加 `axis6.commit`/`axis6.revert` 作为搜索层审计。

### 决策 7 — 事件主题注册 (Oracle B4 修复)

3 个新主题需随 Phase 0 ship 同步提交 **ADR-0068 Appendix A v1.8 amendment** (v1.7 已被 capture-mode 2026-08-29 占用):
- `axis6.commit` / `axis6.revert` / `axis6.degraded` — owner=MCTSWorkflowSearch cognitive 模块

tasks.md 必须包含此任务 + ship gate git status 必须含 adr-0068 文件。

### 决策 8 — ADR-0061-08 v1.1 amendment 状态

amendment 是 ADR-0061-08 v1.0 的**增量扩展**:
- 不修改 v1.0 头文件 (仅增量)
- 原 ctor 保留 (委托新 ctor)
- 评审通过后: v1.0 状态行追加 v1.1 链接 + amendment flip Approved + orchestration doc v1.6 changelog + **docs/README.md adr/skill 表 adr-0061-08 行状态修正** (stale 🔍 Proposed → ✅ V1 Shipped)

### 决策 9 — 与 Meta-Cognitive Agent 的边界

Axis6 通过**搜索算法 + 横切 hook + 治理门 + 事件总线**分布式实现, **不需要 Meta-Cognitive Agent 类**。与 ADR-0085 §决策 5 "V1 不实施 Meta-Agent 自管理" 完全兼容。

## 接口

### 新增头文件增量 (.h)

```cpp
// include/agenticdsl/cognitive/mcts_workflow_search.h (v1.0 已存在, v1.1 增量)

namespace agenticdsl {

enum class Axis6CognitiveDomain {
  None = 0, Reflect = 1, Search = 2, Compile = 3,
  Reason = 4,        // Phase 1 预留
  Meta_Select = 5,   // V2 阶段
};

struct CognitiveDomainChainConfig {
  bool enable_axis6 = false;
  std::vector<Axis6CognitiveDomain> available_specialists;
  int max_chain_depth = 3;
  double min_eval_improvement = 0.05;      // 绝对值
  int max_nested_search_iterations = 30;
};

struct WorkflowNode {
  // ... 现有 id + 5 轴 ...
  Axis6CognitiveDomain axis6 = Axis6CognitiveDomain::None;  // v1.1 新增
};

class MCTSWorkflowSearch {
 public:
  struct SearchConfig { ... };       // v1.0 不变
  struct SearchResult { ... };       // v1.0 不变

  // v1.0 ctor (保留, 委托新 ctor)
  MCTSWorkflowSearch(evaluator, governor, regression_gate, SearchConfig, bus);

  // v1.1 ctor (新增)
  MCTSWorkflowSearch(evaluator, governor, regression_gate,
                     SearchConfig, CognitiveDomainChainConfig, bus);

  SearchResult search(const TaskSpec& spec);   // v1.0 签名不变
};

} // namespace agenticdsl
```

### 实现增量 (.cpp)

```cpp
// src/modules/cognitive/mcts_workflow_search.cpp (v1.0 已存在, v1.1 增量)

// 原 ctor 委托:
MCTSWorkflowSearch::MCTSWorkflowSearch(evaluator, governor, regression_gate, config, bus)
    : MCTSWorkflowSearch(evaluator, governor, regression_gate,
                         config, CognitiveDomainChainConfig{}, bus) {}

// 新 ctor:
MCTSWorkflowSearch::MCTSWorkflowSearch(evaluator, governor, regression_gate,
                                       config, chain_config, bus)
    : ..., chain_config_(std::move(chain_config)) {
  // R6 兜底: 空/仅 None specialists → 等效 enable_axis6=false + degraded 事件
  if (chain_config_.available_specialists.empty()) {
    chain_config_.enable_axis6 = false;
    if (bus_) bus_->emit(EventBuilder("axis6.degraded")
        .meta({{"reason", "empty_specialists"}}).build());
  }
}

// 子节点扩展 (复用 v1.0 UCB1, 子节点采样 axis6):
void MCTSWorkflowSearch::expand_children(node) {
  for (auto specialist : chain_config_.available_specialists) {
    auto child = create_node_with_axis6(specialist);
    // 深度截断: 连续 axis6≠None 段长度 ≤ max_chain_depth
    if (continuous_axis6_depth(node) >= chain_config_.max_chain_depth) continue;
    node.children.push_back(child);
  }
}

// commit/revert (决策 6):
void MCTSWorkflowSearch::commit_chain(chain, eval_score) {
  MutationContext ctx{/* mutation_id: 链唯一 ID, source_id: MCTS, mutation_kind: L2_dsl, subject_ref: chain_ref, proposed_change: chain_summary, parent_ref: old_chain_ref, version_id: chain.id, mode: MutationMode::Agent, evaluation_refs: {latest eval_id} */};
  MutationDecision decision = governor_->commit(ctx);
  if (decision.approved) {
    if (bus_) bus_->emit(EventBuilder("axis6.commit")
        .meta({{"chain", ...}, {"eval_score", eval_score}, {"depth", chain.size()}}).build());
    // governor 已 emit mutation.committed, MCTS 层不重复 emit governance 主题 (W4 修复)
  } else {
    if (bus_) bus_->emit(EventBuilder("axis6.revert")
        .meta({{"chain", ...}, {"prev_eval_score", ...}, {"denial_reason", decision.denial_reason}}).build());
    // governor 已 emit mutation.denied, MCTS 层不重复 emit
  }
}
```

### 新增测试 (.cpp)

```cpp
// tests/test_mcts_workflow_search_axis6.cpp (新建, ≥6 cases)

TEST_CASE("Axis6CognitiveDomain enum serialization round-trip") { ... }
TEST_CASE("default ctor delegation 行为等同 v1.0 (17 cases 不回归)") { ... }
TEST_CASE("enable_axis6=true + 2-3 specialists → chain depth ≤ max_chain_depth") { ... }
TEST_CASE("max_chain_depth=1 截断到 1 层") { ... }
TEST_CASE("min_eval_improvement 绝对改进阈值触发停机") { ... }
TEST_CASE("R6 空 available_specialists → axis6.degraded 事件 + 行为等同 v1.0") { ... }
```

## 反例 (明确不做)

| 反例 | 拒绝理由 |
|------|----------|
| 虚构 v1.0 签名 `unique_ptr<WorkflowGraph> search(spec, max_iterations)` | Oracle B1: 实装是 `SearchResult search(const TaskSpec&)` + ctor 注入; 设计必须对齐实装 |
| "UCB1 5 维特征向量 → 6 维 ordinal 编码" 叙述 | Oracle B2: UCB1 是 per-child q/visits 树选择, 不是特征向量; axis6 是节点采样属性 |
| 独立 `search_chain` 算法 | Oracle B2: chain 是图级形态, 由 MCTS 树自然生成, 不存在独立 chain 搜索 |
| `understanding_evaluator` Phase 0 死字段 | Oracle 建议: 移至 Phase 1 (本设计已移除该字段, 改为 Phase 1 集成) |
| commit 事件不经 governor commit 直接 emit | Oracle B1 修复: 不变量 3 强制 `governor->commit(ctx).approved` 前置 |
| ADR-0068 主题未注册即 emit | Oracle B4: 附录 A v1.8 amendment 随 Phase 0 同步 ship |
| min_eval_improvement 称 "5%" (相对) | Oracle B5: 统一为绝对差 0.05 |
| 新增 `MetaCognitiveAgent` 类 | ADR-0085 §决策 5 + 决策 9 |

## 跨 change 依赖

### 前置依赖 (全部已 ship)
- ✅ `t20-aflow-mcts` (ADR-0061-08 V1) — mcts_workflow_search.h 已 ship
- ✅ `t19-gepa-phase2-commit` — GEPALoop 已 ship
- ✅ `evaluator-v2-composite` — BehavioralEquivalenceEvaluator 已 ship
- ✅ `mutation-governance-contract` — MutationGovernor L1 已 ship
- ✅ `adr-0081-pre-step-hook-contract` — IAgentHookRegistry L2 已 ship
- ✅ `domain-worker-pool` — DomainWorkerPool 已 ship

### 后续依赖 (不在本 change 范围)
- **Phase 1 (Sprint 27+)**: L2 Hook + BehavioralEquivalence 集成 + 字符串→enum 映射 + example — 2 sprint
- **Phase 1 启动前置 blocker**: **ADR-0086 信用分配契约立项** (当前未立项, 显式声明)
- **V2 阶段**: L2+ mutation variants — 1 sprint

### ADR-0068 Appendix A v1.8 amendment (本 change 同步 ship)
- 3 个 axis6.* 主题注册 (决策 7)

## ADR 兼容性

| ADR | 兼容性 | 验证 |
|-----|--------|------|
| ADR-0020 V3 二分法 | ✅ 扩展 (Domain 侧多 cognitive 注册项) | git diff 0 行 |
| ADR-0061-08 v1.0 | ✅ 增量 (Axis6 + ChainConfig + ctor, 17 cases 0 回归) | tests pass |
| ADR-0083 V2 | ✅ 复用 (BehavioralEquivalence, Phase 1) | git diff 0 行 |
| ADR-0084 V1 L1 | ✅ 复用 (L1 authorize 前置 commit) | 决策 6 |
| ADR-0085 §决策 5 | ✅ 兼容 (分布式, 无 MetaAgent) | 决策 9 |
| ADR-0068 Event Contract | 🟡 需 Appendix A v1.8 amendment (决策 7, 同步 ship) | tasks §3.x 必含 |
| ADR-0019 IInteractionBus | ✅ 复用 (3 事件走 bus) | git diff 0 行 |
