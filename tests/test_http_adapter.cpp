// tests/test_http_adapter.cpp
// Sprint 16 Coverage Backfill: 测试 src/common/llm/http_adapter.cpp
#include "catch_amalgamated.hpp"
#include "common/llm/http_adapter.h"
#include "common/llm/llm_config.h"
#include "common/llm/llm_types.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <stop_token>

using namespace agenticdsl;

TEST_CASE("HttpLLMAdapter is_available returns true when api_url configured", "[llm][http]") {
  LLMConfig cfg;
  cfg.provider = "http";
  cfg.api_url = "http://127.0.0.1:99999";
  HttpLLMAdapter adapter(cfg);
  REQUIRE(adapter.is_available());
}

TEST_CASE("HttpLLMAdapter provider_name and config accessors", "[llm][http]") {
  LLMConfig cfg;
  cfg.provider = "custom-http";
  cfg.api_url = "http://localhost:8080";

  HttpLLMAdapter adapter(cfg);
  REQUIRE(adapter.provider_name() == "custom-http");
  REQUIRE(adapter.config().api_url == "http://localhost:8080");
}

TEST_CASE("HttpLLMAdapter generate to unreachable host returns NetworkError", "[llm][http]") {
  LLMConfig cfg;
  cfg.provider = "http";
  cfg.api_url = "http://127.0.0.1:1";  // 端口 1 不会被监听
  cfg.timeout_seconds = 2;
  HttpLLMAdapter adapter(cfg);

  GenerationRequest req;
  req.prompt = "test";

  std::stop_token token;
  auto result = adapter.generate(req, token);
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().code == LLMError::Code::NetworkError);
}

TEST_CASE("HttpLLMAdapter with mock server returns 200 result", "[llm][http]") {
  httplib::Server svr;
  svr.Post("/v1/chat/completions", [](const httplib::Request& req, httplib::Response& res) {
    nlohmann::json response = {
        {"choices", nlohmann::json::array({{
            {"message", {{"content", "Hello from mock"}}},
            {"finish_reason", "stop"}
        }})}
    };
    res.set_content(response.dump(), "application/json");
  });

  int port = svr.bind_to_any_port("127.0.0.1");
  REQUIRE(port > 0);
  std::thread server_thread([&svr]() { svr.listen_after_bind(); });

  // 等待 server 启动
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  LLMConfig cfg;
  cfg.provider = "http";
  cfg.api_url = "http://127.0.0.1:" + std::to_string(port);
  HttpLLMAdapter adapter(cfg);

  GenerationRequest req;
  req.prompt = "test";
  std::stop_token token;
  auto result = adapter.generate(req, token);

  svr.stop();
  if (server_thread.joinable()) server_thread.join();

  REQUIRE(result.has_value());
  REQUIRE(result.value().text == "Hello from mock");
  REQUIRE(result.value().finish_reason == "stop");
}

TEST_CASE("HttpLLMAdapter error mapping: 404 → InvalidRequest", "[llm][http]") {
  httplib::Server svr;
  svr.Post("/v1/chat/completions", [](const httplib::Request& req, httplib::Response& res) {
    res.status = 404;
    res.set_content(R"({"error": {"message": "Not Found"}})", "application/json");
  });

  int port = svr.bind_to_any_port("127.0.0.1");
  REQUIRE(port > 0);
  std::thread server_thread([&svr]() { svr.listen_after_bind(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  LLMConfig cfg;
  cfg.provider = "http";
  cfg.api_url = "http://127.0.0.1:" + std::to_string(port);
  HttpLLMAdapter adapter(cfg);

  GenerationRequest req;
  req.prompt = "test";
  std::stop_token token;
  auto result = adapter.generate(req, token);

  svr.stop();
  if (server_thread.joinable()) server_thread.join();

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().code == LLMError::Code::InvalidRequest);
}

TEST_CASE("HttpLLMAdapter error mapping: 500 → ServerError", "[llm][http]") {
  httplib::Server svr;
  svr.Post("/v1/chat/completions", [](const httplib::Request& req, httplib::Response& res) {
    res.status = 500;
    res.set_content("Internal Server Error", "text/plain");
  });

  int port = svr.bind_to_any_port("127.0.0.1");
  REQUIRE(port > 0);
  std::thread server_thread([&svr]() { svr.listen_after_bind(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  LLMConfig cfg;
  cfg.provider = "http";
  cfg.api_url = "http://127.0.0.1:" + std::to_string(port);
  HttpLLMAdapter adapter(cfg);

  GenerationRequest req;
  req.prompt = "test";
  std::stop_token token;
  auto result = adapter.generate(req, token);

  svr.stop();
  if (server_thread.joinable()) server_thread.join();

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().code == LLMError::Code::ServerError);
}

TEST_CASE("HttpLLMAdapter generate_stream yields 8-char chunks", "[llm][http]") {
  httplib::Server svr;
  svr.Post("/v1/chat/completions", [](const httplib::Request& req, httplib::Response& res) {
    nlohmann::json response = {
        {"choices", nlohmann::json::array({{
            {"message", {{"content", "abcdefghijklmnop"}}},  // 16 chars
            {"finish_reason", "stop"}
        }})}
    };
    res.set_content(response.dump(), "application/json");
  });

  int port = svr.bind_to_any_port("127.0.0.1");
  std::thread server_thread([&svr]() { svr.listen_after_bind(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  LLMConfig cfg;
  cfg.provider = "http";
  cfg.api_url = "http://127.0.0.1:" + std::to_string(port);
  HttpLLMAdapter adapter(cfg);

  GenerationRequest req;
  req.prompt = "test";
  std::stop_token token;
  auto stream = adapter.generate_stream(req, token);

  std::string accumulated;
  while (auto chunk = stream->next(token)) {
    accumulated += *chunk;
  }

  svr.stop();
  if (server_thread.joinable()) server_thread.join();

  REQUIRE(accumulated == "abcdefghijklmnop");
  // 16 chars / 8 char chunks = 2 chunks
}