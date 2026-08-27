// src/modules/ir/trajectory_ir_backend.cpp
// 功能描述：TrajectoryIR V1 backends (T15 Phase 0 桩)
//          to_sft_data() / to_otel_spans() — Phase 2 填充真实序列化
// 设计依据：openspec/changes/t15-trajectory-ir/specs/trajectory-ir/spec.md
//          "V1 Backends (to_sft_data + to_otel_spans)" Requirement
// 作者：HydraForge Sprint 24 T15 ship
// 最后修改日期：2026-08-27

#include "agenticdsl/ir/trajectory_ir.h"

namespace agenticdsl::ir {

// Phase 0 桩: 返回空 JSON (Phase 2 实现 SFT schema 序列化)
nlohmann::json TrajectoryIR::to_sft_data(const CanonicalIR& /*canonical*/) {
  return nlohmann::json::object();
}

// Phase 0 桩: 返回空 JSON (Phase 2 实现 OTLP spans 序列化)
nlohmann::json TrajectoryIR::to_otel_spans(const CanonicalIR& /*canonical*/) {
  return nlohmann::json::object();
}

}  // namespace agenticdsl::ir
