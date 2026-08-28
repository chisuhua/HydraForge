# Tasks: t20-aflow-mcts

> **TDD 5 步结构**: 每任务按 Write failing test → Verify fail → Implement → Verify pass → Commit
> **关键不变量**: 既有契约**零修改**（IEvaluator V2 / SkillCompiler / TrajectoryIR / MutationGovernor / BehavioralRegressionGate）。T20 是**搜索编排层**，不是契约层。
> **V1 简化**: Mock 模板实例化避免外部 LLM 依赖；MCTS UCB1 + IEvaluator V2 加权聚合。

## Phase 0: MCTSWorkflowSearch 契约 (估时 0.3 sprint)

- [x] **T0.1** Write failing test: `tests/test_mcts_workflow_search.cpp` 骨架（≥ 10 cases 占位）
  - `mcts_workflow_initialization`
  - `mcts_node_5_axis_template`
  - `mcts_edge_combination_rules`
  - `mcts_search_ucb1_selection`
  - `mcts_search_expansion`
  - `mcts_search_simulation`
  - `mcts_search_backpropagation`
  - `mcts_reward_evaluator_v2_weighted`
  - `mcts_regression_gate_integration`
  - `mcts_mutation_governor_authorization`
- [x] **T0.2** Verify fail: 编译失败（`fatal error: 'cognitive/mcts_workflow_search.h' file not found`）
- [x] **T0.3** Implement minimal: `include/agenticdsl/cognitive/mcts_workflow_search.h`:
  ```cpp
  namespace agenticdsl {
  struct WorkflowNode {
      // 5 轴模板实例化
      enum class Axis1Template { Linear, Branching, Loop, Parallel };
      enum class Axis2Param { Temperature, MaxTokens, TopP };
      enum class Axis3Tool { None, Calculator, Search, Custom };
      enum class Axis4Control { Sequential, Parallel, Loop };
      enum class Axis5Error { Retry, Fallback, Abort };
      Axis1Template axis1;
      Axis2Param axis2;
      Axis3Tool axis3;
      Axis4Control axis4;
      Axis5Error axis5;
  };
  struct WorkflowEdge {
      std::string from_node_id;
      std::string to_node_id;
      std::string combination_rule;  // "sequential" | "conditional" | "loop"
  };
  struct WorkflowGraph {
      std::vector<WorkflowNode> nodes;
      std::vector<WorkflowEdge> edges;
  };
  class MCTSWorkflowSearch {
  public:
      struct SearchConfig {
          int max_iterations = 100;
          double exploration_weight = 1.414;
          std::string source_id = "R_T20_AFLOW";
      };
      struct SearchResult {
          std::shared_ptr<WorkflowGraph> best_workflow;
          double best_reward;
          int iterations_used;
      };
      MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                         std::shared_ptr<MutationGovernor> governor,
                         std::shared_ptr<BehavioralRegressionGate> regression_gate,
                         SearchConfig config = {});
      SearchResult search(const TaskSpec& spec);
  };
  }
  ```
- [x] **T0.4** Verify pass: 编译成功，10 cases 编译通过（运行时仍 FAIL）
- [x] **T0.5** Commit: `feat(cognitive): MCTSWorkflowSearch contract skeleton (T0)`

## Phase 1: MCTS 算法核心 (估时 0.5 sprint)

- [x] **T1.1** Write failing test: `mcts_ucb1_selection_best_arm` (UCB1 选择最优子节点)
- [x] **T1.2** Write failing test: `mcts_ucb1_selection_exploration_exploitation_balance` (UCB1 平衡探索与利用)
- [x] **T1.3** Write failing test: `mcts_search_convergence_100_iterations` (100 iterations 收敛)
- [x] **T1.4** Verify fail: 3 cases FAIL（UCB1 未实现）
- [x] **T1.5** Implement: `src/modules/cognitive/mcts_workflow_search.cpp`:
  - `MCTSTree` 内部结构: `std::vector<MCTSNode>` + `std::unordered_map<node_id, MCTSNode>`
  - `MCTSNode`: state (WorkflowGraph) + children + visits + reward_sum
  - UCB1 选择: `argmax(quality + exploration_weight * sqrt(log(parent_visits) / visits))`
  - 扩展 (expansion): 添加新子节点 (新 5 轴模板实例化)
  - 模拟 (simulation): 评估 reward
  - 反向传播 (backpropagation): 更新 visits + reward_sum
- [x] **T1.6** Verify pass: 3 cases PASS + Phase 0 编译通过
- [x] **T1.7** Commit: `feat(cognitive): MCTS UCB1 + expansion/simulation/backpropagation (T1)`

## Phase 2: 集成 IEvaluator V2 + 回归门禁 + 变异授权 (估时 0.4 sprint)

