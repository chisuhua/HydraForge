// src/common/io/stderr_logger.h
// StderrLogger - 生产用 ILogger 实现, 委托既有 agenticdsl::log::* 全局门面
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §6.1 (A1)
// 作者: chat-session-pdk-lift Change 1
// 日期: 2026-09-11
//
// A1 修订: 避免与 src/common/log/log.h 形成双门面漂移 —
// 本类不重新实现日志格式/级别门控, 仅做 LogLevel → log::Level 转换 + 委托 log::emit。

#pragma once

#include "agenticdsl/contract/ilogger.h"

namespace agenticdsl {

// 生产用 ILogger 实现 - 委托既有 agenticdsl::log::* 全局门面
//
// 2026-09-11 实施偏差: 不加 "[chat] " 前缀 (plan 原文加了), 因为本类是
// 通用 ILogger 实现, 不应硬编码 chat 领域前缀; 调用方自行在消息内携带上下文。
class StderrLogger : public ILogger {
 public:
  void log(LogLevel level, std::string_view message) override;
};

}  // namespace agenticdsl
