// src/common/io/stdin_input_source.h
// StdinInputSource - 保留 self-pipe + poll(2) 多 fd 架构 (Sprint 31 死锁修复)
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §6.1 (A3)
// 作者: chat-session-pdk-lift Change 1
// 日期: 2026-09-11
//
// A3 强制约束: 必须保留 Sprint 31 死锁修复的
//   poll([STDIN_FILENO, pipe_read_fd_], timeout) 多 fd 架构。
// 禁止回退为裸 std::getline, 否则 TTY 环境下 ctest 必然 hang
// (AGENTS.md 模式 #5 + §模式 #6 Sprint 31 case study)。

#pragma once

#include "agenticdsl/contract/iinput_source.h"

#include <atomic>

namespace agenticdsl {

class StdinInputSource : public IInputSource {
 public:
  StdinInputSource();
  ~StdinInputSource() override;

  std::optional<std::string> read_line(
      std::chrono::milliseconds timeout) override;
  bool has_input() const override;
  bool at_eof() const override;
  void wake() override;
  void close() override;

 private:
  int pipe_read_fd_ = -1;
  int pipe_write_fd_ = -1;
  std::atomic<bool> shutdown_requested_{false};
  std::atomic<bool> at_eof_{false};
};

}  // namespace agenticdsl
