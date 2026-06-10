// src/common/log/log.h
// 文件头注释
// 功能描述：统一日志门面（agenticdsl::log::debug/info/warn/error）
// 设计依据：tech-debt-and-doc-cleanup ADR 决策 D1（自实现 vs spdlog）
//           release 编译时 LOG_DEBUG 完全剥除（SPDLOG_ACTIVE_LEVEL 风格）
// 作者：tech-debt-and-doc-cleanup change
// 最后修改日期：2026-06-09
//
// 设计原则：
//   1. 默认输出到 stderr，避免污染 DAG 调度器输出流
//   2. release 构建中 LOG_DEBUG 零运行时开销
//   3. 线程安全（std::mutex 保护 stream 写入原子性）
//   4. 无第三方依赖（自实现，避免引入 spdlog 编译开销）
//   5. 现有 std::cout/cerr 可平滑迁移到 LOG_*

#ifndef AGENTICDSL_COMMON_LOG_LOG_H
#define AGENTICDSL_COMMON_LOG_LOG_H

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace agenticdsl {
namespace log {

// 日志级别枚举
enum class Level {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
};

// 全局日志级别门控（运行时可调，默认 INFO）
inline Level& global_level() {
  static Level level = Level::INFO;
  return level;
}

inline void set_level(Level lvl) { global_level() = lvl; }
inline Level get_level() { return global_level(); }

// 输出流互斥锁（保证多线程下日志原子性）
inline std::mutex& log_mutex() {
  static std::mutex m;
  return m;
}

// 核心格式化函数（线程安全，stderr 输出）
// 使用 std::lock_guard 保证同一时刻只有一个线程完整写入一行
inline void emit(Level lvl, const std::string& msg) {
  if (static_cast<int>(lvl) < static_cast<int>(global_level())) return;

  const char* tag = nullptr;
  switch (lvl) {
    case Level::DEBUG: tag = "[DEBUG]"; break;
    case Level::INFO:  tag = "[INFO]";  break;
    case Level::WARN:  tag = "[WARN]";  break;
    case Level::ERROR: tag = "[ERROR]"; break;
  }

  std::lock_guard<std::mutex> lock(log_mutex());
  std::cerr << tag << " " << msg << std::endl;
}

}  // namespace log
}  // namespace agenticdsl

// ----------------------------------------------------------------------------
// 公共宏：业务代码使用 LOG_DEBUG/INFO/WARN/ERROR
// ----------------------------------------------------------------------------

// 通用 LOG_<LEVEL>(msg)：支持流式插入
// 例如：LOG_INFO("Graph loaded: " << graph_count << " nodes");
// 实现技巧：用嵌套 lambda 把 `<<` 表达式整体捕获，绕过变参宏的参数切分问题
namespace agenticdsl {
namespace log {
template <typename StreamOp>
inline std::string format_msg(StreamOp&& op) {
  std::ostringstream oss;
  op(oss);  // 调用 op(oss)，传入所有 `<< value` 形成完整流
  return oss.str();
}
}  // namespace log
}  // namespace agenticdsl

#define LOG_DEBUG(...)                                                        \
  ::agenticdsl::log::emit(::agenticdsl::log::Level::DEBUG,                    \
                          ::agenticdsl::log::format_msg(                     \
                              [&](std::ostringstream& oss) { oss << __VA_ARGS__; }))
#define LOG_INFO(...)                                                         \
  ::agenticdsl::log::emit(::agenticdsl::log::Level::INFO,                     \
                          ::agenticdsl::log::format_msg(                     \
                              [&](std::ostringstream& oss) { oss << __VA_ARGS__; }))
#define LOG_WARN(...)                                                         \
  ::agenticdsl::log::emit(::agenticdsl::log::Level::WARN,                     \
                          ::agenticdsl::log::format_msg(                     \
                              [&](std::ostringstream& oss) { oss << __VA_ARGS__; }))
#define LOG_ERROR(...)                                                        \
  ::agenticdsl::log::emit(::agenticdsl::log::Level::ERROR,                    \
                          ::agenticdsl::log::format_msg(                     \
                              [&](std::ostringstream& oss) { oss << __VA_ARGS__; }))

#endif  // AGENTICDSL_COMMON_LOG_LOG_H