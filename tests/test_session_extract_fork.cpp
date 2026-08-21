// tests/test_session_extract_fork.cpp
// 功能描述：path-extraction fork 验证 (ADR-0079 §决策 D9)
//          ≥ 6 cases: extract() header parent_file_id + 内容完整性 + 边界
// 设计依据：openspec/changes/adr-0079-v1-2-amend (P6)
// 作者：HydraForge Sprint 22 P6 ship
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "session_agent.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
  oss << "test_session_extract_" << tag << "_" << ::getpid() << "_" << n;
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

TEST_CASE("extract 创建新 file_id，header 含 parent_file_id",
          "[adr0079][v1_2][extract][header]") {
  auto dir = make_temp_dir("header");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:src") =
      make_session("sst:src", {"m0", "m1", "m2", "m3", "m4"});

  // extract 从 node_id "sst:src:3"（seq=3, 指向 m0-m2）
  std::string new_id = store.extract("sst:src:3");

  REQUIRE_FALSE(new_id.empty());
  // 新 file_id 以 sst: 开头（D10 命名空间）
  REQUIRE(new_id.substr(0, 4) == "sst:");
  auto& extracted = store.get_or_create(new_id);
  // header parent_file_id 存源 file_id
  REQUIRE(extracted.meta["parent_file_id"] == "sst:src");
  // header branch_at_node_id 存原始 node_id
  REQUIRE(extracted.meta["branch_at_node_id"] == "sst:src:3");
}

TEST_CASE("extract 内容 = 从起始到 seq 指向的消息",
          "[adr0079][v1_2][extract][content]") {
  auto dir = make_temp_dir("content");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:src") =
      make_session("sst:src", {"a", "b", "c", "d"});

  std::string new_id = store.extract("sst:src:2");
  REQUIRE_FALSE(new_id.empty());
  auto& extracted = store.get_or_create(new_id);

  // 应包含前 2 条消息
  REQUIRE(extracted.messages.size() == 2);
  REQUIRE(extracted.messages[0].content == "a");
  REQUIRE(extracted.messages[1].content == "b");
}

TEST_CASE("extract seq=1 只包含第一条消息",
          "[adr0079][v1_2][extract][first]") {
  auto dir = make_temp_dir("first");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:src") =
      make_session("sst:src", {"x", "y", "z"});

  std::string new_id = store.extract("sst:src:1");
  REQUIRE_FALSE(new_id.empty());
  auto& extracted = store.get_or_create(new_id);
  REQUIRE(extracted.messages.size() == 1);
  REQUIRE(extracted.messages[0].content == "x");
}

TEST_CASE("extract seq=0 返回空（无效 node_id）",
          "[adr0079][v1_2][extract][zero]") {
  auto dir = make_temp_dir("zero");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:src") =
      make_session("sst:src", {"a", "b"});

  std::string result = store.extract("sst:src:0");
  REQUIRE(result.empty());
}

TEST_CASE("extract seq 超出范围返回空",
          "[adr0079][v1_2][extract][oob]") {
  auto dir = make_temp_dir("oob");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:src") =
      make_session("sst:src", {"only_one"});

  std::string result = store.extract("sst:src:999");
  REQUIRE(result.empty());
}

TEST_CASE("extract 源 session 不存在返回空",
          "[adr0079][v1_2][extract][notfound]") {
  auto dir = make_temp_dir("nf");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);

  std::string result = store.extract("sst:nonexistent:1");
  REQUIRE(result.empty());
}

TEST_CASE("extract node_id 格式无效返回空",
          "[adr0079][v1_2][extract][bad_format]") {
  auto dir = make_temp_dir("badfmt");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);

  // 缺少 seq
  std::string r1 = store.extract("sst:src");
  REQUIRE(r1.empty());
  // 空字符串
  std::string r2 = store.extract("");
  REQUIRE(r2.empty());
}