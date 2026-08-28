# t20-aflow-mcts

## Why

ADR-0061-08 (AFlow-style MCTS 工作流搜索) 处于 🔍 Proposed 状态（P2, v2 候选），但 cap-map §八 T20 已 ✅ APPROVED (Sprint 26 末 R 轨 spike)，所有前置已 ship：
- ✅ T15 Trajectory IR ship（commit `9c7c6da`，独立序列化视图）
- ✅ IEvaluator V1 ship（commit `21dd622`）+ V2 ship（commit `314561e`，BehavioralEquivalence + Composite）
- ✅ T19 GEPA Phase 2 commit ship（commit `35578d9`，单 agent 反思循环 MVP）
- ✅ T21 Prompt Evidence Gate ship（commit `abd3bd3`，prompt 质量门控）
- ✅ T17 SkillCompiler ship（commit `21dd622`，L3 变异对象生成器）

**ADR-0061-08 状态需同步翻转**（与 ADR-0083 / ADR-0084 同样模式）：
- 🔍 Proposed → ✅ Approved（V1 实施 ship 后翻转）

**Oracle 评审关键发现** (cap-map §八 + ADR-0061-08 §决策):
- AFlow (ICLR 2025 Oral, arXiv:2410.10762)：MCTS 在代码表示的工作流空间内自动发现 workflow
- 6 个 benchmark 比手设计 +5.7%、比自动化基线 +19.5%
- 决策 1 — MCTS 状态空间：节点 = 现有 5 轴模板的实例化，边 = 模板组合规则
- 决策 2 — 集成方式：`MCTSWorkflowSearch::search(spec, max_iterations=100)`
- 决策 3 — V1 简化（cap-map §八 T20）：spike 阶段，1-2 月 R 轨 spike

**审计依据**：
- ADR-0061-08 🔍 Proposed (2026-07-16起草，v2 候选)
- cap-map §八 T20 ✅ APPROVED (ADR-0083 ✅ + T15 ✅)
- T15 + IEvaluator V1+V2 + T19 + T21 全部 ship

**前置依赖**（全部已满足）：
- ✅ T15 Trajectory IR ship (工作流搜索空间)
- ✅ IEvaluator V1+V2 ship (奖励信号可比性 + CompositeEvaluator 加权)
- ✅ T14 行为回归 ship (regression test pass rate)
- ✅ T17 SkillCompiler ship (workflow graph 生成)
- ✅ T19 GEPA Phase 2 commit ship (单 agent 反思循环 R 轨 spike 类比)
- ✅ T21 Prompt Evidence Gate ship (prompt 质量门控前置)
- ✅ ADR-0074 ✅ Approved (Wave 2 Phase 2.2 ship)

## What Changes

**新增 MCTSWorkflowSearch 引擎** (`src/modules/cognitive/mcts_workflow_search.{h,cpp}`):

1. **MCTS 状态空间** (ADR-0061-08 §决策 1):
   - 节点: WorkflowNode (现有 5 轴模板的实例化):
     - axis 1: template variant (workflow shape)
     - axis 2: parameter values (temperature, max_tokens, etc.)
     - axis 3: tool selection (subset of IToolRegistry)
     - axis 4: control flow (sequential / parallel / loop)
     - axis 5: error handling strategy (retry / fallback / abort)
   - 边: WorkflowEdge (模板组合规则)
   - 状态: WorkflowGraph (DAG representation)

2. **MCTS 搜索算法** (经典 UCB1 + AFlow 改进):
   ```cpp
   class MCTSWorkflowSearch {
   public:
       struct SearchConfig {
           int max_iterations = 100;        // AFlow 默认值
           double exploration_weight = 1.414; // UCB1 sqrt(2)
           double discount_factor = 1.0;
           std::string source_id = "R_T20_AFLOW";
       };
       struct SearchResult {
           std::shared_ptr<WorkflowGraph> best_workflow;
           double best_reward;
           std::vector<SearchIteration> iterations;
       };
       
       MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                          std::shared_ptr<MutationGovernor> governor,
                          std::shared_ptr<BehavioralRegressionGate> regression_gate,
                          SearchConfig config = {});
       
       SearchResult search(const TaskSpec& spec);
   };
   ```

3. **奖励函数** (复用 IEvaluator V2 CompositeEvaluator):
   - TaskSuccessEvaluator (V1, weight=0.3) + BehavioralEquivalenceEvaluator (V2, weight=0.7)
   - 加权聚合: scalar 加权平均 + quality 众数

