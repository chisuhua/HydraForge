// tests/test_helpers/in_memory_input_source.h
// 测试用 IInputSource - 预填输入 + push_for_test() 注入 (Pattern #5)
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §7.2 (A2)
// 作者: chat-session-pdk-lift Change 1
// 日期: 2026-09-11
//
// A2 修订: 测试 double 放 tests/test_helpers/ (与 http_mock_server.h 惯例一致),
// **不**放 src/common/io/ (生产库不应携带测试替身)。
//
// 2026-09-14 实施偏差: 声明与实现同为 header-only (inline)。
// 原因: tests/CMakeLists.txt 的 add_catch_test 是 per-file 独立 target
// (file(GLOB test_*.cpp) → 每文件一个可执行), 无共享测试 helper 静态库;
// 若把实现放 .cpp, 每个测试 target 都需显式 target_sources, 维护成本高。
// http_mock_server.h 用 .h 声明 + 测试内联实现同理。

#pragma once

#include "agenticdsl/contract/iinput_source.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <utility>

namespace agenticdsl::test {

class InMemoryInputSource : public IInputSource {
 public:
  // 预填输入(测试启动时设置)
  void enqueue_input(std::string line) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      queue_.push(std::move(line));
    }
    cv_.notify_all();
  }

  // 测试中动态注入(替代原 try_push_*_for_test 后门)
  void push_for_test(std::string line) { enqueue_input(std::move(line)); }

  std::optional<std::string> read_line(
      std::chrono::milliseconds timeout) override {
    std::unique_lock<std::mutex> lock(mtx_);
    const bool ready = cv_.wait_for(lock, timeout, [this] {
      return !queue_.empty() || closed_;
    });
    if (!ready) return std::nullopt;   // timeout
    if (closed_) {
      at_eof_.store(true, std::memory_order_release);
      return std::nullopt;             // closed (EOF 语义)
    }
    if (queue_.empty()) return std::nullopt;
    auto line = std::move(queue_.front());
    queue_.pop();
    return line;
  }

  bool has_input() const override {
    std::lock_guard<std::mutex> lock(mtx_);
    return !closed_ && !queue_.empty();
  }

  bool at_eof() const override { return at_eof_.load(std::memory_order_acquire); }

  void close() override {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      closed_ = true;
    }
    cv_.notify_all();
  }

 private:
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::queue<std::string> queue_;
  bool closed_ = false;
  std::atomic<bool> at_eof_{false};
};

}  // namespace agenticdsl::test
