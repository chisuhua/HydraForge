// Phase B Step 5: E2E mid-loop cancellation test
// Verifies end-to-end cancellation chain: MockBlockingProvider observes
// stop_token + CancellationRegistry propagates cancellation through loop_agent.

#include <atomic>
#include <chrono>
#include <memory>
#include <stop_token>
#include <thread>

#include <catch_amalgamated.hpp>
#include <nlohmann/json.hpp>

#include "cancellation_registry.h"
#include "common/llm/llm_types.h"
#include "mock_blocking_provider.h"

using namespace pdk_chat_demo;
using namespace pdk_chat_demo::testing;
using agenticdsl::GenerationRequest;
using agenticdsl::ILLMProvider;
using agenticdsl::LLMError;
using agenticdsl::Result;

TEST_CASE("MockBlockingProvider cancels within 100ms on stop_requested",
          "[cancellation][e2e][mock]") {
  MockBlockingProvider provider(std::chrono::seconds(5));
  std::stop_source source;
  GenerationRequest req;

  std::thread canceller([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    source.request_stop();
  });

  auto start = std::chrono::steady_clock::now();
  auto result = provider.generate(req, source.get_token());
  auto elapsed = std::chrono::steady_clock::now() - start;

  canceller.join();

  CHECK(elapsed < std::chrono::milliseconds(100));
  CHECK_FALSE(result.has_value());
  if (!result.has_value()) {
    CHECK(result.error().code == LLMError::Code::Cancelled);
  }
}

TEST_CASE("MockBlockingProvider returns success when not cancelled within max_block",
          "[cancellation][e2e][mock]") {
  MockBlockingProvider provider(std::chrono::milliseconds(50));
  GenerationRequest req;

  auto result = provider.generate(req, std::stop_token{});

  CHECK(result.has_value());
  if (result.has_value()) {
    CHECK(result.value().text.find("completed") != std::string::npos);
  }
}

TEST_CASE("CancellationRegistry end-to-end token propagation",
          "[cancellation][e2e][registry]") {
  // Simulates ChatSession flow:
  // 1. ChatSession creates stop_source, registers in registry, gets id
  // 2. ChatSession puts cancellation_id in loop_args JSON
  // 3. loop_agent parses id, resolves token via registry
  // 4. loop_agent forwards token to ILLMProvider.generate()
  // 5. MockBlockingProvider observes stop_requested
  // 6. request_stop() propagates from caller to MockBlockingProvider

  CancellationRegistry registry;
  auto caller_source = std::make_shared<std::stop_source>();
  std::string id = registry.register_source(caller_source);

  // Simulate loop_agent resolution from cancellation_id
  auto resolved_token = registry.resolve_token(id);
  REQUIRE(resolved_token.stop_possible());

  // Thread B: simulate request_stop after 30ms
  std::atomic<bool> cancelled{false};
  std::thread request_thread([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    caller_source->request_stop();
    cancelled.store(true);
  });

  // Thread A: simulate loop_agent → MockBlockingProvider with resolved token
  MockBlockingProvider provider(std::chrono::seconds(5));
  GenerationRequest req;

  auto start = std::chrono::steady_clock::now();
  auto result = provider.generate(req, resolved_token);
  auto elapsed = std::chrono::steady_clock::now() - start;

  request_thread.join();

  CHECK(cancelled.load());
  CHECK(elapsed < std::chrono::milliseconds(100));
  CHECK_FALSE(result.has_value());
}

TEST_CASE("Default token never cancels MockBlockingProvider",
          "[cancellation][e2e][mock]") {
  MockBlockingProvider provider(std::chrono::milliseconds(50));
  GenerationRequest req;

  // std::stop_token{} has no associated stop_source — never cancellable
  auto result = provider.generate(req, std::stop_token{});

  CHECK(result.has_value());
}

TEST_CASE("Token identity preserved through registry resolve_source",
          "[cancellation][e2e][registry]") {
  CancellationRegistry registry;
  auto caller_source = std::make_shared<std::stop_source>();
  std::string id = registry.register_source(caller_source);

  // resolve_source returns the same shared_ptr
  auto resolved = registry.resolve_source(id);
  REQUIRE(resolved != nullptr);
  CHECK(resolved.get() == caller_source.get());

  // request_stop on caller_source observable from resolved
  caller_source->request_stop();
  CHECK(resolved->get_token().stop_requested());
}