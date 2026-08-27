// src/modules/ir/trajectory_ir_backend.cpp
// 功能描述：TrajectoryIR V1 backends (T15 Phase 2)
//          to_sft_data()   — CanonicalIR → SFT 训练数据 JSON schema
//                            (nodes/edges/steps/sft_metadata 字段)
//          to_otel_spans() — CanonicalIR → OTLP spans JSON
//                            (trace_id/span_id/parent_span_id/timestamps)
// 设计依据：openspec/changes/t15-trajectory-ir/specs/trajectory-ir/spec.md
//          "V1 Backends (to_sft_data + to_otel_spans)" Requirement
// 边界：V2 延后 to_rl_data / to_eval_data
// 作者：HydraForge Sprint 24 T15 ship
// 最后修改日期：2026-08-27

#include "agenticdsl/ir/trajectory_ir.h"

#include <sstream>
#include <unordered_map>

namespace agenticdsl::ir {

namespace {

// 确定性短 hash (十六进制), 用于 span_id / trace_id 派生
std::string short_hash(const std::string& input) {
  std::ostringstream oss;
  oss << std::hex << std::hash<std::string>{}(input);
  return oss.str();
}

}  // namespace

// CanonicalIR → SFT 训练数据 JSON
// schema: {nodes, edges, steps, sft_metadata}
nlohmann::json TrajectoryIR::to_sft_data(const CanonicalIR& canonical) {
  nlohmann::json out = nlohmann::json::object();

  auto nodes = nlohmann::json::array();
  for (const auto& n : canonical.canonical_nodes) {
    nodes.push_back(
        {{"id", n.id}, {"type", n.type}, {"metadata", n.metadata}});
  }
  out["nodes"] = std::move(nodes);

  auto edges = nlohmann::json::array();
  for (const auto& e : canonical.canonical_edges) {
    edges.push_back({{"from", e.from}, {"to", e.to}, {"weight", e.weight}});
  }
  out["edges"] = std::move(edges);

  auto steps = nlohmann::json::array();
  for (const auto& s : canonical.canonical_steps) {
    steps.push_back({{"node_id", s.node_id}, {"metadata", s.metadata}});
  }
  out["steps"] = std::move(steps);

  out["sft_metadata"] = {
      {"format", "trajectory-ir-sft-v1"},
      {"node_count", canonical.canonical_nodes.size()},
      {"edge_count", canonical.canonical_edges.size()},
      {"step_count", canonical.canonical_steps.size()},
      {"trajectory_hash", TrajectoryIR::hash(canonical)},
  };
  return out;
}

// CanonicalIR → OTLP spans JSON
// 每个 node 一个 span; parent_span_id 取首条入边源节点 (无父则为 "0")
// timestamps V1 确定性伪值 (索引推导), 保证序列化可重现
nlohmann::json TrajectoryIR::to_otel_spans(const CanonicalIR& canonical) {
  const std::string trace_id = short_hash(TrajectoryIR::hash(canonical));

  // 首入边索引: to → from (父节点)
  std::unordered_map<std::string, std::string> parent_of;
  for (const auto& e : canonical.canonical_edges) {
    parent_of.emplace(e.to, e.from);  // 仅保留首个父 (V1 简化)
  }

  auto spans = nlohmann::json::array();
  uint64_t index = 0;
  for (const auto& n : canonical.canonical_nodes) {
    const std::string span_id = short_hash("span:" + n.id);
    std::string parent_span_id = "0";
    if (const auto it = parent_of.find(n.id); it != parent_of.end()) {
      parent_span_id = short_hash("span:" + it->second);
    }
    spans.push_back({
        {"trace_id", trace_id},
        {"span_id", span_id},
        {"parent_span_id", parent_span_id},
        {"name", n.id},
        {"kind", "SPAN_KIND_INTERNAL"},
        {"start_time_unix_nano", index * 1000},
        {"end_time_unix_nano", index * 1000 + 1},
        {"attributes", {{"ir.node_type", n.type}}},
    });
    ++index;
  }

  return nlohmann::json{{"spans", std::move(spans)}, {"trace_id", trace_id}};
}

}  // namespace agenticdsl::ir
