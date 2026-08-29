// src/modules/distillation/file_writer.cpp
// 功能描述: FileDistillationWriter V1 实现 — IDistillationWriter 默认实现
// 依据: ADR-0061-13 §决策 3 + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
// 最后修改日期：2026-08-29

#include "file_writer.h"

#include <chrono>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace agenticdsl {

// 工厂实现
std::unique_ptr<IDistillationWriter> IDistillationWriter::make_file_writer(
    const std::filesystem::path& output_dir,
    const std::string& agent_id) {
  return std::make_unique<FileDistillationWriter>(output_dir, agent_id);
}

FileDistillationWriter::FileDistillationWriter(
    const std::filesystem::path& output_dir,
    std::string agent_id)
    : output_dir_(output_dir), agent_id_(std::move(agent_id)) {
  std::filesystem::create_directories(output_dir_);
}

void FileDistillationWriter::validate_training_protection(
    const DistillationRecord& record) {
  // ≤1.5MB 硬上限（所有模式通用，不限于 Training）
  const std::size_t total = record.input.size() + record.output.size();
  if (total > kMaxRecordSizeBytes) {
    throw std::length_error(
        "FileDistillationWriter: record size " + std::to_string(total) +
        " bytes exceeds 1.5MB hard limit");
  }

  if (record.capture_mode != CaptureMode::Training) return;

  // 三重保护 #1: agent_id 非空
  if (record.agent_id.empty()) {
    throw std::invalid_argument(
        "FileDistillationWriter: Training mode requires non-empty agent_id");
  }
}

std::string FileDistillationWriter::next_filename() const {
  std::ostringstream oss;
  oss << agent_id_ << "_" << std::setw(6) << std::setfill('0') << seq_.load()
      << ".distill.v1.jsonl";
  return oss.str();
}

void FileDistillationWriter::open_new_jsonl() {
  current_filename_ = next_filename();
  jsonl_stream_.open(output_dir_ / current_filename_,
                      std::ios::out | std::ios::app | std::ios::binary);
  if (!jsonl_stream_) {
    throw std::runtime_error(
        "FileDistillationWriter: failed to open " + current_filename_);
  }
}

void FileDistillationWriter::write_record(const DistillationRecord& record) {
  std::lock_guard<std::mutex> lock(mu_);

  validate_training_protection(record);

  // 每 record 一个文件（便于切片）：<agent_id>_<seq>.distill.v1.jsonl
  open_new_jsonl();

  // 序列化为 JSON 行（v1: 简单 nlohmann::json dump）
  nlohmann::json j;
  j["input"] = record.input;
  j["output"] = record.output;
  j["steps"] = nlohmann::json::array();
  for (const auto& s : record.steps) {
    j["steps"].push_back({{"thought", s.thought}, {"tool_name", s.tool_name},
                           {"tool_args", s.tool_args}, {"observation", s.observation},
                           {"latency_ms", s.latency_ms}});
  }
  j["reward"] = {{"scalar", record.reward.scalar},
                 {"confidence", record.reward.confidence},
                 {"quality", static_cast<int>(record.reward.quality)}};
  j["trace_id"] = record.trace_id;
  j["source_event"] = record.source_event;
  j["agent_id"] = record.agent_id;
  j["teacher_version"] = record.teacher_version;
  j["generation_timestamp_ms"] = record.generation_timestamp_ms;
  j["capture_mode"] = agenticdsl::to_string(record.capture_mode);
  j["convergence"] = {{"agent_id", record.convergence.agent_id},
                       {"teacher_version", record.convergence.teacher_version},
                       {"task_id", record.convergence.task_id},
                       {"trace_id", record.convergence.trace_id}};

  jsonl_stream_ << j.dump() << "\n";
  jsonl_stream_.flush();  // V1: 同步 fsync（性能 v2 优化）
  jsonl_stream_.close();

  // 序列号递增（每 record 一个文件，便于切片）
  ++seq_;
}

void FileDistillationWriter::close() {
  std::lock_guard<std::mutex> lock(mu_);
  if (jsonl_stream_.is_open()) {
    jsonl_stream_.flush();
    jsonl_stream_.close();
  }
  closed_ = true;
}

void FileDistillationWriter::finalize(const DistillationMetadata& meta) {
  std::lock_guard<std::mutex> lock(mu_);
  // 确保 close 已调用（内部直接操作流，避免重复加锁死锁）
  if (!closed_) {
    if (jsonl_stream_.is_open()) {
      jsonl_stream_.flush();
      jsonl_stream_.close();
    }
    closed_ = true;
  }

  // 写 meta.json
  nlohmann::json j;
  j["version"] = meta.version;
  j["total_examples"] = meta.total_examples;
  j["dataset_hash"] = meta.dataset_hash;
  j["generation_config"] = meta.generation_config;

  std::filesystem::path meta_path = output_dir_ /
      (agent_id_ + "_" + std::to_string(seq_.load()) + ".distill.v1.meta.json");
  std::ofstream ofs(meta_path);
  if (!ofs) {
    throw std::runtime_error(
        "FileDistillationWriter: failed to write meta.json");
  }
  ofs << j.dump();
  ofs.close();
}

}  // namespace agenticdsl