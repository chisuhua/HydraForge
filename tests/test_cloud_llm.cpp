// tests/test_cloud_llm.cpp
/**
 * @file test_cloud_llm.cpp
 * @brief CloudLLMAdapter + MockLLMProvider 单元测试
 * @date 2026-06-07
 *
 * 覆盖用例（11+）：
 * - LLMConfig 解析（env / file / direct）
 * - CloudLLMAdapter 构造 / 状态查询 / 配置管理
 * - MockLLMProvider 固定响应 / 队列响应 / 错误注入 / 延迟
 * - MockLLMProvider 流式 token 返回 / 取消支持
 * - 调用历史记录
 */

#include "catch_amalgamated.hpp"

#include "common/llm/cloud_adapter.h"
#include "common/llm/llm_config.h"
#include "common/llm/mock_provider.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <stop_token>

using namespace agenticdsl;

// ================================
// LLMConfig 解析
// ================================

TEST_CASE("LLMConfig resolve_api_key direct field", "[cloud_llm][config]") {
  LLMConfig cfg;
  cfg.api_key = "sk-direct-key";
  CHECK(cfg.resolve_api_key() == "sk-direct-key");
}

TEST_CASE("LLMConfig resolve_api_key from env", "[cloud_llm][config]") {
  // 写入环境变量（跨平台兼容：setenv / putenv）
  setenv("AGENTICDSL_TEST_KEY", "sk-from-env", 1);
  LLMConfig cfg;
  cfg.api_key_env = "AGENTICDSL_TEST_KEY";
  CHECK(cfg.resolve_api_key() == "sk-from-env");
  unsetenv("AGENTICDSL_TEST_KEY");
}

TEST_CASE("LLMConfig resolve_api_key from file", "[cloud_llm][config]") {
  // 写入临时文件
  std::string tmp_path = "/tmp/agenticdsl_test_key.txt";
  {
    std::ofstream out(tmp_path);
    out << "sk-from-file\n";
  }
  LLMConfig cfg;
  cfg.api_key_file = tmp_path;
  CHECK(cfg.resolve_api_key() == "sk-from-file");
  std::remove(tmp_path.c_str());
}

TEST_CASE("LLMConfig resolve_api_key env takes priority over direct",
          "[cloud_llm][config]") {
  setenv("AGENTICDSL_TEST_KEY2", "sk-env-priority", 1);
  LLMConfig cfg;
  cfg.api_key = "sk-direct-fallback";
  cfg.api_key_env = "AGENTICDSL_TEST_KEY2";
  CHECK(cfg.resolve_api_key() == "sk-env-priority");
  unsetenv("AGENTICDSL_TEST_KEY2");
}

TEST_CASE("LLMConfig resolve_api_key returns empty when none set",
          "[cloud_llm][config]") {
  LLMConfig cfg;
  cfg.api_key_env = "AGENTICDSL_NONEXISTENT_KEY_12345";
  cfg.api_key_file = "/nonexistent/path";
  CHECK(cfg.resolve_api_key().empty());
}

// ================================
// CloudLLMAdapter 状态查询
// ================================

TEST_CASE("CloudLLMAdapter constructs with valid config", "[cloud_llm]") {
  LLMConfig cfg;
  cfg.provider = "openai";
  cfg.api_key = "sk-test";
  cfg.model = "gpt-4o-mini";
  CloudLLMAdapter adapter(cfg);
  CHECK(adapter.provider_name() == "openai");
  CHECK(adapter.model_name() == "gpt-4o-mini");
  CHECK(adapter.is_available() == true);
}

TEST_CASE("CloudLLMAdapter rejects empty api_key", "[cloud_llm]") {
  LLMConfig cfg;
  cfg.provider = "openai";
  cfg.api_key = "";  // 显式置空
  CloudLLMAdapter adapter(cfg);
  CHECK(adapter.is_available() == false);
}

TEST_CASE("CloudLLMAdapter provider_name returns config.provider",
          "[cloud_llm]") {
  LLMConfig cfg;
  cfg.api_key = "sk-test";
  cfg.provider = "anthropic";
  CloudLLMAdapter adapter(cfg);
  CHECK(adapter.provider_name() == "anthropic");
}

TEST_CASE("CloudLLMAdapter update_config swaps config", "[cloud_llm]") {
  LLMConfig cfg;
  cfg.api_key = "sk-initial";
  cfg.model = "gpt-4o-mini";
  cfg.provider = "openai";
  CloudLLMAdapter adapter(cfg);
  CHECK(adapter.model_name() == "gpt-4o-mini");

  LLMConfig new_cfg = cfg;
  new_cfg.model = "gpt-4o";
  adapter.update_config(new_cfg);
  CHECK(adapter.model_name() == "gpt-4o");
}

// ================================
// MockLLMProvider 同步模式
// ================================

TEST_CASE("MockLLMProvider fixed response", "[cloud_llm][mock]") {
  MockLLMProvider mock;
  mock.set_fixed_response("hello from mock");

  auto result = mock.generate(GenerationRequest{"test"}, {});
  REQUIRE(result.has_value());
  CHECK(result.value().text == "hello from mock");
}

