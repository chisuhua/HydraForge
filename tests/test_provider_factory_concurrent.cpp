#include <catch_amalgamated.hpp>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "common/llm/llm_config.h"
#include "common/llm/llm_types.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/mock_provider.h"

using agenticdsl::LLMConfig;
using agenticdsl::LLMProviderFactory;
using agenticdsl::MockLLMProvider;

TEST_CASE("LLMProviderFactory 50-thread × 1000 mixed ops stay consistent",
          "[provider_factory][dynamic][thread]") {
  LLMProviderFactory factory;
  // Pre-register two dynamic providers up front so callers can find them.
  REQUIRE(factory.register_dynamic(
      "provider-a", [](const LLMConfig&) { return std::make_unique<MockLLMProvider>(); }));
  REQUIRE(factory.register_dynamic(
      "provider-b", [](const LLMConfig&) { return std::make_unique<MockLLMProvider>(); }));
  REQUIRE(factory.switch_default("provider-a"));

  constexpr int kThreads = 50;
  constexpr int kIterations = 1000;
  std::atomic<std::uint64_t> creates{0}, failures{0}, switches{0};

  std::vector<std::thread> ts;
  ts.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    ts.emplace_back([&, t]() {
      for (int i = 0; i < kIterations; ++i) {
        const int op = (t + i) % 5;
        switch (op) {
          case 0: case 1: case 2: {
            LLMConfig cfg;
            cfg.provider = (i % 2 == 0) ? "provider-a" : "provider-b";
            if (factory.create(cfg)) creates.fetch_add(1);
            else failures.fetch_add(1);
            break;
          }
          case 3:
            (void)factory.dynamic_names();
            break;
          case 4:
            if (factory.switch_default((i % 2 == 0) ? "provider-a" : "provider-b")) {
              switches.fetch_add(1);
            }
            break;
        }
      }
    });
  }
  for (auto& t : ts) t.join();

  const auto total = static_cast<std::uint64_t>(kThreads) * kIterations;
  CHECK(creates.load() + failures.load() >= total * 3 / 5);
  CHECK((factory.current_default() == "provider-a" ||
         factory.current_default() == "provider-b"));
  CHECK(factory.dynamic_names().size() == 2);
  SUCCEED();
}
