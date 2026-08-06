#include <catch_amalgamated.hpp>

#include <memory>

#include "common/llm/llm_config.h"
#include "common/llm/llm_types.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/mock_provider.h"

#include "provider_agent.h"

using namespace agenticdsl;
using namespace pdk_provider_agent;
using nlohmann::json;

TEST_CASE("End-to-end: register_dynamic → refresh → switch route create()",
          "[provider][integration]") {
  LLMProviderFactory factory;
  ProviderRegistry registry;

  // 1. Register two providers
  json args_a = json::object();
  args_a["name"] = "p-a";
  args_a["backend"] = "mock";
  args_a["api_url"] = "http://a";
  args_a["models"] = json::array({json::object({{"id", "ma"}})});

  json args_b = json::object();
  args_b["name"] = "p-b";
  args_b["backend"] = "mock";
  args_b["api_url"] = "http://b";
  args_b["models"] = json::array({json::object({{"id", "mb"}})});

  REQUIRE(invoke_register_dynamic_tool(factory, registry, args_a)["ok"] == true);
  REQUIRE(invoke_register_dynamic_tool(factory, registry, args_b)["ok"] == true);

  // 2. Refresh verifies catalog shape (in-process transport injected below)
  registry.seed_for_test({
      {"p-a", ProviderInfo{"p-a", "http://a", "/x", "", {{"ma", ModelConfig{"ma"}}}}},
      {"p-b", ProviderInfo{"p-b", "http://b", "/x", "", {{"mb", ModelConfig{"mb"}}}}}});
  registry.set_refresh_transport_for_test(
      [](const ProviderInfo&) { return json{{"data", json::array({json{{"id", "ma"}}})}}; });
  const auto r = registry.refresh("p-a");
  CHECK(r.ok);
  CHECK(registry.list_models("p-a") == std::vector<std::string>{"ma"});

  // 3. Switch default and verify subsequent create() routes correctly
  REQUIRE(invoke_switch_tool(factory, "p-b")["ok"] == true);
  LLMConfig empty;
  empty.provider.clear();
  auto p = factory.create(empty);
  REQUIRE(p != nullptr);
  CHECK(dynamic_cast<MockLLMProvider*>(p.get()) != nullptr);
}
