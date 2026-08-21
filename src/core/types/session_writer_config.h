// src/core/types/session_writer_config.h
// 功能描述：SessionWriter 配置（ADR-0079 v1.1 D5 + D6）
//          默认全关；调用方显式 enable_session_writer 后才生效。
//          与 EventLogConfig 对称：互不干扰，独立互斥锁。
// 设计依据：ADR-0079 v1.1 + SessionWriter 实施 (P5 session-writer-bridge)
// 最后修改日期：2026-08-20

#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>

namespace agenticdsl {

struct SessionWriterConfig {
  bool session_writer_enabled = false;       // 默认关，向后兼容
  std::string session_id;                    // 必填（启用时），D10 命名空间前缀 (sm:<uuid>)
  std::filesystem::path writer_dir =
      std::filesystem::path("~/.hydraforge/sessions");

  // D6 topic 过滤白名单：仅这些 topic 触发 JSONL 写入
  // 默认包含 ADR-0079 v1.1 D6 表的 13 个 topic
  static const std::size_t kWhitelistedTopicCount = 13;

  // 落盘与文件管理
  size_t max_file_size = 100 * 1024 * 1024;   // 100 MB
  size_t max_rotation_files = 3;
  std::chrono::milliseconds flush_interval{100};
};

}  // namespace agenticdsl