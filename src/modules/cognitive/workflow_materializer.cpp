// src/modules/cognitive/workflow_materializer.cpp
// T1 workflow-materializer-v1: WorkflowGraph → DSL 文本具体化桥
#include "agenticdsl/cognitive/workflow_materializer.h"

#include <sstream>
#include <string>

#include "core/types/tool_result.h"  // ToolResult::success (BusEvent payload)

namespace agenticdsl::cognitive {

namespace {
constexpr const char* kDynamicPrefix = "/dynamic/mcts/";
}

std::string Materializer::axis6_tool_name(WorkflowNode::Axis6CognitiveDomain axis6) const {
  // 对齐 T5 cognitive-tools: evolution::* 命名 (Metis finding #2 统一)
  switch (axis6) {
    case WorkflowNode::Axis6CognitiveDomain::Reflect:
      return "evolution::reflect";
    case WorkflowNode::Axis6CognitiveDomain::Search:
      return "evolution::search";
    case WorkflowNode::Axis6CognitiveDomain::Compile:
      return "evolution::compile";
    case WorkflowNode::Axis6CognitiveDomain::Meta_Select:
    case WorkflowNode::Axis6CognitiveDomain::Reason:
    case WorkflowNode::Axis6CognitiveDomain::None:
      return "";
  }
  return "";
}

std::string Materializer::render_node(const WorkflowNode& node) const {
  std::ostringstream yaml;
  yaml << "  - id: " << node.id << "\n";

  // axis6 cognitive 节点 → tool_call (V1 走 tool_call 路线, design §决策 5)
  std::string tool_name = axis6_tool_name(node.axis6);
  if (!tool_name.empty()) {
    yaml << "    type: tool_call\n";
    yaml << "    tool: \"" << tool_name << "\"\n";
    yaml << "    args:\n";
    yaml << "      prompt: \"{{ task }}\"\n";
    return yaml.str();
  }

  // Axis5 Error → 错误处理边
  if (node.axis5 == WorkflowNode::Axis5Error::Abort) {
    yaml << "    type: end\n";
    yaml << "    termination_mode: hard\n";
    return yaml.str();
  }
  if (node.axis5 == WorkflowNode::Axis5Error::Retry) {
    yaml << "    on_failure: \"/dynamic/retry\"\n";
  }
  if (node.axis5 == WorkflowNode::Axis5Error::Fallback) {
    yaml << "    on_failure: \"/dynamic/fallback\"\n";
  }

  // Axis1 Branching + Axis4 Parallel → fork 节点
  if (node.axis1 == WorkflowNode::Axis1Template::Branching &&
      node.axis4 == WorkflowNode::Axis4Control::Parallel) {
    yaml << "    type: fork\n";
    yaml << "    branches:\n";
    yaml << "      - \"/dynamic/branch_a\"\n";
    yaml << "      - \"/dynamic/branch_b\"\n";
    return yaml.str();
  }

  // Axis1 Branching + Axis4 Sequential → join 节点 (分支聚合收口)
  if (node.axis1 == WorkflowNode::Axis1Template::Branching &&
      node.axis4 == WorkflowNode::Axis4Control::Sequential) {
    yaml << "    type: join\n";
    yaml << "    wait_for: [\"branch_a\", \"branch_b\"]\n";
    return yaml.str();
  }

  // Axis4 Parallel + 非 Branching → join 节点 (branch 聚合)
  if (node.axis4 == WorkflowNode::Axis4Control::Parallel &&
      node.axis1 != WorkflowNode::Axis1Template::Branching) {
    yaml << "    type: join\n";
    yaml << "    wait_for: [\"branch_a\", \"branch_b\"]\n";
    return yaml.str();
  }

  // Axis3 Tool → tool_call
  if (node.axis3 == WorkflowNode::Axis3Tool::Calculator) {
    yaml << "    type: tool_call\n";
    yaml << "    tool: \"math::calculate\"\n";
    yaml << "    args:\n";
    yaml << "      expr: \"{{ expr }}\"\n";
    return yaml.str();
  }
  if (node.axis3 == WorkflowNode::Axis3Tool::Search) {
    yaml << "    type: tool_call\n";
    yaml << "    tool: \"search::query\"\n";
    yaml << "    args:\n";
    yaml << "      query: \"{{ query }}\"\n";
    return yaml.str();
  }

  // 默认 assign (数据传递)
  yaml << "    type: assign\n";
  yaml << "    assign:\n";
  yaml << "      result: \"{{ input }}\"\n";
  return yaml.str();
}

std::optional<std::string> Materializer::materialize_to_dsl(const WorkflowGraph& graph) {
  // 兜底: 空图 → nullopt (等同 v1.0 无可映射工作流)
  if (graph.nodes.empty()) {
    return std::nullopt;
  }

  std::string task_id = graph.task_id.empty() ? "unnamed" : graph.task_id;
  std::ostringstream md;
  md << "### AgenticDSL `" << kDynamicPrefix << task_id << "`\n";
  md << "```yaml\n";
  md << "# --- BEGIN AgenticDSL ---\n";
  md << "graph_type: subgraph\n";
  md << "nodes:\n";
  for (const auto& node : graph.nodes) {
    md << render_node(node);
  }
  md << "# --- END AgenticDSL ---\n";
  md << "```\n";

  std::string dsl_text = md.str();

  // lineage 事件 (workflow_hash + output_path + task_id)
  if (bus_) {
    nlohmann::json payload;
    payload["output_path"] = std::string(kDynamicPrefix) + task_id;
    payload["task_id"] = task_id;
    payload["node_count"] = static_cast<int>(graph.nodes.size());
    bus_->emit(BusEvent{"workflow.materialized", ToolResult::success(payload)});
  }

  return dsl_text;
}

}  // namespace agenticdsl::cognitive