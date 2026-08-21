// src/core/session_writer.h
// 功能描述：SessionWriter（ADR-0079 v1.1 D5 + D6 + SessionWriter 实施）
//          订阅 IInteractionBus 事件，过滤 D6 白名单 13 topic，写入 per-session JSONL。
//          与 EventLogWriter 平行订阅，互不干扰（独立互斥锁）。
//          默认 OFF；DSLEngine::enable_session_writer() 显式 opt-in。
// 设计依据：ADR-0079 v1.1 + SessionWriter 实施 (P5 session-writer-bridge)
// 最后修改日期：2026-08-20

#pragma once

#include "agenticdsl/contract/bus_event.h"
#include "core/types/session_writer_config.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace agenticdsl {

class IInteractionBus;

class SessionWriter {
 public:
  SessionWriter(SessionWriterConfig config,
                std::shared_ptr<IInteractionBus> bus);
  ~SessionWriter();

  SessionWriter(const SessionWriter&) = delete;
  SessionWriter& operator=(const SessionWriter&) = delete;

  // 同步刷写并停止 flush 线程（析构时自动调用）
  void stop();

  // 强制同步落盘（关键节点，如 session 结束）
  void flush_sync();

  // 显式追加一条记录（manual mode，绕过 bus filter）
  void append(const std::string& type,
              const std::string& role,
              const nlohmann::json& payload);

  // 读取会话记录（离线分析 / crash recovery）
  static std::vector<nlohmann::json> read(
      const std::string& session_id,
      const std::filesystem::path& writer_dir);

  // D6 白名单查询（用于测试 + 文档生成）
  static const std::unordered_set<std::string>& whitelisted_topics();

  // D6 topic → JSONL type 映射（用于序列化）
  static std::string topic_to_type(const std::string& topic);

 private:
  // 在 bus 上注册 subscribe
  void subscribe_to_bus();

  // 处理一个事件（emit 路径，bus 回调 + D6 filter）
  void on_bus_event(const BusEvent& event);

  // flush 线程入口
  void flush_loop();

  // 单条记录序列化为 JSONL 一行
  std::string serialize_record(const std::string& type,
                                const std::string& role,
                                const nlohmann::json& payload,
                                std::int64_t ts_ms) const;

  // 文件 rotation
  void rotate_if_needed();

  SessionWriterConfig config_;
  std::shared_ptr<IInteractionBus> bus_;

  std::filesystem::path writer_path_;

  std::ofstream file_;
  std::mutex buffer_mutex_;
  struct PendingRecord {
    std::string type;
    std::string role;
    nlohmann::json payload;
  };
  std::queue<PendingRecord> buffer_;
  std::condition_variable buffer_cv_;
  std::atomic<bool> running_{true};
  std::thread flush_thread_;

  std::atomic<std::uint64_t> current_file_size_{0};
};

}  // namespace agenticdsl