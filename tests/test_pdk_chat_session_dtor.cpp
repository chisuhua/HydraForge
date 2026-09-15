// tests/test_pdk_chat_session_dtor.cpp
// fix-tsan-residual-2026-09-15 regression guard: ChatSession::Impl::~Impl() dtor race
// 关联: openspec/changes/fix-tsan-residual-2026-09-15/proposal.md (R2)
//       ITimerService contract line 86 "外部注入 timer 必须自行保证生命周期"
//
// Pre-fix: ~Impl() 写 periodic_id_ (race with TimerGuard) + cancel() 后不等待
// in-flight callback (callback 访问已销毁成员 → UB). TSan 报告 test_chat_session
// + test_chat_session_queues dtor race.
// Post-fix: periodic_id_ atomic + barrier wait in_flight_callbacks == 0.

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/timer_service.h"
#include "agenticdsl/pdk/chat_session.h"

#include <chrono>
#include <memory>
#include <thread>

TEST_CASE("ChatSession ~Impl() with active timer + input_thread is dtor-safe",
          "[pdk][chat_session][dtor][tsan]") {
  hydraforge::pdk::AgentConfig agent_cfg;
  hydraforge::pdk::SessionConfig session_cfg;
  // enable_input_thread=true 让 input_thread 启动 + timer 注册 periodic 50ms.
  // 这是触发 ~Impl() dtor race 的必要前提 (否则 periodic_id_ 永远 0, 无 race).
  session_cfg.enable_input_thread = true;
  session_cfg.persist_dir = "";  // 避免 ensure_dir_0700 IO

  // Test 在每个 SECTION 构造 + 析构一次, 验证周期性触发 timer callback 的
  // 情况下 ~Impl() 不引发 TSan warning 或 UB. 多 iter 提升捕获概率.
  for (int iter = 0; iter < 10; ++iter) {
    SECTION("iter " + std::to_string(iter) + ": construct with active timer, dtor mid-cycle") {
      auto timer = agenticdsl::make_default_timer_service();
      REQUIRE(timer != nullptr);

      {
        hydraforge::pdk::ChatSession session(
            nullptr, nullptr, nullptr,
            agent_cfg, session_cfg,
            nullptr,                  // CancellationRegistry
            timer.get(),              // ITimerService* — 触发 timer 路径
            nullptr, nullptr, nullptr, std::nullopt);
        // 让 timer 注册 (line 861-869 area) 并至少 fire 一次 (50ms 周期).
        // fire 后立即析构: callback 可能 in-flight, ~Impl() 必须 wait barrier.
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
      }  // session.~ChatSession() → impl_->~Impl() (D8 5 步析构 + barrier)
      // 若 barrier 未生效, callback 仍在访问 Impl 成员 → TSan warning / crash
    }
  }
}

TEST_CASE("ChatSession ~Impl() with no timer + no input_thread is fast no-op",
          "[pdk][chat_session][dtor][tsan]") {
  // 反向用例: 无 timer + 无 input_thread 时 ~Impl() 必须极简退出, 无 barrier wait
  // (in_flight_callbacks_ 永远 0, cv.wait 立即返回).
  hydraforge::pdk::AgentConfig agent_cfg;
  hydraforge::pdk::SessionConfig session_cfg;
  session_cfg.enable_input_thread = false;  // 默认
  session_cfg.persist_dir = "";

  hydraforge::pdk::ChatSession session(
      nullptr, nullptr, nullptr,
      agent_cfg, session_cfg);
  // 直接析构 — 不应有任何 timer cancel 或 barrier wait
}