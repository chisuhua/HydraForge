#include <catch_amalgamated.hpp>
#include <cstdio>
#include <string>
#ifndef PDK_CHAT_DEMO_PATH
#define PDK_CHAT_DEMO_PATH "pdk_chat_demo"
#endif

namespace {
struct RunResult { int status; std::string combined; };

RunResult run_demo(const std::string& args, const std::string& stdin_input = "") {
  std::string command = "\"" PDK_CHAT_DEMO_PATH "\" " + args + " 2>&1";
  if (!stdin_input.empty()) {
    command = "printf '" + stdin_input + "' | " + command;
  }
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) return {-1, ""};
  std::string out; char buf[512];
  while (fgets(buf, sizeof(buf), pipe)) out += buf;
  return {pclose(pipe), out};
}
}

TEST_CASE("--fork with nonexistent node fails with diagnostic",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--fork nonexistent_node --mock");
  CHECK(r.status != 0);
  CHECK(r.combined.find("--fork") != std::string::npos);
  CHECK(r.combined.find("nonexistent_node") != std::string::npos);
  CHECK(r.combined.find("--help") != std::string::npos);
}

TEST_CASE("--fork without --session on empty session fails with diagnostic",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--fork node_42 --mock");
  CHECK(r.status != 0);
  CHECK(r.combined.find("--fork") != std::string::npos);
  CHECK(r.combined.find("--help") != std::string::npos);
}

TEST_CASE("--name with --session on existing session fails with scope error",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--session missing_session --name new-name --mock");
  CHECK(r.status != 0);
  CHECK(r.combined.find("--name") != std::string::npos);
  CHECK(r.combined.find("--help") != std::string::npos);
}

TEST_CASE("--fork missing value at end of args is rejected by parser",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--fork");
  CHECK(r.status != 0);
  CHECK(r.combined.find("--help") != std::string::npos);
}

TEST_CASE("--name missing value at end of args is rejected by parser",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--name");
  CHECK(r.status != 0);
  CHECK(r.combined.find("--help") != std::string::npos);
}

TEST_CASE("--fork and --name appear in --help output",
          "[cli][stage3][session-tree][e2e]") {
  const auto r = run_demo("--help");
  CHECK(r.status == 0);
  CHECK(r.combined.find("--fork") != std::string::npos);
  CHECK(r.combined.find("--name") != std::string::npos);
}
