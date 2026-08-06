#include <catch_amalgamated.hpp>
#include <stdexcept>

#include "provider_agent.h"

using namespace pdk_provider_agent;
using nlohmann::json;

TEST_CASE("refresh commits valid catalog and marks removed models",
          "[provider][refresh]") {
  ProviderRegistry registry;
  registry.seed_for_test({
      {"demo", ProviderInfo{"demo", "http://demo", "/models", "",
                             {{"model-a", ModelConfig{"model-a"}},
                              {"model-b", ModelConfig{"model-b"}}}}}});
  registry.set_refresh_transport_for_test(
      [](const ProviderInfo&) { return json{{"data", json::array({json{{"id", "model-a"}}})}}; });

  const auto result = registry.refresh("demo");
  REQUIRE(result.ok);
  CHECK(result.provider == "demo");
  CHECK(result.added.empty());
  REQUIRE(result.removed.size() == 1);
  CHECK(result.removed.front() == "model-b");
  CHECK(result.model_count == 1);
  CHECK_FALSE(result.last_refresh.empty());
  CHECK(registry.removed_models("demo").at("model-b"));
  CHECK(registry.list_models("demo") == std::vector<std::string>{"model-a"});
}

TEST_CASE("refresh failure preserves prior catalog and surfaces error_code",
          "[provider][refresh]") {
  ProviderRegistry registry;
  registry.seed_for_test(
      {{"demo", ProviderInfo{"demo", "http://demo", "/models", "",
                              {{"model-a", ModelConfig{"model-a"}}}}}});
  registry.set_refresh_transport_for_test(
      [](const ProviderInfo&) -> json { throw std::runtime_error("timeout"); });

  const auto result = registry.refresh("demo");
  CHECK_FALSE(result.ok);
  CHECK(result.error_code == "retryable");
  CHECK_FALSE(result.warning.empty());
  CHECK(registry.list_models("demo") == std::vector<std::string>{"model-a"});
  CHECK(registry.removed_models("demo").empty());
}

TEST_CASE("refresh rejects invalid schema without mutating catalog",
          "[provider][refresh]") {
  ProviderRegistry registry;
  registry.seed_for_test(
      {{"demo", ProviderInfo{"demo", "http://demo", "/models", "",
                              {{"model-a", ModelConfig{"model-a"}}}}}});
  registry.set_refresh_transport_for_test([](const ProviderInfo&) {
    return json{{"data", json::array({json{{"name", "no-id-field"}}})}};
  });

  const auto result = registry.refresh("demo");
  CHECK_FALSE(result.ok);
  CHECK(result.error_code == "validation");
  CHECK(registry.list_models("demo") == std::vector<std::string>{"model-a"});
}

TEST_CASE("refresh returns unknown-provider without calling transport",
          "[provider][refresh]") {
  ProviderRegistry registry;
  std::atomic<int> transport_calls{0};
  registry.set_refresh_transport_for_test(
      [&](const ProviderInfo&) -> json {
        transport_calls.fetch_add(1);
        return json::object();
      });

  const auto result = registry.refresh("does-not-exist");
  CHECK_FALSE(result.ok);
  CHECK(result.error_code == "unknown-provider");
  CHECK(transport_calls.load() == 0);
}