TEST_CASE("MockLLMProvider enqueue responses sequentially",
          "[cloud_llm][mock]") {
  MockLLMProvider mock;
  mock.enqueue_response("first");
  mock.enqueue_response("second");
  mock.enqueue_response("third");

  auto r1 = mock.generate(GenerationRequest{""}, {});
  auto r2 = mock.generate(GenerationRequest{""}, {});
  auto r3 = mock.generate(GenerationRequest{""}, {});

  REQUIRE(r1.has_value());
  REQUIRE(r2.has_value());
  REQUIRE(r3.has_value());
  CHECK(r1.value().text == "first");
  CHECK(r2.value().text == "second");
  CHECK(r3.value().text == "third");
}

TEST_CASE("MockLLMProvider falls back to fixed when queue empty",
          "[cloud_llm][mock]") {
  MockLLMProvider mock;
  mock.set_fixed_response("fallback");
  // 队列为空，固定响应生效
  auto r = mock.generate(GenerationRequest{""}, {});
  REQUIRE(r.has_value());
  CHECK(r.value().text == "fallback");
}

TEST_CASE("MockLLMProvider simulates error", "[cloud_llm][mock]") {
  MockLLMProvider mock;
  mock.set_simulate_error(LLMError::Code::RateLimited, "test 429");

  auto r = mock.generate(GenerationRequest{""}, {});
  CHECK_FALSE(r.has_value());
  CHECK(r.error().code == LLMError::Code::RateLimited);
  CHECK(r.error().message == "test 429");
}

TEST_CASE("MockLLMProvider records call history", "[cloud_llm][mock]") {
  MockLLMProvider mock;
  mock.set_fixed_response("ok");
  CHECK(mock.call_count() == 0);

  mock.generate(GenerationRequest{"prompt 1"}, {});
  mock.generate(GenerationRequest{"prompt 2"}, {});

  CHECK(mock.call_count() == 2);
  CHECK(mock.call_history()[0].prompt == "prompt 1");
  CHECK(mock.call_history()[1].prompt == "prompt 2");

  mock.clear_history();
  CHECK(mock.call_count() == 0);
}

// ================================
// MockLLMProvider 流式
// ================================

TEST_CASE("MockLLMProvider stream returns configured tokens",
          "[cloud_llm][mock][stream]") {
  MockLLMProvider mock;
  mock.set_stream_tokens({"Hello", ", ", "world", "!"});

  auto stream = mock.generate_stream(GenerationRequest{""}, {});
  REQUIRE(stream != nullptr);
  CHECK(stream->is_active() == true);

  CHECK(*stream->next({}) == "Hello");
  CHECK(*stream->next({}) == ", ");
  CHECK(*stream->next({}) == "world");
  CHECK(*stream->next({}) == "!");

  auto end = stream->next({});
  CHECK_FALSE(end.has_value());
  CHECK(stream->is_active() == false);
}

TEST_CASE("MockLLMProvider stream stops on cancellation",
          "[cloud_llm][mock][stream]") {
  MockLLMProvider mock;
  mock.set_stream_tokens({"a", "b", "c"});

  std::stop_source src;
  auto stream = mock.generate_stream(GenerationRequest{""}, src.get_token());

  // 第一个 token 正常
  auto t1 = stream->next(src.get_token());
  CHECK(t1.has_value());

  // 请求停止
  src.request_stop();

  // 后续 next 返回 nullopt
  auto t2 = stream->next(src.get_token());
  CHECK_FALSE(t2.has_value());
  CHECK(stream->is_active() == false);
}

// ================================
// CloudLLMAdapter 可用性 + 错误传播
// ================================

TEST_CASE("CloudLLMAdapter generate returns error when not available",
          "[cloud_llm]") {
  LLMConfig cfg;
  cfg.api_key = "";  // 不可用
  CloudLLMAdapter adapter(cfg);

  auto r = adapter.generate(GenerationRequest{"hello"}, {});
  CHECK_FALSE(r.has_value());
  CHECK(r.error().code == LLMError::Code::InvalidRequest);
}

TEST_CASE("CloudLLMAdapter is_available respects env", "[cloud_llm]") {
  setenv("AGENTICDSL_TEST_KEY3", "sk-via-env", 1);
  LLMConfig cfg;
  cfg.api_key_env = "AGENTICDSL_TEST_KEY3";
  CloudLLMAdapter adapter(cfg);
  CHECK(adapter.is_available() == true);
  unsetenv("AGENTICDSL_TEST_KEY3");
}

// ================================
// 集成：MockLLMProvider 作为 ILLMProvider 基类
// ================================

TEST_CASE("MockLLMProvider can be used via ILLMProvider base pointer",
          "[cloud_llm][polymorphism]") {
  // 构造时通过具体类型设置
  auto mock = std::make_unique<MockLLMProvider>();
  mock->set_fixed_response("polymorphic");

  // 之后用基类指针调用 ILLMProvider 接口
  std::unique_ptr<ILLMProvider> provider = std::move(mock);
  auto r = provider->generate(GenerationRequest{"x"}, {});
  REQUIRE(r.has_value());
  CHECK(r.value().text == "polymorphic");
}
