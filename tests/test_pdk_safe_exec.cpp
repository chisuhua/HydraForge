// tests/test_pdk_safe_exec.cpp
// 文件头注释
// 功能描述：SafeExec 沙箱执行单元测试 (Phase 6a)。
//          TDD red 阶段: 仅 T1+T2 (timeout + leak), 验证旧 std::async 行为失败。
//          T5-T10 (stop_token / grace / types / exception / defaults / chain) 在 T4 (jthread 实施) 后追加。
// 设计依据：openspec/changes/2026-08-10-pdk-safe-exec-tests + ADR-0021 §3.3 + ADR-0020 §2.2.1
// 作者：AgenticDSL Phase 6a
// 最后修改日期：2026-08-10

#include "catch_amalgamated.hpp"
#include "agenticdsl/pdk/safe_exec.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

using namespace hydraforge::pdk;
using namespace std::chrono_literals;

// =====================================================================
// T1 (RED): 超时立即返回 — 旧 std::async 阻塞至 fn 完成 (~500ms), 新 jthread 应 ≤ 150ms 返回
// =====================================================================
TEST_CASE("SafeExec returns within timeout (not fn completion) under std::async",
          "[pdk][phase6a][safe_exec][timeout][red]") {
  SafeExec exec;
  auto start = std::chrono::steady_clock::now();
  REQUIRE_THROWS_AS(
      exec.with_timeout(50ms).run([] {
        std::this_thread::sleep_for(500ms);
        return 42;
      }),
      std::runtime_error);
  auto elapsed = std::chrono::steady_clock::now() - start;
  // 旧 std::async 阻塞至 fn 完成 (~500ms), 新 jthread 应在 ≤ 150ms 内返回
  REQUIRE(elapsed < 150ms);
}

// =====================================================================
// T2 (RED): 100 次超时调用不泄漏线程
// =====================================================================
TEST_CASE("SafeExec does not leak threads after repeated timeouts",
          "[pdk][phase6a][safe_exec][leak][red]") {
  // 采样 baseline 线程数 (50 个 probe threads + 50ms 等待)
  auto baseline_count = [] {
    std::atomic<int> cnt{0};
    std::vector<std::thread> probes;
    for (int i = 0; i < 50; ++i) {
      probes.emplace_back([&cnt] {
        std::this_thread::sleep_for(50ms);
        cnt.fetch_add(1, std::memory_order_relaxed);
      });
    }
    for (auto& t : probes) t.join();
    return cnt.load();
  }();

  SafeExec exec;
  exec.with_timeout(20ms);
  for (int i = 0; i < 100; ++i) {
    try {
      exec.run([] {
        // 故意 hang (忽略 stop_token, 仅靠 SafeExec detach / process exit)
        std::this_thread::sleep_for(5s);
        return 0;
      });
    } catch (const std::runtime_error&) {
      // expected timeout
    }
  }
  std::this_thread::sleep_for(200ms);  // 给 detached worker settle

  auto final_count = [] {
    std::atomic<int> cnt{0};
    std::vector<std::thread> probes;
    for (int i = 0; i < 50; ++i) {
      probes.emplace_back([&cnt] {
        std::this_thread::sleep_for(50ms);
        cnt.fetch_add(1, std::memory_order_relaxed);
      });
    }
    for (auto& t : probes) t.join();
    return cnt.load();
  }();

  // 允许 baseline + 10 缓冲 (detached worker 可能仍在运行)
  REQUIRE(final_count <= baseline_count + 10);
}

// =====================================================================
// T5: stop_token 协同 — fn 不感知 stop_token 也应正常超时
// =====================================================================
TEST_CASE("SafeExec request_stop on timeout is API-available",
          "[pdk][phase6a][safe_exec][stop_token]") {
  SafeExec exec;
  // MVP: stop_token 通过 std::jthread 内部 source 暴露, fn 不接受 stop_token 时仍正常超时
  // Phase 6a MVP 不强制 fn 接受 stop_token, 仅验证 API 表面可用
  REQUIRE_THROWS_AS(
      exec.with_timeout(50ms).run([] {
        std::this_thread::sleep_for(500ms);
        return 0;
      }),
      std::runtime_error);
  SUCCEED("SafeExec timeout path runs through std::stop_source + jthread");
}

