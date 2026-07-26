// tests/test_interaction_bus_glob.cpp
// Change B: glob pattern subscribe — exact match, single wildcard,
// multi-wildcard, no-match, race test.
#include "catch_amalgamated.hpp"
#include "agenticdsl/contract/inmemory_bus.h"

#include <atomic>
#include <thread>

TEST_CASE("glob subscribe: exact match (no wildcard)", "[interaction_bus][glob]") {
  agenticdsl::InMemoryBus bus;
  std::atomic<size_t> count{0};

  bus.subscribe("inference.lifecycle.idle", [&](const agenticdsl::BusEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  });

  bus.emit("inference.lifecycle.idle", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 1);

  bus.emit("inference.lifecycle.running", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 1);
}

TEST_CASE("glob subscribe: single wildcard *", "[interaction_bus][glob]") {
  agenticdsl::InMemoryBus bus;
  std::atomic<size_t> count{0};

  bus.subscribe("inference.*", [&](const agenticdsl::BusEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  });

  bus.emit("inference.lifecycle.idle", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 1);

  bus.emit("inference.lifecycle.running", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 2);

  bus.emit("inference.lifecycle.error", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 3);
}

TEST_CASE("glob subscribe: multi-wildcard *.error.*", "[interaction_bus][glob]") {
  agenticdsl::InMemoryBus bus;
  std::atomic<size_t> count{0};

  bus.subscribe("*.error.*", [&](const agenticdsl::BusEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  });

  bus.emit("inference.error.oom", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 1);

  bus.emit("temporal.error.timeout", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 2);

  // should NOT match (no "error" section)
  bus.emit("inference.timeout.oom", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 2);
}

TEST_CASE("glob subscribe: no match", "[interaction_bus][glob]") {
  agenticdsl::InMemoryBus bus;
  std::atomic<size_t> count{0};

  bus.subscribe("other.*", [&](const agenticdsl::BusEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  });

  bus.emit("inference.lifecycle.idle", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 0);
}

TEST_CASE("glob subscribe: unsubscribe", "[interaction_bus][glob]") {
  agenticdsl::InMemoryBus bus;
  std::atomic<size_t> count{0};

  size_t token = bus.subscribe("inference.*", [&](const agenticdsl::BusEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  });

  bus.emit("inference.lifecycle.idle", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 1);

  bus.unsubscribe(token);

  bus.emit("inference.lifecycle.running", "test");
  bus.wait_for_drain();
  REQUIRE(count.load() == 1);
}

TEST_CASE("glob subscribe: race — subscribe/unsubscribe during dispatch",
          "[interaction_bus][glob][concurrency]") {
  agenticdsl::InMemoryBus bus;
  std::atomic<size_t> received{0};

  auto token = bus.subscribe("race.*", [&](const agenticdsl::BusEvent&) {
    received.fetch_add(1, std::memory_order_relaxed);
  });

  // emit many events while toggling subscription
  std::thread producer([&]() {
    for (int i = 0; i < 1000; ++i) {
      bus.emit("race.event", std::to_string(i));
    }
  });

  std::thread toggler([&]() {
    for (int i = 0; i < 100; ++i) {
      size_t t = bus.subscribe("race.*", [&](const agenticdsl::BusEvent&) {
        received.fetch_add(1, std::memory_order_relaxed);
      });
      bus.unsubscribe(t);
    }
  });

  producer.join();
  toggler.join();
  bus.wait_for_drain();

  (void)token;
  REQUIRE(received.load() >= 1000);
}