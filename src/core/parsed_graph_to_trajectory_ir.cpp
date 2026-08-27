// src/core/parsed_graph_to_trajectory_ir.cpp
// 功能描述：TrajectoryIR::from_parsed_graph() 单向 Converter (T15 Phase 0 桩)
//          Phase 0: 桩实现 (返回空 ParsedIR), Phase 1 填充真实转换
// 设计依据：ADR-0061-06 v1.1 (独立序列化视图, ParsedGraph 零修改)
// 作者：HydraForge Sprint 24 T15 ship
// 最后修改日期：2026-08-27

#include "agenticdsl/ir/trajectory_ir.h"

namespace agenticdsl::ir {

// Phase 0 桩: 返回空 ParsedIR (Phase 1 实现 Nodes/Edges 真实转换)
TrajectoryIR::ParsedIR TrajectoryIR::from_parsed_graph(const ParsedGraph& /*pg*/) {
  return ParsedIR{};
}

}  // namespace agenticdsl::ir
