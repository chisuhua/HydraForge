#include <catch_amalgamated.hpp>
#include <cstdio>
#include <string>
#ifndef PDK_CHAT_DEMO_PATH
#define PDK_CHAT_DEMO_PATH "pdk_chat_demo"
#endif
TEST_CASE("--mock reaches the existing mock startup path", "[cli][stage3][e2e]") {
  const std::string command = "printf 'exit\\n' | \"" PDK_CHAT_DEMO_PATH "\" --mock 2>&1";
  FILE* pipe = popen(command.c_str(), "r");
  REQUIRE(pipe != nullptr);
  std::string output; char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;
  const int status = pclose(pipe);
  CHECK(status == 0);
  CHECK(output.find("Mock mode: provider=mock, model=test") != std::string::npos);
  CHECK(output.find("Using MockLLMProvider") != std::string::npos);
}

TEST_CASE("--help shows generated usage and exits 0", "[cli][stage3][e2e]") {
  const std::string command = "\"" PDK_CHAT_DEMO_PATH "\" --help 2>&1";
  FILE* pipe = popen(command.c_str(), "r");
  REQUIRE(pipe != nullptr);
  std::string output; char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;
  const int status = pclose(pipe);
  CHECK(status == 0);
  CHECK(output.find("--mock") != std::string::npos);
  CHECK(output.find("--session") != std::string::npos);
  bool has_print = output.find("-p") != std::string::npos || output.find("--print") != std::string::npos;
  CHECK(has_print);
  CHECK(output.find("--provider") != std::string::npos);
  CHECK(output.find("--offline") != std::string::npos);
  CHECK(output.find("--fork") != std::string::npos);
  CHECK(output.find("--name") != std::string::npos);
}

TEST_CASE("unknown flag returns nonzero exit with diagnostic", "[cli][stage3][e2e]") {
  const std::string command = "\"" PDK_CHAT_DEMO_PATH "\" --not-a-real-flag 2>&1";
  FILE* pipe = popen(command.c_str(), "r");
  REQUIRE(pipe != nullptr);
  std::string output; char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;
  const int status = pclose(pipe);
  CHECK(status != 0);
  bool found_diagnostic = output.find("not-a-real-flag") != std::string::npos || output.find("--help") != std::string::npos;
  CHECK(found_diagnostic);
}
