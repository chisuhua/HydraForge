// src/modules/distillation/file_writer.h
// 功能描述: FileDistillationWriter V1 — IDistillationWriter 默认实现
// 依据: ADR-0061-13 §决策 3 + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//        Requirements: IDistillationWriter FileDistillationWriter 实现 + Training 三重保护 + ≤1.5MB 硬约束
// 关键不变量:
//   - 3 虚函数对齐 ADR-0061-13 决策 3：write_record + close + finalize(meta)
//   - 1 静态工厂 make_file_writer（实现于 .cpp）
//   - ≤1.5MB 硬上限（input + output 总大小）
//   - Training 模式 + agent_id 空 → throw invalid_argument
//   - 命名：<agent_id>_<seq>.distill.v1.jsonl + .meta.json
// 最后修改日期：2026-08-29

#pragma once

#include "agenticdsl/contract/idistillation_writer.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace agenticdsl {

class FileDistillationWriter : public IDistillationWriter {
 public:
  // 工厂函数实现（Phase 1）
  static std::unique_ptr<IDistillationWriter> make_file_writer(
      const std::filesystem::path& output_dir,
      const std::string& agent_id);

  FileDistillationWriter(const std::filesystem::path& output_dir,
                          std::string agent_id);
  ~FileDistillationWriter() override = default;

  void write_record(const DistillationRecord& record) override;
  void close() override;
  void finalize(const DistillationMetadata& meta) override;

 private:
  std::filesystem::path output_dir_;
  std::string agent_id_;
  std::ofstream jsonl_stream_;
  std::atomic<std::uint64_t> seq_{0};  // 文件序列号
  std::string current_filename_;       // 当前 .jsonl 文件名
  std::mutex mu_;                       // write_record 互斥（V1 单线程假定，多线程需外部同步）
  bool closed_ = false;

  // ≤1.5MB 硬上限
  static constexpr std::size_t kMaxRecordSizeBytes = 1.5 * 1024 * 1024;

  void validate_training_protection(const DistillationRecord& record);
  std::string next_filename() const;
  void open_new_jsonl();
};

}  // namespace agenticdsl