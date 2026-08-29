// include/agenticdsl/types/distillation_record.h
// 功能描述: DistillationRecord / StepRecord / ConvergenceMeta 值类型
// 设计依据: docs/adr/skill/adr-0061-13-distillation-output-format.md §决策 2（字段全集）
//          + docs/adr/adr-0080-v1-2-amendment-d10-decouple.md（D10.v1.2.1 CaptureMode）
//          + docs/adr/adr-0083-evaluator-reward-contract.md（RewardSignal 已 ship）
//          + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//
// 修正（D6 + D7）: 与 ADR-0061-13 §决策 2 完全对齐，含 input/output 字段；
//                  StepRecord.reward 真嵌入 RewardSignal struct（非 double 拍平）。
// 关键不变量:
//   - DistillationRecord 字段全集对齐 ADR-0061-13 §决策 2
//   - input + output ≤ 1.5MB（ADR 不变量 1，防内存爆炸）
//   - input ≤ 64KB / output ≤ 1MB（ADR-0061-13 字段约束）
//   - ≤ 20 步（V1 简化）
//   - trajectory_jsonl / policy_jsonl 是序列化产物（由 FileDistillationWriter 生成）
//   - ConvergenceMeta.agent_id 与 record.agent_id 含义不同（前者是 Training 三重保护 #1）
// 作者：AgenticDSL / capture-mode-and-distillation-writer-v1 Phase 0
// 最后修改日期：2026-08-29
#pragma once

#include "agenticdsl/types/reward_signal.h"   // ✅ 修正 D1: types/ 而非 contract/
#include "agenticdsl/types/capture_mode.h"

#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace agenticdsl {

// StepRecord: ReAct 步骤（对齐 ADR-0061-13 §决策 2）
struct StepRecord {
  std::string thought;           // ReAct thought (可选)
  std::string tool_name;         // 若 action 是 tool call
  nlohmann::json tool_args;
  std::string observation;
  std::uint64_t latency_ms = 0;
};

// ConvergenceMeta: 蒸馏会话元数据（Training 三重保护 #1 校验目标）
struct ConvergenceMeta {
  std::string agent_id;                                       // 三重保护 #1
  std::string teacher_version;
  std::string task_id;
  std::string trace_id;                                       // ADR-0080 v1.1 causal_time 对齐
  std::chrono::system_clock::time_point created_at;
};

// DistillationRecord: 蒸馏数据主记录（对齐 ADR-0061-13 §决策 2 字段全集）
struct DistillationRecord {
  // 输入（ADR-0080 D10.4 对齐）
  std::string input;            // 必须 ≤ 64KB
  std::string output;           // 必须 ≤ 1MB
  std::vector<StepRecord> steps;  // V1: ≤ 20 步

  // 评估信号（修正 D7：真嵌入 RewardSignal struct，而非 double 拍平）
  RewardSignal reward;          // ADR-0083 已 ship，含 quality/scalar/confidence

  // 元数据
  std::string trace_id;         // EventLog causal_time 引用
  std::string source_event;     // llm.request / llm.response event_id
  std::string agent_id;
  std::string teacher_version;  // 教师 Agent 版本 (e.g. "v1.0.0")
  std::uint64_t generation_timestamp_ms = 0;

  // CaptureMode（D10.v1.2.1 关联）
  CaptureMode capture_mode = CaptureMode::Off;

  // ConvergenceMeta（Training 模式必填）
  ConvergenceMeta convergence;
};

// V1 不变量（编译期断言）
static_assert(sizeof(CaptureMode) == 1, "CaptureMode must be 1 byte");

}  // namespace agenticdsl