// tests/test_capture_mode.cpp
// 功能描述: CaptureMode 三态枚举单元测试 + 零编译覆盖（蒸馏契约全头文件）
// 依据: openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//        Requirements: CaptureMode 三态 + IDistillationWriter 纯虚契约
// 修正注: 本测试文件 include 全部 3 个新头文件，防止头文件语法错误静默 ship（Oracle + Metis 一致发现）
// 作者：AgenticDSL / capture-mode-and-distillation-writer-v1 Phase 0
// 最后修改日期：2026-08-29
#include <catch_amalgamated.hpp>

// 全部 3 个新头文件（关键：防零编译覆盖盲区）
#include "agenticdsl/types/capture_mode.h"
#include "agenticdsl/types/distillation_record.h"
#include "agenticdsl/contract/idistillation_writer.h"

using agenticdsl::CaptureMode;
using agenticdsl::to_string;
using agenticdsl::parse_capture_mode;
using agenticdsl::kDefaultCaptureMode;
using agenticdsl::DistillationRecord;
using agenticdsl::StepRecord;
using agenticdsl::ConvergenceMeta;
using agenticdsl::IDistillationWriter;
using agenticdsl::IDistillationWriterPtr;

TEST_CASE("enum_serialization_round_trip", "[capture_mode][phase0]") {
  // 正向：to_string 3 值
  REQUIRE(to_string(CaptureMode::Off) == "Off");
  REQUIRE(to_string(CaptureMode::Online) == "Online");
  REQUIRE(to_string(CaptureMode::Training) == "Training");

  // 反向：parse 3 值
  REQUIRE(parse_capture_mode("Off") == CaptureMode::Off);
  REQUIRE(parse_capture_mode("Online") == CaptureMode::Online);
  REQUIRE(parse_capture_mode("Training") == CaptureMode::Training);

  // 合成 round-trip（spec Scenario "to_string/parse round-trip"）
  for (auto m : {CaptureMode::Off, CaptureMode::Online, CaptureMode::Training}) {
    REQUIRE(parse_capture_mode(to_string(m)) == m);
  }
}

TEST_CASE("parse_invalid_string_throws", "[capture_mode][phase0]") {
  REQUIRE_THROWS_AS(parse_capture_mode("invalid"), std::invalid_argument);
  REQUIRE_THROWS_AS(parse_capture_mode(""), std::invalid_argument);
  REQUIRE_THROWS_AS(parse_capture_mode("OFF"), std::invalid_argument);  // 大小写敏感
  REQUIRE_THROWS_AS(parse_capture_mode("off"), std::invalid_argument);
}

TEST_CASE("default_value_and_static_asserts", "[capture_mode][phase0]") {
  // 默认值（修正：测试 kDefaultCaptureMode 常量本身，而非 EventLogConfig 字段，
  // 因为 Phase 0 不 touch event_log_config.h —— 该 Scenario 在 Phase 1 验证）
  REQUIRE(kDefaultCaptureMode == CaptureMode::Off);
  REQUIRE(static_cast<uint8_t>(CaptureMode::Off) == 0);
  REQUIRE(static_cast<uint8_t>(CaptureMode::Online) == 1);
  REQUIRE(static_cast<uint8_t>(CaptureMode::Training) == 2);
}

// 编译期断言（plan 风险 1 提及但 Step 2 测试未实现 —— 修正）
static_assert(sizeof(CaptureMode) == 1, "CaptureMode must be 1 byte (uint8_t)");
static_assert(CaptureMode::Off == static_cast<CaptureMode>(0), "Off=0 stable for JSONL header persistence");

// 零编译覆盖（修正 D4）：确保 distillation_record.h 和 idistillation_writer.h 在 Phase 0 真的被编译
TEST_CASE("phase0_headers_compile_smoke", "[capture_mode][phase0][headers]") {
  // DistillationRecord 默认构造可工作
  DistillationRecord default_record;
  REQUIRE(default_record.capture_mode == CaptureMode::Off);
  REQUIRE(default_record.input.empty());
  REQUIRE(default_record.output.empty());
  REQUIRE(default_record.steps.empty());

  // ConvergenceMeta 默认构造
  ConvergenceMeta default_meta;
  REQUIRE(default_meta.agent_id.empty());

  // IDistillationWriter 是抽象类（不能实例化，只能用 static_assert）
  static_assert(std::is_abstract_v<IDistillationWriter>,
                "IDistillationWriter must be abstract (3 pure virtual methods)");

  // 工厂函数签名存在（编译期验证）
  static_assert(std::is_same_v<decltype(&IDistillationWriter::make_file_writer),
                              std::unique_ptr<IDistillationWriter>(*)(
                                  const std::filesystem::path&,
                                  const std::string&)>);
}
