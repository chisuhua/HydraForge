// pdk/temporal_agent/src/workflow_callback_channel.cpp
// 功能描述：WorkflowCallbackChannel 实现 - 通过 ITimerService periodic callback
//          替代原 std::thread + busy-poll (Sprint 28 microkernel 第 1 件迁移)
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.2
//          + openspec/changes/2026-09-10-kernel-timer-service/design.md §D6
// 线程安全：handlers_ 受 handlers_mu_ 保护; running_ 为 atomic
//          handler 调用异常 try/catch 隔离
// 作者：pkgm-temporal-agent Phase 2 → Sprint 28 microkernel migration
// 最后修改日期：2026-09-16 (fix-timer-callback-dtor-race: in-flight barrier)

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
    std::shared_ptr<ITemporalBackend> backend,
    agenticdsl::ITimerService* timer) {
  if (running_.load(std::memory_order_relaxed)) {
    return;
  }
  backend_ = std::move(backend);

  if (timer != nullptr) {
    timer_ = timer;
    owned_timer_.reset();
  } else {
    owned_timer_ = agenticdsl::make_default_timer_service();
    timer_ = owned_timer_.get();
  }

  running_.store(true, std::memory_order_relaxed);
  // periodic 累积 deadline 语义 (与 Linux timerfd_settime 一致):
  // 下次触发 = prev_deadline + period, 不是 now+period
  // 避免 handler 慢时周期漂移
  //
  // fix-timer-callback-dtor-race-2026-09-16: timer callback 用 RAII guard 追踪 in-flight
  // (复用 Day 1 ChatSession 修复同模式). 即使 body 抛异常, RAII 析构无条件 decrement +
  // notify, 避免 ~WorkflowCallbackChannel barrier wait 永远 hang。
  periodic_id_.store(timer_->register_periodic(
      kPollInterval, [this] {
        in_flight_callbacks_.fetch_add(1, std::memory_order_acq_rel);
        // RAII: 异常路径仍 decrement + notify (避免 deadlock)
        struct CallbackGuard {
          std::atomic<int>* ctr;
          std::condition_variable* cv;
          ~CallbackGuard() {
            ctr->fetch_sub(1, std::memory_order_acq_rel);
            cv->notify_all();
          }
        } guard{&in_flight_callbacks_, &in_flight_cv_};
        poll_once();
      }), std::memory_order_release);
}

void WorkflowCallbackChannel::stop() {
  if (!running_.exchange(false, std::memory_order_relaxed)) {
    return;
  }
  // fix-timer-callback-dtor-race-2026-09-16: 5 步析构顺序 + step ①.5 barrier wait
  // (per ITimerService contract line 86 + AGENTS.md 模式 #5 + Day 1 ChatSession 修复模式):
  // cancel timer 不互斥 callback, 必须等 in-flight 结束前不进入 step ② timer_=nullptr。
  auto id = periodic_id_.exchange(0, std::memory_order_acq_rel);
  if (id != 0 && timer_ != nullptr) {
    timer_->cancel(id);
  }
  // ①.5 barrier wait: cancel 不互斥 callback, 必须等所有 in-flight 结束
  // 否则 callback 进入 body 访问 this->backend_ / this->handlers_ / this->workflow_id_
  // 时 WorkflowCallbackChannel 可能已开始析构 → UB.
  {
    std::unique_lock<std::mutex> lock(in_flight_mutex_);
    in_flight_cv_.wait(lock, [this] {
      return in_flight_callbacks_.load(std::memory_order_acquire) == 0;
    });
  }
  // ② timer_=nullptr (现在安全, 因为所有 callback 已结束)
  timer_ = nullptr;
  // ③ owned_timer_ 析构 (unique_ptr RAII → ~TimerService → ~jthread → join)
  owned_timer_.reset();
}

void WorkflowCallbackChannel::poll_once() {
  if (!backend_) {
    return;
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
    // TimerService worker 内部已 try-catch + catch(...) 隔离, 这里再 double-guard
    try {
      handler_copy(sig.payload);
    } catch (...) {
      // 异常被吞掉, 后续 timer tick 继续
    }
  }
}

}  // namespace pdk_temporal_agent
