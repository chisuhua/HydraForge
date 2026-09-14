// src/common/io/stdin_input_source.cpp
// StdinInputSource 实现 - self-pipe + poll(2) (Sprint 31 架构迁移)
// 日期: 2026-09-11
//
// 迁移来源: examples/pdk_chat_demo/chat_session.cpp Impl::input_thread_main()
// (Sprint 31 chat-session-read-timeout ship, commit 864fe71)。
// 迁移原则: 语义等价 — poll 多 fd + 100ms clamp + EINTR 重试 + self-pipe wake-up byte,
// 仅把 "读一行" 从 ChatSession::Impl 内部逻辑上移到本类。

#include "stdin_input_source.h"

#include <cerrno>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace agenticdsl {

namespace {

constexpr int kDefaultPollTimeoutMs = 100;

}  // namespace

StdinInputSource::StdinInputSource() {
  // pipe2(O_CLOEXEC | O_NONBLOCK): 不被 exec 子进程继承 + write 不阻塞
  // 失败时 fd 保持 -1 (防御性 default, read_line 检查 fd >= 0 才加入 pollfd 数组)
  int fds[2] = {-1, -1};
  if (::pipe2(fds, O_CLOEXEC | O_NONBLOCK) == 0) {
    pipe_read_fd_ = fds[0];
    pipe_write_fd_ = fds[1];
  }
}

StdinInputSource::~StdinInputSource() {
  // 析构顺序 (Sprint 31 D4 语义): 先 write 端后 read 端, 设 -1 防 double-close
  if (pipe_write_fd_ >= 0) {
    ::close(pipe_write_fd_);
    pipe_write_fd_ = -1;
  }
  if (pipe_read_fd_ >= 0) {
    ::close(pipe_read_fd_);
    pipe_read_fd_ = -1;
  }
}

void StdinInputSource::close() {
  shutdown_requested_.store(true, std::memory_order_release);
  // self-pipe trick: 写 1 byte wake-up byte 让阻塞的 poll 立即返回
  // (EAGAIN 容忍: 1 byte/周期 vs 64KB pipe buffer, 不会满)
  if (pipe_write_fd_ >= 0) {
    char byte = 'x';
    ssize_t r = ::write(pipe_write_fd_, &byte, 1);
    (void)r;  // EAGAIN acceptable
  }
}

bool StdinInputSource::has_input() const {
  if (shutdown_requested_.load(std::memory_order_acquire)) return false;
  if (at_eof_.load(std::memory_order_acquire)) return false;
  struct pollfd fds[1];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;
  fds[0].revents = 0;
  return ::poll(fds, 1, 0) > 0;
}

bool StdinInputSource::at_eof() const {
  return at_eof_.load(std::memory_order_acquire);
}

std::optional<std::string> StdinInputSource::read_line(
    std::chrono::milliseconds timeout) {
  if (shutdown_requested_.load(std::memory_order_acquire)) {
    return std::nullopt;
  }

  // **核心 (A3)**: poll 多 fd 监听 [STDIN_FILENO, pipe_read_fd_]
  struct pollfd fds[2];
  int nfds = 0;
  fds[nfds].fd = STDIN_FILENO;
  fds[nfds].events = POLLIN;
  fds[nfds].revents = 0;
  ++nfds;
  if (pipe_read_fd_ >= 0) {
    fds[nfds].fd = pipe_read_fd_;
    fds[nfds].events = POLLIN;
    fds[nfds].revents = 0;
    ++nfds;
  }

  int timeout_ms = static_cast<int>(timeout.count());
  if (timeout_ms <= 0) timeout_ms = kDefaultPollTimeoutMs;  // clamp, 防 busy loop

  // EINTR 重试 (Sprint 30 / SkillInterpreter §6.3 §3.3 模式)
  int n;
  do {
    n = ::poll(fds, nfds, timeout_ms);
  } while (n < 0 && errno == EINTR);

  if (n <= 0) return std::nullopt;  // timeout 或 poll error

  // 处理 wake-up byte (self-pipe): 读 1 段清空, 不做业务逻辑
  if (pipe_read_fd_ >= 0 && (fds[1].revents & POLLIN)) {
    char drain[16];
    ssize_t dr;
    do {
      dr = ::read(pipe_read_fd_, drain, sizeof(drain));
    } while (dr < 0 && errno == EINTR);
    // 字节累积无业务副作用, 仅清空 buffer
    if (shutdown_requested_.load(std::memory_order_acquire)) {
      return std::nullopt;
    }
  }

  // 仅 wake-up byte 就绪 (stdin 无输入) → 本次不读 stdin
  if (!(fds[0].revents & (POLLIN | POLLHUP))) {
    return std::nullopt;
  }

  // stdin 就绪 → 读一行 (Sprint 30 getline 语义, 保留)
  std::string line;
  if (!std::getline(std::cin, line)) {
    at_eof_.store(true, std::memory_order_release);
    return std::nullopt;  // EOF
  }
  return line;
}

}  // namespace agenticdsl
