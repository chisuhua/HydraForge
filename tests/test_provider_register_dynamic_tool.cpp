#include <catch_amalgamated.hpp>

#include <memory>

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/iprovider_factory.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"
#include "common/policy/approval_handler.h"
#include "common/policy/execution_policy.h"
#include "common/tools/registry.h"

#include "provider_agent.h"

using namespace agenticdsl;
using namespace pdk_provider_agent;
using namespace nlohmann;
using json = nlohmann::json;

json valid_input() {
  json j;
  j["name"] = "runtime-provider";
  j["backend"] = "mock";
  j["api_url"] = "http://runtime";
  j["models"] = json::array();
  j["models"].push_back(json{{"id", "model-a"}});
  return j;
}

TEST_CASE("register_dynamic tool registers factory callback and provider",
          "[provider][register]") {
  LLMProviderFactory factory;
  ProviderRegistry registry;

  const auto r = invoke_register_dynamic_tool(factory, registry, valid_input());
  REQUIRE(r["ok"] == true);
  CHECK(factory.has_dynamic("runtime-provider"));
  CHECK(registry.list_providers().size() == 1);

  LLMConfig config;
  config.provider = "runtime-provider";
  REQUIRE(factory.create(config) != nullptr);
}

TEST_CASE("register_dynamic rejects invalid and duplicate definitions without mutation",
          "[provider][register]") {
  LLMProviderFactory factory;
  ProviderRegistry registry;

  const json invalid_args = [&](){
    json j;
    j["name"] = "";
    j["backend"] = "unsupported";
    j["models"] = json::array();
    return j;
  }();

  const auto invalid = invoke_register_dynamic_tool(factory, registry, invalid_args);
  bool invalid_ok = invalid["ok"].get<bool>();
  CHECK_FALSE(invalid_ok);
  CHECK(invalid["error_code"] == "validation");
  CHECK(factory.dynamic_names().empty());
  CHECK(registry.list_providers().empty());

  REQUIRE(invoke_register_dynamic_tool(factory, registry, valid_input())["ok"] == true);
  const auto dup = invoke_register_dynamic_tool(factory, registry, valid_input());
  bool dup_ok = dup["ok"].get<bool>();
  CHECK_FALSE(dup_ok);
  CHECK(dup["error_code"] == "duplicate-provider");
  CHECK(factory.dynamic_names().size() == 1);
}
