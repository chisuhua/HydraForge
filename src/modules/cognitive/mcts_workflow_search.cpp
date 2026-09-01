// src/modules/cognitive/mcts_workflow_search.cpp
// 功能描述：AFlow 风格 MCTS 工作流搜索编排层实现 (T20, ADR-0061-08, Sprint 24)
//          标准 MCTS 四步: UCB1 选择 → 扩展 (5 轴模板实例化) → 模拟
//          (IEvaluator V2 奖励) → 反向传播 (visits + reward_sum)。
//          Phase 2 集成: BehavioralRegressionGate 回归门 (候选劣化拒绝)
//          + IMutationGovernor 变异授权 (propose → commit, V1 仅 L1_prompt)。
//          V1 简化: Mock 模板实例化 (不调用 LLM); AFlow 改进 deferred V2。
// 设计依据：docs/adr/skill/adr-0061-08-aflow-search.md
//          + openspec/changes/t20-aflow-mcts/tasks.md
// 作者：HydraForge Sprint 24 T20 ship
// 最后修改日期：2026-08-28

#include "agenticdsl/cognitive/mcts_workflow_search.h"

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/ir/trajectory_ir.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"
#include "core/types/tool_result.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace agenticdsl {

namespace {

// ============================================================================
// MCTSTree 内部结构 (V1: vector 存储 + 稀疏索引)
// ============================================================================
struct MCTSNode {
  int id = 0;
  WorkflowGraph state;  // 该节点代表的工作流图 (搜索状态)
  std::vector<int> children;
  int parent = -1;
  int visits = 0;
  double reward_sum = 0.0;
};

// ============================================================================
// 5 轴模板随机实例化 (V1 mock: 预设模板组合, 不触发 LLM)
// ============================================================================
WorkflowNode generate_random_node(std::mt19937& rng) {
  WorkflowNode node;
  node.axis1 = static_cast<WorkflowNode::Axis1Template>(rng() % 4);
  node.axis2 = static_cast<WorkflowNode::Axis2Param>(rng() % 3);
  node.axis3 = static_cast<WorkflowNode::Axis3Tool>(rng() % 4);
  node.axis4 = static_cast<WorkflowNode::Axis4Control>(rng() % 3);
  node.axis5 = static_cast<WorkflowNode::Axis5Error>(rng() % 3);
  return node;
}

const char* combination_rule(std::mt19937& rng) {
  static const char* kRules[] = {"sequential", "conditional", "loop"};
  return kRules[rng() % 3];
}

const char* axis3_name(WorkflowNode::Axis3Tool t) {
  switch (t) {
    case WorkflowNode::Axis3Tool::None:
      return "none";
    case WorkflowNode::Axis3Tool::Calculator:
      return "calculator";
    case WorkflowNode::Axis3Tool::Search:
      return "search";
    case WorkflowNode::Axis3Tool::Custom:
      return "custom";
  }
  return "none";
}

bool has_search_tool(const WorkflowGraph& g) {
  for (const auto& n : g.nodes) {
    if (n.axis3 == WorkflowNode::Axis3Tool::Search) {
      return true;
    }
  }
  return false;
}

bool has_parallel(const WorkflowGraph& g) {
  for (const auto& n : g.nodes) {
    if (n.axis1 == WorkflowNode::Axis1Template::Parallel ||
        n.axis4 == WorkflowNode::Axis4Control::Parallel) {
      return true;
    }
  }
  return false;
}

// RewardSignal.scalar ∈ [-1,1] → 归一化 q ∈ [0,1] (UCB1 使用)
double normalize(double scalar) { return (scalar + 1.0) / 2.0; }

WorkflowGraph make_root_graph(const TaskSpec& spec) {
  WorkflowGraph g;
  WorkflowNode start;
  start.id = spec.task_id + ":wf:start";
  g.nodes.push_back(std::move(start));
  return g;
}

// V1 mock: 合成工作流执行轨迹 (不触发真实执行/LLM)
ExecutionTrace trace_of(const WorkflowGraph& graph, const TaskSpec& spec,
                        int iteration) {
  ExecutionTrace trace;
  trace.trace_id = "mcts:" + spec.task_id + ":" + std::to_string(iteration);
  trace.final_result = ToolResult::success(nlohmann::json{
      {"task_id", spec.task_id},
      {"iteration", iteration},
      {"nodes", static_cast<int>(graph.nodes.size())},
      {"edges", static_cast<int>(graph.edges.size())},
      {"has_search_tool", has_search_tool(graph)},
      {"has_parallel", has_parallel(graph)}});
  return trace;
}

// V1 mock: 合成行为回归特征结果 (decline=true → 候选劣于基线, 指纹显著偏离)
std::vector<ToolResult> synthesize_results(const WorkflowGraph& /*graph*/,
                                           bool decline) {
  if (decline) {
    ToolResult r =
        ToolResult::error(ErrorCode::Unknown, "behavioral_decline",
                          nlohmann::json::object());
    r.latency_ms = 100000;
    return {r};
  }
  ToolResult r = ToolResult::success(nlohmann::json::object());
  r.latency_ms = 100;
  r.meta["tokens_used"] = 1000;
  return {r};
}

std::string workflow_summary(const WorkflowGraph& g) {
  return "workflow:" + std::to_string(g.nodes.size()) + "nodes:" +
         std::to_string(g.edges.size()) + "edges";
}

// 工作流 → TrajectoryIR CanonicalIR hash (搜索空间表示, T15)
std::string workflow_hash(const WorkflowGraph& g, const TaskSpec& spec) {
  ir::TrajectoryIR::CanonicalIR canonical;
  for (const auto& n : g.nodes) {
    canonical.canonical_nodes.push_back(ir::TrajectoryIR::NodeRecord{
        n.id, axis3_name(n.axis3), nlohmann::json::object()});
  }
  for (const auto& e : g.edges) {
    canonical.canonical_edges.push_back(
        ir::TrajectoryIR::EdgeRecord{e.from_node_id, e.to_node_id, 1.0});
  }
  canonical.metadata["task_id"] = spec.task_id;
  return ir::TrajectoryIR::hash(canonical);
}

void emit_event(const std::shared_ptr<IInteractionBus>& bus,
                const std::string& topic, const nlohmann::json& args) {
  if (bus) {
    bus->emit(EventBuilder(topic).args(args).build());
  }
}

}  // namespace

