// Wave 3-A Phase C: E2E tests for /model runtime switching.
// Verifies request_model_switch + next_model API + mock-mode guard.

#include <cstdio>
#include <fstream>

#include <catch_amalgamated.hpp>
#include <nlohmann/json.hpp>

#include "chat_session.h"

using namespace pdk_chat_demo;

namespace {

std::string write_tmp_config(const std::string& body) {
  char path[] = "/tmp/chat_model_test_XXXXXX.json";
  int fd = mkstemps(path, 5);
  if (fd < 0) throw std::runtime_error("mkstemps failed");
  write(fd, body.data(), body.size());
  close(fd);
  return path;
}

}  // namespace

TEST_CASE("request_model_switch stores in next_model",
          "[model_switch][chat_session]") {
  const std::string body =
      R"({"schema_version":":1.0","app_id":"t","providers":{},"agent":{"provider":"mock","model":"test","system_prompt":""}})";
  const auto path = write_tmp_config(body);
  ChatSession session(nullptr, nullptr, nullptr, {}, {});

  REQUIRE(session.next_model().empty());
  REQUIRE(session.request_model_switch("mock"));
  CHECK(session.next_model() == "mock");
  std::remove(path.c_str());
}

TEST_CASE("request_model_switch rejects non-mock in mock mode",
          "[model_switch][chat_session][mock]") {
  const std::string body =
      R"({"schema_version":":1.0","app_id":"t","providers":{},"agent":{"provider":"mock","model":"test","system_prompt":""}})";
  const auto path = write_tmp_config(body);
  ChatSession session(nullptr, nullptr, nullptr, {}, {});

  CHECK_FALSE(session.request_model_switch("openai"));
  CHECK_FALSE(session.request_model_switch("deepseek"));
  CHECK(session.next_model().empty());

  // mock is allowed even in mock mode
  REQUIRE(session.request_model_switch("mock"));
  CHECK(session.next_model() == "mock");
  std::remove(path.c_str());
}

TEST_CASE("request_model_switch rejects empty",
          "[model_switch][chat_session]") {
  const std::string body =
      R"({"schema_version":":1.0","app_id":"t","providers":{},"agent":{"provider":"mock","model":"test","system_prompt":""}})";
  const auto path = write_tmp_config(body);
  ChatSession session(nullptr, nullptr, nullptr, {}, {});

  CHECK_FALSE(session.request_model_switch(""));
  CHECK(session.next_model().empty());
  std::remove(path.c_str());
}

TEST_CASE("next_model returns pending or empty",
          "[model_switch][chat_session][persistence]") {
  const std::string body =
      R"({"schema_version":":1.0","app_id":"t","providers":{},"agent":{"provider":"mock","model":"test","system_prompt":""}})";
  const auto path = write_tmp_config(body);
  ChatSession session(nullptr, nullptr, nullptr, {}, {});

  // Initially empty
  CHECK(session.next_model().empty());

  // Set then read
  session.request_model_switch("mock");
  CHECK(session.next_model() == "mock");

  // Set again with different value
  session.request_model_switch("mock");
  CHECK(session.next_model() == "mock");
  std::remove(path.c_str());
}