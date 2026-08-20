// tests/test_mock_bus_canonical.cpp
// 功能描述：Canonical MockBus fixture 自测试 (ADR-0019)
//          验证 9 处重复 MockBus 实现的统一 canonical fixture 行为正确性。
// 设计依据：openspec/changes/mock-bus-canonical-extract (P12) + ADR-0019 IInteractionBus
// 作者：P12 mock-bus-canonical-extract change
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "test_helpers/mock_bus.h"

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/bus_event.h"

using namespace agenticdsl;
using agenticdsl::test::MockBus;

// =====================================================================
// Test 1: emit(BusEvent) 主路径 — 兼容 #3/#7/#8/#9 全 BusEvent 存储
// =====================================================================
TEST_CASE("MockBus emit(BusEvent) appends to events and topics", "[mock_bus][canonical]") {
  MockBus bus;
  REQUIRE(bus.events.empty());
  REQUIRE(bus.topics.empty());

  BusEvent e;
  e.topic = "llm.request";
  e.payload.ok = true;
  e.payload.data = {{"prompt", "hello"}};
  bus.emit(e);

  REQUIRE(bus.events.size() == 1);
  REQUIRE(bus.topics.size() == 1);
  REQUIRE(bus.events[0].topic == "llm.request");
  REQUIRE(bus.events[0].payload.data["prompt"] == "hello");
  REQUIRE(bus.topics[0] == "llm.request");
}

// =====================================================================
// Test 2: emit(string, string) 重载 — 包装为 BusEvent (兼容 #1)
// =====================================================================
TEST_CASE("MockBus emit(string, string) wraps as BusEvent", "[mock_bus][canonical]") {
  MockBus bus;

  bus.emit("user.input", "Write hello world");

  REQUIRE(bus.events.size() == 1);
  REQUIRE(bus.events[0].topic == "user.input");
  REQUIRE(bus.events[0].payload.meta.contains("content"));
  REQUIRE(bus.events[0].payload.meta["content"] == "Write hello world");
  REQUIRE(bus.topics[0] == "user.input");
}

// =====================================================================
// Test 3: subscribe + emit 触发 callback — 兼容 #7/#8/#9
// =====================================================================
TEST_CASE("MockBus subscribe then emit triggers callback", "[mock_bus][canonical]") {
  MockBus bus;

  size_t callback_count = 0;
  std::string received_topic;
  bus.subscribe("llm.request",
                [&](const BusEvent& e) {
                  ++callback_count;
                  received_topic = e.topic;
                });

  BusEvent e;
  e.topic = "llm.request";
  bus.emit(e);

  REQUIRE(callback_count == 1);
  REQUIRE(received_topic == "llm.request");
}

// =====================================================================
// Test 4: 同一 topic 多个 subscriber — 全部触发
// =====================================================================
TEST_CASE("MockBus multiple subscribers on same topic all invoked", "[mock_bus][canonical]") {
  MockBus bus;

  size_t count_a = 0;
  size_t count_b = 0;
  bus.subscribe("tool.call", [&](const BusEvent&) { ++count_a; });
  bus.subscribe("tool.call", [&](const BusEvent&) { ++count_b; });

  BusEvent e;
  e.topic = "tool.call";
  bus.emit(e);

  REQUIRE(count_a == 1);
  REQUIRE(count_b == 1);
}

// =====================================================================
// Test 5: count(topic) helper
// =====================================================================
TEST_CASE("MockBus count(topic) returns event count for topic", "[mock_bus][canonical]") {
  MockBus bus;

  BusEvent e1, e2, e3;
  e1.topic = "llm.request";
  e2.topic = "tool.call";
  e3.topic = "llm.request";

  bus.emit(e1);
  bus.emit(e2);
  bus.emit(e3);

  REQUIRE(bus.count("llm.request") == 2);
  REQUIRE(bus.count("tool.call") == 1);
  REQUIRE(bus.count("nonexistent") == 0);
}

// =====================================================================
// Test 6: last(topic) helper — 返回最后一个匹配事件
// =====================================================================
TEST_CASE("MockBus last(topic) returns last event for topic", "[mock_bus][canonical]") {
  MockBus bus;

  BusEvent e1, e2;
  e1.topic = "llm.request";
  e1.payload.data = {{"seq", 1}};
  e2.topic = "llm.request";
  e2.payload.data = {{"seq", 2}};

  bus.emit(e1);
  bus.emit(e2);

  const BusEvent* last = bus.last("llm.request");
  REQUIRE(last != nullptr);
  REQUIRE(last->payload.data["seq"] == 2);

  // 另一个 topic 无事件
  REQUIRE(bus.last("tool.call") == nullptr);
}

