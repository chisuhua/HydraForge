// tests/test_mcts_workflow_search.cpp
// 功能描述：AFlow 风格 MCTS 工作流搜索编排层测试套件 (T20, ADR-0061-08, Sprint 24)
//          Phase 0 契约骨架 10 cases + Phase 1 MCTS 算法 3 cases
//          + Phase 2 V2 集成 3 cases + Phase 3 事件发射 1 case = 17 cases 总
//          测试 MCTSWorkflowSearch 编排层：5 轴模板搜索空间 + UCB1 选择
//          + 扩展/模拟/反向传播 + IEvaluator V2 加权奖励
//          + BehavioralRegressionGate 回归门 + IMutationGovernor 变异授权
// 设计依据：openspec/changes/t20-aflow-mcts/tasks.md + specs/
//          + docs/adr/skill/adr-0061-08-aflow-search.md
// 作者：HydraForge Sprint 24 T20 ship
// 最后修改日期：2026-08-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/cognitive/mcts_workflow_search.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/imutation_governance.h"
#include "agenticdsl/cognitive/behavioral_equivalence_evaluator.h"
#include "agenticdsl/cognitive/composite_evaluator.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"
#include "core/types/tool_result.h"

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace agenticdsl;

namespace {

// ============================================================================
// 测试替身: RecordingBus — 记录全部事件 (顺序敏感断言)
// ============================================================================
class RecordingBus : public IInteractionBus {
 public:
  std::vector<BusEvent> events;

  void emit(const BusEvent& event) override { events.push_back(event); }
  void emit(const std::string& /*event_type*/,
            const std::string& /*content*/) override {}
  size_t subscribe(const std::string& /*event_type*/,
                   std::function<void(const BusEvent&)> /*callback*/) override {
    return 0;
  }
  void unsubscribe(size_t /*token*/) override {}

  std::vector<const BusEvent*> mcts_events(const std::string& topic) const {
    std::vector<const BusEvent*> out;
    for (const auto& e : events) {
      if (e.topic == topic) {
        out.push_back(&e);
      }
    }
    return out;
  }
};

// ============================================================================
// 测试替身: StubEvaluator — 可配置模式的 IEvaluator
//   Mode::Fixed           固定 quality (默认 Excellent)
//   Mode::ScoreByOk       按 trace.final_result.ok 评分 (ok → excellent / !ok → poor)
//   Mode::ScoreBySearchTool  按数据中是否含 Search 工具节点评分 (收敛测试)
// ============================================================================
class StubEvaluator : public IEvaluator {
 public:
  enum class Mode { Fixed, ScoreByOk, ScoreBySearchTool };
  Mode mode = Mode::Fixed;
  mutable int evaluate_calls = 0;
  mutable int compare_calls = 0;

  RewardSignal evaluate(const ExecutionTrace& trace) const override {
    ++evaluate_calls;
    switch (mode) {
      case Mode::ScoreByOk:
        return trace.final_result.ok ? RewardSignal::excellent(0.9)
                                     : RewardSignal::poor(0.9);
      case Mode::ScoreBySearchTool: {
        const bool has_search = trace.final_result.data.value("has_search_tool", false);
        return has_search ? RewardSignal::excellent(0.95)
                          : RewardSignal::acceptable(0.2);
      }
      case Mode::Fixed:
      default:
        return RewardSignal::excellent(0.9);
    }
  }

  int compare(const ExecutionTrace& /*a*/,
              const ExecutionTrace& /*b*/) const override {
    ++compare_calls;
    return 0;
  }
};

// ============================================================================
// 测试替身: DeclineEvaluator — 基线 (单节点) 高分, 候选 (扩展后) 低分
//           制造"搜索无改善 → 劣化"场景, 验证回归门拒绝
// ============================================================================
class DeclineEvaluator : public IEvaluator {
 public:
  mutable int evaluate_calls = 0;

