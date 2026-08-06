#include <catch_amalgamated.hpp>
#include <initializer_list>
#include <vector>
#include "cli_args_parser.h"

namespace {
pdk_chat_demo::CliParseResult parse(std::initializer_list<const char*> values) {
  std::vector<char*> argv;
  for (const char* value : values) argv.push_back(const_cast<char*>(value));
  return pdk_chat_demo::parse_cli_args(static_cast<int>(argv.size()), argv.data());
}
}

TEST_CASE("parser accepts mock mode", "[cli][stage3]") {
  char program[] = "pdk_chat_demo";
  char mock[] = "--mock";
  char* argv[] = {program, mock};
  const auto result = pdk_chat_demo::parse_cli_args(2, argv);
  REQUIRE(result.ok);
  CHECK(result.options.mock);
  CHECK_FALSE(result.options.print);
  CHECK_FALSE(result.options.offline);
}

TEST_CASE("declaration table contains exactly the five flags", "[cli][stage3]") {
  const auto& table = pdk_chat_demo::cli_flag_declarations();
  REQUIRE(table.size() == 5);
  CHECK(table[0].long_name == "mock");
  CHECK(table[1].long_name == "session");
  CHECK(table[2].long_name == "print");
  CHECK(table[2].short_name == "p");
  CHECK(table[3].long_name == "provider");
  CHECK(table[4].long_name == "offline");
}

TEST_CASE("all declarations map to CliOptions", "[cli][stage3]") {
  const auto result = parse({"pdk_chat_demo", "--mock", "--session", "demo-session", "-p", "--provider", "deepseek", "--offline"});
  REQUIRE(result.ok);
  CHECK(result.options.mock);
  CHECK(result.options.session_id == "demo-session");
  CHECK(result.options.print);
  CHECK(result.options.provider == "deepseek");
  CHECK(result.options.offline);
}

TEST_CASE("optional values have explicit defaults", "[cli][stage3]") {
  const auto result = parse({"pdk_chat_demo"});
  REQUIRE(result.ok);
  CHECK_FALSE(result.options.mock);
  CHECK_FALSE(result.options.print);
  CHECK_FALSE(result.options.offline);
  CHECK(result.options.session_id.empty());
  CHECK(result.options.provider.empty());
}

TEST_CASE("mock and session remain independent", "[cli][stage3]") {
  char a[] = "pdk_chat_demo"; char b[] = "--mock"; char c[] = "--session"; char d[] = "demo-session";
  char* argv[] = {a, b, c, d};
  const auto result = pdk_chat_demo::parse_cli_args(4, argv);
  REQUIRE(result.ok);
  CHECK(result.options.mock);
  CHECK(result.options.session_id == "demo-session");
}

TEST_CASE("offline is not mock", "[cli][stage3]") {
  char a[] = "pdk_chat_demo"; char b[] = "--offline"; char* argv[] = {a, b};
  const auto result = pdk_chat_demo::parse_cli_args(2, argv);
  REQUIRE(result.ok);
  CHECK(result.options.offline);
  CHECK_FALSE(result.options.mock);
}

TEST_CASE("invalid input returns a diagnostic and nonzero parse status", "[cli][stage3]") {
  char a[] = "pdk_chat_demo"; char b[] = "--not-a-real-flag"; char* argv[] = {a, b};
  const auto unknown = pdk_chat_demo::parse_cli_args(2, argv);
  CHECK_FALSE(unknown.ok);
  CHECK(unknown.error.find("not-a-real-flag") != std::string::npos);
  CHECK(unknown.error.find("--help") != std::string::npos);
  char c[] = "--session"; char* missing[] = {a, c};
  const auto no_value = pdk_chat_demo::parse_cli_args(2, missing);
  CHECK_FALSE(no_value.ok);
  CHECK(no_value.error.find("session") != std::string::npos);
  CHECK(no_value.error.find("--help") != std::string::npos);
}

TEST_CASE("short and long print spellings are equivalent", "[cli][stage3]") {
  char a[] = "pdk_chat_demo"; char b[] = "-p"; char c[] = "--print";
  char* short_argv[] = {a, b}; char* long_argv[] = {a, c};
  const auto short_result = pdk_chat_demo::parse_cli_args(2, short_argv);
  const auto long_result = pdk_chat_demo::parse_cli_args(2, long_argv);
  REQUIRE(short_result.ok); REQUIRE(long_result.ok);
  CHECK(short_result.options.print); CHECK(long_result.options.print);
}
