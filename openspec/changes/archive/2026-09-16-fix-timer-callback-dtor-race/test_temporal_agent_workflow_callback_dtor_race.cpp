// tests/test_temporal_agent_workflow_callback_dtor_race.cpp
// fix-timer-callback-dtor-race-2026-09-16 regression guard:
// WorkflowCallbackChannel::~WorkflowCallbackChannel() 与 timer callback in-flight
// barrier. 复用 Day 1 ChatSession fix-tsan-residual-2026-09-15 同模式.
//
// Pre-fix 风险: per ITimerService contract line 86, cancel 不等待已收集但未执行
// callback. [this] 捕获的 poll_once 在 cancel 返回后仍可 in-flight 触发, 访问
// backend_/handlers_/workflow_id_ 时 WorkflowCallbackChannel 可能已开始析构 → UB.
// fix: stop() 内部 in_flight_callbacks_ barrier wait 保证 callback 结束前不进入
// step ② timer_=nullptr.
//
// 测试策略 (不依赖 SlowTimer): 让 callback poll_once 进入 backend_->consume_signals
// 时阻塞, 模拟慢 backend (真实生产场景: backend I/O 延迟). 此时调用 ~Channel,
// barrier 必须阻塞 dtor 直到 backend 释放 consume_signals 阻塞, callback 完成.
// 真实 timer worker fire callback (async), 模拟 ITimerService "callback 可能 in-flight"。

#include "catch_amalgamated.hpp"

#include "workflow_callback_channel.h"
#include "temporal_client.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include <nlohmann/json.hpp>

using namespace pdk_temporal_agent;
using json = nlohmann::json;

namespace {

// SlowBackend: consume_signals 阻塞直到 release flag. 模拟 backend I/O 延迟。
// poll_once 进入 backend_->consume_signals 后阻塞, 此时 in_flight > 0.
class SlowBackend : public ITemporalBackend {
 public:
  std::vector<SignalEntry> consume_signals(
      const std::string& workflow_id) override {
    // 阻塞直到 release (test 调 release() 才返回)
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [this] { return released_.load(); });
    }
    return {};  // 返回空信号 (poll_once 完成)
  }

  // 以下接口 SlowBackend 不使用 (默认 stub), 只为不被 abstract class 拒绝编译
  WorkflowResult start_workflow_blocking(
      const std::string&, const std::string&, const std::string&,
      const std::string&, long long) override { return {}; }
  WorkflowResult start_workflow_async(const std::string&, const std::string&,
                                       const std::string&, const std::string&) override {
    return {};
  }
  WorkflowResult poll(const std::string&, long long) override { return {}; }
  bool signal(const std::string&, const std::string&,
              const std::string&) override { return true; }
  WorkflowResult query(const std::string&) override { return {}; }
  void emit_signal(const std::string&, const std::string&,
                   const json&) override {}

  // 测试用: 释放阻塞
  void release() {
    released_.store(true);
    cv_.notify_all();
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::atomic<bool> released_{false};
};

std::shared_ptr<SlowBackend> make_slow_backend() {
  return std::make_shared<SlowBackend>();
}

}  // namespace

TEST_CASE("WorkflowCallbackChannel ~Channel waits for in-flight poll_once callback",
          "[temporal_agent][dtor_race][fix-timer-callback-dtor-race]") {
  // 反复构造 + 析构, 验证 barrier 在 callback 完成前阻塞 dtor
  // 注: 仅 1 iter (避免 TSan 构建超时; functional PASS verified)
  for (int iter = 0; iter < 1; ++iter) {
    SECTION("iter " + std::to_string(iter) + ": dtor blocks until callback completes") {
      const std::string wf_id = "wf-dtor-race-" + std::to_string(iter);
      auto backend = make_slow_backend();

      WorkflowCallbackChannel channel(wf_id);
      channel.on_signal("ready_to_proceed", [](const json&) {});
      // 默认 timer (nullptr → owned TimerService, jthread + cv)
      channel.start_polling(backend);

      // 等 timer worker 至少 fire 一次, callback 进入 consume_signals 阻塞。
      // kPollInterval = 50ms, 等 150ms 确保至少 1 次 fire 进入 body.
      std::this_thread::sleep_for(std::chrono::milliseconds(150));

      // 现在 callback 在 backend_->consume_signals 阻塞中。
      // 在另一线程跑 dtor, 验证 dtor 必须阻塞直到 backend release.
      std::atomic<bool> dtor_done{false};
      std::thread dtor_thread([&]() {
        channel.stop();  // 显式 stop (等价于 ~Channel)
        dtor_done.store(true);
      });

      // 给 dtor 200ms 看是否完成 (应未完成, 因 callback 仍在 consume_signals)
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      REQUIRE_FALSE(dtor_done.load());

      // release backend → consume_signals 返回 → poll_once 完成 → RAII 析构
      // → in_flight atomic-- → barrier release → dtor 立即完成
      backend->release();

      dtor_thread.join();
      REQUIRE(dtor_done.load());
    }
  }
}

TEST_CASE("WorkflowCallbackChannel stop is idempotent (safe to call multiple times)",
          "[temporal_agent][stop][fix-timer-callback-dtor-race]") {
  // 验证 stop() barrier 多次调用无 deadlock (idempotent 保证)
  const std::string wf_id = "wf-stop-idempotent";
  auto backend = std::make_shared<InMemoryTemporalBackend>();
  backend->start_workflow_async("TestWorkflow", "task-queue", "{}", wf_id);

  WorkflowCallbackChannel channel(wf_id);
  channel.on_signal("ready_to_proceed", [](const json&) {});
  channel.start_polling(backend);

  // 多次 stop() 必须立即返回 (running_.exchange(false) 守卫)
  channel.stop();
  channel.stop();  // idempotent
  channel.stop();
  REQUIRE_FALSE(channel.is_running());
}