// pdk/temporal_agent/src/workflow_callback_channel.h
// 功能描述：Workflow -> Agent Signal 双向通信通道
//          后台线程 long-poll 后端 consume_signals(), 收到信号后调用注册的 handler。
//          handler 异常被隔离 (不终止 poll 线程)。
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.2
//          .rddf/plans/pkgm-temporal-agent.md Task 2
// 线程安全：handlers_ 受 mutex 保护; stop() 确保 poll 线程 join
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "temporal_client.h"

namespace pdk_temporal_agent {

class WorkflowCallbackChannel {
 public:
  using SignalHandler = std::function<void(const nlohmann::json&)>;

  explicit WorkflowCallbackChannel(std::string workflow_id);
  ~WorkflowCallbackChannel();

  WorkflowCallbackChannel(const WorkflowCallbackChannel&) = delete;
  WorkflowCallbackChannel& operator=(const WorkflowCallbackChannel&) = delete;

  // 注册信号处理器 (可在 start_polling 前或运行中调用)
  void on_signal(const std::string& signal_name, SignalHandler handler);

  // 启动后台 long-poll 线程
  void start_polling(std::shared_ptr<ITemporalBackend> backend);

  // 停止轮询 (设置 running_=false, join 线程)
  void stop();

  // 是否正在轮询
  bool is_running() const { return running_.load(std::memory_order_relaxed); }

 private:
  void poll_loop();

  std::string workflow_id_;
  std::shared_ptr<ITemporalBackend> backend_;
  std::unordered_map<std::string, SignalHandler> handlers_;
  std::mutex handlers_mu_;
  std::thread poll_thread_;
  std::atomic<bool> running_{false};

  static constexpr auto kPollInterval = std::chrono::milliseconds(50);
};

}  // namespace pdk_temporal_agent
