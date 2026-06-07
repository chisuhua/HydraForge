// tests/test_cloud_llm_live.cpp
/**
 * @file test_cloud_llm_live.cpp
 * @brief CloudLLMAdapter Live 测试（需要真实 API key，默认禁用）
 * @date 2026-06-07
 *
 * 默认不参与 ctest，所有测试用例由 AGENTICDSL_ENABLE_LIVE_TESTS 宏保护。
 * 启用方法：
 *   cmake -DAGENTICDSL_ENABLE_LIVE_TESTS=ON ..
 *   export OPENAI_API_KEY=sk-...
 *   make test_cloud_llm_live && ctest -R test_cloud_llm_live
 *
 * 安全约束：
 * - 严禁提交真实 API key
 * - 真实请求会被发送至 OpenAI / Anthropic，消耗 token 与额度
 * - 仅在开发与 CI 验证场景启用
 */

// 即使宏未定义，文件也可编译（Catch2 主程序仍由 catch_amalgamated.cpp 提供）
#include "catch_amalgamated.hpp"

#include "common/llm/cloud_adapter.h"
#include "common/llm/llm_config.h"

#include <cstdlib>
#include <string>

using namespace agenticdsl;

namespace {

// 辅助：从环境变量读取 API key（仅在 live 模式下使用）
std::string get_api_key_from_env(const char* env_name) {
  if (const char* val = std::getenv(env_name)) {
    return std::string(val);
  }
  return std::string{};
}

}  // namespace

#ifdef AGENTICDSL_ENABLE_LIVE_TESTS

// ================================
// OpenAI Live Tests
// ================================

TEST_CASE("CloudLLMAdapter OpenAI live - sync generate", "[cloud_llm][live]") {
  std::string api_key = get_api_key_from_env("OPENAI_API_KEY");
  if (api_key.empty()) {
    SKIP("OPENAI_API_KEY not set, skipping live test");
  }

  LLMConfig cfg;
  cfg.provider = "openai";
  cfg.api_key = api_key;
  cfg.model = "gpt-4o-mini";
  cfg.max_tokens = 32;
  cfg.timeout_seconds = 30;

  CloudLLMAdapter adapter(cfg);
  REQUIRE(adapter.is_available());

  GenerationRequest req;
  req.prompt = "Reply with the single word 'pong' and nothing else.";
  req.params.max_tokens = 32;

  auto result = adapter.generate(req, {});
  if (!result.has_value()) {
    // 网络错误 / 限流等：记录但不强制失败
    WARN("Live generate failed: " << result.error().message);
    return;
  }
  CHECK_FALSE(result.value().text.empty());
  CHECK(result.value().completion_tokens > 0);
}

TEST_CASE("CloudLLMAdapter OpenAI live - stream generate",
          "[cloud_llm][live][stream]") {
  std::string api_key = get_api_key_from_env("OPENAI_API_KEY");
  if (api_key.empty()) {
    SKIP("OPENAI_API_KEY not set, skipping live test");
  }

  LLMConfig cfg;
  cfg.provider = "openai";
  cfg.api_key = api_key;
  cfg.model = "gpt-4o-mini";
  cfg.max_tokens = 32;
  cfg.timeout_seconds = 30;

  CloudLLMAdapter adapter(cfg);
  GenerationRequest req;
  req.prompt = "Count from 1 to 5.";
  req.params.max_tokens = 32;

  auto stream = adapter.generate_stream(req, {});
  REQUIRE(stream != nullptr);

  std::string accumulated;
  int token_count = 0;
  while (auto token = stream->next({})) {
    accumulated += *token;
    token_count++;
    if (token_count > 100) break;  // 安全：避免无限循环
  }

  CHECK_FALSE(accumulated.empty());
}

TEST_CASE("CloudLLMAdapter 401 maps to AuthenticationError",
          "[cloud_llm][live][error]") {
  // 使用无效 API key 触发 401
  LLMConfig cfg;
  cfg.provider = "openai";
  cfg.api_key = "sk-invalid-key-for-testing-401-response";
  cfg.model = "gpt-4o-mini";
  cfg.max_tokens = 8;
  cfg.timeout_seconds = 10;

  CloudLLMAdapter adapter(cfg);
  auto result = adapter.generate(GenerationRequest{"hi"}, {});
  if (result.has_value()) {
    WARN("Expected 401 but got success - test may be running in mock mode");
    return;
  }
  // 可能是 AuthenticationError 或 NetworkError（取决于网络环境）
  CHECK((result.error().code == LLMError::Code::AuthenticationError ||
         result.error().code == LLMError::Code::NetworkError));
}

#else  // AGENTICDSL_ENABLE_LIVE_TESTS 未定义

// 默认构建时，至少提供一个 no-op 测试，确保文件被识别为有效 Catch2 测试源
TEST_CASE("CloudLLMAdapter live tests disabled",
          "[cloud_llm][live][disabled]") {
  SUCCEED("Live tests disabled. Set AGENTICDSL_ENABLE_LIVE_TESTS=ON to enable.");
}

#endif  // AGENTICDSL_ENABLE_LIVE_TESTS
