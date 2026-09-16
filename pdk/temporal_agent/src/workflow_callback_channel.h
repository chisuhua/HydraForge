// pdk/temporal_agent/src/workflow_callback_channel.h
// 功能描述：Workflow -> Agent Signal 双向通信通道
//          使用 ITimerService (Sprint 28 microkernel 第 1 件) 替代原 std::thread busy-poll:
//          消除 200ms busy-poll, 改用 timer_->register_periodic(50ms, ...)
//          handler 异常被隔离 (不终止 timer 线程)。
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.2
//          + openspec/changes/2026-09-10-kernel-timer-service/design.md §D6
// 线程安全：handlers_ 受 mutex 保护; stop() 确保 timer cancel
// 作者：pkgm-temporal-agent Phase 2 → Sprint 28 microkernel migration
// 最后修改日期：2026-09-12

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "temporal_client.h"

#include "agenticdsl/contract/timer_service.h"

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

  // 启动后台 long-poll (通过 ITimerService 注册 periodic callback)
  // @param timer nullptr 时内部创建 TimerService (默认 std::jthread 实现)
  //              非 nullptr 时使用注入的 timer (测试可注入 mock)
  //
  // 生命周期契约 (per Oracle session `ses_f6f8ab1dbffeBh5kdvNi1SEE3k` SHIP-with-fixes):
  // - 注入 timer 时, 调用方必须保证: channel 析构前, timer 的 callback 已无 in-flight 执行
  //   (cancel 不等待已收集但未执行的 callback, [this] 捕获可能触发 UAF).
  // - 推荐: 调用方先 stop() (cancel periodic timer), 再析构 timer.
  // - 默认 nullptr 路径安全 (unique_ptr<ITimerService> owned_timer_ RAII 自动 join).
  //
  // fix-timer-callback-dtor-race-2026-09-16: stop() 内部加 in-flight callback barrier wait
  // (per ITimerService contract line 86 + AGENTS.md 模式 #5/#9 + Day 1 ChatSession fix 模式):
  // cancel timer 后, wait in_flight_callbacks_ == 0 保证 callback 结束前不进入
  // members 析构. 显式同步即使 caller 已遵循推荐顺序, 仍消除调用方误用 race.
  void start_polling(std::shared_ptr<ITemporalBackend> backend,
                     agenticdsl::ITimerService* timer = nullptr);

  // 停止轮询 (cancel periodic timer + wait in-flight callback barrier).
  // 安全可重复调用 (idempotent).
  void stop();

  // 是否正在轮询
  bool is_running() const { return running_.load(std::memory_order_relaxed); }

 private:
  // 单次 poll: 拉取 signals + 分发 handlers
  // 异常隔离: handler 调用 try-catch + catch(...)
  void poll_once();

  std::string workflow_id_;
  std::shared_ptr<ITemporalBackend> backend_;
  std::unordered_map<std::string, SignalHandler> handlers_;
  std::mutex handlers_mu_;

  // Sprint 28 microkernel migration: std::thread → ITimerService
  // owned_timer_ 持有所有权 (unique_ptr, RAII 自动析构),
  // timer_ 是观察者指针 (由 owned_timer_ 或外部传入 timer 初始化)
  std::unique_ptr<agenticdsl::ITimerService> owned_timer_;
  agenticdsl::ITimerService* timer_{nullptr};
  // fix-timer-callback-dtor-race-2026-09-16: periodic_id_ 改 atomic — stop()
  // 与 input_thread 路径 (ChatSession 修复同模式) 不会并发写同一内存。
  std::atomic<agenticdsl::ITimerService::TimerId> periodic_id_{0};

  std::atomic<bool> running_{false};

  // fix-timer-callback-dtor-race-2026-09-16: in-flight callback barrier (复用 Day 1
  // ChatSession 修复同模式). poll_once 由 timer callback 调用, 持有 [this] 引用.
  // 取消 timer 不等待 in-flight callback, stop() 必须 barrier 等 poll_once 结束前
  // 才能让 members 析构 (backend_/handlers_/workflow_id_ 都会被 callback 访问).
  std::atomic<int> in_flight_callbacks_{0};
  std::mutex in_flight_mutex_;
  std::condition_variable in_flight_cv_;

  static constexpr auto kPollInterval = std::chrono::milliseconds(50);
};

}  // namespace pdk_temporal_agent
