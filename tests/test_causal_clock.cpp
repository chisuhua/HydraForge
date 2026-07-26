// tests/test_causal_clock.cpp
// Change C: CausalClock 单元测试 — 单调性 / 线程安全 / merge / happens-before / soak
#include "catch_amalgamated.hpp"
#include "agenticdsl/contract/causal_clock.h"

#include <atomic>
#include <thread>
#include <vector>

TEST_CASE("CausalClock monotonic — tick always increases", "[causal_clock]") {
    agenticdsl::event::CausalClock clk;
    auto t1 = clk.tick();
    auto t2 = clk.tick();
    auto t3 = clk.tick();
    REQUIRE(t1 < t2);
    REQUIRE(t2 < t3);
    REQUIRE(clk.now() == 3);
}

TEST_CASE("CausalClock thread safety — 10x1000 ticks = 10000", "[causal_clock][concurrency]") {
    agenticdsl::event::CausalClock clk;
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&clk]() {
            for (int j = 0; j < 1000; ++j) clk.tick();
        });
    }
    for (auto& t : threads) t.join();
    REQUIRE(clk.now() == 10000);
}

TEST_CASE("CausalClock merge — takes max", "[causal_clock]") {
    agenticdsl::event::CausalClock clk;
    clk.tick();
    clk.merge(1000);
    REQUIRE(clk.now() >= 1000);
    clk.tick();
    REQUIRE(clk.now() >= 1001);
}

TEST_CASE("CausalClock happens_before", "[causal_clock]") {
    using agenticdsl::event::CausalClock;
    REQUIRE(CausalClock::happens_before(1, 2));
    REQUIRE_FALSE(CausalClock::happens_before(2, 1));
    REQUIRE_FALSE(CausalClock::happens_before(5, 5));
}

TEST_CASE("CausalClock soak — 3 producers, consumer monotonic per-producer",
          "[causal_clock][soak]") {
    agenticdsl::event::CausalClock clk;
    std::vector<agenticdsl::event::CausalClock::TimePoint> producer_times[3];

    std::vector<std::thread> producers;
    for (int p = 0; p < 3; ++p) {
        producers.emplace_back([&clk, &producer_times, p]() {
            for (int i = 0; i < 100; ++i) {
                producer_times[p].push_back(clk.tick());
            }
        });
    }
    for (auto& t : producers) t.join();

    for (int p = 0; p < 3; ++p) {
        for (size_t i = 1; i < producer_times[p].size(); ++i) {
            REQUIRE(producer_times[p][i - 1] < producer_times[p][i]);
        }
    }

    REQUIRE(clk.now() == 300);
}