#include <catch_amalgamated.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "agenticdsl/contract/event_builder.h"
#include "common/llm/llm_config.h"
#include "common/llm/llm_types.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/mock_provider.h"
#include "common/policy/approval_handler.h"
#include "common/policy/execution_policy.h"
#include "common/tools/registry.h"

#include "provider_agent.h"

using namespace agenticdsl;
using namespace pdk_provider_agent;
using nlohmann::json;

TEST_CASE("provider/switch switches factory default atomically",
          "[provider][switch]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "p-a", [](const LLMConfig&) { return std::make_unique<MockLLMProvider>(); }));
  REQUIRE(factory.register_dynamic(
      "p-b", [](const LLMConfig&) { return std::make_unique<MockLLMProvider>(); }));

  const auto r1 = invoke_switch_tool(factory, "p-b");
  REQUIRE(r1["ok"] == true);
  CHECK(factory.current_default() == "p-b");

  LLMConfig empty;
  empty.provider.clear();
  auto p = factory.create(empty);
  REQUIRE(p != nullptr);
  CHECK(dynamic_cast<MockLLMProvider*>(p.get()) != nullptr);
}

TEST_CASE("provider/switch rejects unknown provider without mutation",
          "[provider][switch]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "p-a", [](const LLMConfig&) { return std::make_unique<MockLLMProvider>(); }));
  REQUIRE(factory.switch_default("p-a"));

  const auto before = factory.current_default();
  const auto r = invoke_switch_tool(factory, "p-missing");
  bool ok = r["ok"].get<bool>();
  CHECK_FALSE(ok);
  CHECK(r["error_code"] == "unknown-provider");
  CHECK(factory.current_default() == before);
}

TEST_CASE("concurrent switch + create converges to a single stable default",
          "[provider][switch][thread]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "p-a", [](const LLMConfig&) { return std::make_unique<MockLLMProvider>(); }));
  REQUIRE(factory.register_dynamic(
      "p-b", [](const LLMConfig&) { return std::make_unique<MockLLMProvider>(); }));

  std::atomic<int> a_wins{0}, b_wins{0};
  std::vector<std::thread> ts;
  for (int i = 0; i < 32; ++i) {
    ts.emplace_back([&, i]() {
      const std::string target = (i % 2 == 0) ? "p-a" : "p-b";
      const auto r = invoke_switch_tool(factory, target);
      if (r["ok"] == true) {
        if (factory.current_default() == "p-a") a_wins.fetch_add(1);
        else b_wins.fetch_add(1);
      }
    });
  }
  for (auto& t : ts) t.join();

  CHECK((a_wins.load() + b_wins.load()) == 32);
  CHECK((factory.current_default() == "p-a" || factory.current_default() == "p-b"));
}