MCTSWorkflowSearch::MCTSWorkflowSearch(
    std::shared_ptr<IEvaluator> evaluator,
    std::shared_ptr<IMutationGovernor> governor,
    std::shared_ptr<BehavioralRegressionGate> regression_gate)
    : MCTSWorkflowSearch(std::move(evaluator), std::move(governor),
                         std::move(regression_gate), SearchConfig{}, nullptr) {}

MCTSWorkflowSearch::MCTSWorkflowSearch(
    std::shared_ptr<IEvaluator> evaluator,
    std::shared_ptr<IMutationGovernor> governor,
    std::shared_ptr<BehavioralRegressionGate> regression_gate,
    SearchConfig config,
    std::shared_ptr<IInteractionBus> bus)
    : evaluator_(std::move(evaluator)),
      governor_(std::move(governor)),
      regression_gate_(std::move(regression_gate)),
      bus_(std::move(bus)),
      config_(std::move(config)) {}

// T2 + T6 ctor overload: budget_controller 注入
MCTSWorkflowSearch::MCTSWorkflowSearch(
    std::shared_ptr<IEvaluator> evaluator,
    std::shared_ptr<IMutationGovernor> governor,
    std::shared_ptr<BehavioralRegressionGate> regression_gate,
    SearchConfig config,
    std::shared_ptr<IInteractionBus> bus,
    std::shared_ptr<IBudgetController> budget_controller)
    : evaluator_(std::move(evaluator)),
      governor_(std::move(governor)),
      regression_gate_(std::move(regression_gate)),
      bus_(std::move(bus)),
      budget_controller_(std::move(budget_controller)),
      config_(std::move(config)) {}

