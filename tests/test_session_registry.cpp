// tests/test_session_registry.cpp
// 功能描述：SessionRegistry 单元测试 (C11 Phase 5 Stage 1 Step 1)
// 测试范围：SessionRegistry 类的 5 个公开方法 + 4 个 session.* 工具
// 设计依据：tasks.md §5 (unit tests) + proposal.md §验证标准
// 作者：AgenticDSL Phase 5 / Sprint 20 C11
// 最后修改日期：2026-07-04

#include "catch_amalgamated.hpp"

#include <thread>
#include <atomic>

#include "core/types/session_registry.h"
#include "core/types/session.h"
#include "agenticdsl/types/session_config.h"

using namespace agenticdsl;

TEST_CASE("SessionRegistry: create + get + list", "[session_registry][c11]") {
  SessionRegistry registry;

  SECTION("create_session returns unique ID") {
    SessionConfig cfg;
    cfg.name = "test-user";
    auto id1 = registry.create_session(cfg);
    auto id2 = registry.create_session(cfg);
    REQUIRE(id1 != id2);
    REQUIRE(!id1.empty());
    REQUIRE(!id2.empty());
  }

  SECTION("get_session returns valid UserSession") {
    SessionConfig cfg;
    cfg.name = "test-user";
    auto id = registry.create_session(cfg);
    auto& session = registry.get_session(id);
    REQUIRE(session.user_id() == "test-user");
  }

  SECTION("get_session throws on missing ID") {
    REQUIRE_THROWS_AS(registry.get_session("nonexistent"), std::out_of_range);
  }

  SECTION("list_sessions returns all IDs") {
    registry.create_session(SessionConfig{});
    registry.create_session(SessionConfig{});
    registry.create_session(SessionConfig{});
    auto ids = registry.list_sessions();
    REQUIRE(ids.size() == 3);
  }
}

TEST_CASE("SessionRegistry: destroy_session", "[session_registry][c11]") {
  SessionRegistry registry;

  SECTION("destroy removes session") {
    auto id = registry.create_session(SessionConfig{});
    REQUIRE_NOTHROW(registry.destroy_session(id));
    REQUIRE_THROWS_AS(registry.get_session(id), std::out_of_range);
  }

  SECTION("destroy nonexistent is idempotent no-op") {
    REQUIRE_NOTHROW(registry.destroy_session("nonexistent"));
  }

  SECTION("destroy clears from list") {
    auto id = registry.create_session(SessionConfig{});
    registry.create_session(SessionConfig{});
    registry.destroy_session(id);
    auto ids = registry.list_sessions();
    REQUIRE(ids.size() == 1);
  }
}

TEST_CASE("SessionRegistry: is_in_flight", "[session_registry][c11]") {
  SessionRegistry registry;

  SECTION("new session is not in-flight") {
    auto id = registry.create_session(SessionConfig{});
    REQUIRE(registry.is_in_flight(id) == false);
  }

  SECTION("nonexistent session is not in-flight") {
    REQUIRE(registry.is_in_flight("nonexistent") == false);
  }
}

TEST_CASE("SessionRegistry: SessionVars set/get", "[session_registry][c11]") {
  SessionRegistry registry;

  SECTION("set and get session variable") {
    auto id = registry.create_session(SessionConfig{});
    auto& session = registry.get_session(id);

    session.set_session_var("key1", nlohmann::json::parse(R"({"value": 42})"));
    auto val = session.get_session_var("key1");
    REQUIRE(val["value"] == 42);
  }

  SECTION("get nonexistent key returns null json") {
    auto id = registry.create_session(SessionConfig{});
    auto& session = registry.get_session(id);
    REQUIRE(session.get_session_var("missing").is_null());
  }

  SECTION("session_vars isolated between sessions") {
    auto id1 = registry.create_session(SessionConfig{});
    auto id2 = registry.create_session(SessionConfig{});

    registry.get_session(id1).set_session_var("x", 100);
    registry.get_session(id2).set_session_var("x", 200);

    REQUIRE(registry.get_session(id1).get_session_var("x") == 100);
    REQUIRE(registry.get_session(id2).get_session_var("x") == 200);
  }
}

TEST_CASE("SessionRegistry: thread safety — concurrent create/destroy", "[session_registry][c11][concurrent]") {
  SessionRegistry registry;
  constexpr int kIterations = 100;

  // Multiple threads creating and destroying sessions concurrently
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&registry]() {
      for (int i = 0; i < kIterations / 4; ++i) {
        SessionConfig cfg;
        cfg.name = "concurrent-" + std::to_string(i);
        auto id = registry.create_session(cfg);
        REQUIRE(!id.empty());
        registry.destroy_session(id);
      }
    });
  }

  for (auto& t : threads) t.join();

  // After all threads finish, registry should be empty
  auto ids = registry.list_sessions();
  REQUIRE(ids.empty());
}

TEST_CASE("SessionRegistry: concurrent list_sessions during create", "[session_registry][c11][concurrent]") {
  SessionRegistry registry;
  std::atomic<bool> running{true};

  // Writer thread creates sessions
  std::thread writer([&]() {
    for (int i = 0; i < 50; ++i) {
      SessionConfig cfg;
      cfg.name = "list-test-" + std::to_string(i);
      registry.create_session(cfg);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    running = false;
  });

  // Reader thread lists sessions while writes happen
  int successful_lists = 0;
  while (running) {
    auto ids = registry.list_sessions();
    successful_lists++;
    REQUIRE(ids.size() <= 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  writer.join();
  REQUIRE(successful_lists > 0);
  REQUIRE(registry.list_sessions().size() == 50);
}