// =====================================================================
// Test 7: clear() — events 清空, subscribers 保留
// =====================================================================
TEST_CASE("MockBus clear resets events but preserves subscribers", "[mock_bus][canonical]") {
  MockBus bus;

  size_t callback_count = 0;
  bus.subscribe("test.event", [&](const BusEvent&) { ++callback_count; });

  BusEvent e;
  e.topic = "test.event";
  bus.emit(e);
  REQUIRE(bus.events.size() == 1);
  REQUIRE(callback_count == 1);

  bus.clear();
  REQUIRE(bus.events.empty());
  REQUIRE(bus.topics.empty());

  // Subscriber 保留 — emit 仍触发
  bus.emit(e);
  REQUIRE(callback_count == 2);
  REQUIRE(bus.events.size() == 1);
}

// =====================================================================
// Test 8: topics vector 镜像事件 topics — 兼容 #2/#4/#5/#6
// =====================================================================
TEST_CASE("MockBus topics vector mirrors event topics in order", "[mock_bus][canonical]") {
  MockBus bus;

  BusEvent e1, e2, e3;
  e1.topic = "first";
  e2.topic = "second";
  e3.topic = "third";
  bus.emit(e1);
  bus.emit(e2);
  bus.emit(e3);

  REQUIRE(bus.topics.size() == 3);
  REQUIRE(bus.topics[0] == "first");
  REQUIRE(bus.topics[1] == "second");
  REQUIRE(bus.topics[2] == "third");
}

// =====================================================================
// Test 9: emit 不触发其他 topic 的 subscriber (精确匹配)
// =====================================================================
TEST_CASE("MockBus emit does not invoke other-topic subscribers", "[mock_bus][canonical]") {
  MockBus bus;

  size_t wrong_count = 0;
  size_t right_count = 0;
  bus.subscribe("topic.A", [&](const BusEvent&) { ++wrong_count; });
  bus.subscribe("topic.B", [&](const BusEvent&) { ++right_count; });

  BusEvent e;
  e.topic = "topic.B";
  bus.emit(e);

  REQUIRE(wrong_count == 0);
  REQUIRE(right_count == 1);
}

// =====================================================================
// Test 10: subscribe 返回递增 token (兼容 #7/#8/#9)
// =====================================================================
TEST_CASE("MockBus subscribe returns incremental unique tokens", "[mock_bus][canonical]") {
  MockBus bus;

  size_t t1 = bus.subscribe("a", [](const BusEvent&) {});
  size_t t2 = bus.subscribe("b", [](const BusEvent&) {});
  size_t t3 = bus.subscribe("c", [](const BusEvent&) {});

  REQUIRE(t1 != t2);
  REQUIRE(t2 != t3);
  REQUIRE(t1 != t3);
  REQUIRE(t1 > 0);  // token 从 1 开始（非 0，避免与 InMemoryBus default-initialized 混淆）
}

TEST_CASE("MockBus wildcard subscribe receives all events", "[mock_bus][canonical][wildcard]") {
  MockBus bus;
  size_t received = 0;
  bus.subscribe("*", [&](const BusEvent&) { ++received; });

  BusEvent e1, e2, e3;
  e1.topic = "llm.request";
  e2.topic = "tool.call";
  e3.topic = "agent.spawned";
  bus.emit(e1);
  bus.emit(e2);
  bus.emit(e3);

  REQUIRE(received == 3);
  REQUIRE(bus.events.size() == 3);
}

TEST_CASE("MockBus exact + wildcard subscribe both fire", "[mock_bus][canonical][wildcard]") {
  MockBus bus;
  size_t exact_count = 0;
  size_t wildcard_count = 0;
  bus.subscribe("llm.request", [&](const BusEvent&) { ++exact_count; });
  bus.subscribe("*", [&](const BusEvent&) { ++wildcard_count; });

  BusEvent e1, e2;
  e1.topic = "llm.request";
  e2.topic = "tool.call";
  bus.emit(e1);
  bus.emit(e2);

  REQUIRE(exact_count == 1);
  REQUIRE(wildcard_count == 2);
}