// T2 commit API: chain 单主体归因 (source_id=MCTS, decision 5)
MCTSWorkflowSearch::CommitResult MCTSWorkflowSearch::commit_chain(
    const std::vector<WorkflowNode::Axis6CognitiveDomain>& chain) {
  CommitResult cr;
  cr.failure_mode = "unknown";
  cr.mutation_id = "axis6:" + std::to_string(reinterpret_cast<uintptr_t>(&chain));

  // 决策 5 兜底: chain 为空或 None
  if (chain.empty() || (chain.size() == 1 && chain[0] == WorkflowNode::Axis6CognitiveDomain::None)) {
    cr.failure_mode = "empty_chain_or_all_none";
    cr.approved = false;
    emit_event(bus_, "axis6.degraded",
               {{"reason", cr.failure_mode}, {"chain_size", static_cast<int>(chain.size())}});
    return cr;
  }

  // 决策 5: max_chain_depth 硬截断 (per cognitive_domain_chain config)
  int max_depth = cognitive_domain_chain_.max_chain_depth > 0 ? cognitive_domain_chain_.max_chain_depth : 3;
  if (static_cast<int>(chain.size()) > max_depth) {
    cr.failure_mode = "chain_depth_exceeded";
    cr.approved = false;
    emit_event(bus_, "axis6.degraded",
               {{"reason", cr.failure_mode},
                {"requested_depth", static_cast<int>(chain.size())},
                {"max_depth", max_depth}});
    return cr;
  }

  // emit started (W4: MCTS 层 emit, governance 层 emit 由 governor 负责)
  emit_event(bus_, "axis6.search.started",
             {{"chain", nlohmann::json(chain)},
              {"source_id", "MCTS"}});

  // B1 修复: governor->commit(ctx).approved 判定
  // 单主体归因: source_id=MCTS (决策 5)
  MutationContext ctx;
  ctx.source_id = "MCTS";
  ctx.subject_ref = "WorkflowNode";
  ctx.proposed_change = "axis6 chain: " + std::to_string(chain.size()) + " steps";
  ctx.mode = MutationMode::Agent;
  cr.mutation_id = ctx.mutation_id;
  if (!governor_) {
    cr.failure_mode = "no_governor";
    cr.approved = false;
    emit_event(bus_, "axis6.commit.reverted",
               {{"mutation_id", cr.mutation_id}, {"reason", "no_governor"}});
    return cr;
  }
  auto decision = governor_->commit(ctx);
  cr.approved = decision.approved;
  if (decision.approved) {
    cr.failure_mode = "";
    emit_event(bus_, "axis6.commit.committed",
               {{"mutation_id", cr.mutation_id},
                {"chain_size", static_cast<int>(chain.size())}});
  } else {
    cr.failure_mode = decision.denial_reason;
    emit_event(bus_, "axis6.commit.reverted",
               {{"mutation_id", cr.mutation_id},
                {"reason", decision.denial_reason}});
  }
  return cr;
}

// T2 can_execute: check axis6 是否可执行
bool MCTSWorkflowSearch::can_execute(const WorkflowNode& node) const {
  return node.axis6 != WorkflowNode::Axis6CognitiveDomain::None;
}

