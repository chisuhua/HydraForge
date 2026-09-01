// include/agenticdsl/cognitive/mcts_workflow_search.h
// 功能描述：AFlow 风格 MCTS 工作流搜索编排层契约 (T20, ADR-0061-08, Sprint 24)
//          5 轴模板实例化工作流图 (WorkflowNode/WorkflowEdge/WorkflowGraph)
//          + UCB1 MCTS 搜索 (选择/扩展/模拟/反向传播)
//          + IEvaluator V2 (CompositeEvaluator) 奖励
//          + BehavioralRegressionGate 回归门 (包装 T14 自由函数)
//          + IMutationGovernor 变异授权 (L1 workflow variants)
//          本类是搜索编排层，不是契约层：全部依赖既有契约接口
//          (IEvaluator / IMutationGovernor / IInteractionBus / behavioral_regression
//          自由函数 / TrajectoryIR::hash)，不修改任何既有契约头文件。
// 设计依据：docs/adr/skill/adr-0061-08-aflow-search.md
//          + openspec/changes/t20-aflow-mcts/tasks.md
// 作者：HydraForge Sprint 24 T20 ship
// 最后修改日期：2026-08-28
#pragma once

#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/imutation_governance.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/testing/behavioral_regression.h"
#include "modules/budget/budget_controller.h"
#include "agenticdsl/types/execution_trace.h"
#include "core/types/tool_result.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace agenticdsl {

// ============================================================================
// TaskSpec — 搜索任务描述 (V1: 高层任务摘要 + 图规模上限)
// ============================================================================
struct TaskSpec {
  std::string task_id;   // 任务唯一标识
  std::string goal;      // 高层任务描述 (V1 不解析语义)
  int max_nodes = 8;     // 工作流图节点数上限
};

// ============================================================================
// WorkflowNode — 5 轴模板实例化 (ADR-0061-08 §决策 1)
// T2 amendment: 第 6 轴 Axis6CognitiveDomain (cognitive domain composition chain)
struct WorkflowNode {
  enum class Axis1Template { Linear, Branching, Loop, Parallel };
  enum class Axis2Param { Temperature, MaxTokens, TopP };
  enum class Axis3Tool { None, Calculator, Search, Custom };
  enum class Axis4Control { Sequential, Parallel, Loop };
  enum class Axis5Error { Retry, Fallback, Abort };
  enum class Axis6CognitiveDomain { None, Reflect, Search, Compile, Meta_Select, Reason };

  std::string id;
  Axis1Template axis1 = Axis1Template::Linear;
  Axis2Param axis2 = Axis2Param::Temperature;
  Axis3Tool axis3 = Axis3Tool::None;
  Axis4Control axis4 = Axis4Control::Sequential;
  Axis5Error axis5 = Axis5Error::Retry;
  Axis6CognitiveDomain axis6 = Axis6CognitiveDomain::None;
};

// ============================================================================
// WorkflowEdge — 模板组合边
// ============================================================================
struct WorkflowEdge {
  std::string from_node_id;
  std::string to_node_id;
  std::string combination_rule;  // "sequential" | "conditional" | "loop"
};

// ============================================================================
// WorkflowGraph — 搜索状态 (DAG 工作流图)
// ============================================================================
struct WorkflowGraph {
  std::string task_id;              // 搜索任务标识 (T1 materializer 输出路径用)
  std::vector<WorkflowNode> nodes;
  std::vector<WorkflowEdge> edges;
};

// ============================================================================
// UCB1 上置信界 (纯函数, 实现与测试共用)
//   child_visits == 0 → +inf (未访问子节点强制优先探索)
//   否则 quality + exploration_weight * sqrt(log(parent_visits) / child_visits)
// ============================================================================
inline double ucb1_value(double child_q, int child_visits, int parent_visits,
                         double exploration_weight) {
  if (child_visits <= 0) {
    return std::numeric_limits<double>::infinity();
  }
  if (parent_visits <= 0) {
    return child_q;
  }
  const double log_parent = std::log(static_cast<double>(parent_visits));
  const double denom = static_cast<double>(child_visits);
  return child_q + exploration_weight * std::sqrt(log_parent / denom);
}

