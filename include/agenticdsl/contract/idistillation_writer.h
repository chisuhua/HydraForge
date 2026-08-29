// include/agenticdsl/contract/idistillation_writer.h
// 功能描述: IDistillationWriter L1 契约层接口（3 虚函数 + 1 静态工厂）
// 设计依据: docs/adr/skill/adr-0061-13-distillation-output-format.md §决策 3（完整对齐）
//          + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//
// 修正（D3）: 与 ADR-0061-13 §决策 3 完全对齐。
//   - 原 plan: 2 虚函数（write + finalize()）
//   - 修正版: 3 虚函数（write_record + close + finalize(meta)）+ 1 静态工厂（make_file_writer）
//   - 偏离决策: 无偏离（完全对齐 Approved ADR）
//   - 阶段边界: write_record + close 在 Phase 1 FileDistillationWriter 实现；
//                finalize(meta) 在 Phase 1 末尾实现（接收 meta.json 元数据）；
//                make_file_writer 工厂在 Phase 1 实现。
// 关键不变量:
//   - L1 契约层独立（不 #include src/ 任何头文件）
//   - 3 纯虚函数（write_record + close + finalize(meta)）+ 1 静态工厂
//   - 默认析构（virtual ~IDistillationWriter() = default）
//   - V1 FileDistillationWriter 假定单写线程；多线程写需外部同步（V2 扩展）
// 作者：AgenticDSL / capture-mode-and-distillation-writer-v1 Phase 0
// 最后修改日期：2026-08-29
#pragma once

#include "agenticdsl/types/distillation_record.h"

#include <filesystem>
#include <memory>
#include <string>

namespace agenticdsl {

// V1 元数据（用于 finalize() 写 meta.json，ADR-0061-13 决策 1）
struct DistillationMetadata {
  std::string version;                 // 元数据 schema 版本
  std::uint64_t total_examples = 0;    // 蒸馏数据集大小
  std::string dataset_hash;            // SHA256 of entire dataset
  nlohmann::json generation_config;    // 训练配置（teacher_version, capture_mode 等）
};

class IDistillationWriter {
 public:
  virtual ~IDistillationWriter() = default;

  // 写入单条 record（V1: 同步落盘 + fsync）
  virtual void write_record(const DistillationRecord& record) = 0;

  // flush + 关闭（析构时自动调用）
  virtual void close() = 0;

  // 元数据（生成结束后写入 meta.json）
  virtual void finalize(const DistillationMetadata& meta) = 0;

  // 工厂函数（Phase 1 FileDistillationWriter 实现）
  static std::unique_ptr<IDistillationWriter> make_file_writer(
      const std::filesystem::path& output_dir,
      const std::string& agent_id);
};

using IDistillationWriterPtr = std::unique_ptr<IDistillationWriter>;

}  // namespace agenticdsl