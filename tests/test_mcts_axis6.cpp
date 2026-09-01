// tests/test_mcts_axis6.cpp
// T2 mcts-axis6-cognitive-domain: 第 6 轴 enum + commit_chain() + can_execute()
#include "catch_amalgamated.hpp"
#include "agenticdsl/cognitive/mcts_workflow_search.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/imutation_governance.h"
#include "agenticdsl/testing/behavioral_regression.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "modules/budget/budget_controller.h"
#include <memory>

using namespace agenticdsl;

namespace {
class StubEvaluator : public IEvaluator {
public:
    RewardSignal evaluate(const ExecutionTrace&) const override { return RewardSignal::excellent(0.9); }
    int compare(const ExecutionTrace&, const ExecutionTrace&) const override { return 0; }
};
class StubGovernor : public IMutationGovernor {
public:
    bool approved = true;
    std::string denial_reason;
    MutationDecision propose(const MutationContext&) override { return {true, "", ""}; }
    MutationDecision commit(const MutationContext&) override { return {approved, denial_reason, ""}; }
    void revert(const MutationContext&, const std::string&, const std::string&) override {}
};
class RecordingBus : public IInteractionBus {
public:
    std::vector<BusEvent> events;
    void emit(const BusEvent& event) override { events.push_back(event); }
    void emit(const std::string&, const std::string&) override {}
    size_t subscribe(const std::string&, std::function<void(const BusEvent&)>) override { return 0; }
    void unsubscribe(size_t) override {}
};
}

TEST_CASE("axis6_enum_defaults_to_None_v10_compat", "[mcts][axis6][t2]") {
  WorkflowNode node;
  REQUIRE(node.axis1 == WorkflowNode::Axis1Template::Linear);
  REQUIRE(node.axis6 == WorkflowNode::Axis6CognitiveDomain::None);
}

TEST_CASE("axis6_chain_commits_with_governor_approved", "[mcts][axis6][t2]") {
  auto eval = std::make_shared<StubEvaluator>();
  auto gov = std::make_shared<StubGovernor>();
  auto bus = std::make_shared<RecordingBus>();
  auto regression = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch searcher(eval, gov, regression,
                              MCTSWorkflowSearch::SearchConfig{}, bus);
  const auto result = searcher.commit_chain({
      WorkflowNode::Axis6CognitiveDomain::Reflect,
      WorkflowNode::Axis6CognitiveDomain::Compile});
  REQUIRE(result.approved);
  REQUIRE(result.failure_mode.empty());
  bool started = false, committed = false;
  for (const auto& e : bus->events) {
    if (e.topic == "axis6.search.started") started = true;
    if (e.topic == "axis6.commit.committed") committed = true;
  }
  REQUIRE(started);
  REQUIRE(committed);
}

TEST_CASE("axis6_chain_reverts_when_governor_denies", "[mcts][axis6][t2]") {
  auto eval = std::make_shared<StubEvaluator>();
  auto gov = std::make_shared<StubGovernor>();
  gov->approved = false;
  gov->denial_reason = "test_deny";
  auto bus = std::make_shared<RecordingBus>();
  auto regression = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch searcher(eval, gov, regression,
                              MCTSWorkflowSearch::SearchConfig{}, bus);
  const auto result = searcher.commit_chain({WorkflowNode::Axis6CognitiveDomain::Reflect});
  REQUIRE_FALSE(result.approved);
  REQUIRE(result.failure_mode == "test_deny");
  bool reverted = false;
  for (const auto& e : bus->events) {
    if (e.topic == "axis6.commit.reverted") reverted = true;
  }
  REQUIRE(reverted);
}

TEST_CASE("axis6_chain_depth_exceeded_emits_degraded", "[mcts][axis6][t2]") {
  auto eval = std::make_shared<StubEvaluator>();
  auto gov = std::make_shared<StubGovernor>();
  auto bus = std::make_shared<RecordingBus>();
  auto regression = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch searcher(eval, gov, regression,
                              MCTSWorkflowSearch::SearchConfig{}, bus);
  // default max_chain_depth = 3, 4 elements exceeds
  const auto result = searcher.commit_chain({
      WorkflowNode::Axis6CognitiveDomain::Reflect,
      WorkflowNode::Axis6CognitiveDomain::Search,
      WorkflowNode::Axis6CognitiveDomain::Compile,
      WorkflowNode::Axis6CognitiveDomain::Meta_Select});
  REQUIRE_FALSE(result.approved);
  REQUIRE(result.failure_mode == "chain_depth_exceeded");
  bool degraded = false;
  for (const auto& e : bus->events) {
    if (e.topic == "axis6.degraded") degraded = true;
  }
  REQUIRE(degraded);
}

TEST_CASE("axis6_empty_chain_emits_degraded", "[mcts][axis6][t2]") {
  auto eval = std::make_shared<StubEvaluator>();
  auto gov = std::make_shared<StubGovernor>();
  auto bus = std::make_shared<RecordingBus>();
  auto regression = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch searcher(eval, gov, regression,
                              MCTSWorkflowSearch::SearchConfig{}, bus);
  const auto result = searcher.commit_chain({});
  REQUIRE_FALSE(result.approved);
  REQUIRE(result.failure_mode == "empty_chain_or_all_none");
  bool degraded = false;
  for (const auto& e : bus->events) {
    if (e.topic == "axis6.degraded") degraded = true;
  }
  REQUIRE(degraded);
}

TEST_CASE("axis6_can_execute_returns_false_for_None", "[mcts][axis6][t2]") {
  auto eval = std::make_shared<StubEvaluator>();
  auto gov = std::make_shared<StubGovernor>();
  auto regression = std::make_shared<BehavioralRegressionGate>();
  MCTSWorkflowSearch searcher(eval, gov, regression, MCTSWorkflowSearch::SearchConfig{});
  WorkflowNode node;
  REQUIRE_FALSE(searcher.can_execute(node));
  node.axis6 = WorkflowNode::Axis6CognitiveDomain::Reflect;
  REQUIRE(searcher.can_execute(node));
}