  RewardSignal evaluate(const ExecutionTrace& trace) const override {
    ++evaluate_calls;
    const int nodes = trace.final_result.data.value("nodes", 1);
    return nodes <= 1 ? RewardSignal::excellent(0.95)
                      : RewardSignal::poor(0.9);
  }

  int compare(const ExecutionTrace& /*a*/,
              const ExecutionTrace& /*b*/) const override {
    return 0;
  }
};

// ============================================================================
// 测试替身: StubGovernor — 可配置 approve/deny 的 IMutationGovernor
// ============================================================================
class StubGovernor : public IMutationGovernor {
 public:
  bool propose_approved = true;
  bool commit_approved = true;
  mutable int propose_calls = 0;
  mutable int commit_calls = 0;
  mutable std::string last_kind;

  MutationDecision propose(const MutationContext& ctx) override {
    ++propose_calls;
    last_kind = ctx.mutation_kind;
    if (propose_approved) {
      return MutationDecision{true, "", ""};
    }
    return MutationDecision{false, "simulated_denial", "test"};
  }

  MutationDecision commit(const MutationContext& /*ctx*/) override {
    ++commit_calls;
    if (commit_approved) {
      return MutationDecision{true, "", ""};
    }
    return MutationDecision{false, "commit_denied", "test"};
  }

  void revert(const MutationContext& /*ctx*/,
              const std::string& /*target_version*/,
              const std::string& /*rollback_reason*/) override {}
};

// ============================================================================
// 辅助: 构造标准 MCTSWorkflowSearch (默认试探)
// ============================================================================
std::shared_ptr<MCTSWorkflowSearch> make_search(
    const std::shared_ptr<IEvaluator>& evaluator,
    const std::shared_ptr<IMutationGovernor>& governor,
    const std::shared_ptr<BehavioralRegressionGate>& gate,
    MCTSWorkflowSearch::SearchConfig config = {},
    const std::shared_ptr<IInteractionBus>& bus = nullptr) {
  return std::make_shared<MCTSWorkflowSearch>(evaluator, governor, gate, config, bus);
}

TaskSpec make_spec(const std::string& task_id) {
  TaskSpec spec;
  spec.task_id = task_id;
  spec.goal = "find best workflow";
  return spec;
}

}  // anonymous namespace

// ============================================================================
// Phase 0: MCTSWorkflowSearch 契约骨架 (10 cases)
// ============================================================================

TEST_CASE("mcts_workflow_initialization", "[mcts][phase0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 10;
  config.source_id = "R_T20_AFLOW";
  config.exploration_weight = 1.414;

  auto search = make_search(evaluator, governor, gate, config);
  REQUIRE(true);  // 构造成功
}

TEST_CASE("mcts_node_5_axis_template", "[mcts][phase0]") {
  // 5 轴模板枚举值范围合法 (axis1: 4 值, axis2/4/5: 3 值, axis3: 4 值)
  WorkflowNode node;
  node.axis1 = WorkflowNode::Axis1Template::Parallel;
  node.axis2 = WorkflowNode::Axis2Param::TopP;
  node.axis3 = WorkflowNode::Axis3Tool::Search;
  node.axis4 = WorkflowNode::Axis4Control::Loop;
  node.axis5 = WorkflowNode::Axis5Error::Abort;
  REQUIRE(static_cast<int>(node.axis1) >= 0);
  REQUIRE(static_cast<int>(node.axis1) <= 3);
  REQUIRE(static_cast<int>(node.axis2) >= 0);
  REQUIRE(static_cast<int>(node.axis2) <= 2);
  REQUIRE(static_cast<int>(node.axis3) >= 0);
  REQUIRE(static_cast<int>(node.axis3) <= 3);
  REQUIRE(static_cast<int>(node.axis4) >= 0);
  REQUIRE(static_cast<int>(node.axis4) <= 2);
  REQUIRE(static_cast<int>(node.axis5) >= 0);
  REQUIRE(static_cast<int>(node.axis5) <= 2);
}

