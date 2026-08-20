// src/core/event_log.h
// 功能描述：AppendOnlyEventLog 写入器（ADR-0080 v1.1 D5 + D12 + D2）
//          订阅 IInteractionBus 全量事件，append 到 per-agent JSONL。
//          排序键：(causal_time, event_id)。；持久化字段：ts_wall + causal_time。
//          默认 OFF；DSLEngine::enable_event_log() 显式 opt-in。
// 设计依据：ADR-0080 v1.1 amendment
// 最后修改日期：2026-08-12

#pragma once

#include "agenticdsl/contract/bus_event.h"
#include "core/types/event_log_config.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace agenticdsl {

class IInteractionBus;

class EventLogWriter {
 public:
  EventLogWriter(EventLogConfig config,
                 std::shared_ptr<IInteractionBus> bus);
  ~EventLogWriter();

  EventLogWriter(const EventLogWriter&) = delete;
  EventLogWriter& operator=(const EventLogWriter&) = delete;

  // 同步刷写并停止 flush 线程（析构时自动调用）
  void stop();

  // 强制同步落盘（用于关键节点，如 session 结束）
  void flush_sync();

  // 读取事件（离线分析）
  static std::vector<BusEvent> read(
      const std::string& agent_id,
      const std::filesystem::path& log_dir);

  // Query filter (ADR-0080 P4 event-log-query-api)
  struct QueryFilter {
    std::string topic_glob;
    uint64_t    start_causal_time = 0;
    uint64_t    end_causal_time   = UINT64_MAX;
    bool        has_time_window   = false;
  };

  std::vector<BusEvent> read(
      const std::string& agent_id,
      uint64_t start_causal_time,
      uint64_t end_causal_time) const;

  std::vector<BusEvent> query(
      const std::string& agent_id,
      const QueryFilter& filter,
      size_t max_count) const;

 private:
  // 在 bus 上注册 subscribe（start）
  void subscribe_to_bus();

  // 处理一个事件（emit 路径，bus 回调）
  void on_bus_event(const BusEvent& event);

  // flush 线程入口
  void flush_loop();

  // 单条事件序列化为 JSONL 一行
  std::string serialize(const BusEvent& event) const;

  // 文件 rotation
  void rotate_if_needed();

  EventLogConfig config_;
  std::shared_ptr<IInteractionBus> bus_;

  std::filesystem::path log_path_;

  std::ofstream file_;
  std::mutex buffer_mutex_;
  std::queue<BusEvent> buffer_;
  std::condition_variable buffer_cv_;
  std::atomic<bool> running_{true};
  std::thread flush_thread_;

  std::atomic<std::uint64_t> current_file_size_{0};
};

}  // namespace agenticdsl