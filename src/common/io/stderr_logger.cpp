// src/common/io/stderr_logger.cpp
// StderrLogger 实现 - 桥接 agenticdsl::log 门面
// 日期: 2026-09-11

#include "stderr_logger.h"

#include "common/log/log.h"

namespace agenticdsl {

void StderrLogger::log(LogLevel level, std::string_view message) {
  // **2026-09-11 Momus 修订**: `agenticdsl::log` 命名空间**只**暴露单一函数
  // `log::emit(Level, const std::string&)` (见 `src/common/log/log.h:52`)。
  // `log::info/warn/error/debug` 自由函数**不存在**
  // (只有 `LOG_INFO/WARN/ERROR/DEBUG` 4 个宏, 见 log.h:88-103)。
  // 必须用 `log::emit(Level, std::string)` 形式; 否则编译失败。
  auto to_log_level = [](LogLevel lvl) -> log::Level {
    switch (lvl) {
      case LogLevel::kDebug: return log::Level::DEBUG;
      case LogLevel::kInfo:  return log::Level::INFO;
      case LogLevel::kWarn:  return log::Level::WARN;
      case LogLevel::kError: return log::Level::ERROR;
    }
    return log::Level::INFO;
  };

  log::emit(to_log_level(level), std::string(message));
}

}  // namespace agenticdsl