- [x] **T2.1** Write failing test: `mcts_reward_evaluator_v2_composite` (CompositeEvaluator 加权聚合)
- [x] **T2.2** Write failing test: `mcts_regression_gate_rejects_decline` (BehavioralRegressionGate 拒绝回归)
- [x] **T2.3** Write failing test: `mcts_mutation_governor_authorizes_commit` (L1 workflow commit 授权)
- [x] **T2.4** Verify fail: 3 cases FAIL
- [x] **T2.5** Implement: MCTS 集成层:
  - 奖励函数: 调用 `IEvaluator::evaluate(trace)` (V2 CompositeEvaluator)
  - 回归门禁: 调用 `BehavioralRegressionGate::compute_fingerprint + hotelling_t2_test`
  - 变异授权: 调用 `MutationGovernor::propose()` → `commit()` 或 reject
- [x] **T2.6** Verify pass: 3 cases PASS
- [x] **T2.7** Commit: `feat(cognitive): MCTS integration with V2 evaluator + regression gate + governor (T2)`

## Phase 3: 事件发射 + ADR-0068 v1.5 (估时 0.2 sprint)

- [x] **T3.1** Write failing test: `mcts_event_emission` (4 事件主题发射)
- [x] **T3.2** Verify fail: 事件未发射
- [x] **T3.3** Implement: MCTS 添加 EventBuilder emit:
  - 搜索开始 → emit `mcts.search.started` (search_id, task_spec)
  - 每次 iteration → emit `mcts.search.iteration` (iteration, reward, node_id)
  - 搜索完成 → emit `mcts.search.completed` (search_id, best_workflow, best_reward, iterations_used)
  - 搜索失败 → emit `mcts.search.failed` (search_id, failure_reason)
- [x] **T3.4** Modify `docs/adr/adr-0068-event-emission-contract.md` 附录 A v1.4 → v1.5:
  - 注册 4 个 `mcts.*` 主题（owner: MCTSWorkflowSearch）
- [x] **T3.5** Verify pass: 1 case PASS + T1+T2 全过
- [x] **T3.6** Commit: `feat(cognitive): MCTS event emission + ADR-0068 v1.5 (T3)`

## Phase 4: 文档同步 + ship (估时 0.2 sprint)

- [x] **T4.1** Modify `docs/adr/skill/adr-0061-08-aflow-search.md` 头部 `##状态`:
  - 🔍 Proposed → ✅ Approved (V1 ship, 评审通过 2026-08-28)
- [x] **T4.2** Modify `docs/architecture/capability-application-map-2026-08.md`:
  - 头部 v2.2 → v2.3 + 最后验证 2026-08-28
  - §一 +1（能力 #29 AFlow MCTS）
  - §八 T20 → ✅ Completed
  - §七 changelog v2.3
- [x] **T4.3** Modify `docs/active-status.md` §一 T20 跟踪段
- [x] **T4.4` Modify `docs/architecture/self-evolution-architecture-2026-08.md` §四
- [x] **T4.5` Modify `docs/architecture/adr-implementation-status-gap-analysis.md` §一 总计行 + Approved 列表追加 ADR-0061-08
- [x] **T4.6` Verify: `python3 tools/adr_lint.py` + `docs_drift_audit.py` 全通过
- [x] **T4.7` Verify: `openspec validate --changes --strict` PASS
- [x] **T4.8` Verify: `ctest --output-on-failure` 全量 0 回归（动态基线）
- [x] **T4.9` Commit: `feat(mcts): ship T20 AFlow MCTS V1 - C2 自进化高级工作流搜索解锁`
- [x] **T4.10` `openspec archive t20-aflow-mcts`

## 总估时

- Phase 0: 0.3 sprint
- Phase 1: 0.5 sprint
- Phase 2: 0.4 sprint
- Phase 3: 0.2 sprint
- Phase 4: 0.2 sprint
- **总估时: ~1.6 sprint**（接近 1-2 月，符合 cap-map §八 T20 估时）

## 明确 out of scope (V2+ deferred)

- 真实 LLM API 集成生成 workflow 节点（V1 仅 Mock）
- Pareto 多目标评估（依赖 IEvaluator V3+）
- TrajectoryFidelity 评估（依赖 T15 V2 schema）
- 跨 session 经验积累（V2 + SessionManager V2）
- 在线 workflow 部署（V2 + EnvBackend V2）

## 关键不变量（强制遵守）

- ❌ IEvaluator V2 / SkillCompiler / TrajectoryIR / MutationGovernor / BehavioralRegressionGate 公开 API **零修改**
- ❌ 修改既有 ADR Approved 状态（除 ADR-0061-08 本身状态翻转）
- ❌ 真实 LLM API 调用（仅 Mock）
- ❌ 在测试失败时强行 commit
- ❌ 硬编码 ctest 数字

## V1 简化策略

- **Mock 模板实例化**: V1 不调用 LLM 生成 workflow 节点，使用预设 5 轴模板组合
- **UCB1 + 经典 MCTS**: 实施标准 MCTS 算法，AFlow 改进 deferred V2
- **IEvaluator V2 加权**: TaskSuccess (0.3) + BehavioralEquivalence (0.7) 加权聚合
- **MutationGovernor L1 only**: V1 仅提交 L1 workflow variants，L2+ deferred