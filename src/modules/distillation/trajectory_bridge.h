// src/modules/distillation/trajectory_bridge.h
// 功能描述: TrajectoryIR CanonicalIR → DistillationRecord bridge 声明 (Phase 2)
// 设计依据: openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//           Requirements: TrajectoryIR bridge + payload redact 复用 T21 hash_prompt
// 关键不变量:
//   - 位于 agenticdsl::distillation namespace（与 FileDistillationWriter 对齐）
//   - agent_id / teacher_version 从 canonical.metadata 映射
//   - canonical_steps → StepRecord 映射（thought/tool_name/observation/latency_ms）
//   - payload redact: step metadata 中 prompt/input 字段复用 hash_prompt() 脱敏
//   - V1 偏差: DistillationRecord 无 trajectory_jsonl 字段（Phase 0 冻结），
//     脱敏落在 StepRecord.tool_args 层（FileDistillationWriter 序列化时 JSONL 不含原始 prompt）
// 作者：AgenticDSL / capture-mode-and-distillation-writer-v1 Phase 2
// 最后修改日期：2026-08-30
#pragma once

#include "agenticdsl/ir/trajectory_ir.h"
#include "agenticdsl/types/distillation_record.h"

#include <string>

namespace agenticdsl::distillation {

// TrajectoryIR CanonicalIR → DistillationRecord
// V1: metadata(agent_id/teacher_version) + canonical_steps → StepRecord
//     payload redact: 复用 hash_prompt()（T21，不新造），step metadata 中
//     prompt/input 字段替换为 hash（JSONL 序列化后不含原始 prompt）
DistillationRecord from_trajectory_ir(
    const ir::TrajectoryIR::CanonicalIR& canonical,
    const std::string& trace_id = "",
    const std::string& source_event = "");

}  // namespace agenticdsl::distillation
