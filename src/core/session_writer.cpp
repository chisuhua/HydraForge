// src/core/session_writer.cpp
// 功能描述：SessionWriter 实现（ADR-0079 v1.1 D5 + D6 + SessionWriter 实施）
//          订阅 bus 事件 → D6 topic 过滤 → JSONL append。
//          与 EventLogWriter 独立，互不阻塞（独立 file_ + buffer_mutex_）。
// 设计依据：ADR-0079 v1.1 + SessionWriter 实施 (P5 session-writer-bridge)
// 最后修改日期：2026-08-20

#include "core/session_writer.h"

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>

namespace agenticdsl {

namespace {

std::int64_t now_unix_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

const std::unordered_set<std::string>& SessionWriter::whitelisted_topics() {
  // ADR-0079 v1.1 D6 表 — 13 个 bus topic 映射到 JSONL type
  static const std::unordered_set<std::string> topics = {
      "conversation.user_message",       // → conversation (role=user)
      "conversation.assistant_message", // → conversation (role=assistant)
      "attempt.started",                // → attempt
      "attempt.ended",                  // → attempt (带 result)
      "phase.completed",                // → attempt (phase 字段更新)
      "branch.created",                 // → attempt (带 branch_id)
      "llm.request",                    // → step
      "llm.response",                   // → step
      "tool.execution.start",           // → step (tool_results 数组)
      "tool.execution.end",             // → step (tool_results 数组)
      "dsl.call.started",               // → execution
      "dsl.call.completed",             // → execution
      "attempt.converged"               // → convergence
  };
  return topics;
}

std::string SessionWriter::topic_to_type(const std::string& topic) {
  if (topic == "conversation.user_message" ||
      topic == "conversation.assistant_message") {
    return "conversation";
  }
  if (topic == "attempt.started" || topic == "attempt.ended" ||
      topic == "phase.completed" || topic == "branch.created") {
    return "attempt";
  }
  if (topic == "llm.request" || topic == "llm.response" ||
      topic == "tool.execution.start" || topic == "tool.execution.end") {
    return "step";
  }
  if (topic == "dsl.call.started" || topic == "dsl.call.completed") {
    return "execution";
  }
  if (topic == "attempt.converged") {
    return "convergence";
  }
  return "";
}

SessionWriter::SessionWriter(SessionWriterConfig config,
                              std::shared_ptr<IInteractionBus> bus)
    : config_(std::move(config)), bus_(std::move(bus)) {
  std::error_code ec;
  std::filesystem::create_directories(config_.writer_dir, ec);
  writer_path_ = config_.writer_dir / (config_.session_id + ".v1.jsonl");
  file_.open(writer_path_, std::ios::out | std::ios::app);
  if (file_.is_open()) {
    file_.seekp(0, std::ios::end);
    current_file_size_ = static_cast<std::uint64_t>(file_.tellp());
  }
  flush_thread_ = std::thread(&SessionWriter::flush_loop, this);
  if (bus_) {
    subscribe_to_bus();
  }
}

SessionWriter::~SessionWriter() { stop(); }

void SessionWriter::stop() {
  if (!running_.exchange(false)) return;
  buffer_cv_.notify_all();
  if (flush_thread_.joinable()) flush_thread_.join();
  if (file_.is_open()) {
    file_.flush();
    file_.close();
  }
}

void SessionWriter::flush_sync() {
  std::vector<PendingRecord> snapshot;
  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    snapshot.reserve(buffer_.size());
    while (!buffer_.empty()) {
      snapshot.push_back(std::move(buffer_.front()));
      buffer_.pop();
    }
  }
  if (!file_.is_open() || snapshot.empty()) return;
  for (const auto& r : snapshot) {
    auto line = serialize_record(r.type, r.role, r.payload, now_unix_ms());
    file_ << line << "\n";
    current_file_size_ += line.size() + 1;
    rotate_if_needed();
  }
  file_.flush();
}

void SessionWriter::append(const std::string& type,
                            const std::string& role,
                            const nlohmann::json& payload) {
  std::lock_guard<std::mutex> lock(buffer_mutex_);
  buffer_.push(PendingRecord{type, role, payload});
  buffer_cv_.notify_one();
}

void SessionWriter::subscribe_to_bus() {
  if (!bus_) return;
  bus_->subscribe("*", [this](const BusEvent& event) {
    on_bus_event(event);
  });
}

void SessionWriter::on_bus_event(const BusEvent& event) {
  // D6 白名单过滤：非清单内的事件直接丢弃
  if (whitelisted_topics().find(event.topic) == whitelisted_topics().end()) {
    return;
  }
  std::string type = topic_to_type(event.topic);
  if (type.empty()) return;

  // role 提取：仅 conversation.* 有 role 字段
  std::string role;
  if (type == "conversation") {
    role = (event.topic == "conversation.user_message") ? "user" : "assistant";
  }

  // payload：直接从 ToolResult::data 复制
  append(type, role, event.payload.data);
}

void SessionWriter::flush_loop() {
  while (running_.load()) {
    std::vector<PendingRecord> snapshot;
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
    for (const auto& r : snapshot) {
      auto line = serialize_record(r.type, r.role, r.payload, now_unix_ms());
      file_ << line << "\n";
      current_file_size_ += line.size() + 1;
      rotate_if_needed();
    }
    file_.flush();
  }
}

std::string SessionWriter::serialize_record(
    const std::string& type,
    const std::string& role,
    const nlohmann::json& payload,
    std::int64_t ts_ms) const {
  nlohmann::json j;
  j["v"] = 1;
  j["type"] = type;
  j["ts"] = ts_ms;
  if (!role.empty()) j["role"] = role;
  j["payload"] = payload;
  return j.dump();
}

void SessionWriter::rotate_if_needed() {
  if (current_file_size_.load() < config_.max_file_size) return;
  // 简化 rotation：关闭当前文件，rename 为 .1，后续 .2/.3
  // 实现不依赖 logging 库，3 个文件循环覆盖
  if (!file_.is_open()) return;
  file_.close();
  std::error_code ec;
  std::filesystem::path base = writer_path_;
  // 删除最老的
  std::filesystem::remove(base.string() + "." +
                              std::to_string(config_.max_rotation_files),
                              ec);
  // 逐个 rename
  for (size_t i = config_.max_rotation_files; i > 1; --i) {
    std::filesystem::path src = base.string() + "." + std::to_string(i - 1);
    std::filesystem::path dst = base.string() + "." + std::to_string(i);
    std::filesystem::rename(src, dst, ec);
  }
  std::filesystem::rename(base, base.string() + ".1", ec);
  // 重新打开新文件
  file_.open(writer_path_, std::ios::out | std::ios::trunc);
  current_file_size_ = 0;
}

std::vector<nlohmann::json> SessionWriter::read(
    const std::string& session_id,
    const std::filesystem::path& writer_dir) {
  std::vector<nlohmann::json> records;
  std::filesystem::path path = writer_dir / (session_id + ".v1.jsonl");
  std::ifstream f(path);
  if (!f.is_open()) return records;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    try {
      records.push_back(nlohmann::json::parse(line));
    } catch (...) {}
  }
  return records;
}

}  // namespace agenticdsl