TEST_CASE("mcts_edge_combination_rules", "[mcts][phase0]") {
  // 组合规则合法: "sequential" | "conditional" | "loop"
  const std::vector<std::string> valid = {"sequential", "conditional", "loop"};
  WorkflowGraph graph;
  WorkflowNode a;
  a.id = "n1";
  WorkflowNode b;
  b.id = "n2";
  graph.nodes = {a, b};
  for (const auto& rule : valid) {
    WorkflowEdge e;
    e.from_node_id = "n1";
    e.to_node_id = "n2";
    e.combination_rule = rule;
    graph.edges.push_back(e);
  }
  REQUIRE(graph.nodes.size() == 2);
  REQUIRE(graph.edges.size() == 3);
  REQUIRE(graph.edges[0].combination_rule == "sequential");
  REQUIRE(graph.edges[1].combination_rule == "conditional");
  REQUIRE(graph.edges[2].combination_rule == "loop");
}

TEST_CASE("mcts_search_ucb1_selection", "[mcts][phase0]") {
  // 占位 — Phase 1 实现 UCB1 选择后: 搜索返回最佳工作流
  auto evaluator = std::make_shared<StubEvaluator>();
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 20;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("ucb1"));
  REQUIRE(result.best_workflow != nullptr);
}

TEST_CASE("mcts_search_expansion", "[mcts][phase0]") {
  // 占位 — Phase 1 实现扩展后: 搜索空间随迭代扩展 (最佳工作流节点增多)
  auto evaluator = std::make_shared<StubEvaluator>();
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 20;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("expand"));
  REQUIRE(result.best_workflow != nullptr);
  REQUIRE(result.best_workflow->nodes.size() >= 2);  // 至少扩展了 1 个新节点
}

TEST_CASE("mcts_search_simulation", "[mcts][phase0]") {
  // 占位 — Phase 1 实现模拟后: 每次扩展调用 IEvaluator 评估奖励
  auto evaluator = std::make_shared<StubEvaluator>();
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 20;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("simulate"));
  REQUIRE(result.best_workflow != nullptr);
  REQUIRE(evaluator->evaluate_calls >= 1);  // 模拟阶段完成过奖励评估
}

TEST_CASE("mcts_search_backpropagation", "[mcts][phase0]") {
  // 占位 — Phase 1 实现反向传播后: 迭代数与奖励被正确追踪
  auto evaluator = std::make_shared<StubEvaluator>();
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 20;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("backprop"));
  REQUIRE(result.iterations_used == 20);
  // StubEvaluator 全 ok → excellent(scalar=1.0) → 归一化 q=(1.0+1)/2 = 1.0
  REQUIRE(result.best_reward == Catch::Approx(1.0));
}

TEST_CASE("mcts_reward_evaluator_v2_weighted", "[mcts][phase0]") {
  // 占位 — Phase 2 V2 加权聚合后: CompositeEvaluator {Stub(ok) 0.7 + BEV 0.3}
  //   scalar = 0.7*1.0 + 0.3*0.0 = 0.7 → 归一化 q = (0.7+1)/2 = 0.85
  auto stub = std::make_shared<StubEvaluator>();
  stub->mode = StubEvaluator::Mode::ScoreByOk;
  auto bev = std::make_shared<BehavioralEquivalenceEvaluator>();
  auto evaluator = std::make_shared<CompositeEvaluator>(
      std::vector<std::shared_ptr<IEvaluator>>{stub, bev},
      std::vector<double>{0.7, 0.3});
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 20;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("weighted"));
  REQUIRE(result.best_workflow != nullptr);
  REQUIRE(result.best_reward == Catch::Approx(0.85));
}

TEST_CASE("mcts_regression_gate_integration", "[mcts][phase0]") {
  // 占位 — Phase 2 回归门集成后: happy path 全流程成功
  auto evaluator = std::make_shared<StubEvaluator>();
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 20;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("gate"));
  REQUIRE(result.success);
}

