#include <catch_amalgamated.hpp>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <core/session_manager.h>

namespace fs = std::filesystem;

namespace {
fs::path make_temp_dir() {
  static std::atomic<int> counter{0};
  fs::path base = fs::temp_directory_path() /
                  ("hydra_session_rename_" + std::to_string(counter.fetch_add(1)) +
                   "_" + std::to_string(std::rand()));
  fs::remove_all(base);
  fs::create_directories(base);
  return base;
}
}

TEST_CASE("rename_session persists name to JSONL", "[session-manager][rename]") {
  const auto dir = make_temp_dir();
  agenticdsl::SessionManager mgr(dir);
  mgr.open("test-sess");
  mgr.rename_session("my-debug-session");
  mgr.flush_append(agenticdsl::SessionNode{
      mgr.next_node_id(), "", "main",
      nlohmann::json{{"role", "user"}, {"text", "hello"}}});

  agenticdsl::SessionManager mgr2(dir);
  mgr2.open("test-sess");
  auto nodes = mgr2.load_jsonl();
  bool found = false;
  for (const auto& n : nodes) {
    if (n.content.value("type", "") == "session_meta" &&
        n.content.value("name", "") == "my-debug-session") {
      found = true;
      break;
    }
  }
  REQUIRE(found);
  fs::remove_all(dir);
}

TEST_CASE("rename_session with empty name is a no-op", "[session-manager][rename]") {
  const auto dir = make_temp_dir();
  agenticdsl::SessionManager mgr(dir);
  mgr.open("test-sess");
  mgr.rename_session("");
  mgr.flush_append(agenticdsl::SessionNode{
      mgr.next_node_id(), "", "main",
      nlohmann::json{{"role", "user"}, {"text", "hello"}}});

  agenticdsl::SessionManager mgr2(dir);
  mgr2.open("test-sess");
  auto nodes = mgr2.load_jsonl();
  for (const auto& n : nodes) {
    REQUIRE(n.content.value("type", "") != "session_meta");
  }
  fs::remove_all(dir);
}
