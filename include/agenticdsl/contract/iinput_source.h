// include/agenticdsl/contract/iinput_source.h
// IInputSource 接口 - 解耦 stdin/网络/TUI 输入源
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §6.1
// 作者: chat-session-pdk-lift Change 1
// 日期: 2026-09-11
//
// §6.1 实施偏差 (2026-09-14, 3 方法 → 4 方法):
//   原设计 3 个虚方法 (read_line/has_input/close) 无法区分
//   "timeout 到期返回 nullopt" 与 "EOF 返回 nullopt"。
//   test_pdk_chat_demo_stdin_e2e 的 "EOF 优雅退出" 场景依赖
//   EOF → stop_input_thread_ → pop_next_input 返回 nullopt → main loop 退出;
//   若 EOF 与 timeout 不可区分, main loop 将永远 timeout 轮询, E2E hang。
//   故新增第 4 个虚方法 at_eof()。

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace agenticdsl {

class IInputSource {
 public:
  virtual ~IInputSource() = default;

  // 阻塞读一行, timeout 后返回 nullopt; EOF 返回 nullopt (用 at_eof() 区分)
  virtual std::optional<std::string> read_line(
      std::chrono::milliseconds timeout) = 0;

  // 非阻塞检查是否有可用输入
  virtual bool has_input() const = 0;

  // EOF 后返回 true (与 timeout 的 nullopt 区分, 见文件头偏差说明)
  virtual bool at_eof() const = 0;

  // 关闭输入源(用于优雅退出 / signal handler); 关闭后 read_line 返回 nullopt
  virtual void close() = 0;
};

}  // namespace agenticdsl
