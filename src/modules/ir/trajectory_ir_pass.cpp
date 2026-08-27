// src/modules/ir/trajectory_ir_pass.cpp
// 功能描述：ConstantFoldingPass V1 占位 (T15 Phase 0 桩)
//          Phase 0: 桩实现 (返回空), Phase 2 改为 pass-through (输入==输出)
//          V2 扩展: 真实常量折叠 + 死代码消除
// 设计依据：openspec/changes/t15-trajectory-ir/specs/trajectory-ir/spec.md
//          "V1 Pass 占位 (ConstantFoldingPass)" Requirement
// 作者：HydraForge Sprint 24 T15 ship
// 最后修改日期：2026-08-27

#include "agenticdsl/ir/trajectory_ir.h"

namespace agenticdsl::ir {

// Phase 0 桩: 返回空 CanonicalIR (Phase 2 改 pass-through)
TrajectoryIR::CanonicalIR ConstantFoldingPass::run(
    const TrajectoryIR::CanonicalIR& /*input*/) {
  return TrajectoryIR::CanonicalIR{};
}

}  // namespace agenticdsl::ir
