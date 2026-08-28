# Design: t20-aflow-mcts

## Context

ADR-0061-08 (AFlow-style MCTS 工作流搜索) 处于 🔍 Proposed (P2, v2 候选)，但 cap-map §八 T20 已 ✅ APPROVED (Sprint 26 末 R 轨 spike)。本 change 实施 V1 MCTSWorkflowSearch + 同步翻转 ADR-0061-08 状态。

所有前置 2026-08-26/27/28 已 ship：
- T15 Trajectory IR (commit `9c7c6da`)
- IEvaluator V1 (commit `21dd622`) + V2 (commit `314561e`)
- T17 SkillCompiler (commit `21dd622`)
- T19 GEPA Phase 2 commit (commit `35578d9`)
- T21 Prompt Evidence Gate (commit `abd3bd3`)
- G11 变异治理 ✅ Closed

T20 是**搜索编排层**，不是契约层。所有依赖通过既有 API 调用，零契约修改。

## Scope Boundaries

### 范围 IN
- MCTSWorkflowSearch 编排引擎 (`src/modules/cognitive/mcts_workflow_search.{h,cpp}`)
- 5 轴模板 + WorkflowNode + WorkflowEdge + WorkflowGraph
- UCB1 选择算法 (selection/expansion/simulation/backpropagation)
- IEvaluator V2 CompositeEvaluator 集成
- BehavioralRegressionGate 集成
- MutationGovernor L1 授权
- 4 个 `mcts.*` 事件主题注册 (ADR-0068 v1.4 → v1.5)
- ≥ 10 测试 cases
- ADR-0061-08 状态翻转 🔍 → ✅
- 文档同步

### 范围 OUT
- 既有契约修改 (IEvaluator V2 / SkillCompiler / TrajectoryIR / MutationGovernor / BehavioralRegressionGate)
- 真实 LLM API 集成
- L2+ mutation variants
- Pareto 多目标评估
- TrajectoryFidelity 评估
- 跨 session 经验积累

## Design Decisions

### D1 — MCTSWorkflowSearch 是编排层

```cpp
class MCTSWorkflowSearch {
public:
    MCTSWorkflowSearch(shared_ptr<IEvaluator> evaluator,
                       shared_ptr<MutationGovernor> governor,
                       shared_ptr<BehavioralRegressionGate> regression_gate,
                       SearchConfig config = {});
    SearchResult search(const TaskSpec& spec);
};
```

理由：3 个既有契约作为依赖注入，MCTS 仅编排搜索循环。

### D2 — 5 轴模板状态空间

每节点 5 维实例化：
- Axis1: Template variant (Linear / Branching / Loop / Parallel)
- Axis2: Parameter (Temperature / MaxTokens / TopP)
- Axis3: Tool selection (None / Calculator / Search / Custom)
- Axis4: Control flow (Sequential / Parallel / Loop)
- Axis5: Error handling (Retry / Fallback / Abort)

理由：5 轴覆盖 AFlow paper 关键维度，模板组合爆炸可控。

### D3 — UCB1 选择 + 经典 MCTS 4 阶段

```
Selection (UCB1) → Expansion (新子节点) → Simulation (reward) → Backpropagation (visits + reward_sum)
```

UCB1: `argmax(quality + c * sqrt(log(parent_visits) / child_visits))`

理由：经典 MCTS 算法成熟，AFlow 改进 deferred V2。

### D4 — IEvaluator V2 加权聚合

```cpp
auto evaluator = std::make_shared<CompositeEvaluator>(
    std::vector<std::shared_ptr<IEvaluator>>{
        std::make_shared<TaskSuccessEvaluator>(),        // V1, weight=0.3
        std::make_shared<BehavioralEquivalenceEvaluator>() // V2, weight=0.7
    },
    std::vector<double>{0.3, 0.7}
);
```

理由：BehavioralEquivalence 权重高于 TaskSuccess（工作流质量更关键）。

### D5 — L1 only (V1 简化)

V1 仅提交 `mutation_kind="L1_prompt"` workflow variants。L2/L3/L4 由 MutationGovernor V1 拒绝（per G11 V1 边界）。

理由：L3 skill 需 SkillCompiler V2 schema（deferred），L4 weight 显式禁止。

### D6 — Mock 模板实例化（V1 简化）

V1 不调用 LLM 生成 workflow 节点，使用预设 5 轴模板组合：

```cpp
WorkflowNode generate_random_node(std::mt19937& rng) {
    WorkflowNode node;
    node.axis1 = static_cast<Axis1Template>(rng() % 4);
    node.axis2 = static_cast<Axis2Param>(rng() % 3);
    // ...
    return node;
}
```

理由：避免外部 LLM API 依赖，确保测试可重现。

### D7 — 4 个 mcts.* 事件主题

| 主题 | Owner | Payload |
|------|-------|---------|
| `mcts.search.started` | MCTSWorkflowSearch | `search_id`, `task_spec` |
| `mcts.search.iteration` | MCTSWorkflowSearch | `iteration`, `reward`, `node_id` |
| `mcts.search.completed` | MCTSWorkflowSearch | `search_id`, `best_workflow`, `best_reward`, `iterations_used` |
| `mcts.search.failed` | MCTSWorkflowSearch | `search_id`, `failure_reason` |

理由：补全 MCTS 搜索的完整事件流。

## Risks

| 风险 | 缓解 |
|---|---|
| MCTS 状态空间爆炸 | max_iterations=100 限制 + UCB1 平衡 |
| Mock 模板质量差 | 5 轴预设覆盖主要 workflow 模式 |
| IEvaluator V2 加权不准确 | 复用 IEvaluator V2 已 ship 实现 |
| MutationGovernor 拒绝 | L1 only + G11 V1 已 ship |
| ctest 数字硬编码 | tasks.md 禁止 + 动态基线 |
| ADR-0061-08 状态翻转风险 | 与 ADR-0083/0084 同模式已 ship 验证 |

## Verification Gates

- ≥ 10 cases test_mcts_workflow_search PASS
- 既有契约 0 diff
- ctest 191+ 全量零回归（动态计数）
- adr_lint 82 ADR PASS
- docs_drift_audit 0 NEW drift
- ADR-0061-08 ✅ Approved

## Dependencies

### 满足
- ✅ T15 Trajectory IR ship
- ✅ IEvaluator V1+V2 ship
- ✅ T17 SkillCompiler ship
- ✅ G11 变异治理 ✅ Approved + ship
- ✅ T14 行为回归 ship
- ✅ T19 GEPA Phase 2 commit ship
- ✅ T21 Prompt Evidence Gate ship

### 不依赖
- T22 Fine-tune (V2 事件驱动)
- V3+ 评估器
- 真实 LLM API

## Out of Scope (V2+ deferred)

- 真实 LLM API 集成
- L2+ mutation variants
- Pareto 多目标评估
- TrajectoryFidelity 评估
- 跨 session 经验积累
- 在线 workflow 部署

## Success Criteria

- T20 AFlow MCTS V1 ✅ Shipped
- 既有契约 0 diff
- 10+ cases PASS
- ctest 191+ 全量零回归
- ADR-0061-08 ✅ Approved
- cap-map §一 +1 (能力 #29)
- OpenSpec archive 完成