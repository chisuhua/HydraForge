// src/modules/ir/trajectory_ir_pass.cpp
// 功能描述：ConstantFoldingPass V1 占位 (T15 Phase 2)
//          V1: pass-through (输入 CanonicalIR 原样返回, 字段值完全等价)
//          V2 扩展: 真实常量折叠 + 死代码消除 (DeadCodeElim / LoopUnroll)
// 设计依据：openspec/changes/t15-trajectory-ir/specs/trajectory-ir/spec.md
//          "V1 Pass 占位 (ConstantFoldingPass)" Requirement
// 作者：HydraForge Sprint 24 T15 ship
// 最后修改日期：2026-08-27

#include "agenticdsl/ir/trajectory_ir.h"

namespace agenticdsl::ir {

// V1 占位: pass-through (输入 == 输出)
TrajectoryIR::CanonicalIR ConstantFoldingPass::run(
    const TrajectoryIR::CanonicalIR& input) {
  return input;
}

}  // namespace agenticdsl::ir
