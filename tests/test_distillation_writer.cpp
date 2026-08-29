// tests/test_distillation_writer.cpp
// 功能描述: FileDistillationWriter V1 单元测试（写 + close + finalize + make_file_writer 工厂 + Training 三重保护 + ≤1.5MB 硬约束 + 审计事件）
// 依据: openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//        Requirements: BREAKING 字段迁移 + IDistillationWriter FileDistillationWriter + payload redact + capture_mode_downgrade 审计事件

#include "catch_amalgamated.hpp"

#include "agenticdsl/types/capture_mode.h"
#include "agenticdsl/types/distillation_record.h"
#include "agenticdsl/contract/idistillation_writer.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "src/modules/distillation/file_writer.h"  // D4: 零编译覆盖 — FileDistillationWriter 实现头文件

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using agenticdsl::CaptureMode;
using agenticdsl::DistillationRecord;
using agenticdsl::StepRecord;
using agenticdsl::DistillationMetadata;
using agenticdsl::IDistillationWriter;
using agenticdsl::IDistillationWriterPtr;
using agenticdsl::RewardSignal;
using agenticdsl::FileDistillationWriter;

namespace fs = std::filesystem;

// Helper: 创建临时目录
static fs::path make_temp_dir(const std::string& prefix) {
  fs::path p = fs::temp_directory_path() / (prefix + "_" + std::to_string(std::rand()));
  fs::create_directories(p);
  return p;
}

// Helper: 构造测试 record
static DistillationRecord make_test_record(const std::string& agent_id, CaptureMode mode) {
  DistillationRecord r;
  r.input = "test_input";
  r.output = "test_output";
  r.steps.push_back(StepRecord{"thought1", "tool1", nlohmann::json::object(), "obs1", 100});
  r.reward = RewardSignal{};  // 默认 RewardSignal
  r.trace_id = "trace_001";
  r.source_event = "llm.response";
  r.agent_id = agent_id;
  r.teacher_version = "v1.0.0";
  r.generation_timestamp_ms = 1234567890;
  r.capture_mode = mode;
  r.convergence.agent_id = agent_id;
  r.convergence.teacher_version = "v1.0.0";
  r.convergence.task_id = "task_001";
  r.convergence.trace_id = "trace_001";
  return r;
}

// Case 1: FileDistillationWriter 基本 write_record + close 流程
TEST_CASE("file_writer_write_record_creates_jsonl", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_write");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "test_agent");
  REQUIRE(writer != nullptr);

  DistillationRecord r = make_test_record("test_agent", CaptureMode::Online);
  writer->write_record(r);
  writer->close();

  // 验证文件存在：<agent_id>_<seq>.distill.v1.jsonl
  bool found = false;
  for (const auto& entry : fs::directory_iterator(temp_dir)) {
    if (entry.path().extension() == ".jsonl") {
      found = true;
      std::ifstream ifs(entry.path());
      std::string line;
      REQUIRE(std::getline(ifs, line));
      REQUIRE(line.find("test_input") != std::string::npos);
      REQUIRE(line.find("test_output") != std::string::npos);
      REQUIRE(line.find("\"agent_id\":\"test_agent\"") != std::string::npos);
      REQUIRE(line.find("\"capture_mode\":\"Online\"") != std::string::npos);
      REQUIRE(line.find("\"trace_id\":\"trace_001\"") != std::string::npos);
    }
  }
  REQUIRE(found);
  fs::remove_all(temp_dir);
}

// Case 2: finalize 写 meta.json
TEST_CASE("file_writer_finalize_writes_meta_json", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_meta");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "test_agent");

  DistillationRecord r = make_test_record("test_agent", CaptureMode::Online);
  writer->write_record(r);

  DistillationMetadata meta;
  meta.version = "v1";
  meta.total_examples = 1;
  meta.dataset_hash = "abc123";
  meta.generation_config = {{"teacher_version", "v1.0.0"}, {"capture_mode", "Online"}};
  writer->finalize(meta);

  // 验证 meta.json 存在
  bool found = false;
  for (const auto& entry : fs::directory_iterator(temp_dir)) {
    if (entry.path().filename().string().find(".meta.json") != std::string::npos) {
      found = true;
      std::ifstream ifs(entry.path());
      std::string content((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
      REQUIRE(content.find("\"version\":\"v1\"") != std::string::npos);
      REQUIRE(content.find("\"total_examples\":1") != std::string::npos);
      REQUIRE(content.find("\"dataset_hash\":\"abc123\"") != std::string::npos);
    }
  }
  REQUIRE(found);
  fs::remove_all(temp_dir);
}

// Case 3: ≤1.5MB 硬上限（超限 throw std::length_error）
TEST_CASE("file_writer_size_limit_throws_length_error", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_size");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "test_agent");

  DistillationRecord r = make_test_record("test_agent", CaptureMode::Online);
  r.input = std::string(2 * 1024 * 1024, 'x');  // 2MB > 1.5MB 上限
  REQUIRE_THROWS_AS(writer->write_record(r), std::length_error);

  fs::remove_all(temp_dir);
}

// Case 4: Training 模式 + agent_id 为空 → throw
TEST_CASE("training_mode_empty_agent_id_throws", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_empty_agent");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "");

  DistillationRecord r = make_test_record("", CaptureMode::Training);
  REQUIRE_THROWS_AS(writer->write_record(r), std::invalid_argument);

  fs::remove_all(temp_dir);
}

// Case 5: Training 模式 + agent_id 非空 → PASS（成功写入）
TEST_CASE("training_mode_valid_agent_id_succeeds", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_training");
  // 路径包含 train 以满足"路径必含 train|distill" 隐含规则
  fs::path train_dir = temp_dir / "train_data";
  fs::create_directories(train_dir);

  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(train_dir, "training_agent");
  REQUIRE(writer != nullptr);

  DistillationRecord r = make_test_record("training_agent", CaptureMode::Training);
  writer->write_record(r);
  writer->close();
  // 不抛异常即视为通过

  fs::remove_all(temp_dir);
}

// Case 6: write_record + write_record 序列号递增
TEST_CASE("file_writer_sequence_increments", "[distillation_writer][phase1]") {
  fs::path temp_dir = make_temp_dir("distill_seq");
  IDistillationWriterPtr writer = IDistillationWriter::make_file_writer(temp_dir, "seq_agent");

  DistillationRecord r1 = make_test_record("seq_agent", CaptureMode::Online);
  DistillationRecord r2 = make_test_record("seq_agent", CaptureMode::Online);
  writer->write_record(r1);
  writer->write_record(r2);
  writer->close();

  // 验证有 2 个不同 seq 文件（filename 含序列号）
  int jsonl_count = 0;
  for (const auto& entry : fs::directory_iterator(temp_dir)) {
    if (entry.path().extension() == ".jsonl") jsonl_count++;
  }
  REQUIRE(jsonl_count == 2);

  fs::remove_all(temp_dir);
}