TEST_CASE("mcts_mutation_governor_authorization", "[mcts][phase0]") {
  // 占位 — Phase 2 变异授权集成后: 最佳工作流经 governor propose → commit
  auto evaluator = std::make_shared<StubEvaluator>();
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 20;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("governor"));
  REQUIRE(result.success);
  REQUIRE(governor->propose_calls > 0);
  REQUIRE(governor->commit_calls > 0);
  REQUIRE(governor->last_kind == "L1_prompt");  // V1 仅 L1 workflow variants
}
// ============================================================================
// Phase 1: MCTS 算法核心 (3 cases)
// ============================================================================

TEST_CASE("mcts_ucb1_selection_best_arm", "[mcts][phase1]") {
  // 等访问量下, 高质量臂 UCB1 值更高 → 选择最优臂
  const double c = 1.414;
  const double best = ucb1_value(0.9, 10, 20, c);
  const double worse = ucb1_value(0.5, 10, 20, c);
  REQUIRE(best > worse);
  // 未访问子节点 → +inf (保证探索)
  REQUIRE(std::isinf(ucb1_value(0.0, 0, 1, c)));
  // 低访问量 + 高探索权重 → 低质量但未充分访问的臂胜出 (探索-利用平衡)
  REQUIRE(ucb1_value(0.5, 3, 20, 3.0) > ucb1_value(0.9, 10, 20, 3.0));
}

TEST_CASE("mcts_ucb1_selection_exploration_exploitation_balance",
          "[mcts][phase1]") {
  // c=0: 纯利用 → 高质量臂胜出
  REQUIRE(ucb1_value(0.9, 5, 10, 0.0) > ucb1_value(0.6, 5, 10, 0.0));
  // 大 c: 低访问但尚不确定的臂获得更高 UCB1 (探索优先)
  REQUIRE(ucb1_value(0.6, 2, 12, 10.0) > ucb1_value(0.9, 10, 12, 10.0));
  // c 单调性: 高权重只会让未充分访问的臂更有吸引力
  REQUIRE(ucb1_value(0.6, 2, 12, 10.0) > ucb1_value(0.6, 2, 12, 1.0));
}

TEST_CASE("mcts_search_convergence_100_iterations", "[mcts][phase1]") {
  // 100 iterations 收敛: 奖励函数偏好含 Search 工具节点的工作流
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->mode = StubEvaluator::Mode::ScoreBySearchTool;
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 100;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("conv_100"));

  REQUIRE(result.iterations_used == 100);
  REQUIRE(result.best_workflow != nullptr);
  // 含 Search 工具节点 → excellent(scalar=1.0) → 归一化 q=1.0
  REQUIRE(result.best_reward == Catch::Approx(1.0));
  bool has_search = false;
  for (const auto& n : result.best_workflow->nodes) {
    if (n.axis3 == WorkflowNode::Axis3Tool::Search) {
      has_search = true;
    }
  }
  REQUIRE(has_search);
  REQUIRE(result.success);
}

// ============================================================================
// Phase 2: 集成 IEvaluator V2 + 回归门禁 + 变异授权 (3 cases)
// ============================================================================

TEST_CASE("mcts_reward_evaluator_v2_composite", "[mcts][phase2]") {
  // CompositeEvaluator 加权聚合: {Stub(ok) 0.7 + BehavioralEquivalence 0.3}
  //   scalar = 0.7*1.0 + 0.3*0.0 = 0.7 → 归一化 q = (0.7+1)/2 = 0.85
  auto stub = std::make_shared<StubEvaluator>();
  stub->mode = StubEvaluator::Mode::ScoreByOk;
  auto bev = std::make_shared<BehavioralEquivalenceEvaluator>();
  auto evaluator = std::make_shared<CompositeEvaluator>(
      std::vector<std::shared_ptr<IEvaluator>>{stub, bev},
      std::vector<double>{0.7, 0.3});
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 50;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("v2_composite"));

  REQUIRE(result.best_workflow != nullptr);
  REQUIRE(result.best_reward == Catch::Approx(0.85));
  REQUIRE(stub->evaluate_calls > 0);  // Composite 聚合调用了子评估器
}