MCTSWorkflowSearch::SearchResult MCTSWorkflowSearch::search(
    const TaskSpec& spec) {
  SearchResult result;
  if (!evaluator_ || !governor_ || config_.max_iterations <= 0) {
    result.failure_mode = "invalid_configuration";
    emit_event(bus_, "mcts.search.failed",
               {{"search_id", spec.task_id},
                {"failure_reason", result.failure_mode}});
    return result;
  }

  const std::string search_id = spec.task_id;
  emit_event(bus_, "mcts.search.started",
             {{"search_id", search_id}, {"task_id", spec.task_id}});

  // 根节点: 单 start 节点工作流
  std::vector<MCTSNode> tree;
  tree.reserve(static_cast<std::size_t>(config_.max_iterations) + 1);
  MCTSNode root;
  root.id = 0;
  root.state = make_root_graph(spec);
  tree.push_back(std::move(root));

  std::mt19937 rng(config_.random_seed);

  double best_reward = -1.0;
  std::shared_ptr<WorkflowGraph> best_workflow;
  std::string best_trace_id;

  for (int iteration = 0; iteration < config_.max_iterations; ++iteration) {
    // 1) Selection: UCB1 沿"已满"节点下潜, 停在首个可扩展节点
    int current = 0;
    while (!tree[current].children.empty() &&
           static_cast<int>(tree[current].children.size()) >=
               config_.max_children_per_node) {
      int best_child = -1;
      double best_ucb = -std::numeric_limits<double>::infinity();
      for (const int cid : tree[current].children) {
        const MCTSNode& child = tree[cid];
        const double q =
            child.visits > 0 ? child.reward_sum / child.visits : 0.0;
        const double ucb =
            ucb1_value(q, child.visits, tree[current].visits,
                       config_.exploration_weight);
        if (ucb > best_ucb) {
          best_ucb = ucb;
          best_child = cid;
        }
      }
      if (best_child < 0) {
        break;
      }
      current = best_child;
    }

    // 2) Expansion: 克隆父状态 + 添加 1 个 5 轴随机节点 + 1 条组合边
    MCTSNode child;
    child.id = static_cast<int>(tree.size());
    child.parent = current;
    child.state = tree[current].state;  // 深拷贝
    if (static_cast<int>(child.state.nodes.size()) < spec.max_nodes) {
      WorkflowNode node = generate_random_node(rng);
      node.id = spec.task_id + ":wf:" + std::to_string(child.id);
      child.state.nodes.push_back(std::move(node));
      WorkflowEdge edge;
      edge.from_node_id = tree[current].state.nodes.back().id;
      edge.to_node_id = child.state.nodes.back().id;
      edge.combination_rule = combination_rule(rng);
      child.state.edges.push_back(std::move(edge));
    }
    tree[current].children.push_back(child.id);
    tree.push_back(std::move(child));

    // 3) Simulation: IEvaluator (V2 CompositeEvaluator) 评估奖励
    const MCTSNode& node = tree.back();
    const ExecutionTrace candidate_trace = trace_of(node.state, spec, iteration);
    const RewardSignal signal = evaluator_->evaluate(candidate_trace);
    const double reward = normalize(signal.scalar);

    // 4) Backpropagation: 沿路径更新 visits + reward_sum
    for (int nid = node.id; nid >= 0; nid = tree[nid].parent) {
      tree[nid].visits += 1;
      tree[nid].reward_sum += reward;
    }

    emit_event(bus_, "mcts.search.iteration",
               {{"search_id", search_id},
                {"iteration", iteration},
                {"reward", reward},
                {"node_id", node.id}});

    if (reward > best_reward) {
      best_reward = reward;
      best_workflow = std::make_shared<WorkflowGraph>(node.state);
      best_trace_id = candidate_trace.trace_id;
    }
  }

  result.best_reward = best_reward;
  result.best_workflow = best_workflow;
  result.iterations_used = config_.max_iterations;

  // 回归门禁: 候选 vs 基线 — 行为劣化拒绝 (V1 mock 合成特征)
  const ExecutionTrace baseline_trace = trace_of(tree[0].state, spec, -1);
  const double baseline_reward =
      normalize(evaluator_->evaluate(baseline_trace).scalar);
  const std::vector<ToolResult> baseline_results =
      synthesize_results(tree[0].state, /*decline=*/false);
  const bool declined = best_reward < baseline_reward;
  const std::vector<ToolResult> candidate_results =
      synthesize_results(*best_workflow, declined);
  if (regression_gate_ &&
      !regression_gate_->allows(baseline_results, candidate_results)) {
    result.failure_mode = "behavioral_regression_failed";
    emit_event(bus_, "mcts.search.failed",
               {{"search_id", search_id},
                {"failure_reason", result.failure_mode}});
    return result;
  }

  // 变异授权: propose → commit (V1 仅 L1 workflow variants)
  MutationContext proposal;
  proposal.mutation_id =
      spec.task_id + ":mcts:" + std::to_string(result.iterations_used);
  proposal.source_id = config_.source_id;
  proposal.mutation_kind = "L1_prompt";
  proposal.subject_ref = spec.task_id;
  proposal.parent_ref = spec.task_id;
  proposal.proposed_change = workflow_summary(*best_workflow);
  proposal.mode = MutationMode::Yolo;
  proposal.evaluation_refs = {best_trace_id};
  const MutationDecision proposed = governor_->propose(proposal);
  if (!proposed.approved) {
    result.failure_mode = "proposal_denied";
    emit_event(bus_, "mcts.search.failed",
               {{"search_id", search_id},
                {"failure_reason", result.failure_mode}});
    return result;
  }
  const MutationDecision committed = governor_->commit(proposal);
  if (!committed.approved) {
    result.failure_mode = "commit_denied";
    emit_event(bus_, "mcts.search.failed",
               {{"search_id", search_id},
                {"failure_reason", result.failure_mode}});
    return result;
  }

  result.success = true;
  emit_event(bus_, "mcts.search.completed",
             {{"search_id", search_id},
              {"best_workflow", workflow_hash(*best_workflow, spec)},
              {"best_reward", best_reward},
              {"iterations_used", result.iterations_used}});
  return result;
}

}  // namespace agenticdsl
