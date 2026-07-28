// pdk/temporal_agent/src/workflow_callback_channel.cpp
// 功能描述：WorkflowCallbackChannel 实现 - long-poll 后端 consume_signals + handler 分发
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.2
//          .rddf/plans/pkgm-temporal-agent.md Task 2
// 线程安全：handlers_ 受 handlers_mu_ 保护; running_ 为 atomic
//          poll_loop 中 handler 调用异常被 try/catch 隔离
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "workflow_callback_channel.h"

namespace pdk_temporal_agent {

WorkflowCallbackChannel::WorkflowCallbackChannel(std::string workflow_id)
    : workflow_id_(std::move(workflow_id)) {}

WorkflowCallbackChannel::~WorkflowCallbackChannel() {
  stop();
}

void WorkflowCallbackChannel::on_signal(const std::string& signal_name,
                                         SignalHandler handler) {
  std::lock_guard<std::mutex> lock(handlers_mu_);
  handlers_[signal_name] = std::move(handler);
}

void WorkflowCallbackChannel::start_polling(
    std::shared_ptr<ITemporalBackend> backend) {
  if (running_.load(std::memory_order_relaxed)) {
    return;
  }
  backend_ = std::move(backend);
  running_.store(true, std::memory_order_relaxed);
  poll_thread_ = std::thread(&WorkflowCallbackChannel::poll_loop, this);
}

void WorkflowCallbackChannel::stop() {
  if (!running_.exchange(false, std::memory_order_relaxed)) {
    return;
  }
  if (poll_thread_.joinable()) {
    poll_thread_.join();
  }
}

void WorkflowCallbackChannel::poll_loop() {
  while (running_.load(std::memory_order_relaxed)) {
    if (!backend_) {
      std::this_thread::sleep_for(kPollInterval);
      continue;
    }

    auto signals = backend_->consume_signals(workflow_id_);
    for (const auto& sig : signals) {
      SignalHandler handler_copy;
      {
        std::lock_guard<std::mutex> lock(handlers_mu_);
        auto it = handlers_.find(sig.signal_name);
        if (it == handlers_.end()) {
          continue;
        }
        handler_copy = it->second;
      }
      // handler 在锁外调用 (避免死锁 + 异常隔离)
      try {
        handler_copy(sig.payload);
      } catch (...) {
        // 异常被吞掉, poll 线程继续运行
      }
    }

    std::this_thread::sleep_for(kPollInterval);
  }
}

}  // namespace pdk_temporal_agent