TEST_CASE("mcts_regression_gate_rejects_decline", "[mcts][phase2]") {
  // 1) 门禁包装直接验证: 健康基线 vs 劣化候选 → Fail 拒绝
  auto gate = std::make_shared<BehavioralRegressionGate>();
  ToolResult healthy = ToolResult::success(nlohmann::json::object());
  healthy.latency_ms = 100;
  healthy.meta["tokens_used"] = 1000;
  ToolResult declining =
      ToolResult::error(ErrorCode::Unknown, "behavioral_decline",
                        nlohmann::json::object());
  declining.latency_ms = 100000;
  REQUIRE(gate->allows({healthy}, {healthy}));
  REQUIRE_FALSE(gate->allows({healthy}, {declining}));

  // 2) MCTS 层: 候选劣于基线 (搜索无改善) → 回归门拒绝, 不提交
  auto evaluator = std::make_shared<DeclineEvaluator>();
  auto governor = std::make_shared<StubGovernor>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 50;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("decline"));

  REQUIRE_FALSE(result.success);
  REQUIRE(result.failure_mode == "behavioral_regression_failed");
  REQUIRE(governor->propose_calls == 0);
  REQUIRE(governor->commit_calls == 0);
}

TEST_CASE("mcts_mutation_governor_authorizes_commit", "[mcts][phase2]") {
  // 最优工作流经 MutationGovernor propose → commit (L1 workflow variants)
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->mode = StubEvaluator::Mode::ScoreByOk;  // 全 ok → 无劣化
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 30;
  auto search = make_search(evaluator, governor, gate, config);

  const auto result = search->search(make_spec("authorize"));

  REQUIRE(result.success);
  REQUIRE(result.best_workflow != nullptr);
  REQUIRE(governor->propose_calls == 1);
  REQUIRE(governor->commit_calls == 1);
  REQUIRE(governor->last_kind == "L1_prompt");
}

// ============================================================================
// Phase 3: 事件发射 (1 case)
// ============================================================================

TEST_CASE("mcts_event_emission", "[mcts][phase3]") {
  // happy path: started ×1 + iteration ×N + completed ×1, 无 failed
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->mode = StubEvaluator::Mode::ScoreByOk;
  auto governor = std::make_shared<StubGovernor>();
  auto gate = std::make_shared<BehavioralRegressionGate>();
  auto bus = std::make_shared<RecordingBus>();
  MCTSWorkflowSearch::SearchConfig config;
  config.max_iterations = 20;
  auto search = make_search(evaluator, governor, gate, config, bus);

  const auto result = search->search(make_spec("events"));

  REQUIRE(result.success);
  REQUIRE(bus->mcts_events("mcts.search.started").size() == 1);
  REQUIRE(bus->mcts_events("mcts.search.iteration").size() == 20);
  REQUIRE(bus->mcts_events("mcts.search.completed").size() == 1);
  REQUIRE(bus->mcts_events("mcts.search.failed").empty());

  // failure path: 候选劣化 → mcts.search.failed ×1, 无 completed
  auto decline_eval = std::make_shared<DeclineEvaluator>();
  auto bus2 = std::make_shared<RecordingBus>();
  MCTSWorkflowSearch::SearchConfig decline_config;
  decline_config.max_iterations = 20;
  auto decline_search =
      make_search(decline_eval, governor, gate, decline_config, bus2);

  const auto decline_result =
      decline_search->search(make_spec("decline-events"));

  REQUIRE_FALSE(decline_result.success);
  REQUIRE(bus2->mcts_events("mcts.search.started").size() == 1);
  REQUIRE(bus2->mcts_events("mcts.search.iteration").size() == 20);
  REQUIRE(bus2->mcts_events("mcts.search.failed").size() == 1);
  REQUIRE(bus2->mcts_events("mcts.search.completed").empty());
}
