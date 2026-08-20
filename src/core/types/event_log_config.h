// src/core/types/event_log_config.h
// 功能描述：EventLog 配置（ADR-0080 v1.1 D11 EngineConfig 字段）
//          默认全关；调用方显式 enable_event_log 后才生效。
// 设计依据：ADR-0080 v1.1 amendment §决策 D11
// 最后修改日期：2026-08-12

#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>

namespace agenticdsl {

struct EventLogConfig {
  bool event_log_enabled = false;            // 默认关，向后兼容
  std::string event_log_agent_id;            // 必填（启用时）
  std::filesystem::path event_log_dir =
      std::filesystem::path("~/.hydraforge/event_log");
  bool capture_prompt_bytes = false;         // D10：默认关（隐私）

  // D4 字段（v1 决策）
  size_t max_file_size = 100 * 1024 * 1024;  // 100 MB
  size_t max_rotation_files = 3;
  std::chrono::milliseconds flush_interval{100};
};

}  // namespace agenticdsl