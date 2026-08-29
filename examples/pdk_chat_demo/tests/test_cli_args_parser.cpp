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

TEST_CASE("declaration table contains exactly the ten flags", "[cli][stage3]") {
  const auto& table = pdk_chat_demo::cli_flag_declarations();
  REQUIRE(table.size() == 10);
  CHECK(table[0].long_name == "mock");
  CHECK(table[1].long_name == "session");
  CHECK(table[2].long_name == "print");
  CHECK(table[2].short_name == "p");
  CHECK(table[3].long_name == "provider");
  CHECK(table[4].long_name == "offline");
  CHECK(table[5].long_name == "fork");
  CHECK(table[6].long_name == "name");
  CHECK(table[9].long_name == "allow-training-capture");
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

TEST_CASE("--fork populates fork_node_id", "[cli][stage3][session-tree]") {
  char a[] = "pdk_chat_demo"; char b[] = "--fork"; char c[] = "node_42";
  char* argv[] = {a, b, c};
  const auto result = pdk_chat_demo::parse_cli_args(3, argv);
  REQUIRE(result.ok);
  CHECK(result.options.fork_node_id == "node_42");
}

TEST_CASE("--name populates session_name", "[cli][stage3][session-tree]") {
  char a[] = "pdk_chat_demo"; char b[] = "--name"; char c[] = "my-debug-session";
  char* argv[] = {a, b, c};
  const auto result = pdk_chat_demo::parse_cli_args(3, argv);
  REQUIRE(result.ok);
  CHECK(result.options.session_name == "my-debug-session");
}

TEST_CASE("combined --session and --fork parse both", "[cli][stage3][session-tree]") {
  char a[] = "pdk_chat_demo"; char b[] = "--session"; char c[] = "sess_abc";
  char d[] = "--fork";     char e[] = "node_42";
  char* argv[] = {a, b, c, d, e};
  const auto result = pdk_chat_demo::parse_cli_args(5, argv);
  REQUIRE(result.ok);
  CHECK(result.options.session_id == "sess_abc");
  CHECK(result.options.fork_node_id == "node_42");
}

TEST_CASE("--fork missing value at end-of-args is rejected", "[cli][stage3][session-tree]") {
  char a[] = "pdk_chat_demo"; char b[] = "--fork";
  char* argv[] = {a, b};
  const auto result = pdk_chat_demo::parse_cli_args(2, argv);
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("--help") != std::string::npos);
}

TEST_CASE("--system-prompt overwrites default via cli flag", "[cli_parser][system_prompt]") {
  char a[] = "pdk_chat_demo";
  char b[] = "--system-prompt";
  char c[] = "Be terse.";
  char* argv[] = {a, b, c};
  const auto r = pdk_chat_demo::parse_cli_args(3, argv);
  REQUIRE(r.ok);
  REQUIRE(r.options.system_prompt == "Be terse.");
  REQUIRE(r.options.append_system_prompt.empty());
}

TEST_CASE("--append-system-prompt sets only append field", "[cli_parser][system_prompt]") {
  char a[] = "pdk_chat_demo";
  char b[] = "--append-system-prompt";
  char c[] = "Always end with a joke.";
  char* argv[] = {a, b, c};
  const auto r = pdk_chat_demo::parse_cli_args(3, argv);
  REQUIRE(r.ok);
  REQUIRE(r.options.system_prompt.empty());
  REQUIRE(r.options.append_system_prompt == "Always end with a joke.");
}

TEST_CASE("--help mentions both system-prompt flags", "[cli_parser][system_prompt][help]") {
  char a[] = "pdk_chat_demo";
  char b[] = "--help";
  char* argv[] = {a, b};
  const auto r = pdk_chat_demo::parse_cli_args(2, argv);
  REQUIRE(r.show_help);
  CHECK(r.help.find("--system-prompt") != std::string::npos);
  CHECK(r.help.find("--append-system-prompt") != std::string::npos);
}

TEST_CASE("--allow-training-capture parses to true", "[cli][stage3][training-capture]") {
  char a[] = "pdk_chat_demo"; char b[] = "--allow-training-capture"; char* argv[] = {a, b};
  const auto result = pdk_chat_demo::parse_cli_args(2, argv);
  REQUIRE(result.ok);
  CHECK(result.options.allow_training_capture);
  CHECK_FALSE(result.options.mock);
}

TEST_CASE("--allow-training-capture default false", "[cli][stage3][training-capture]") {
  char a[] = "pdk_chat_demo";
  char* argv[] = {a};
  const auto result = pdk_chat_demo::parse_cli_args(1, argv);
  REQUIRE(result.ok);
  CHECK_FALSE(result.options.allow_training_capture);
}

TEST_CASE("missing value after --system-prompt returns error with help", "[cli_parser][system_prompt]") {
  char a[] = "pdk_chat_demo";
  char b[] = "--system-prompt";
  char* argv[] = {a, b};
  const auto r = pdk_chat_demo::parse_cli_args(2, argv);
  CHECK_FALSE(r.ok);
  CHECK(r.error.find("--help") != std::string::npos);
}
