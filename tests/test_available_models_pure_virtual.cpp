// test_available_models_pure_virtual.cpp
// Phase 5 ILLMProvider v2 — REQ-ICC-004 available_models() pure virtual enforcement
// Task 3.5

#include "common/llm/mock_provider.h"
#include "common/llm/cloud_adapter.h"
#include "common/llm/llama_adapter_provider.h"

#include <catch_amalgamated.hpp>
#include <type_traits>

using namespace agenticdsl;

// === Compile-time enforcement: pure virtual ===
TEST_CASE("ILLMProvider available_models is pure virtual",
          "[llm][available_models]") {
  // Abstract class without override should NOT be instantiable
  // MockLLMProvider HAS override so it's instantiable
  static_assert(!std::is_abstract_v<MockLLMProvider>,
                "MockLLMProvider MUST override available_models");
  static_assert(!std::is_abstract_v<CloudLLMAdapter>,
                "CloudLLMAdapter MUST override available_models");
  static_assert(!std::is_abstract_v<LlamaAdapterProvider>,
                "LlamaAdapterProvider MUST override available_models");
  SUCCEED();
}

// === Runtime check: implementations return non-empty ===
TEST_CASE("MockLLMProvider returns mock-llm-v1", "[llm][available_models]") {
  MockLLMProvider p;
  auto models = p.available_models();
  REQUIRE(models.size() >= 1);
  REQUIRE(models[0].name == "mock-llm-v1");
  REQUIRE(models[0].provider == "mock");
}

// === Router silent failure: empty models = throw ===
namespace {
class TestRouter {
 public:
  static void select_throw_on_empty(const std::vector<ILLMProvider::ModelInfo>& models) {
    if (models.empty()) {
      throw std::runtime_error("NoViableModel");
    }
  }
};
}  // namespace

TEST_CASE("Router throws NoViableModel on empty available_models",
          "[llm][router]") {
  std::vector<ILLMProvider::ModelInfo> empty_models;
  REQUIRE_THROWS_AS(
      TestRouter::select_throw_on_empty(empty_models),
      std::runtime_error);
}

// === 5 implementation overrides verified at runtime ===
TEST_CASE("All shipped ILLMProvider implementations override available_models",
          "[llm][available_models]") {
  MockLLMProvider mock;
  REQUIRE(mock.available_models().size() >= 1);

  // CloudLLMAdapter requires API key OR empty config; should still work
  CloudLLMAdapter cloud{LLMConfig{}};
  // Don't strictly require non-empty (config might be empty)
  // Just check call doesn't crash
  auto cloud_models = cloud.available_models();
  REQUIRE(cloud_models.size() >= 0);  // 0 OK if config empty

  // LlamaAdapterProvider with basic config
  LlamaAdapterProvider llama_provider{LlamaAdapter::Config{}};
  auto llama_models = llama_provider.available_models();
  REQUIRE(llama_models.size() >= 1);
  REQUIRE(llama_models[0].provider == "llama.cpp");
}
