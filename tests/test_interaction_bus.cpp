// tests/test_interaction_bus.cpp
// 文件头注释
// 功能描述：InMemoryBus 集成测试（4 个 TEST_CASE）
//          覆盖 1000x 并发 emit / try_pop / unsubscribe / 多 subscriber
// 设计依据：plan §15
// 作者：AgenticDSL Phase 0 / Track A
// 最后修改日期：2026-06-08

#include "catch_amalgamated.hpp"

#include "common/contract/iinteraction_bus.h"
#include "common/contract/inmemory_bus.h"
#include "core/types/tool_result.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace agenticdsl;

// === Test 1: 1000x 并发 emit（10 线程 × 100 次） ===
TEST_CASE("InMemoryBus concurrent emit 1000x", "[contract][thread]") {
  InMemoryBus bus;
  std::atomic<int> count{0};
  bus.subscribe("test", [&](const ToolResult&) { ++count; });

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < 100; ++j) {
        bus.emit("test", ToolResult::success({{"i", j}}));
      }
    });
  }
  for (auto& t : threads) t.join();

  // 允许轻微延迟（callback 在锁外执行）
  REQUIRE(count.load() >= 1000);
  REQUIRE(count.load() <= 1000);  // 不应重复触发
}

// === Test 2: try_pop 非阻塞 ===
TEST_CASE("InMemoryBus try_pop non-blocking", "[contract][interaction_bus]") {
  InMemoryBus bus;
  std::string event_type;
  ToolResult payload;

  // 空队列 → false
  REQUIRE_FALSE(bus.try_pop(event_type, payload));

  // 入队后 → true
  bus.emit("evt", ToolResult::success({{"k", "v"}}));
  REQUIRE(bus.try_pop(event_type, payload));
  REQUIRE(event_type == "evt");
  REQUIRE(payload.ok);
  REQUIRE(payload.data["k"] == "v");

  // 又空 → false
  REQUIRE_FALSE(bus.try_pop(event_type, payload));
}

// === Test 3: unsubscribe 生效 ===
TEST_CASE("InMemoryBus unsubscribe works", "[contract][interaction_bus]") {
  InMemoryBus bus;
  std::atomic<int> count{0};
  size_t token = bus.subscribe("evt", [&](const ToolResult&) { ++count; });

  // unsubscribe 前 → callback 触发
  bus.emit("evt", ToolResult::success({}));
  REQUIRE(count.load() == 1);

  // unsubscribe
  bus.unsubscribe(token);

  // unsubscribe 后 → callback 不再触发
  bus.emit("evt", ToolResult::success({}));
  REQUIRE(count.load() == 1);
}

// === Test 4: 多 subscriber 都收到 ===
TEST_CASE("InMemoryBus multiple subscribers", "[contract][interaction_bus]") {
  InMemoryBus bus;
  std::atomic<int> count_a{0};
  std::atomic<int> count_b{0};
  bus.subscribe("evt", [&](const ToolResult&) { ++count_a; });
  bus.subscribe("evt", [&](const ToolResult&) { ++count_b; });

  for (int i = 0; i < 5; ++i) {
    bus.emit("evt", ToolResult::success({}));
  }
  REQUIRE(count_a.load() == 5);
  REQUIRE(count_b.load() == 5);

  // 验证 queue 也入了 5 次
  std::string t;
  ToolResult p;
  for (int i = 0; i < 5; ++i) {
    REQUIRE(bus.try_pop(t, p));
  }
  REQUIRE_FALSE(bus.try_pop(t, p));
}
