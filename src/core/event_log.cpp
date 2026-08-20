// src/core/event_log.cpp
// 功能描述：EventLogWriter 实现（ADR-0080 v1.1 D5 + D12 + D2）
// 设计依据：ADR-0080 v1.1 amendment
// 最后修改日期：2026-08-12

#include "core/event_log.h"

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/iinteraction_bus.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace agenticdsl {

namespace {

std::int64_t now_unix_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string next_event_id() {
  static std::atomic<std::uint64_t> counter{0};
  auto ts = now_unix_ms();
  return "evt-" + std::to_string(ts) + "-" + std::to_string(counter.fetch_add(1));
}

}  // namespace

EventLogWriter::EventLogWriter(EventLogConfig config,
                                 std::shared_ptr<IInteractionBus> bus)
    : config_(std::move(config)), bus_(std::move(bus)) {
  std::error_code ec;
  std::filesystem::create_directories(config_.event_log_dir, ec);
  log_path_ = config_.event_log_dir / (config_.event_log_agent_id + ".v1.jsonl");
  file_.open(log_path_, std::ios::out | std::ios::app);
  if (!file_.is_open()) {
    // 文件无法打开不影响 bus 回调（事件仍入队，flush 时丢弃）
  } else {
    // 记录当前文件大小
    file_.seekp(0, std::ios::end);
    current_file_size_ =
 static_cast<std::uint64_t>(file_.tellp());
    file_.seekp(0, std::ios::end);
  }

  flush_thread_ = std::thread(&EventLogWriter::flush_loop, this);
  if (bus_) {
    subscribe_to_bus();
  }
}

EventLogWriter::~EventLogWriter() { stop(); }

void EventLogWriter::stop() {
  if (!running_.exchange(false)) return;
  buffer_cv_.notify_all();
  if (flush_thread_.joinable()) {
    flush_thread_.join();
  }
  flush_sync();
  if (file_.is_open()) {
    file_.flush();
    file_.close();
  }
}

void EventLogWriter::flush_sync() {
  std::vector<BusEvent> snapshot;
  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    snapshot.reserve(buffer_.size());
    while (!buffer_.empty()) {
      snapshot.push_back(std::move(buffer_.front()));
      buffer_.pop();
    }
  }
  if (!file_.is_open() || snapshot.empty()) return;
  for (const auto& e : snapshot) {
    auto line = serialize(e);
    file_ << line << "\n";
    current_file_size_ += line.size() + 1;
    rotate_if_needed();
  }
  file_.flush();
}

void EventLogWriter::subscribe_to_bus() {
  if (!bus_) return;
  bus_->subscribe("*", [this](const BusEvent& event) {
    on_bus_event(event);
  });
}

void EventLogWriter::on_bus_event(const BusEvent& event) {
  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    buffer_.push(event);
  }
  buffer_cv_.notify_one();
}

void EventLogWriter::flush_loop() {
  while (running_.load()) {
    std::vector<BusEvent> snapshot;
    {
      std::unique_lock<std::mutex> lock(buffer_mutex_);
      buffer_cv_.wait_for(lock, config_.flush_interval,
                           [this] { return !buffer_.empty() || !running_.load(); });
      snapshot.reserve(buffer_.size());
      while (!buffer_.empty()) {
        snapshot.push_back(std::move(buffer_.front()));
        buffer_.pop();
      }
    }
    if (!file_.is_open() || snapshot.empty()) continue;
    for (const auto& e : snapshot) {
      auto line = serialize(e);
      file_ << line << "\n";
      current_file_size_ += line.size() + 1;
      rotate_if_needed();
    }
    file_.flush();
  }
}

std::string EventLogWriter::serialize(const BusEvent& event) const {
  nlohmann::json j;
  j["v"] = 1;
  j["event_id"] = next_event_id();
  j["ts_wall"] = now_unix_ms();
  j["causal_time"] = event.causal_time;
  j["topic"] = event.topic;
  j["agent_id"] = config_.event_log_agent_id;
  if (event.payload.trace_id.has_value() && !event.payload.trace_id->empty()) {
    j["session_id"] = *event.payload.trace_id;
  }
  j["payload"] = event.payload.data;
  if (event.payload.error_code.has_value()) {
    j["payload"]["_error_code"] =
 static_cast<int>(event.payload.error_code.value());
  }
  if (event.payload.latency_ms > 0) {
    j["payload"]["_latency_ms"] = event.payload.latency_ms;
  }
  return j.dump();
}

void EventLogWriter::rotate_if_needed() {
  if (current_file_size_ < config_.max_file_size) return;
  if (!file_.is_open()) return;
  file_.flush();
  file_.close();
  // 简化 rotation：仅 .rotation.1（保留最近 N 个）
  std::error_code ec;
  for (size_t i = config_.max_rotation_files; i > 0; --i) {
    auto src = (i == 1) ? log_path_
                           : std::filesystem::path(log_path_.string() +
                                                  ".rotation." + std::to_string(i - 1));
    auto dst = std::filesystem::path(log_path_.string() + ".rotation." +
                                      std::to_string(i));
    std::filesystem::rename(src, dst, ec);
  }
  file_.open(log_path_, std::ios::out | std::ios::trunc);
  current_file_size_ = 0;
}

std::vector<BusEvent> EventLogWriter::read(
    const std::string& agent_id,
    const std::filesystem::path& log_dir) {
  // 占位实现：实际读取由离线分析工具负责（不在本 ADR 范围）
  return {};
}

}  // namespace agenticdsl