// include/agenticdsl/pdk/cancellation_registry.h
// CancellationRegistry — stop_token propagation registry (PDK lift)
// 设计依据: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §4 Change 1 (Task 6, D6.1)
// 作者: chat-session-pdk-lift Change 1
// 日期: 2026-09-11
//
// Lift 溯源: examples/pdk_chat_demo/cancellation_registry.{h,cpp}
//   (Phase B chat-async-io-cancellation-chain, 2026-08-09)。
// 原实现位于**全局 namespace**(无 namespace 包裹); 本 lift 移入 `hydraforge::pdk` (D6.1)。
// 兼容: examples/pdk_chat_demo/cancellation_registry.h 保留为转发 shim + 全局 using,
//   1 个 Sprint 兼容期 (loop_agent .so + 4 个 examples 测试仍按全局名引用)。

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <unordered_map>

namespace hydraforge::pdk {

class CancellationRegistry {
 public:
  // Register a new stop_source, return unique id (timestamp_ms + counter)
  std::string register_source(std::shared_ptr<std::stop_source> source);

  // Resolve id to stop_token. Returns empty token if not found.
  std::stop_token resolve_token(const std::string& id);

  // Resolve id to shared stop_source (for request_stop propagation).
  // Returns nullptr if not found.
  std::shared_ptr<std::stop_source> resolve_source(const std::string& id);

  // Remove id from registry (called by ChatSession destructor or after chat() returns).
  void unregister(const std::string& id);

 private:
  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<std::stop_source>> sources_;
  std::atomic<uint64_t> counter_{0};
};

}  // namespace hydraforge::pdk
