// tests/test_cancellation_registry.cpp
// CancellationRegistry 单元测试 (Phase B: chat-async-io-cancellation-chain)

#include "catch_amalgamated.hpp"

#include "cancellation_registry.h"

TEST_CASE("CancellationRegistry register/resolve round-trip", "[cancel_registry]") {
  CancellationRegistry reg;
  auto source = std::make_shared<std::stop_source>();
  std::string id = reg.register_source(source);
  REQUIRE(source != nullptr);
  REQUIRE_FALSE(id.empty());
  
  auto token = reg.resolve_token(id);
  REQUIRE(token.stop_possible());
  REQUIRE_FALSE(token.stop_requested());
  
  source->request_stop();
  REQUIRE(token.stop_requested());
}

TEST_CASE("CancellationRegistry resolve unknown id returns empty", "[cancel_registry]") {
  CancellationRegistry reg;
  auto token = reg.resolve_token("nonexistent");
  REQUIRE_FALSE(token.stop_possible());
}

TEST_CASE("CancellationRegistry unregister removes entry", "[cancel_registry]") {
  CancellationRegistry reg;
  auto source = std::make_shared<std::stop_source>();
  std::string id = reg.register_source(source);
  reg.unregister(id);
  REQUIRE_FALSE(reg.resolve_token(id).stop_possible());
}

TEST_CASE("CancellationRegistry resolve_source returns shared_ptr", "[cancel_registry]") {
  CancellationRegistry reg;
  auto source = std::make_shared<std::stop_source>();
  std::string id = reg.register_source(source);

  auto resolved = reg.resolve_source(id);
  REQUIRE(resolved != nullptr);
  REQUIRE(resolved.get() == source.get());

  resolved->request_stop();
  REQUIRE(reg.resolve_token(id).stop_requested());

  REQUIRE(reg.resolve_source("nonexistent") == nullptr);
}
