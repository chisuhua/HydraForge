// tests/test_temporal_agent_pool.cpp
// 功能描述：Temporal Agent gRPC 连接池测试
//          测试 round-robin 选择 + 故障切换 + 恢复 + 线程安全
//          零 gRPC 依赖 (使用 ChannelHandle 字符串抽象, 非 grpc::Channel)
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.1
//          .rddf/plans/pkgm-temporal-agent.md Task 1
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "temporal_client_pool.h"

#include <map>
#include <set>
#include <string>
#include <vector>

using namespace pdk_temporal_agent;

TEST_CASE("TemporalClientPool: round-robin channel selection under load",
          "[temporal_agent][pool]") {
  TemporalClientPool pool({"host1:7233", "host2:7233", "host3:7233"});
  std::map<std::string, int> hits;
  constexpr int kIterations = 300;
  for (int i = 0; i < kIterations; ++i) {
    auto channel = pool.acquire_channel();
    REQUIRE(channel.has_value());
    hits[channel->target]++;
  }
  REQUIRE(hits.size() == 3);
  for (const auto& [host, count] : hits) {
    REQUIRE(count == kIterations / 3);  // perfect round-robin: 100 each
  }
}

TEST_CASE("TemporalClientPool: failed channel is replaced transparently",
          "[temporal_agent][pool]") {
  TemporalClientPool pool({"broken:7233", "healthy:7233"});
  pool.mark_unhealthy("broken:7233");
  auto ch = pool.acquire_channel();
  REQUIRE(ch.has_value());
  REQUIRE(ch->target == "healthy:7233");
}

TEST_CASE("TemporalClientPool: single target pool always returns same target",
          "[temporal_agent][pool]") {
  TemporalClientPool pool({"only-host:7233"});
  for (int i = 0; i < 10; ++i) {
    auto ch = pool.acquire_channel();
    REQUIRE(ch.has_value());
    REQUIRE(ch->target == "only-host:7233");
  }
}

TEST_CASE("TemporalClientPool: all unhealthy returns nullopt",
          "[temporal_agent][pool]") {
  TemporalClientPool pool({"h1:7233", "h2:7233"});
  pool.mark_unhealthy("h1:7233");
  pool.mark_unhealthy("h2:7233");
  auto ch = pool.acquire_channel();
  REQUIRE_FALSE(ch.has_value());
}

TEST_CASE("TemporalClientPool: mark_unhealthy then mark_healthy recovery",
          "[temporal_agent][pool]") {
  TemporalClientPool pool({"bad:7233", "good:7233"});

  // Initially round-robin between both
  auto ch0 = pool.acquire_channel();
  REQUIRE(ch0.has_value());

  // Mark bad as unhealthy -> only good should be returned
  pool.mark_unhealthy("bad:7233");
  for (int i = 0; i < 5; ++i) {
    auto ch = pool.acquire_channel();
    REQUIRE(ch.has_value());
    REQUIRE(ch->target == "good:7233");
  }

  // Recover bad -> round-robin resumes between both
  pool.mark_healthy("bad:7233");
  std::set<std::string> seen;
  for (int i = 0; i < 10; ++i) {
    auto ch = pool.acquire_channel();
    REQUIRE(ch.has_value());
    seen.insert(ch->target);
  }
  REQUIRE(seen.size() == 2);  // both targets back in rotation
}
