// include/agenticdsl/contract/ilogger.h
// ILogger 接口 - 仅供测试注入, 生产实现桥接 agenticdsl::log 门面
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §6.1 (A1 修订)
// 作者: chat-session-pdk-lift Change 1
// 日期: 2026-09-11
//
// A1 修订: 只 1 个虚方法, 不新建 LogLevel/LogSourceLocation 自由函数,
// 避免与 src/common/log/log.h 既有门面形成双日志漂移。
// 注意 log.h 的 Level 枚举值与枚举名与这里不同 (DEBUG/INFO/WARN/ERROR vs kDebug/kInfo/...),
// 桥接转换在 StderrLogger 内完成。

#pragma once

#include <string_view>

namespace agenticdsl {

enum class LogLevel { kDebug, kInfo, kWarn, kError };

class ILogger {
 public:
  virtual ~ILogger() = default;
  virtual void log(LogLevel level, std::string_view message) = 0;
};

}  // namespace agenticdsl