// ============================================================================
// BehavioralRegressionGate — T14 行为回归门的 MCTS 层包装
//   包装 compute_fingerprint + hotelling_t2_test 自由函数 (契约零修改)。
//   allows(): 仅 Verdict::Fail 拒绝 (Pass/Inconclusive 放行)
// ============================================================================
class BehavioralRegressionGate {
 public:
  explicit BehavioralRegressionGate(RegressionBudget budget = {})
      : budget_(std::move(budget)) {}

  Verdict compare(const std::vector<ToolResult>& baseline,
                  const std::vector<ToolResult>& candidate) const {
    return hotelling_t2_test(compute_fingerprint(baseline),
                             compute_fingerprint(candidate), budget_);
  }

  bool allows(const std::vector<ToolResult>& baseline,
              const std::vector<ToolResult>& candidate) const {
    return compare(baseline, candidate) != Verdict::Fail;
  }

 private:
  RegressionBudget budget_;
};

// ============================================================================
// MCTSWorkflowSearch — AFlow 风格工作流搜索编排层 (V1, L1_prompt only)
// ============================================================================
class MCTSWorkflowSearch {
 public:
  struct SearchConfig {
    int max_iterations = 100;
    double exploration_weight = 1.414;
    std::string source_id = "R_T20_AFLOW";
    int max_children_per_node = 3;
    std::uint32_t random_seed = 42;
  };

  struct SearchResult {
    std::shared_ptr<WorkflowGraph> best_workflow;  // 最优工作流 (可空)
    double best_reward = -1.0;                     // 归一化 q ∈ [0,1]
    int iterations_used = 0;
    bool success = false;
    std::string failure_mode;  // 空 = success
  };

  /**
   * @brief 便捷构造 (默认 SearchConfig + 无事件总线)
   */
  MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                     std::shared_ptr<IMutationGovernor> governor,
                     std::shared_ptr<BehavioralRegressionGate> regression_gate);

  /**
   * @param evaluator      非空 IEvaluator (V2 CompositeEvaluator 注入)
   * @param governor       非空 IMutationGovernor (L1 变异授权)
   * @param regression_gate 可空 BehavioralRegressionGate (空 = 跳过回归门)
   * @param config         搜索配置
   * @param bus            可空 IInteractionBus (mcts.* 事件发射)
   */
  MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                     std::shared_ptr<IMutationGovernor> governor,
                     std::shared_ptr<BehavioralRegressionGate> regression_gate,
                     SearchConfig config,
                     std::shared_ptr<IInteractionBus> bus = nullptr);

  // T2 + T6 ctor overload: budget_controller 注入
  MCTSWorkflowSearch(std::shared_ptr<IEvaluator> evaluator,
                     std::shared_ptr<IMutationGovernor> governor,
                     std::shared_ptr<BehavioralRegressionGate> regression_gate,
                     SearchConfig config,
                     std::shared_ptr<IInteractionBus> bus,
                     std::shared_ptr<IBudgetController> budget_controller);

  // T2 commit API (单主体归因 source_id=MCTS)
  struct CognitiveDomainChainConfig {
    std::vector<WorkflowNode::Axis6CognitiveDomain> specialists;
    int max_chain_depth = 3;  // 决策 5: 硬截断
  };
  struct CommitResult {
    bool approved = false;
    std::string failure_mode;
    std::string mutation_id;
  };
  CommitResult commit_chain(const std::vector<WorkflowNode::Axis6CognitiveDomain>& chain);
  bool can_execute(const WorkflowNode& node) const;

  SearchResult search(const TaskSpec& spec);

 private:
  std::shared_ptr<IEvaluator> evaluator_;
  std::shared_ptr<IMutationGovernor> governor_;
  std::shared_ptr<BehavioralRegressionGate> regression_gate_;
  std::shared_ptr<IInteractionBus> bus_;
  std::shared_ptr<IBudgetController> budget_controller_;
  CognitiveDomainChainConfig cognitive_domain_chain_;
  SearchConfig config_;
};

}  // namespace agenticdsl
