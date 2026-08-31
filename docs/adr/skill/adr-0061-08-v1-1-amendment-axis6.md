# ADR-0061-08 v1.1 amendment: MCTS Axis6 cognitive_domain composition

**日期**: 2026-08-31
**状态**: ✅ **Approved (2026-08-31)** — Oracle 评审 B1-B3 + W4 修复已落地 (commits `bc157fb` v1.1-draft.3 创建 → `283591f` v2.1 governor commit API + ADR-0068 v1.8 归口 + 双发射语义分离 → `06ddd13` B3 commit-revert 触发统一); 实施载体 OpenSpec change `2026-08-31-mcts-axis6-cognitive-domain` v2.1 已立项 (0/42 tasks); self-review issue 待用户创建后 24h cooling-off; **Phase 1 启动前置 blocker**: ADR-0086 信用分配契约立项 (当前未立项, 见 self-evolution §七 #6 已建议)
**父 ADR**: [adr-0061-08-aflow-search.md](adr-0061-08-aflow-search.md) v1.0
**前置 ADR**:
- ADR-0061-08 v1.0 (✅ Approved + V1 Shipped 2026-08-28) — 5 轴模板 + UCB1 MCTS + IEvaluator V2
- ADR-0020 (✅ Approved) — Cognitive/Domain Worker 隔离, per-instance DSLEngine, 线程级隔离
- ADR-0083 (✅ Approved + V2 Shipped 2026-08-27) — IEvaluator V2 BehavioralEquivalence
- ADR-0084 (✅ Approved + V1 Shipped 2026-08-26) — MutationGovernance L1 workflow variants 授权

**关联文档**:
- `docs/architecture/agent-orchestration-architecture-2026-08.md` v1.5 §十四 M5 + §18.10.1 — cognitive domain 链触发条件
- `openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` — 实施载体 OpenSpec change

**最后更新**: 2026-08-31 (v1.1-draft.3 — Oracle B1-B3 阻塞修复: B1 实装 API `governor->commit(ctx).approved` 判定替换虚构 authorize / B2 chain 语义节点级属性 / B3 commit-revert 触发统一 + W4 双发射语义分离; 执行载体 OpenSpec change `2026-08-31-mcts-axis6-cognitive-domain` v2.1)

---

## 背景

ADR-0061-08 v1.0 (T20 MCTSWorkflowSearch V1, 2026-08-28 ship) 定义了 **5 轴 MCTS 搜索空间**:
- Axis1Template (Linear/Branching/Loop/Parallel) — workflow 结构
- Axis2Param (Temperature/MaxTokens/TopP) — LLM 参数
- Axis3Tool (None/Calculator/Search/Custom) — 工具
- Axis4Control (Sequential/Parallel/Loop) — 控制流
- Axis5Error (Retry/Fallback/Abort) — 错误处理

`orchestration-architecture-2026-08.md` v1.5 §十四 M5 + §18.10.1 提出:
1. **Cognitive domain 显式注册** — `DomainWorkerPool::register_domain_handler("cognitive", ...)` 包装 GEPALoop/MCTSWorkflowSearch/SkillCompiler/IPER 作为 cognitive 任务 specialists
2. **Cognitive ↔ cognitive_domain 循环** — Cognitive Agent 通过 §十八 5 模式委派 cognitive_domain agents
3. **多 CognitiveWorker 递归** — chain `cognitive→cognitive_domain→cognitive_domain→...→other_domain` 中每一层是**独立 CognitiveWorker 实例** (per-instance DSLEngine + 独立线程, ADR-0020 §1 线程级隔离)
4. **自进化生成 cognitive_domain 链** — 通过 MCTS 搜索找到**最适合当前 other_domain 的 cognitive_domain chain**

这 4 个需求合并为 1 个新需求: **MCTS 节点模板需新增第 6 个属性维度**, 每个 WorkflowNode 可标注一个 cognitive_domain specialist, 使 MCTS 生成的 WorkflowGraph 可以描述 cognitive_domain composition chain。这是 ADR-0061-08 v1.1 amendment。

## 决策

### 决策 1 — 新增 Axis6: cognitive_domain composition (节点级属性)

```cpp
// 现有 5 轴 (ADR-0061-08 v1.0, mcts_workflow_search.h):
enum class Axis1Template { Linear, Branching, Loop, Parallel };
enum class Axis2Param    { Temperature, MaxTokens, TopP };
enum class Axis3Tool     { None, Calculator, Search, Custom };
enum class Axis4Control  { Sequential, Parallel, Loop };
enum class Axis5Error    { Retry, Fallback, Abort };

// v1.1 新增 Axis6 (节点级属性, 与 axis1-5 同一抽象层):
enum class Axis6CognitiveDomain {
  None,            // 该节点不绑定 cognitive_domain specialist (退化为 v1.0 行为)
  Reflect,         // 该节点绑定 GEPALoop.reflect_and_commit
  Search,          // 该节点绑定 MCTSWorkflowSearch (嵌套! 需决策 5 预算上限)
  Compile,         // 该节点绑定 SkillCompiler
  Reason,          // 该节点绑定 IPER (ADR-0015, lib/reasoning/iper_loop@v1 未实装, Phase 1 预留)
  Meta_Select,     // 该节点是"选择哪个 cognitive_domain specialist"的决策点 (V2 阶段)
};

// WorkflowNode 扩展 (mcts_workflow_search.h):
struct WorkflowNode {
  // ... 现有 id + 5 轴字段 ...
  Axis6CognitiveDomain axis6 = Axis6CognitiveDomain::None;  // v1.1 新增, 默认 None 等价 v1.0
};
```

**关键澄清 (Oracle B2 修复)**: Axis6 是 **WorkflowNode 的节点级属性**, 与 axis1-5 同一抽象层 — 不是独立的 chain 搜索算法。
- **chain 是 WorkflowGraph 的图级形态**: 当一条 WorkflowGraph 中相邻多个节点的 axis6 ≠ None 且构成 `cognitive→cognitive_domain→...→other_domain` 序列时, 该图表达一条 cognitive_domain composition chain
- **chain 由 MCTS 树自然生成**: MCTS 扩展子节点时, 每个子节点独立采样 axis6 值 (从 `available_specialists` 中), 树中一条从根到叶的路径若包含多个非 None axis6 节点, 即构成一条候选 chain
- **不存在独立的 `search_chain` 算法**: 搜索机制完全复用 v1.0 UCB1 树策略 (per-child `q + C·sqrt(ln N / n)`), 不引入 "5 维特征向量 → 6 维" 的错误叙述

### 决策 2 — CognitiveDomainChainConfig 通过 SearchConfig 注入 (对齐实装 ctor 风格)

**Oracle B1 修复**: v1.0 实装签名是 `MCTSWorkflowSearch(evaluator, governor, regression_gate, SearchConfig, bus)` + `SearchResult search(const TaskSpec& spec)`, 配置全部走 ctor 注入。v1.1 遵循同一风格:

```cpp
class MCTSWorkflowSearch {
 public:
  // v1.0 实装 (保持不变)
  struct SearchConfig {
    int max_iterations = 100;
    double exploration_weight = 1.414;
    std::string source_id = "R_T20_AFLOW";
    int max_children_per_node = 3;
    std::uint32_t random_seed = 42;
  };

  struct SearchResult {
    std::shared_ptr<WorkflowGraph> best_workflow;
    double best_reward = -1.0;
    int iterations_used = 0;
    bool success = false;
    std::string failure_mode;
  };

  // v1.1 新增: chain 配置 (ctor 注入, 与 SearchConfig 同级)
  struct CognitiveDomainChainConfig {
    bool enable_axis6 = false;                               // opt-in (默认 false → 完全等同 v1.0)
    std::vector<Axis6CognitiveDomain> available_specialists; // 来自 §十四 M5 注册 (不变量 5)
    int max_chain_depth = 3;                                 // 图中连续 axis6≠None 节点数上限 (不变量 4)
    double min_eval_improvement = 0.05;                      // chain eval 绝对改进下限 (决策 5, 绝对值非百分比)
    int max_nested_search_iterations = 30;                   // Search specialist 嵌套预算 (决策 5, R3 修复)
  };

  // v1.1 ctor 重载 (增加 chain_config 参数, 原 ctor 保留委托)
  MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                     std::shared_ptr<IMutationGovernor> governor,
                     std::shared_ptr<BehavioralRegressionGate> regression_gate,
                     SearchConfig config,
                     CognitiveDomainChainConfig chain_config,   // v1.1 新增参数
                     std::shared_ptr<IInteractionBus> bus = nullptr);

  // search() 签名不变 (实装真实签名)
  SearchResult search(const TaskSpec& spec);
};
```

**原 ctor 保留** (不含 chain_config), 内部委托新 ctor 并以 `CognitiveDomainChainConfig{}` 默认值 (enable_axis6=false) 调用, 保证 v1.0 调用方零修改 (不变量 2)。

### 决策 3 — chain 语义与评分 (Oracle B2 核心修复)

**chain materialize 规则**:
- MCTS 树中一条根→叶路径上的节点序列 = 一条候选 WorkflowGraph
- 该图中 **axis6≠None 节点的最大连续段长度** = chain 深度 (受 `max_chain_depth` 截断)
- chain 的 specialist 序列 = 该连续段节点的 axis6 值按图序排列

**chain eval 语义** (确定性, 无需真实执行 specialist):
- Phase 0 沿用 v1.0 模拟层: `IEvaluator::evaluate(workflow)` 评估整个 WorkflowGraph (v1.0 机制, 已 ship)
- chain 对 eval 的贡献: 含有效 chain (depth ≤ max_chain_depth 且 specialists ∈ available_specialists) 的图, eval 加权重 (默认 1.0, 即无额外加成 — Phase 0 纯结构搜索, 不做 chain 特有奖励)
- **改进停机的判据**: 连续 N 次迭代 `best_reward` 绝对改进 < `min_eval_improvement` (绝对值 0.05, 非相对百分比) 即停 — 与 v1.0 MCTS 收敛判据同构

**测试确定性**: Phase 0 测试复用 v1.0 Mock evaluator (确定性 q 值), 构造含特定 axis6 组合的 WorkflowGraph 序列, 验证 chain 深度截断与停机行为, 不执行真实 GEPA/MCTS/SkillCompiler。

### 决策 4 — 触发条件 (与 §18.10.1 A1-A5 对齐)

Axis6 仅在以下 4 条件**全部**满足时才应启用 (调用方负责检查, 否则使用 enable_axis6=false):

1. **A1 横切接口监控在线**: L2 IAgentHook 已注册 `cognitive::*` 命名空间 + `cognitive.understanding_check` 事件持续 emit (Phase 1)
2. **A2 BehavioralEquivalenceEvaluator V2 在线**: cognitive agent 的 verdict 可观察 (已 ship)
3. **A3 chain 候选就绪**: `available_specialists` 至少含 2 个非 None 值 (Reflect / Search / Compile 之一)
4. **A4 MutationGovernor 授权**: L1 workflow variants (已 ship); L2+ 仅 V2 阶段需要

**Phase 0 范围**: 仅实装决策 1 (Axis6 enum) + 决策 2 (ChainConfig + ctor) + 决策 3 (chain 语义 + 确定性评分) + 决策 5 (深度截断 + 嵌套预算 + 改进停机)。A1 横切集成 / A2 默认注入 / 多 CognitiveWorker 实例化均属 Phase 1。ADR-0086 信用分配未立项前, Phase 1 不启动 (显式 blocker 声明)。

### 决策 5 — 链深度上限 / 嵌套预算 / 改进停机 (R3 + B5 修复)

| 触发 | 判据 | 来源 |
|------|------|------|
| **max_chain_depth** | 默认 3 (图中连续 axis6≠None 节点数上限, 硬截断) | 防无限递归 (不变量 4) |
| **max_nested_search_iterations** | 默认 30 (axis6=Search 节点的嵌套 MCTS 迭代上限, 区别于主搜索 max_iterations=100) | R3 修复: 3 层嵌套 Search = 30³ ≈ 2.7万 而非 100³ = 百万 |
| **min_eval_improvement** | 默认 0.05 (**绝对值**, best_reward 改进 < 0.05 即停) | Oracle B5 修复: 统一为绝对差, 文档不再称 "5%" |
| **hard budget** | `ExecutionBudget.exceeded()` | §18.10.1 A4 |
| **空 specialists 兜底** | `available_specialists` 为空或仅含 None → enable_axis6 等效 false, 行为等同 v1.0, 并 emit `axis6.degraded` 事件 (R6 修复) | 见决策 7 |

### 决策 6 — commit/revert 事件语义统一 (Oracle B3 修复)

| 事件 | 触发 (唯一定义) | payload |
|------|----------------|---------|
| `axis6.commit` | 新 chain 通过改进阈值 **且** `governor_->commit(ctx)` 返回 `MutationDecision{approved=true}` (L1) | `{chain, eval_score, depth, iterations_used}` |
| `axis6.revert` | `governor_->commit(ctx)` 返回 `MutationDecision{approved=false}` (L1, denial_reason 非空) | `{chain, prev_eval_score, denial_reason}` |
| `axis6.degraded` | `available_specialists` 为空/仅 None (R6 兜底) | `{reason: "empty_specialists"}` |

**governor 调用是 commit 的前置** (不变量 3): 伪代码中 `governor_->commit(ctx).approved` 必须先于 emit axis6.commit。3 个新主题注册见决策 7 (ADR-0068 Appendix A **v1.8** amendment, v1.7 已被 capture-mode 占用 — Oracle B4 修复)。

### 决策 7 — 事件主题注册 (Oracle B4 修复)

本 amendment 新增 3 个主题, 需随 Phase 0 ship 同步提交 **ADR-0068 Appendix A v1.8 amendment**:
- `axis6.commit` — owner=MCTSWorkflowSearch cognitive 模块
- `axis6.revert` — owner=MCTSWorkflowSearch cognitive 模块
- `axis6.degraded` — owner=MCTSWorkflowSearch cognitive 模块

(ADR-0068 Appendix A 已至 v1.7 — capture-mode 2026-08-29; 本 amendment 用 v1.8)

### 决策 8 — 与 Meta-Cognitive Agent 的边界

| 项 | 现有/提议 | 是否需 MetaAgent |
|----|----------|----------------|
| **何时选择 chain** | MCTS UCB1 树选择 (axis6 为节点采样属性) | ❌ 不需要 |
| **何时停机** | 决策 5 四项判据 + (Phase 1) L2 Hook 观察 understanding_complete | ❌ 不需要 |
| **何时提交新 chain** | `governor_->commit(ctx).approved` L1 (Phase 0) / L2+ (V2) | ❌ 不需要 |
| **跨层一致性** | IInteractionBus 事件 + ADR-0068 主题 | ❌ 不需要 |

**结论**: Axis6 通过**搜索算法 + 横切 hook + 治理门 + 事件总线**分布式实现, **不需要 Meta-Cognitive Agent 类**。与 ADR-0085 §决策 5 "V1 不实施 Meta-Agent 自管理" 完全兼容。

## 不变量

- **不变量 1**: v1.0 17 cases / 65 assertions 测试基线 0 回归
- **不变量 2**: `enable_axis6 = false` (默认) 行为 100% 等同 v1.0; 原 ctor (无 chain_config) 委托新 ctor 默认值
- **不变量 3**: axis6.commit 必须先经 `governor_->commit(ctx)` 返回 `MutationDecision{approved=true}`, 再 emit (L2+ deferred V2)
- **不变量 4**: `max_chain_depth` 默认 3, 图中连续 axis6≠None 节点数硬上限
- **不变量 5**: cognitive_domain specialists 来自 §十四 M5 `register_domain_handler("cognitive", ...)` 注册项, **不可硬编码**; 注册名字符串→Axis6CognitiveDomain enum 映射机制属 Phase 1 (调用方注入 enum 值列表)
- **不变量 6**: `tools/adr_lint.py` + `docs_drift_audit.py` + `openspec validate --strict` 全 PASS
- **不变量 7**: 5 个 contract 头文件零修改 (`include/agenticdsl/contract/`)

## 实施

### 阶段 0 (本 amendment 立即, ~1 sprint)

1. **`include/agenticdsl/cognitive/mcts_workflow_search.h`**:
   - 新增 `Axis6CognitiveDomain` enum (6 值)
   - 新增 `CognitiveDomainChainConfig` struct (决策 2 真实字段)
   - 扩展 `WorkflowNode` 加 `axis6` 字段 (默认 None)
   - 新增 ctor 重载 (含 chain_config); 原 ctor 委托
2. **`src/modules/cognitive/mcts_workflow_search.cpp`**:
   - 子节点扩展时从 `available_specialists` 采样 axis6 (复用 v1.0 UCB1, 不引入特征向量叙述)
   - 连续 axis6≠None 节点数 ≤ max_chain_depth 截断
   - 改进停机 (绝对值)
   - `governor_->commit(ctx).approved` → `axis6.commit` / `axis6.revert` / `axis6.degraded` 事件 (governor 内部同时 emit `mutation.committed`/`mutation.denied`, MCTS 层只追加搜索层审计)
3. **`tests/test_mcts_workflow_search_axis6.cpp`** (新建, ≥6 cases):
   - Axis6 enum 序列化 round-trip
   - 默认 `axis6=None` 行为等同 v1.0 (含原 ctor 委托验证)
   - `enable_axis6=true` + 2-3 specialists → 图中 chain 深度 ≤ max_chain_depth
   - `max_chain_depth=1` 截断
   - `min_eval_improvement` 绝对改进阈值触发停机
   - R6 空 specialists → `axis6.degraded` 事件 + 行为等同 v1.0
4. **ADR-0068 Appendix A v1.8 amendment**: 注册 3 个 axis6.* 主题 (决策 7)
5. **ADR-0061-08 v1.0 状态行 + amendment 状态行 + orchestration doc v1.6 changelog** 同步
6. **docs/README.md adr/skill 表 adr-0061-08 行状态修正** (stale 🔍 Proposed → ✅ V1 Shipped; Oracle 评审新发现)

### 阶段 1 (后续 Sprint, 待 A1-A4 + ADR-0086 立项)

5. **L2 IAgentHook handler**: `cognitive::*` 命名空间, emit `cognitive.understanding_check`
6. **BehavioralEquivalenceEvaluator V2 集成**: understanding 停机判据接线
7. **注册名字符串→enum 映射机制** (不变量 5 的调用方侧实现)
8. **多 CognitiveWorker 实例 chain 执行** + `examples/mcts_axis6_cognitive_domain/` 示例
9. **MutationGovernor L2+ workflow variants** (V2 范围)

**Phase 1 启动前置 blocker**: ADR-0086 信用分配契约立项 (当前未立项, 显式声明)。

### 工作量

| 阶段 | 工作量 | 估时 |
|------|--------|------|
| 阶段 0 (enum + ctor + chain 语义 + 6 cases + ADR-0068 v1.8) | 6 cases + 头文件 + 实现 + 3 主题注册 | 1-1.5 sprint |
| 阶段 1 (Hook + 集成 + 映射 + example) | 完整链路 demo + L2 授权 | 2 sprint |

## 风险

| 风险 | 影响 | 缓解 |
|------|------|------|
| **R1 v1.0 行为漂移** | 17 cases 测试失败 | 不变量 1+2: 默认关闭 + 原 ctor 委托 + 阶段 0 基线回归验证 |
| **R2 specialists 硬编码** | 与 §十四 M5 脱节 | 不变量 5: enum 值注入 + grep 0 specialist import + 映射机制 Phase 1 |
| **R3 嵌套 Search 成本爆炸** | 100³ = 百万迭代 | 决策 5: `max_nested_search_iterations=30` 嵌套预算 (30³ ≈ 2.7万) |
| **R4 Meta-Cognitive 概念复活** | ADR-0085 §决策 5 被绕过 | 决策 8: 显式分布式声明 |
| **R5 L1/L2+ 授权混淆** | commit 无授权 | 决策 6: Phase 0 commit = L1 搜索审计; L2+ workflow variants 生效属 V2; 不变量 3 |
| **R6 §十四 M5 未实施** | Axis6 无 specialists | 决策 5 兜底: 空/None → degraded 事件 + 等同 v1.0 |
| **R7 ADR-0086 未立项** | Phase 1 触发条件无 owner | 决策 4: Phase 1 启动前置显式声明 ADR-0086 立项 |
| **R8 事件主题未注册** | axis6.* 成幻影主题 (违反 ADR-0068) | 决策 7: 附录 A v1.8 amendment 同步 ship (tasks 必含) |

## 关联变更

- `docs/adr/skill/adr-0061-08-aflow-search.md` v1.0 `## 状态` 行追加 v1.1 amendment 链接 (Approved 后) ✅ 已 commit `259b9d1` (docs/README.md adr-0061-08 行 stale 修正 + L121 状态: ✅ Approved + V1 Shipped 2026-08-28)
- `docs/adr/adr-0068-event-emission-contract.md` Appendix A v1.8 amendment (3 个 axis6.* 主题, 阶段 0 同步)
- `docs/architecture/agent-orchestration-architecture-2026-08.md` v1.5 §十七 + §18.10.1 已引用本 amendment
- `docs/README.md` adr/skill 表 adr-0061-08 行状态修正 ✅ 已 commit `259b9d1`
- `openspec/changes/2026-08-31-mcts-axis6-cognitive-domain/` 实施载体 + commit `f6744cc` Oracle 判定头

## 评审证据 (Oracle + Metis 综合)

**Oracle session `ses_fa91c94bdffeOraAXCrgkwK05f`** (2026-08-31):
- **B1 阻塞修复** (commit `283591f`): 实装 API `governor->commit(ctx).approved` 判定替换虚构 authorize
- **B3 阻塞修复** (commit `283591f`): commit-revert 触发统一 (单 `commit_chain` 入口)
- **W4 警告修复** (commit `283591f`): 双发射语义分离 (MCTS 层不重复 emit governance 主题, 4 个 `axis6.*` 主题由 MCTS 独占)
- **W4 归口** (commit `06ddd13`): ADR-0068 版本归口 — Axis6 owns v1.8, T1/T2/T3 改用 v1.9+

**Metis 元审查 (2026-08-31)** 验证:
- 评审针对的是 `v1.1-draft.3` (commit `bc157fb`) + `v2.1` (commit `283591f`) + `v2.1` (commit `06ddd13` 同步)
- 0/42 tasks 实施未启动, 但 openspec validate 已 PASS
- ADR-0068 Appendix A v1.8 amendment 注册需在阶段 0 同步 ship (commit `06ddd13` 已在 tasks.md 8 节声明)

**决策回顾**: 7 大决策 (决策 1-7) + 不变量 1-7 + 8 风险 + Phase 1 前置 (ADR-0086 立项), 全部已在 commit `bc157fb` + `283591f` + `06ddd13` 落地; 评审通过 flip 状态。

## 参考

- AFlow: arXiv:2410.10762 (ICLR 2025 Oral)
- ADR-0061-08 v1.0 — 5 轴模板 + UCB1 + IEvaluator V2 基础
- ADR-0083 v1.1 — IEvaluator V2 BehavioralEquivalence evaluator
- ADR-0084 V1 — MutationGovernance L1 (V2 范围 L2+)
- ADR-0061-09 GEPA Loop (Axis6 Reflect specialist)
- orchestration-architecture-2026-08.md v1.5 §十四 M5 + §18.10.1
- Oracle 评审 session `ses_fad1df19effezRRly8ZxHJsp2P` (5 blocker 修复依据)
