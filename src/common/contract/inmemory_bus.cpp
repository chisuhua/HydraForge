// src/common/contract/inmemory_bus.cpp
// Change A (2026-07-26): BusEvent queue migration — emit/subscribe/dispatch all use BusEvent.
#include "agenticdsl/contract/inmemory_bus.h"

#include <algorithm>
#include <utility>

namespace agenticdsl {

InMemoryBus::InMemoryBus()
    : dispatch_thread_(&InMemoryBus::dispatch_loop, this) {}

InMemoryBus::~InMemoryBus() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    stop_.store(true, std::memory_order_release);
    cv_.notify_all();
  }
  if (dispatch_thread_.joinable()) {
    dispatch_thread_.join();
  }
}

void InMemoryBus::emit(const BusEvent& event) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push(event);
  }
  cv_.notify_all();
}

void InMemoryBus::emit(const std::string& event_type,
                        const std::string& content) {
  ToolResult tr = ToolResult::success(
      nlohmann::json::object(),
      nlohmann::json{{"content", content}});
  emit(BusEvent{event_type, tr, std::chrono::steady_clock::now()});
}

size_t InMemoryBus::subscribe(const std::string& event_type,
                               std::function<void(const BusEvent&)> callback) {
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

bool InMemoryBus::try_pop(BusEvent& event) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (queue_.empty()) return false;
  event = std::move(queue_.front());
  queue_.pop();
  return true;
}

void InMemoryBus::wait_for_drain() {
  std::unique_lock<std::mutex> lock(mtx_);
  cv_.wait(lock, [this] {
    return queue_.empty() && in_flight_callbacks_ == 0;
  });
}

void InMemoryBus::dispatch_loop() {
  while (true) {
    BusEvent event;
    bool got_event = false;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait(lock, [this] {
        return stop_.load(std::memory_order_acquire) || !queue_.empty();
      });
      if (stop_.load(std::memory_order_acquire) && queue_.empty() && in_flight_callbacks_ == 0) break;
      if (!queue_.empty()) {
        event = std::move(queue_.front());
        queue_.pop();
        in_flight_callbacks_++;
        got_event = true;
      }
    }
    if (!got_event) continue;

    std::vector<std::function<void(const BusEvent&)>> callbacks;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      auto it = subscribers_.find(event.topic);
      if (it != subscribers_.end()) {
        callbacks.reserve(it->second.size());
        for (const auto& [token, cb] : it->second) {
          (void)token;
          callbacks.push_back(cb);
        }
      }
    }
    for (auto& cb : callbacks) {
      cb(event);
    }

    {
      std::lock_guard<std::mutex> lock(mtx_);
      in_flight_callbacks_--;
      cv_.notify_all();
    }
  }
}

}  // namespace agenticdsl