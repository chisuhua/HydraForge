// src/common/contract/inmemory_bus.cpp
// C2 Day 6-8 (2026-06-27, Sprint 12 P2, ADR-0030 V2):
//   改为 EventBus MPMC 有界队列后端: emit() 仅入队 + notify_one,
//   后台 dispatch 线程从队列取事件, 同步通知 subscribers.
//   解决 ADR-0030 V2 §风险 "bridge 背压" (慢 subscriber 不阻塞 emit)。
#include "agenticdsl/contract/inmemory_bus.h"

#include <algorithm>
#include <utility>

namespace agenticdsl {

InMemoryBus::InMemoryBus()
    : dispatch_thread_(&InMemoryBus::dispatch_loop, this) {}

InMemoryBus::~InMemoryBus() {
  stop_.store(true, std::memory_order_release);
  cv_.notify_all();
  if (dispatch_thread_.joinable()) {
    dispatch_thread_.join();
  }
}

void InMemoryBus::emit(const std::string& event_type,
                       const ToolResult& payload) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push({event_type, payload});
  }
  cv_.notify_one();
}

void InMemoryBus::emit(const std::string& event_type,
                       const std::string& content) {
  ToolResult payload = ToolResult::success(
      nlohmann::json::object(),
      nlohmann::json{{"content", content}});
  emit(event_type, payload);
}

size_t InMemoryBus::subscribe(const std::string& event_type,
                              std::function<void(const ToolResult&)> callback) {
  std::lock_guard<std::mutex> lock(mtx_);
  size_t token = next_token_++;
  subscribers_[event_type].push_back({token, std::move(callback)});
  return token;
}

void InMemoryBus::unsubscribe(size_t token) {
  std::lock_guard<std::mutex> lock(mtx_);
  for (auto& [event_type, vec] : subscribers_) {
    (void)event_type;
    auto it = std::remove_if(vec.begin(), vec.end(),
                              [token](const auto& pair) {
                                return pair.first == token;
                              });
    if (it != vec.end()) {
      vec.erase(it, vec.end());
      break;
    }
  }
}

bool InMemoryBus::try_pop(std::string& event_type, ToolResult& payload) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (queue_.empty()) return false;
  auto front = queue_.front();
  queue_.pop();
  event_type = std::move(front.first);
  payload = std::move(front.second);
  return true;
}

void InMemoryBus::wait_for_drain() {
  std::unique_lock<std::mutex> lock(mtx_);
  cv_.wait(lock, [this] { return queue_.empty(); });
}

void InMemoryBus::dispatch_loop() {
  while (!stop_.load(std::memory_order_acquire)) {
    std::pair<std::string, ToolResult> event;
    bool got_event = false;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait(lock, [this] {
        return stop_.load(std::memory_order_acquire) || !queue_.empty();
      });
      if (stop_.load(std::memory_order_acquire) && queue_.empty()) break;
      if (!queue_.empty()) {
        event = std::move(queue_.front());
        queue_.pop();
        got_event = true;
      }
    }
    if (!got_event) continue;

    std::vector<std::function<void(const ToolResult&)>> callbacks;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      auto it = subscribers_.find(event.first);
      if (it != subscribers_.end()) {
        callbacks.reserve(it->second.size());
        for (const auto& [token, cb] : it->second) {
          (void)token;
          callbacks.push_back(cb);
        }
      }
    }
    for (auto& cb : callbacks) {
      cb(event.second);
    }
  }
}

}  // namespace agenticdsl