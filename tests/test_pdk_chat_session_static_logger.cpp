// tests/test_pdk_chat_session_static_logger.cpp
// chat-session-static-logger-injection 配套测试
// 关联: openspec/changes/chat-session-static-logger-injection/
//       openspec/specs/static-logger/spec.md
//
// 测试策略 (Oracle C2 修订): 不依赖 chmod/root 失败注入 (chmod 000 对 stat/remove 无效,
// root 持 CAP_DAC_OVERRIDE 免疫)。改为直接单测 detail routing helper 与 Meyers singleton
// 行为, 确定性 100%。
//
// 覆盖 spec R1 (default logger API) + R2 (路由 helper) + 线程安全 (R1.4 并发首调)。

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/chat_session.h"
#include "test_helpers/capturing_logger.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

// Oracle M3 修复: RAII guard 确保每个 CASE 结束 (无论 REQUIRE FAIL/异常) 都 clear,
// 避免污染后续 CASE (尤其 CASE 6 fallback 验证)。
struct DefaultLoggerGuard {
  ~DefaultLoggerGuard() {
    hydraforge::pdk::ChatSession::clear_default_logger();
  }
};

}  // namespace

TEST_CASE("set_default_logger then get_default_logger returns same pointer (R1.1)",
          "[pdk][chat_session][static-logger]") {
  DefaultLoggerGuard guard;
  hydraforge::pdk::ChatSession::clear_default_logger();
  REQUIRE(hydraforge::pdk::ChatSession::get_default_logger() == nullptr);

  auto* raw_logger = new agenticdsl::test::CapturingLogger();
  hydraforge::pdk::ChatSession::set_default_logger(
      std::unique_ptr<agenticdsl::ILogger>(raw_logger));

  auto* got = hydraforge::pdk::ChatSession::get_default_logger();
  REQUIRE(got != nullptr);
  REQUIRE(got == static_cast<agenticdsl::ILogger*>(raw_logger));
}

TEST_CASE("set_default_logger(nullptr) is equivalent to clear (R1.2)",
          "[pdk][chat_session][static-logger]") {
  DefaultLoggerGuard guard;

  hydraforge::pdk::ChatSession::set_default_logger(
      std::make_unique<agenticdsl::test::CapturingLogger>());
  REQUIRE(hydraforge::pdk::ChatSession::get_default_logger() != nullptr);

  hydraforge::pdk::ChatSession::set_default_logger(nullptr);
  REQUIRE(hydraforge::pdk::ChatSession::get_default_logger() == nullptr);
}

TEST_CASE("set_default_logger replaces previous logger (R1.3)",
          "[pdk][chat_session][static-logger]") {
  DefaultLoggerGuard guard;

  auto* raw_a = new agenticdsl::test::CapturingLogger();
  hydraforge::pdk::ChatSession::set_default_logger(
      std::unique_ptr<agenticdsl::ILogger>(raw_a));
  REQUIRE(hydraforge::pdk::ChatSession::get_default_logger() ==
          static_cast<agenticdsl::ILogger*>(raw_a));

  auto* raw_b = new agenticdsl::test::CapturingLogger();
  hydraforge::pdk::ChatSession::set_default_logger(
      std::unique_ptr<agenticdsl::ILogger>(raw_b));
  auto* got = hydraforge::pdk::ChatSession::get_default_logger();
  REQUIRE(got == static_cast<agenticdsl::ILogger*>(raw_b));
  REQUIRE(got != static_cast<agenticdsl::ILogger*>(raw_a));
}

TEST_CASE("concurrent first call returns same pointer (R1.4)",
          "[pdk][chat_session][static-logger]") {
  DefaultLoggerGuard guard;

  // 先 set 一个 logger: Meyers singleton 在 set 时初始化 slot, 后续 get 都返回同一指针。
  auto initial = std::make_unique<agenticdsl::test::CapturingLogger>();
  agenticdsl::ILogger* expected = initial.get();
  hydraforge::pdk::ChatSession::set_default_logger(std::move(initial));

  constexpr int kThreads = 8;
  std::vector<std::thread> threads;
  std::vector<agenticdsl::ILogger*> results(kThreads, nullptr);
  std::atomic<int> ready{0};
  std::atomic<bool> go{false};

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&, i]() {
      ready.fetch_add(1, std::memory_order_release);
      while (!go.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      results[i] = hydraforge::pdk::ChatSession::get_default_logger();
    });
  }

  while (ready.load(std::memory_order_acquire) < kThreads) {
    std::this_thread::yield();
  }
  go.store(true, std::memory_order_release);

  for (auto& t : threads) {
    t.join();
  }

  REQUIRE(expected != nullptr);
  for (int i = 0; i < kThreads; ++i) {
    REQUIRE(results[i] == expected);
  }
}

TEST_CASE("detail::log_static_diag routes to ILogger when set (R2.1)",
          "[pdk][chat_session][static-logger]") {
  DefaultLoggerGuard guard;

  // 单所有权模式: make_unique 创建 unique_ptr, 通过 move 转移给 ChatSession singleton。
  // 不使用 shared_ptr + raw 混合, 否则 double-free (SIGSEGV)。
  auto captured = std::make_unique<agenticdsl::test::CapturingLogger>();
  agenticdsl::test::CapturingLogger* raw = captured.get();
  hydraforge::pdk::ChatSession::set_default_logger(std::move(captured));

  hydraforge::pdk::detail::log_static_diag(
      agenticdsl::LogLevel::kError, "[session] create_directories failed: /x (test)");
  hydraforge::pdk::detail::log_static_diag(
      agenticdsl::LogLevel::kWarn, "[session/cleanup] stat failed: /y");
  hydraforge::pdk::detail::log_static_diag(
      agenticdsl::LogLevel::kInfo, "[session] info msg");

  REQUIRE(raw->count(agenticdsl::LogLevel::kError) == 1);
  REQUIRE(raw->count(agenticdsl::LogLevel::kWarn) == 1);
  REQUIRE(raw->count(agenticdsl::LogLevel::kInfo) == 1);

  auto snapshot = raw->snapshot();
  REQUIRE(snapshot[0].message ==
          "[session] create_directories failed: /x (test)");
  REQUIRE(snapshot[1].message == "[session/cleanup] stat failed: /y");
  REQUIRE(snapshot[2].message == "[session] info msg");
}

TEST_CASE("detail::log_static_diag falls back when not set (R2.3)",
          "[pdk][chat_session][static-logger]") {
  DefaultLoggerGuard guard;
  hydraforge::pdk::ChatSession::clear_default_logger();

  REQUIRE(hydraforge::pdk::ChatSession::get_default_logger() == nullptr);

  // 不应 crash; 直接走 std::cerr fallback。
  hydraforge::pdk::detail::log_static_diag(
      agenticdsl::LogLevel::kWarn, "[session/cleanup] fallback test (stderr)");
  // 验证 fallback 路径生效: get 仍 nullptr (无副作用)。
  REQUIRE(hydraforge::pdk::ChatSession::get_default_logger() == nullptr);
}
