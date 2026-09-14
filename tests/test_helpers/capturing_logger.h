// tests/test_helpers/capturing_logger.h
// 测试用 ILogger - 捕获所有 log 调用供断言 (Pattern #5)
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §7.2 (A2)
// 作者: chat-session-pdk-lift Change 1
// 日期: 2026-09-11
//
// A2 修订: 测试 double 放 tests/test_helpers/, 不放 src/common/io/。
// 2026-09-14 实施偏差: header-only (inline), 理由同 in_memory_input_source.h。

#pragma once

#include "agenticdsl/contract/ilogger.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace agenticdsl::test {

struct CapturedLog {
  LogLevel level;
  std::string message;
};

class CapturingLogger : public ILogger {
 public:
  void log(LogLevel level, std::string_view message) override {
    std::lock_guard<std::mutex> lock(mtx_);
    logs_.push_back(CapturedLog{level, std::string(message)});
  }

  std::vector<CapturedLog> snapshot() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return logs_;
  }

  size_t count(LogLevel level) const {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t n = 0;
    for (const auto& l : logs_) {
      if (l.level == level) ++n;
    }
    return n;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    logs_.clear();
  }

 private:
  mutable std::mutex mtx_;
  std::vector<CapturedLog> logs_;
};

}  // namespace agenticdsl::test
