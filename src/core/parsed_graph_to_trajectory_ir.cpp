// src/core/parsed_graph_to_trajectory_ir.cpp
// 功能描述：TrajectoryIR::from_parsed_graph() 单向 Converter (T15 Phase 1)
//          ParsedGraph → ParsedIR 快照 (值类型浅拷贝, 单向不变量)
//          Nodes → NodeRecord (id=path, type=字符串, metadata 拷贝)
//          Edges → EdgeRecord (from node.next 派生, weight=1.0 V1 简化)
//          Steps → V1 空占位 (V2 集成 ADR-0061-13 DistillationRecord.reward)
// 设计依据：docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md
//          + openspec/changes/t15-trajectory-ir/specs/trajectory-ir/spec.md
//          "单向 Converter from_parsed_graph" Requirement
// 关键不变量：ParsedGraph 零修改 (仅 const& 读取); 输出为独立快照,
//            后续修改 ParsedGraph 不影响已生成 ParsedIR
// 作者：HydraForge Sprint 24 T15 ship
// 最后修改日期：2026-08-27

#include "agenticdsl/ir/trajectory_ir.h"

#include "core/types/node.h"

namespace agenticdsl::ir {

namespace {

// NodeType enum → 字符串 (NodeRecord.type 无 enum 依赖, 序列化稳定)
const char* node_type_to_string(NodeType type) {
  switch (type) {
    case NodeType::START:             return "start";
    case NodeType::END:               return "end";
    case NodeType::ASSIGN:            return "assign";
    case NodeType::DSL_CALL:          return "dsl_call";
    case NodeType::TOOL_CALL:         return "tool_call";
    case NodeType::RESOURCE:          return "resource";
    case NodeType::FORK:              return "fork";
    case NodeType::JOIN:              return "join";
    case NodeType::GENERATE_SUBGRAPH: return "generate_subgraph";
    case NodeType::ASSERT:            return "assert";
    case NodeType::YIELD:             return "yield";
  }
  return "unknown";
}

}  // namespace

// 单向 Converter: ParsedGraph (const&) → ParsedIR 快照
// 单向不变量: 全部字段值拷贝 (浅拷贝 nlohmann::json 亦为值语义),
//            不持有 ParsedGraph 任何引用/指针
TrajectoryIR::ParsedIR TrajectoryIR::from_parsed_graph(const ParsedGraph& pg) {
  ParsedIR out;
  out.nodes.reserve(pg.nodes.size());

  for (const auto& node : pg.nodes) {
    // Nodes → NodeRecord (浅拷贝值类型)
    NodeRecord rec;
    rec.id = node->path;
    rec.type = node_type_to_string(node->type);
    rec.metadata = node->metadata;  // nlohmann::json 值拷贝
    out.nodes.push_back(std::move(rec));

    // Edges → EdgeRecord (从 node.next 派生, weight=1.0 V1 简化)
    for (const auto& next_path : node->next) {
      EdgeRecord edge;
      edge.from = node->path;
      edge.to = next_path;
      edge.weight = 1.0;
      out.edges.push_back(std::move(edge));
    }
  }

  // Steps: V1 占位 (空数组; V2 集成 ADR-0061-13 DistillationRecord.reward
  //        从 ExecutionSession TraceRecord 推导)
  return out;
}

}  // namespace agenticdsl::ir
