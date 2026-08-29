// src/modules/distillation/trajectory_bridge.cpp
// 功能描述: TrajectoryIR CanonicalIR → DistillationRecord bridge 实现 (Phase 2)
// 设计依据: openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//           Requirements: TrajectoryIR bridge + payload redact 复用 T21 hash_prompt
// 关键不变量:
//   - metadata.agent_id → record.agent_id, metadata.teacher_version → record.teacher_version
//   - canonical_steps → StepRecord（thought/tool_name/observation/latency_ms + 其余 metadata → tool_args）
//   - payload redact: tool_args 中 prompt/input 字段复用 hash_prompt()（T21, 不新造）
//   - V1 偏差: DistillationRecord 无 trajectory_jsonl 字段（Phase 0 冻结, MUST NOT 修改），
//     脱敏落在 StepRecord.tool_args 层 —— FileDistillationWriter 序列化 JSONL 时
//     tool_args 中的 prompt 已是 hash，原始 prompt 不入盘
// 作者：AgenticDSL / capture-mode-and-distillation-writer-v1 Phase 2
// 最后修改日期：2026-08-30

#include "trajectory_bridge.h"

#include "agenticdsl/prompt/prompt_hash.h"  // T21 hash_prompt — payload redact（复用，不新造）

#include <nlohmann/json.hpp>
#include <utility>

namespace agenticdsl::distillation {

DistillationRecord from_trajectory_ir(
    const ir::TrajectoryIR::CanonicalIR& canonical,
    const std::string& trace_id,
    const std::string& source_event) {
  DistillationRecord record;

  // agent_id / teacher_version from metadata
  if (canonical.metadata.contains("agent_id") &&
      canonical.metadata["agent_id"].is_string()) {
    record.agent_id = canonical.metadata["agent_id"].get<std::string>();
  }
  if (canonical.metadata.contains("teacher_version") &&
      canonical.metadata["teacher_version"].is_string()) {
    record.teacher_version = canonical.metadata["teacher_version"].get<std::string>();
  }

  record.trace_id = trace_id;
  record.source_event = source_event;
  // capture_mode 默认 Off（kDefaultCaptureMode）— bridge 不改变 capture 语义

  // canonical_steps → StepRecord（payload redact 复用 hash_prompt）
  for (const auto& cs : canonical.canonical_steps) {
    StepRecord sr;
    sr.thought = cs.metadata.contains("thought")
                     ? cs.metadata["thought"].get<std::string>()
                     : "";
    sr.tool_name = cs.metadata.contains("tool_name")
                       ? cs.metadata["tool_name"].get<std::string>()
                       : "";
    sr.observation = cs.metadata.contains("observation")
                         ? cs.metadata["observation"].get<std::string>()
                         : "";
    sr.latency_ms = cs.metadata.contains("latency_ms")
                        ? cs.metadata["latency_ms"].get<std::uint64_t>()
                        : 0;

    // 其余 metadata 落入 tool_args；prompt/input 字段 hash 化（T21 复用）
    sr.tool_args = cs.metadata;
    for (const char* sensitive : {"prompt", "input"}) {
      if (sr.tool_args.contains(sensitive) && sr.tool_args[sensitive].is_string()) {
        const std::string raw = sr.tool_args[sensitive].get<std::string>();
        sr.tool_args[sensitive] = hash_prompt(raw);  // 不可逆, 16 hex chars
      }
    }

    record.steps.push_back(std::move(sr));
  }

  return record;
}

}  // namespace agenticdsl::distillation
