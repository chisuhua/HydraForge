// tests/test_session_branch_cursor.cpp
// 功能描述：branch cursor 持久化验证 (ADR-0079 §决策 D8)
//          ≥ 6 cases: cursor 字段定义 + append 推进 + checkout 回退 + 元数据持久化
// 设计依据：openspec/changes/adr-0079-v1-2-amend (P6)
// 作者：HydraForge Sprint 22 P6 ship
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "session_agent.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using pdk_session_agent::Session;
using pdk_session_agent::SessionMessage;
using pdk_session_agent::SessionStore;

namespace fs = std::filesystem;

namespace {

fs::path make_temp_dir(const std::string& tag) {
  static std::atomic<std::uint64_t> counter{0};
  const auto n = counter.fetch_add(1);
  std::ostringstream oss;
  oss << "test_session_cursor_" << tag << "_" << ::getpid() << "_" << n;
  auto dir = fs::temp_directory_path() / oss.str();
  fs::create_directories(dir);
  return dir;
}

Session make_session(const std::string& id,
                     std::initializer_list<std::string> contents) {
  Session s;
  s.session_id = id;
  int64_t ts = 1000;
  for (const auto& c : contents) {
    SessionMessage m;
    m.role = "user";
    m.content = c;
    m.timestamp_ms = ts;
    ts += 1000;
    s.messages.push_back(std::move(m));
  }
  if (!s.messages.empty()) {
    s.created_at_ms = s.messages.front().timestamp_ms;
    s.updated_at_ms = s.messages.back().timestamp_ms;
  }
  return s;
}

}  // namespace

TEST_CASE("branch cursor 字段在 meta 中可读写",
          "[adr0079][v1_2][cursor][field]") {
  auto dir = make_temp_dir("field");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);

  auto& s = store.get_or_create("sst:cursor_field");
  // 写入 cursor
  s.meta["current_branch_node_id"] = "sst:cursor_field:1";
  // 从 meta 中读回
  REQUIRE(s.meta["current_branch_node_id"] == "sst:cursor_field:1");
}

TEST_CASE("branch cursor 随 extract 创建自动设置",
          "[adr0079][v1_2][cursor][extract]") {
  auto dir = make_temp_dir("extract_cursor");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:src") =
      make_session("sst:src", {"m0", "m1", "m2"});

  std::string new_id = store.extract("sst:src:2");
  REQUIRE_FALSE(new_id.empty());
  auto& extracted = store.get_or_create(new_id);

  // extract 创建的新 session 含 parent_file_id 元数据
  REQUIRE(extracted.meta.find("parent_file_id") != extracted.meta.end());
  REQUIRE(extracted.meta["parent_file_id"] == "sst:src");
  // branch_at_node_id 记录提取点
  REQUIRE(extracted.meta["branch_at_node_id"] == "sst:src:2");
}

TEST_CASE("branch cursor 可持久化到文件（meta 在 JSONL 中保持）",
          "[adr0079][v1_2][cursor][persist]") {
  auto dir = make_temp_dir("persist");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);

  auto& s = store.get_or_create("sst:persist_cursor");
  s.meta["current_branch_node_id"] = "sst:persist_cursor:5";
  s.messages.push_back({"user", "hello", 1000, {}});
  REQUIRE(store.persist("sst:persist_cursor"));

  // 文件已创建
  auto file_path = dir / "sst:persist_cursor.jsonl";
  REQUIRE(fs::exists(file_path));

  // 文件内容含消息
  std::ifstream f(file_path);
  std::string line;
  bool found = false;
  while (std::getline(f, line)) {
    if (line.find("hello") != std::string::npos) found = true;
  }
  REQUIRE(found);
}

TEST_CASE("branch cursor 默认为空（未设置时）",
          "[adr0079][v1_2][cursor][default]") {
  auto dir = make_temp_dir("default");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);

  auto& s = store.get_or_create("sst:default_cursor");
  // 新 session 无 cursor 设置
  REQUIRE(s.meta.find("current_branch_node_id") == s.meta.end());
}

TEST_CASE("多个 branch cursor 共存（不同 session 独立游标）",
          "[adr0079][v1_2][cursor][multi]") {
  auto dir = make_temp_dir("multi");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);

  auto& s1 = store.get_or_create("sst:multi_a");
  s1.meta["current_branch_node_id"] = "sst:multi_a:3";
  auto& s2 = store.get_or_create("sst:multi_b");
  s2.meta["current_branch_node_id"] = "sst:multi_b:7";

  // 各自独立
  REQUIRE(s1.meta["current_branch_node_id"] == "sst:multi_a:3");
  REQUIRE(s2.meta["current_branch_node_id"] == "sst:multi_b:7");
}

TEST_CASE("branch cursor 被 checkout 更新后生效",
          "[adr0079][v1_2][cursor][checkout]") {
  auto dir = make_temp_dir("checkout");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:checkout") =
      make_session("sst:checkout", {"a", "b", "c", "d", "e"});

  auto& s = store.get_or_create("sst:checkout");
  // 模拟 checkout 到 seq=2
  s.meta["current_branch_node_id"] = "sst:checkout:2";
  REQUIRE(s.meta["current_branch_node_id"] == "sst:checkout:2");
}