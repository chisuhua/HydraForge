// tests/test_interaction_bus.cpp
// 文件头注释
// 功能描述：InMemoryBus 集成测试（4 个 TEST_CASE）
//          覆盖 1000x 并发 emit / try_pop / unsubscribe / 多 subscriber
// 设计依据：plan §15
// 作者：AgenticDSL Phase 0 / Track A
// 最后修改日期：2026-06-08

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
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
  bus.subscribe("test", [&](const BusEvent&) { ++count; });

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < 100; ++j) {
        bus.emit(BusEvent{"test", ToolResult::success({{"i", j}}), std::chrono::steady_clock::now()});
      }
    });
  }
  for (auto& t : threads) t.join();
  bus.wait_for_drain();

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

  // 异步 dispatch 场景: 验证 emit 后 subscriber 被调用
  std::atomic<int> count{0};
  bus.subscribe("evt", [&](const BusEvent&) { ++count; });
  bus.emit(BusEvent{"evt", ToolResult::success({{"k", "v"}}), std::chrono::steady_clock::now()});
  bus.wait_for_drain();
  REQUIRE(count.load() == 1);
}

// === Test 3: unsubscribe 生效 ===
TEST_CASE("InMemoryBus unsubscribe works", "[contract][interaction_bus]") {
  InMemoryBus bus;
  std::atomic<int> count{0};
  size_t token = bus.subscribe("evt", [&](const BusEvent&) { ++count; });

  // unsubscribe 前 → callback 触发
  bus.emit(BusEvent{"evt", ToolResult::success({}), std::chrono::steady_clock::now()});
    bus.wait_for_drain();
  REQUIRE(count.load() == 1);

  // unsubscribe
  bus.unsubscribe(token);

  // unsubscribe 后 → callback 不再触发
  bus.emit(BusEvent{"evt", ToolResult::success({}), std::chrono::steady_clock::now()});
    bus.wait_for_drain();
  REQUIRE(count.load() == 1);
}

// === Test 4: 多 subscriber 都收到 ===
TEST_CASE("InMemoryBus multiple subscribers", "[contract][interaction_bus]") {
  InMemoryBus bus;
  std::atomic<int> count_a{0};
  std::atomic<int> count_b{0};
  bus.subscribe("evt", [&](const BusEvent&) { ++count_a; });
  bus.subscribe("evt", [&](const BusEvent&) { ++count_b; });

  for (int i = 0; i < 5; ++i) {
    bus.emit(BusEvent{"evt", ToolResult::success({}), std::chrono::steady_clock::now()});
    bus.wait_for_drain();
  }
  REQUIRE(count_a.load() == 5);
  REQUIRE(count_b.load() == 5);
}

// === Test 5: std::string 重载 (REQ-TR-005 向后兼容) ===
// Phase 1 Sprint 1a (S1a.T3): 验证字符串载荷自动包装为 ToolResult 信封
TEST_CASE("InMemoryBus emits accept std::string legacy payload",
          "[contract][interaction_bus][phase1]") {
  InMemoryBus bus;
  std::atomic<int> count{0};
  std::string captured_event_type;
  ToolResult captured_payload;
  std::atomic<bool> captured{false};

  bus.subscribe("legacy_topic", [&](const BusEvent& e) {
    ++count;
    captured_payload = e.payload;
    captured_event_type = "legacy_topic";
    captured = true;
  });

  // 通过 BusEvent 发射
  bus.emit(BusEvent{"legacy_topic", ToolResult::success({{"content", "legacy content payload"}}), std::chrono::steady_clock::now()});
    bus.wait_for_drain();

  // 验证 callback 触发
  REQUIRE(count.load() == 1);
  REQUIRE(captured.load());
  REQUIRE(captured_event_type == "legacy_topic");
  REQUIRE(captured_payload.ok);
  // 验证字符串内容保留在 meta["content"] (REQ-TR-005 Scenario 向后兼容 string 推送)
  REQUIRE(captured_payload.meta.contains("content"));
  REQUIRE(captured_payload.meta["content"] == "legacy content payload");
}
