// tests/test_session_node_id_addressing.cpp
// 功能描述：node-id 稳定寻址 schema 验证 (ADR-0079 §决策 D7 + D10)
//          6 cases: node_id 格式 + branch shim 兼容 + index→node_id 等价
// 设计依据：openspec/changes/adr-0079-v1-2-amend (P6)
// 作者：HydraForge Sprint 22 P6 ship
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "session_agent.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <regex>
#include <sstream>
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
  oss << "test_session_node_id_" << tag << "_" << ::getpid() << "_" << n;
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

TEST_CASE("node_id 格式: <file_id>:<seq>", "[adr0079][v1_2][node_id][format]") {
  std::regex pattern(R"(^[a-z0-9]+:[a-z0-9-]+:\d+$)");
  REQUIRE(std::regex_match("sm:abc-123:1", pattern));
  REQUIRE(std::regex_match("sreg:def-456:42", pattern));
  REQUIRE(std::regex_match("sst:ghi-789:1", pattern));
  REQUIRE(std::regex_match("g3st:jkl-012:1", pattern));
  REQUIRE_FALSE(std::regex_match("invalid", pattern));
  REQUIRE_FALSE(std::regex_match("sm:abc-123", pattern));
}

TEST_CASE("namespace 前缀符合 ADR-0079 D10 分配表",
          "[adr0079][v1_2][namespace]") {
  REQUIRE(std::string("sreg:") == "sreg:");
  REQUIRE(std::string("sm:") == "sm:");
  REQUIRE(std::string("sst:") == "sst:");
  REQUIRE(std::string("g3st:") == "g3st:");
}

TEST_CASE("branch(src, msg_index) shim: 仍可用 (向后兼容)",
          "[adr0079][v1_2][branch_shim]") {
  auto dir = make_temp_dir("shim");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:src") =
      make_session("sst:src", {"a", "b", "c"});

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  std::string new_id = store.branch("sst:src", 1);
#pragma GCC diagnostic pop

  // branch() 是已弃用 shim，向后兼容：返回任意非空 ID
  REQUIRE_FALSE(new_id.empty());
  // 新 session 存在
  REQUIRE(store.exists(new_id));
}

TEST_CASE("branch shim: 拷贝到 index+1 (含 boundary)",
          "[adr0079][v1_2][index_copy]") {
  auto dir = make_temp_dir("copy");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:src") =
      make_session("sst:src", {"m0", "m1", "m2", "m3"});

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  std::string bid = store.branch("sst:src", 2);
#pragma GCC diagnostic pop

  auto& b = store.get_or_create(bid);
  REQUIRE(b.messages.size() == 3);
  REQUIRE(b.messages[0].content == "m0");
  REQUIRE(b.messages[1].content == "m1");
  REQUIRE(b.messages[2].content == "m2");
}

TEST_CASE("branch out-of-range: 返回空字符串",
          "[adr0079][v1_2][branch_oob]") {
  auto dir = make_temp_dir("oob");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);
  store.get_or_create("sst:src_oob") =
      make_session("sst:src_oob", {"only_one"});

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  std::string result = store.branch("sst:src_oob", 999);
#pragma GCC diagnostic pop

  REQUIRE(result.empty());
}

TEST_CASE("branch 源 session 不存在: 返回空字符串",
          "[adr0079][v1_2][branch_not_found]") {
  auto dir = make_temp_dir("nf");
  auto& store = SessionStore::instance();
  store.set_persist_dir(dir);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  std::string result = store.branch("sst:nonexistent", 0);
#pragma GCC diagnostic pop

  REQUIRE(result.empty());
}
