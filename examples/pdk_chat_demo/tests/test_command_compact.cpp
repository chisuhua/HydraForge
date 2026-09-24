// tests/test_command_compact.cpp
// pdk-chat-demo-deduplicate Task 4.2: /compact 接线到 SessionManager::compact()
// (替代 chat-real-llm-coverage Phase A.2 的 placeholder 断言)

#include <catch_amalgamated.hpp>

#include <filesystem>

#include "commands/compact_command.h"
#include "commands/command_globals.h"
#include "core/session_manager.h"

namespace fs = std::filesystem;

namespace {

fs::path make_unique_temp_dir(const std::string& tag) {
  static std::atomic<uint64_t> counter{0};
  const auto n = counter.fetch_add(1);
  const auto pid = static_cast<uint64_t>(::getpid());
  std::ostringstream oss;
  oss << "compact_test_" << tag << "_" << pid << "_" << n;
  auto dir = fs::temp_directory_path() / oss.str();
  fs::create_directories(dir);
  return dir;
}

struct TempDirGuard {
  fs::path path;
  explicit TempDirGuard(const std::string& tag) : path(make_unique_temp_dir(tag)) {}
  ~TempDirGuard() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

class ScopedSessionManager {
 public:
  explicit ScopedSessionManager(const fs::path& dir) : sm_(dir) {
    prev_ = pdk_chat_demo::g_session_manager;
    pdk_chat_demo::g_session_manager = &sm_;
  }
  ~ScopedSessionManager() {
    pdk_chat_demo::g_session_manager = prev_;
  }
  ScopedSessionManager(const ScopedSessionManager&) = delete;
  ScopedSessionManager& operator=(const ScopedSessionManager&) = delete;

  agenticdsl::SessionManager& get() { return sm_; }

 private:
  agenticdsl::SessionManager sm_;
  agenticdsl::SessionManager* prev_;
};

agenticdsl::SessionNode make_node(const std::string& id,
                                 const std::string& parent,
                                 const std::string& branch = "main") {
  agenticdsl::SessionNode n;
  n.id = id;
  n.parent_id = parent;
  n.branch_id = branch;
  n.content = {{"role", "user"}, {"text", "hello " + id}};
  return n;
}

}  // namespace

TEST_CASE("/compact spec fields are correct",
          "[pdk_chat_demo_dedup][compact][command]") {
  auto spec = pdk_chat_demo::make_compact_command_spec();
  REQUIRE(spec.name == "/compact");
  REQUIRE_FALSE(spec.description.empty());
  REQUIRE(spec.plugin_origin == "pdk_chat_demo");
  REQUIRE(spec.handler != nullptr);
}

TEST_CASE("/compact handler returns SessionManager not injected when global is null",
          "[pdk_chat_demo_dedup][compact][command]") {
  auto prev = pdk_chat_demo::g_session_manager;
  pdk_chat_demo::g_session_manager = nullptr;
  auto spec = pdk_chat_demo::make_compact_command_spec();
  agenticdsl::ToolCallContext ctx;
  std::string output = spec.handler(ctx);
  pdk_chat_demo::g_session_manager = prev;

  REQUIRE(output.find("SessionManager not injected") != std::string::npos);
}

TEST_CASE("/compact handler returns no active session when SessionManager exists but not opened",
          "[pdk_chat_demo_dedup][compact][command]") {
  TempDirGuard tmp("not_opened");
  ScopedSessionManager sm(tmp.path);
  auto spec = pdk_chat_demo::make_compact_command_spec();
  agenticdsl::ToolCallContext ctx;
  std::string output = spec.handler(ctx);

  REQUIRE(output.find("no active session") != std::string::npos);
}

TEST_CASE("/compact handler calls SessionManager::compact() on active session",
          "[pdk_chat_demo_dedup][compact][command]") {
  TempDirGuard tmp("compact_active");
  ScopedSessionManager sm(tmp.path);
  sm.get().open("alpha");
  sm.get().flush_append(make_node("root", ""));
  sm.get().flush_append(make_node("n1", "root"));

  auto spec = pdk_chat_demo::make_compact_command_spec();
  agenticdsl::ToolCallContext ctx;
  std::string output = spec.handler(ctx);

  REQUIRE(output.find("Compacted session alpha") != std::string::npos);

  const auto jsonl_path = tmp.path / "alpha.jsonl";
  std::ifstream in(jsonl_path);
  int line_count = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) ++line_count;
  }
  // 2 nodes + 1 branch meta = 3 lines
  REQUIRE(line_count == 3);
}

TEST_CASE("/compact handler does not contain legacy placeholder text",
          "[pdk_chat_demo_dedup][compact][command]") {
  TempDirGuard tmp("no_placeholder");
  ScopedSessionManager sm(tmp.path);
  sm.get().open("beta");
  auto spec = pdk_chat_demo::make_compact_command_spec();
  agenticdsl::ToolCallContext ctx;
  std::string output = spec.handler(ctx);

  REQUIRE(output.find("Compaction not yet wired") == std::string::npos);
  REQUIRE(output.find("Task 8") == std::string::npos);
}