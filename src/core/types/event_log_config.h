// src/core/types/event_log_config.h
// 功能描述: EventLog 配置 — BREAKING 迁移（bool → CaptureMode 枚举）
// 依据: ADR-0080 v1.2 D10.v1.2.1 + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
//       Requirements: BREAKING 字段迁移彻底 + 5 消费者更新 + EventLogConfig 新字段
// 最后修改日期：2026-08-29

#pragma once

#include "agenticdsl/types/capture_mode.h"

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
  CaptureMode capture_mode = CaptureMode::Off;  // ✅ Phase 1 迁移（替换原 bool 字段）

  // D4 字段（v1 决策）
  size_t max_file_size = 100 * 1024 * 1024;  // 100 MB
  size_t max_rotation_files = 3;
  std::chrono::milliseconds flush_interval{100};

  // ✅ Phase 1 新增 helper
  bool effective_capture_enabled() const {
    return capture_mode != CaptureMode::Off;
  }
};

}  // namespace agenticdsl