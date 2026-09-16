// examples/pdk_chat_demo/tests/test_e2e_real_llm_generate_subgraph.cpp
// chat-real-llm-coverage Phase C.2: 真实 LLM GenerateSubGraph 验证

#include "catch_amalgamated.hpp"
#include "test_helpers/real_llm_env.h"
#include <common/llm/llm_config.h>
#include <common/llm/llm_provider_factory.h>
#include <common/llm/llm_types.h>
#include <iostream>
#include <memory>
#include <string>

using namespace agenticdsl;
using pdk_chat_demo::testing::real_llm_config;
using pdk_chat_demo::testing::require_real_llm_env;

namespace {

bool should_skip() {
  if (const char* skip = std::getenv("HYDRAFORGE_SKIP_REAL_LLM");
      skip && std::string(skip) == "1") {
    return true;
  }
  return false;
}

}  // namespace

TEST_CASE("Real LLM: GenerateSubGraph-style prompt 'compute 2+3'",
          "[e2e][realllm][chat][phase-c][c2]") {
  require_real_llm_env();
  if (should_skip()) {
    SUCCEED("skipped: HYDRAFORGE_SKIP_REAL_LLM=1");
    return;
  }
  auto cfg = real_llm_config();
  LLMConfig llm_cfg;
  llm_cfg.provider = cfg.provider;
  llm_cfg.model = cfg.model;
  llm_cfg.api_url = cfg.api_url;
  llm_cfg.api_endpoint = cfg.api_endpoint;
  llm_cfg.api_key = cfg.api_key;
  llm_cfg.max_tokens = 1024;
  llm_cfg.temperature = 0.2f;
  llm_cfg.timeout_seconds = 30;
  llm_cfg.max_retries = 1;

  LLMProviderFactory factory;
  auto provider = factory.create(llm_cfg);
  REQUIRE(provider != nullptr);
  GenerationRequest req("Generate an AgenticDSL subgraph for computing 2+3");
  req.params.model = cfg.model;
  auto result = provider->generate(req, std::stop_token{});
  REQUIRE(result.has_value());
  REQUIRE_FALSE(result.value().text.empty());
  REQUIRE(result.value().completion_tokens > 0);
  std::cerr << "[C.2.2] model=" << cfg.model
            << " tokens=" << result.value().completion_tokens << std::endl;
}

TEST_CASE("Real LLM: GenerateSubGraph complex prompt 'compute factorial of 5'",
          "[e2e][realllm][chat][phase-c][c2-multi]") {
  require_real_llm_env();
  if (should_skip()) {
    SUCCEED("skipped: HYDDAFORGE_SKIP_REAL_LLM=1");
    return;
  }
  auto cfg = real_llm_config();
  LLMConfig llm_cfg;
  llm_cfg.provider = cfg.provider;
  llm_cfg.model = cfg.model;
  llm_cfg.api_url = cfg.api_url;
  llm_cfg.api_endpoint = cfg.api_endpoint;
  llm_cfg.api_key = cfg.api_key;
  llm_cfg.max_tokens = 2048;
  llm_cfg.temperature = 0.2f;
  llm_cfg.timeout_seconds = 60;
  llm_cfg.max_retries = 1;

  LLMProviderFactory factory;
  auto provider = factory.create(llm_cfg);
  REQUIRE(provider != nullptr);
  GenerationRequest req("Generate an AgenticDSL subgraph that computes factorial of 5 step by step");
  req.params.model = cfg.model;
  auto result = provider->generate(req, std::stop_token{});
  REQUIRE(result.has_value());
  REQUIRE_FALSE(result.value().text.empty());
  REQUIRE(result.value().completion_tokens > 10);
  std::cerr << "[C.2.3] factorial tokens=" << result.value().completion_tokens << std::endl;
}
