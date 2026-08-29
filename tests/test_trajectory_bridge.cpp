// tests/test_trajectory_bridge.cpp
// 功能描述: TrajectoryIR CanonicalIR → DistillationRecord bridge 单元测试 (Phase 2)
// 设计依据: openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//           Requirements: TrajectoryIR bridge + payload redact 复用 T21 hash_prompt
// 关键不变量:
//   - from_trajectory_ir 位于 agenticdsl::distillation namespace（与 FileDistillationWriter 对齐）
//   - agent_id / teacher_version 从 canonical.metadata 映射
//   - canonical_steps → record.steps 映射（thought/tool_name/observation/latency_ms）
//   - payload redact: steps metadata 中 prompt/input 字段复用 hash_prompt() 脱敏（不保留原文）
//   - 空 CanonicalIR 安全（零 steps）
// 作者：AgenticDSL / capture-mode-and-distillation-writer-v1 Phase 2
// 最后修改日期：2026-08-30

#include "catch_amalgamated.hpp"

#include "agenticdsl/ir/trajectory_ir.h"
#include "agenticdsl/types/capture_mode.h"
#include "agenticdsl/types/distillation_record.h"
#include "src/modules/distillation/trajectory_bridge.h"  // D4: include 真实实现 header（防零编译覆盖）

#include <string>

using agenticdsl::CaptureMode;
using agenticdsl::DistillationRecord;
using agenticdsl::StepRecord;

namespace {

// helper: 构造带 metadata 的 CanonicalIR
agenticdsl::ir::TrajectoryIR::CanonicalIR make_canonical() {
  agenticdsl::ir::TrajectoryIR::CanonicalIR c;
  c.schema_version = "1.0";
  c.metadata = {{"agent_id", "teacher_v1"}, {"teacher_version", "v1.0.0"}};
  return c;
}

}  // namespace

// Case 1: CanonicalIR → DistillationRecord 基本字段映射
TEST_CASE("bridge_canonical_to_record_basic", "[trajectory_bridge][phase2]") {
  auto canonical = make_canonical();
  canonical.canonical_steps.push_back(
      {"step1", {{"thought", "think"}, {"tool_name", "tool1"}}});

  DistillationRecord record = agenticdsl::distillation::from_trajectory_ir(
      canonical, "trace_abc", "llm.response");

  REQUIRE(record.agent_id == "teacher_v1");
  REQUIRE(record.teacher_version == "v1.0.0");
  REQUIRE(record.trace_id == "trace_abc");
  REQUIRE(record.source_event == "llm.response");
  REQUIRE(record.capture_mode == CaptureMode::Off);  // 默认 Off
  REQUIRE(record.steps.size() == 1);
  REQUIRE(record.steps[0].thought == "think");
  REQUIRE(record.steps[0].tool_name == "tool1");
}

// Case 2: 空 metadata → agent_id / teacher_version 为空
TEST_CASE("bridge_missing_metadata_fields_empty", "[trajectory_bridge][phase2]") {
  agenticdsl::ir::TrajectoryIR::CanonicalIR canonical;  // 空 metadata / 零 steps
  DistillationRecord record = agenticdsl::distillation::from_trajectory_ir(canonical);

  REQUIRE(record.agent_id.empty());
  REQUIRE(record.teacher_version.empty());
  REQUIRE(record.trace_id.empty());
  REQUIRE(record.source_event.empty());
  REQUIRE(record.steps.empty());
}

// Case 3: canonical_steps → StepRecord 全字段映射（含 observation / latency_ms）
TEST_CASE("bridge_steps_map_observation_and_latency", "[trajectory_bridge][phase2]") {
  auto canonical = make_canonical();
  canonical.canonical_steps.push_back(
      {"step1", {{"thought", "t1"}, {"tool_name", "tool1"},
                 {"observation", "obs1"}, {"latency_ms", 42}}});
  canonical.canonical_steps.push_back(
      {"step2", {{"thought", "t2"}}});

  DistillationRecord record = agenticdsl::distillation::from_trajectory_ir(canonical);

  REQUIRE(record.steps.size() == 2);
  REQUIRE(record.steps[0].observation == "obs1");
  REQUIRE(record.steps[0].latency_ms == 42);
  REQUIRE(record.steps[1].thought == "t2");
  REQUIRE(record.steps[1].observation.empty());
  REQUIRE(record.steps[1].latency_ms == 0);
}

// Case 4: payload redact — steps metadata 中 prompt 字段复用 hash_prompt() 脱敏
TEST_CASE("bridge_payload_redact_hashes_prompt", "[trajectory_bridge][phase2][redact]") {
  // spec Requirement: payload redact 复用 T21 — hash_prompt() 调用, 不新造
  const std::string kSecretPrompt = "SuperSecretTrainingPrompt-42";
  auto canonical = make_canonical();
  canonical.canonical_steps.push_back(
      {"step1", {{"thought", "t1"}, {"prompt", kSecretPrompt}}});

  DistillationRecord record = agenticdsl::distillation::from_trajectory_ir(canonical);

  REQUIRE(record.steps.size() == 1);
  // prompt 进入 tool_args（redacted metadata），值 = hash 而非原文
  REQUIRE(record.steps[0].tool_args.contains("prompt"));
  const std::string hashed = record.steps[0].tool_args["prompt"].get<std::string>();
  REQUIRE(hashed != kSecretPrompt);
  REQUIRE(hashed.size() == 16);  // hash_prompt() 返回 16 hex chars
  // 序列化后 JSONL 不含原文
  const std::string dumped = record.steps[0].tool_args.dump();
  REQUIRE(dumped.find(kSecretPrompt) == std::string::npos);
}

// Case 5: 空 CanonicalIR 安全（零 steps / 零 nodes）
TEST_CASE("bridge_empty_canonical_safe", "[trajectory_bridge][phase2]") {
  agenticdsl::ir::TrajectoryIR::CanonicalIR canonical;  // 零 nodes/steps
  REQUIRE_NOTHROW(agenticdsl::distillation::from_trajectory_ir(canonical));
  DistillationRecord record = agenticdsl::distillation::from_trajectory_ir(canonical);
  REQUIRE(record.steps.empty());
  REQUIRE(record.agent_id.empty());
}

// Case 6: payload redact — input 字段同样 hash 脱敏
TEST_CASE("bridge_input_also_redacted", "[trajectory_bridge][phase2][redact]") {
  const std::string kSecretInput = "RawUserInputShouldNotLeak";
  auto canonical = make_canonical();
  canonical.canonical_steps.push_back(
      {"step1", {{"input", kSecretInput}, {"thought", "t1"}}});

  DistillationRecord record = agenticdsl::distillation::from_trajectory_ir(canonical);

  REQUIRE(record.steps[0].tool_args.contains("input"));
  const std::string hashed = record.steps[0].tool_args["input"].get<std::string>();
  REQUIRE(hashed != kSecretInput);
  REQUIRE(hashed.size() == 16);
  REQUIRE(record.steps[0].tool_args.dump().find(kSecretInput) == std::string::npos);
}
