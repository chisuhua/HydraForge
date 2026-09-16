// examples/pdk_chat_demo/tests/test_e2e_real_llm_errors.cpp
// chat-real-llm-coverage Phase G: 真实 LLM 错误处理

#include "catch_amalgamated.hpp"
#include "test_helpers/real_llm_env.h"
#include <common/llm/llm_config.h>
#include <common/llm/llm_provider_factory.h>
#include <common/llm/llm_types.h>
#include <cstdlib>
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

class ScopedEnv {
 public:
  ScopedEnv(const char* key, const char* value) : key_(key) {
    const char* orig = std::getenv(key);
    had_orig_ = (orig != nullptr);
    if (had_orig_) orig_ = orig;
    setenv(key, value, 1);
  }
  ~ScopedEnv() {
    if (had_orig_) {
      setenv(key_.c_str(), orig_.c_str(), 1);
    } else {
      unsetenv(key_.c_str());
    }
  }
 private:
  std::string key_;
  std::string orig_;
  bool had_orig_;
};

}  // namespace

TEST_CASE("Real LLM: bad API key → AuthenticationError",
          "[e2e][realllm][chat][phase-g][g2]") {
  require_real_llm_env();
  if (should_skip()) {
    SUCCEED("skipped: HYDRAFORGE_SKIP_REAL_LLM=1");
    return;
  }
  auto cfg = real_llm_config();
  ScopedEnv bad_key(cfg.env_used.c_str(), "invalid_key_for_test_xxxxxxxxxxxxx");
  LLMConfig llm_cfg;
  llm_cfg.provider = cfg.provider;
  llm_cfg.model = cfg.model;
  llm_cfg.api_url = cfg.api_url;
  llm_cfg.api_endpoint = cfg.api_endpoint;
  llm_cfg.api_key = "invalid_key_for_test_xxxxxxxxxxxxx";
  llm_cfg.max_tokens = 64;
  llm_cfg.timeout_seconds = 15;
  llm_cfg.max_retries = 0;
  LLMProviderFactory factory;
  auto provider = factory.create(llm_cfg);
  REQUIRE(provider != nullptr);
  GenerationRequest req("Hello");
  req.params.model = cfg.model;
  auto result = provider->generate(req, std::stop_token{});
  REQUIRE_FALSE(result.has_value());
  if (!result.has_value()) {
    REQUIRE(result.error().code == LLMError::Code::AuthenticationError);
  }
}

TEST_CASE("Real LLM: unreachable API URL → NetworkError",
          "[e2e][realllm][chat][phase-g][g4]") {
  require_real_llm_env();
  if (should_skip()) {
    SUCCEED("skipped: HYDRAFORGE_SKIP_REAL_LLM=1");
    return;
  }
  LLMConfig llm_cfg;
  llm_cfg.provider = "deepseek";
  llm_cfg.model = "deepseek-chat";
  llm_cfg.api_url = "https://nonexistent.invalid.host.for.test:9999";
  llm_cfg.api_endpoint = "/chat/completions";
  llm_cfg.api_key = "fake_key";
  llm_cfg.max_tokens = 64;
  llm_cfg.timeout_seconds = 5;
  llm_cfg.max_retries = 0;
  LLMProviderFactory factory;
  auto provider = factory.create(llm_cfg);
  REQUIRE(provider != nullptr);
  GenerationRequest req("Hello");
  req.params.model = "deepseek-chat";
  auto result = provider->generate(req, std::stop_token{});
  REQUIRE_FALSE(result.has_value());
  if (!result.has_value()) {
    REQUIRE(result.error().code == LLMError::Code::NetworkError);
  }
}
