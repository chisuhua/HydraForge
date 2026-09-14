// pdk/chat_session/src/cancellation_registry.cpp
// CancellationRegistry 实现 (PDK lift from examples/pdk_chat_demo/cancellation_registry.cpp)
// 日期: 2026-09-11

#include "agenticdsl/pdk/cancellation_registry.h"

#include <chrono>

namespace hydraforge::pdk {

std::string CancellationRegistry::register_source(
    std::shared_ptr<std::stop_source> source) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Generate unique id: timestamp_ms + counter
  auto now = std::chrono::steady_clock::now();
  auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch())
                          .count();

  std::string id =
      std::to_string(timestamp_ms) + "_" + std::to_string(counter_.fetch_add(1));

  sources_[id] = std::move(source);
  return id;
}

std::stop_token CancellationRegistry::resolve_token(const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = sources_.find(id);
  if (it == sources_.end()) {
    return std::stop_token{};
  }
  return it->second->get_token();
}

std::shared_ptr<std::stop_source> CancellationRegistry::resolve_source(
    const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = sources_.find(id);
  if (it == sources_.end()) {
    return nullptr;
  }
  return it->second;
}

void CancellationRegistry::unregister(const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  sources_.erase(id);
}

}  // namespace hydraforge::pdk
