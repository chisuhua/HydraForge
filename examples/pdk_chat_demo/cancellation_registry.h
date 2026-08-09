// cancellation_registry.h - Cancellation Registry for stop_token propagation
// Phase B: chat-async-io-cancellation-chain
// Associates cancellation_id (string) with shared stop_source for cross-thread cancellation

#pragma once

#include <memory>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>

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
