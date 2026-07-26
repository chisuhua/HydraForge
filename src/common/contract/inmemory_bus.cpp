// src/common/contract/inmemory_bus.cpp
// Change A (2026-07-26): BusEvent queue migration.
// Change B (2026-07-26): glob pattern subscribe — dual-path dispatch.
#include "agenticdsl/contract/inmemory_bus.h"

#include <algorithm>
#include <utility>

namespace agenticdsl {

namespace {

// glob_match("inference.*", "inference.lifecycle.idle") → true
// Supports * (match any chars) and ? (match single char).
// No regex — sufficient for PDK event naming conventions (<50 wildcard subscribers).
bool glob_match(const std::string& pattern, const std::string& topic) {
  size_t pi = 0, ti = 0;
  size_t star_p = std::string::npos, star_t = 0;

  while (ti < topic.size()) {
    if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == topic[ti])) {
      ++pi; ++ti;
    } else if (pi < pattern.size() && pattern[pi] == '*') {
      star_p = pi++;
      star_t = ti;
    } else if (star_p != std::string::npos) {
      pi = star_p + 1;
      ti = ++star_t;
    } else {
      return false;
    }
  }

  while (pi < pattern.size() && pattern[pi] == '*') ++pi;
  return pi == pattern.size();
}

bool has_wildcard(const std::string& s) {
  return s.find('*') != std::string::npos || s.find('?') != std::string::npos;
}

}  // anonymous namespace

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
    auto e = event;
    e.causal_time = causal_clock_.tick();
    queue_.push(std::move(e));
  }
  cv_.notify_all();
}

void InMemoryBus::emit(const std::string& event_type,
                        const std::string& content) {
  ToolResult tr = ToolResult::success(
      nlohmann::json::object(),
      nlohmann::json{{"content", content}});
  BusEvent e{event_type, tr, std::chrono::steady_clock::now()};
  e.causal_time = causal_clock_.tick();
  emit(std::move(e));
}

size_t InMemoryBus::subscribe(const std::string& event_type,
                               std::function<void(const BusEvent&)> callback) {
  std::lock_guard<std::mutex> lock(mtx_);
  size_t token = next_token_++;

  if (has_wildcard(event_type)) {
    wildcard_subscribers_[event_type].push_back({token, std::move(callback)});
  } else {
    exact_subscribers_[event_type].push_back({token, std::move(callback)});
  }
  return token;
}

void InMemoryBus::unsubscribe(size_t token) {
  std::lock_guard<std::mutex> lock(mtx_);
  for (auto* map : {&exact_subscribers_, &wildcard_subscribers_}) {
    for (auto& [event_type, vec] : *map) {
      (void)event_type;
      auto it = std::remove_if(vec.begin(), vec.end(),
                                [token](const auto& pair) {
                                  return pair.first == token;
                                });
      if (it != vec.end()) {
        vec.erase(it, vec.end());
        return;
      }
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

      // Fast path: exact match (O(1))
      auto it = exact_subscribers_.find(event.topic);
      if (it != exact_subscribers_.end()) {
        callbacks.reserve(it->second.size());
        for (const auto& [token, cb] : it->second) {
          (void)token;
          callbacks.push_back(cb);
        }
      }

      // Slow path: glob match (O(w), w = wildcard subscriber count < 50)
      for (const auto& [pattern, cbs] : wildcard_subscribers_) {
        if (glob_match(pattern, event.topic)) {
          callbacks.reserve(callbacks.size() + cbs.size());
          for (const auto& [token, cb] : cbs) {
            (void)token;
            callbacks.push_back(cb);
          }
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