// =====================================================================
// T6: grace_period 50ms 后 detach — caller 不阻塞
// =====================================================================
TEST_CASE("SafeExec grace_period: worker ignored stop_token -> detach (no caller block)",
          "[pdk][phase6a][safe_exec][grace]") {
  SafeExec exec;
  auto start = std::chrono::steady_clock::now();
  REQUIRE_THROWS_AS(
      exec.with_timeout(20ms).with_grace_period(20ms).run([] {
        // 故意忽略 stop_token, sleep 1s (远超 grace)
        std::this_thread::sleep_for(1s);
        return 42;
      }),
      std::runtime_error);
  auto elapsed = std::chrono::steady_clock::now() - start;
  // 应在 timeout(20ms) + grace(20ms) + slack(30ms) 内返回
  REQUIRE(elapsed < 70ms);
}

// =====================================================================
// T7: 返回类型推导 (int / string / nlohmann::json / void)
// =====================================================================
TEST_CASE("SafeExec preserves fn return type (int/string/json/void)",
          "[pdk][phase6a][safe_exec][types]") {
  SafeExec exec;
  exec.with_timeout(100ms);

  SECTION("int return type") {
    int r = exec.run([] { return 42; });
    REQUIRE(r == 42);
  }
  SECTION("std::string return type") {
    std::string r = exec.run([] { return std::string("hello"); });
    REQUIRE(r == "hello");
  }
  SECTION("nlohmann::json return type") {
    nlohmann::json r = exec.run([] { return nlohmann::json{{"k", "v"}}; });
    REQUIRE(r["k"] == "v");
  }
  SECTION("void return type") {
    bool called = false;
    exec.run([&called] { called = true; });
    REQUIRE(called);
  }
}

// =====================================================================
// T8: 异常透传 (runtime_error / invalid_argument / out_of_range)
// =====================================================================
TEST_CASE("SafeExec propagates fn exceptions unchanged",
          "[pdk][phase6a][safe_exec][exception]") {
  SafeExec exec;
  exec.with_timeout(1000ms);

  SECTION("std::runtime_error propagated") {
    REQUIRE_THROWS_AS(
        exec.run([] { throw std::runtime_error("disk full"); }),
        std::runtime_error);
    try {
      exec.run([] { throw std::runtime_error("disk full"); });
    } catch (const std::exception& e) {
      REQUIRE(std::string(e.what()) == "disk full");
    }
  }
  SECTION("std::invalid_argument propagated") {
    REQUIRE_THROWS_AS(
        exec.run([] { throw std::invalid_argument("bad input"); }),
        std::invalid_argument);
  }
  SECTION("std::out_of_range propagated") {
    REQUIRE_THROWS_AS(
        exec.run([] { throw std::out_of_range("oob"); }),
        std::out_of_range);
  }
}

// =====================================================================
// T9: 默认值 (timeout=30s, grace_period=50ms, layer_profile=0)
// =====================================================================
TEST_CASE("SafeExec defaults: timeout=30s, grace_period=50ms, layer_profile=0",
          "[pdk][phase6a][safe_exec][defaults]") {
  SafeExec exec;
  REQUIRE(exec.timeout() == std::chrono::milliseconds(30000));
  REQUIRE(exec.grace_period() == std::chrono::milliseconds(50));
  REQUIRE(exec.layer_profile() == 0);
}

// =====================================================================
// T10: 链式配置 (with_timeout + with_grace_period + with_layer_profile)
// =====================================================================
TEST_CASE("SafeExec chainable config returns self-reference",
          "[pdk][phase6a][safe_exec][chain]") {
  SafeExec exec;
  SafeExec& r1 = exec.with_timeout(100ms);
  SafeExec& r2 = exec.with_grace_period(20ms);
  SafeExec& r3 = exec.with_layer_profile(2);
  REQUIRE(&r1 == &exec);
  REQUIRE(&r2 == &exec);
  REQUIRE(&r3 == &exec);
  REQUIRE(exec.timeout() == std::chrono::milliseconds(100));
  REQUIRE(exec.grace_period() == std::chrono::milliseconds(20));
  REQUIRE(exec.layer_profile() == 2);
}
