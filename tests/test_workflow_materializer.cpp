// tests/test_workflow_materializer.cpp
// T1 workflow-materializer-v1: WorkflowGraph → DSL 文本具体化测试
#include "catch_amalgamated.hpp"
#include "agenticdsl/cognitive/workflow_materializer.h"
#include "agenticdsl/cognitive/mcts_workflow_search.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "core/types/tool_result.h"
#include <memory>
#include <string>

using namespace agenticdsl;

namespace {
class RecordingBus : public IInteractionBus {
public:
  std::vector<BusEvent> events;
  void emit(const BusEvent& event) override { events.push_back(event); }
  void emit(const std::string&, const std::string&) override {}
  size_t subscribe(const std::string&, std::function<void(const BusEvent&)>) override { return 0; }
  void unsubscribe(size_t) override {}
};

WorkflowNode make_node(const std::string& id,
                       WorkflowNode::Axis6CognitiveDomain a6 =
                           WorkflowNode::Axis6CognitiveDomain::None) {
  WorkflowNode n;
  n.id = id;
  n.axis6 = a6;
  return n;
}
}  // namespace

TEST_CASE("materializer: empty graph returns nullopt", "[materializer][t1]") {
  cognitive::Materializer m;
  WorkflowGraph g;
  REQUIRE_FALSE(m.materialize_to_dsl(g).has_value());
}

TEST_CASE("materializer: linear graph produces DSL text with start/assign/end",
          "[materializer][t1]") {
  cognitive::Materializer m;
  WorkflowGraph g;
  g.task_id = "t1_linear";
  g.nodes.push_back(make_node("start"));
  g.nodes.push_back(make_node("compute"));
  g.nodes.push_back(make_node("end"));
  auto dsl = m.materialize_to_dsl(g);
  REQUIRE(dsl.has_value());
  REQUIRE(dsl->find("### AgenticDSL `/dynamic/mcts/t1_linear`") != std::string::npos);
  REQUIRE(dsl->find("type: assign") != std::string::npos);
}

TEST_CASE("materializer: axis6=Reflect produces evolution::reflect tool_call",
          "[materializer][t1][axis6]") {
  cognitive::Materializer m;
  WorkflowGraph g;
  g.task_id = "t1_axis6";
  g.nodes.push_back(make_node("reflect", WorkflowNode::Axis6CognitiveDomain::Reflect));
  auto dsl = m.materialize_to_dsl(g);
  REQUIRE(dsl.has_value());
  REQUIRE(dsl->find("tool: \"evolution::reflect\"") != std::string::npos);
}

TEST_CASE("materializer: branching axis1 produces fork/join", "[materializer][t1]") {
  cognitive::Materializer m;
  WorkflowGraph g;
  g.task_id = "t1_branch";
  WorkflowNode branch;
  branch.id = "branch_root";
  branch.axis1 = WorkflowNode::Axis1Template::Branching;
  branch.axis4 = WorkflowNode::Axis4Control::Parallel;
  g.nodes.push_back(std::move(branch));
  WorkflowNode join;
  join.id = "join_point";
  join.axis1 = WorkflowNode::Axis1Template::Branching;
  join.axis4 = WorkflowNode::Axis4Control::Sequential;
  g.nodes.push_back(std::move(join));
  auto dsl = m.materialize_to_dsl(g);
  REQUIRE(dsl.has_value());
  REQUIRE(dsl->find("type: fork") != std::string::npos);
  REQUIRE(dsl->find("type: join") != std::string::npos);
}

TEST_CASE("materializer: emit workflow.materialized event with lineage", "[materializer][t1][bus]") {
  auto bus = std::make_shared<RecordingBus>();
  cognitive::Materializer m;
  m.set_bus(bus);
  WorkflowGraph g;
  g.task_id = "t1_lineage";
  g.nodes.push_back(make_node("start"));
  auto dsl = m.materialize_to_dsl(g);
  REQUIRE(dsl.has_value());
  REQUIRE_FALSE(bus->events.empty());
  const auto& last = bus->events.back();
  REQUIRE(last.topic == "workflow.materialized");
  REQUIRE(last.payload.data.contains("output_path"));
  REQUIRE(last.payload.data["output_path"] == "/dynamic/mcts/t1_lineage");
  REQUIRE(last.payload.data.contains("task_id"));
}