4. **回归门禁** (复用 T14 BehavioralRegressionGate):
   - compute_fingerprint + hotelling_t2_test 验证搜索到的工作流不引入退化
   - 失败 → reject candidate workflow, 继续搜索

5. **变异授权** (复用 G11 MutationGovernor):
   - 每个 MCTS iteration 提交 workflow 候选 → MutationGovernor::propose()
   - L1/L2 门禁 → commit() (实际执行 workflow 更新)
   - L3/L4 拒绝 → reject candidate

6. **事件发射** (新增 4 个 `mcts.*` 主题):
   - `mcts.search.started` (MCTS 搜索开始)
   - `mcts.search.iteration` (每次 iteration 结束 + reward)
   - `mcts.search.completed` (搜索完成 + best_workflow)
   - `mcts.search.failed` (搜索失败 + reason)

**新增测试** (`tests/test_mcts_workflow_search.cpp`):
- ≥ 10 cases 覆盖完整 MCTS 搜索流程
- Mock UCB1 选择 + Mock 模板实例化
- 真实 IEvaluator V2 + MutationGovernment + BehaviorRegressionGate
- 状态空间爆炸边界测试 (max_iterations=100 收敛)
- 回归拒绝测试 + 变异授权拒绝测试
- 4 事件主题发射验证

## Impact

**影响范围**:
- SkillCompiler **零修改** (作为依赖使用)
- TrajectoryIR **零修改** (作为搜索空间表示)
- IEvaluator V2 **零修改** (作为奖励函数)
- MutationGovernor **零修改** (作为变异授权)
- BehavioralRegressionGate **零修改** (作为回归门禁)
- ADR-0068 仅**附录 A 增量注册** 4 个新主题 (符合既有 amendment 流程)

**下游解锁**:
- C2 自进化高级 (cap-map §三)
- T22 Fine-tune 事件驱动训练路径 (workflow 自动化)
- B7 自进化基础应用 MVP (GEPA 单 agent + AFlow 多 workflow 协同)

**V1 边界** (per ADR-0061-08 §决策 3 + cap-map §八 T20):
- ✅ MCTS 状态空间 (5 轴模板)
- ✅ UCB1 选择算法
- ✅ IEvaluator V2 加权聚合奖励
- ✅ Mock 模板实例化（V1 简化，避免外部 LLM）
- ⏸ 真实 LLM API 集成生成 workflow 节点 (V2 deferred)
- ⏸ Pareto 多目标评估 (依赖 IEvaluator V3+)
- ⏸ 跨 session 经验积累 (V2)

**Breaking Changes**: 无 (仅新增模块与 4 个 ADR-0068 主题)

## ship gate 验证

- `python3 tools/adr_lint.py` 通过 (≥82 ADR)
- `python3 tools/docs_drift_audit.py` 通过 (无新增 CRITICAL drift)
- `openspec validate --changes --strict` PASS
- `ctest --output-on-failure` 全量 0 回归 (动态基线, 190→191 净增)
- `ctest -R test_mcts_workflow_search` ≥ 10 cases / ≥ 30 assertions PASS
- `grep "class MCTSWorkflowSearch" include/agenticdsl/cognitive/` 命中
- ADR-0068 附录 A 新增 4 个 `mcts.*` 主题
- cap-map §一 +1 (新能力 #29 AFlow MCTS)
- ADR-0061-08 头部 🔍 Proposed → ✅ Approved (V1 ship 同步状态翻转)
- cap-map §八 T20 → ✅ Completed

## 关联文档

- `docs/adr/skill/adr-0061-08-aflow-search.md` (待状态翻转 🔍 → ✅)
- `docs/adr/adr-0061-agent-evolution-and-solidification.md` (父 ADR)
- `docs/adr/adr-0061-02-behavioral-regression.md` (T14 ship)
- `docs/adr/adr-0061-03-skill-compiler.md` (T17 ship)
- `docs/adr/adr-0068-event-emission-contract.md` (附录 A v1.4 → v1.5)
- `include/agenticdsl/contract/ievaluator.h` (V2 ship, 零修改)
- `include/agenticdsl/contract/imutation_governance.h` (G11 ship, 零修改)
- `include/agenticdsl/ir/trajectory_ir.h` (T15 ship, 零修改)
- `include/agenticdsl/cognitive/skill_compiler.h` (T17 ship, 零修改)
- `include/agenticdsl/testing/behavioral_regression.h` (T14 ship, 零修改)
- ADR-0074 ✅ Approved (T21 ship, prompt 质量门控前置)
- AFlow paper: arXiv:2410.10762 (ICLR 2025 Oral)