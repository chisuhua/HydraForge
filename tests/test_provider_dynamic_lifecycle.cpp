#include <catch_amalgamated.hpp>

#include <memory>

#include "common/llm/llm_config.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"

using namespace agenticdsl;

TEST_CASE("dynamic callback survives temporary config destruction",
          "[provider_factory][dynamic][lifetime]") {
  LLMProviderFactory factory;
  std::shared_ptr<std::string> captured = std::make_shared<std::string>("from-outer-scope");
  REQUIRE(factory.register_dynamic(
      "shared-state",
      [captured](const LLMConfig&) {
        (void)captured;
        return std::make_unique<MockLLMProvider>();
      }));

  {
    // Temporary value captured in the callback — destroyed at scope end.
    const std::string temp_value = "gone-after-scope";
    REQUIRE(factory.register_dynamic(
        "value-captured",
        [temp_value](const LLMConfig&) {
          (void)temp_value;  // owned by value; survives caller scope
          return std::make_unique<MockLLMProvider>();
        }));
  }  // temp_value destructor runs here

  LLMConfig cfg_a;
  cfg_a.provider = "shared-state";
  REQUIRE(factory.create(cfg_a) != nullptr);

  LLMConfig cfg_b;
  cfg_b.provider = "value-captured";
  REQUIRE(factory.create(cfg_b) != nullptr);
}

TEST_CASE("provider construction happens after the factory lock is released",
          "[provider_factory][dynamic][lifetime]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "throw-inside-callback",
      [](const LLMConfig&) -> std::unique_ptr<ILLMProvider> {
        throw std::runtime_error("boom");
      }));

  LLMConfig cfg;
  cfg.provider = "throw-inside-callback";
  REQUIRE_THROWS_AS(factory.create(cfg), std::runtime_error);

  // After construction failed, the factory must still be functional.
  CHECK(factory.has_dynamic("throw-inside-callback"));
  REQUIRE(factory.switch_default("throw-inside-callback"));
  LLMConfig empty;
  empty.provider.clear();
  REQUIRE_THROWS_AS(factory.create(empty), std::runtime_error);
}
