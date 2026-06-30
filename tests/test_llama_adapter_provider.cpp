// tests/test_llama_adapter_provider.cpp
// Sprint 16 Coverage Backfill: 测试 src/common/llm/llama_adapter_provider.cpp
#include "catch_amalgamated.hpp"
#include "common/llm/llama_adapter_provider.h"
#include "common/llm/llm_types.h"

using namespace agenticdsl;

TEST_CASE("LlamaAdapterProvider constructor with null adapter throws", "[llm][provider]") {
  std::unique_ptr<LlamaAdapter> null_adapter;
  REQUIRE_THROWS_AS(
      LlamaAdapterProvider(std::move(null_adapter)),
      std::invalid_argument);
}

TEST_CASE("LlamaAdapterProvider constructor with Config creates provider", "[llm][provider]") {
  LlamaAdapter::Config config;
  config.api_url = "http://invalid:99999";
  config.n_ctx = 512;

  LlamaAdapterProvider provider(config);
  REQUIRE(provider.underlying() != nullptr);
}

TEST_CASE("LlamaAdapterProvider generate with cancelled token returns Cancelled", "[llm][provider]") {
  LlamaAdapter::Config config;
  config.api_url = "http://invalid:99999";
  LlamaAdapterProvider provider(config);

  GenerationRequest req;
  req.prompt = "test prompt";

  std::stop_source ss;
  ss.request_stop();
  std::stop_token token = ss.get_token();

  auto result = provider.generate(req, token);
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().code == LLMError::Code::Cancelled);
}

TEST_CASE("LlamaAdapterProvider generate with invalid url returns ServerError", "[llm][provider]") {
  LlamaAdapter::Config config;
  config.api_url = "http://invalid-host-12345:99999";
  config.n_ctx = 128;  // 最小 ctx 加快初始化
  LlamaAdapterProvider provider(config);

  GenerationRequest req;
  req.prompt = "test prompt";

  std::stop_token token;
  auto result = provider.generate(req, token);
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().code == LLMError::Code::ServerError);
}

TEST_CASE("LlamaAdapterProvider underlying() returns wrapped adapter", "[llm][provider]") {
  LlamaAdapter::Config config;
  config.api_url = "http://localhost:8080";
  LlamaAdapterProvider provider(config);

  LlamaAdapter* raw = provider.underlying();
  REQUIRE(raw != nullptr);
}