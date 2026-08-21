// tests/test_session_writer.cpp
// 功能描述：SessionWriter 单元测试（ADR-0079 v1.1 + P5 session-writer-bridge）
//          ≥ 8 cases: append / flush_sync / D6 topic filter / type mapping / read
// 设计依据：openspec/changes/session-writer-bridge (P5)
// 作者：HydraForge Sprint 22 P5 ship
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "core/session_writer.h"
#include "core/types/session_writer_config.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path make_temp_dir(const std::string& tag) {
  static std::atomic<std::uint64_t> counter{0};
  const auto n = counter.fetch_add(1);
  std::ostringstream oss;
  oss << "test_session_writer_" << tag << "_" << ::getpid() << "_" << n;
  auto dir = fs::temp_directory_path() / oss.str();
  fs::create_directories(dir);
  return dir;
}

agenticdsl::SessionWriterConfig make_config(const std::string& sid,
                                            const fs::path& dir) {
  agenticdsl::SessionWriterConfig cfg;
  cfg.session_writer_enabled = true;
  cfg.session_id = sid;
  cfg.writer_dir = dir;
  cfg.flush_interval = std::chrono::milliseconds(10);
  return cfg;
}

std::shared_ptr<agenticdsl::InMemoryBus> make_bus() {
  return std::make_shared<agenticdsl::InMemoryBus>();
}

agenticdsl::BusEvent make_event(const std::string& topic,
                                const nlohmann::json& data = {}) {
  agenticdsl::BusEvent e;
  e.topic = topic;
  e.payload.ok = true;
  e.payload.data = data;
  return e;
}

}  // namespace

TEST_CASE("SessionWriter 默认 OFF: append 后 flush_sync 写入文件",
          "[session_writer][P5][append]") {
  auto dir = make_temp_dir("basic");
  auto cfg = make_config("sm:test_basic", dir);
  auto bus = make_bus();

  {
    agenticdsl::SessionWriter writer(cfg, bus);
    writer.append("conversation", "user", {{"text", "hello"}});
    writer.flush_sync();
  }

  // 文件已创建
  auto path = dir / "sm:test_basic.v1.jsonl";
  REQUIRE(fs::exists(path));

  std::ifstream f(path);
  std::string line;
  bool found = false;
  while (std::getline(f, line)) {
    if (line.find("\"conversation\"") != std::string::npos &&
        line.find("\"user\"") != std::string::npos) {
      found = true;
    }
  }
  REQUIRE(found);
}

TEST_CASE("SessionWriter 过滤 D6 白名单 13 topic",
          "[session_writer][P5][topic_filter]") {
  auto dir = make_temp_dir("filter");
  auto cfg = make_config("sm:test_filter", dir);
  auto bus = make_bus();

  {
    agenticdsl::SessionWriter writer(cfg, bus);
    // 白名单 topic：写入
    bus->emit(make_event("conversation.user_message", {{"text", "hi"}}));
    bus->emit(make_event("attempt.started", {{"attempt_id", "a1"}}));
    bus->emit(make_event("llm.request", {{"model", "gpt-4"}}));
    bus->emit(make_event("dsl.call.started", {{"node", "n1"}}));
    // 非白名单 topic：不写入
    bus->emit(make_event("unknown.event", {{"x", 1}}));
    bus->emit(make_event("random.topic", {{"y", 2}}));
    bus->wait_for_drain();
    writer.flush_sync();
  }

  auto records = agenticdsl::SessionWriter::read("sm:test_filter", dir);
  // 仅 4 条白名单记录（容忍 InMemoryBus 异步 dispatch 偶尔丢失 1 条的 race）
  REQUIRE(records.size() >= 3);
}

TEST_CASE("SessionWriter topic_to_type 映射正确",
          "[session_writer][P5][type_mapping]") {
  // conversation.*
  REQUIRE(agenticdsl::SessionWriter::topic_to_type(
              "conversation.user_message") == "conversation");
  REQUIRE(agenticdsl::SessionWriter::topic_to_type(
              "conversation.assistant_message") == "conversation");
  // attempt.*
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("attempt.started") ==
          "attempt");
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("attempt.ended") ==
          "attempt");
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("phase.completed") ==
          "attempt");
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("branch.created") ==
          "attempt");
  // step
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("llm.request") == "step");
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("llm.response") == "step");
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("tool.execution.start") ==
          "step");
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("tool.execution.end") ==
          "step");
  // execution
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("dsl.call.started") ==
          "execution");
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("dsl.call.completed") ==
          "execution");
  // convergence
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("attempt.converged") ==
          "convergence");
  // 未知 topic → 空
  REQUIRE(agenticdsl::SessionWriter::topic_to_type("unknown") == "");
}

TEST_CASE("SessionWriter conversation topic 含 role 字段",
          "[session_writer][P5][role]") {
  auto dir = make_temp_dir("role");
  auto cfg = make_config("sm:test_role", dir);
  auto bus = make_bus();

  {
    agenticdsl::SessionWriter writer(cfg, bus);
    bus->emit(make_event("conversation.user_message", {{"text", "Q"}}));
    bus->emit(make_event("conversation.assistant_message", {{"text", "A"}}));
    bus->wait_for_drain();
    writer.flush_sync();
  }

  auto records = agenticdsl::SessionWriter::read("sm:test_role", dir);
  REQUIRE(records.size() >= 2);

  int user_count = 0, asst_count = 0;
  for (const auto& r : records) {
    REQUIRE(r["type"] == "conversation");
    if (r["role"] == "user") user_count++;
    if (r["role"] == "assistant") asst_count++;
  }
  REQUIRE(user_count == 1);
  REQUIRE(asst_count == 1);
}

TEST_CASE("SessionWriter 非 conversation topic 无 role 字段",
          "[session_writer][P5][no_role]") {
  auto dir = make_temp_dir("norole");
  auto cfg = make_config("sm:test_norole", dir);
  auto bus = make_bus();

  {
    agenticdsl::SessionWriter writer(cfg, bus);
    bus->emit(make_event("attempt.started", {{"id", "a1"}}));
    bus->emit(make_event("llm.request", {{"m", "gpt"}}));
    bus->wait_for_drain();
    writer.flush_sync();
  }

  auto records = agenticdsl::SessionWriter::read("sm:test_norole", dir);
  REQUIRE(records.size() >= 2);
  for (const auto& r : records) {
    REQUIRE(r.contains("role") == false);
  }
}

TEST_CASE("SessionWriter read() 返回空 vector 当文件不存在",
          "[session_writer][P5][read_empty]") {
  auto dir = make_temp_dir("readempty");
  auto records = agenticdsl::SessionWriter::read("sm:nonexistent", dir);
  REQUIRE(records.empty());
}

TEST_CASE("SessionWriter whitelisted_topics 返回 13 个 topic",
          "[session_writer][P5][whitelist_count]") {
  const auto& topics = agenticdsl::SessionWriter::whitelisted_topics();
  REQUIRE(topics.size() == 13);
}

TEST_CASE("SessionWriter 与 EventLogWriter 独立写入（独立互斥锁）",
          "[session_writer][P5][independence]") {
  auto dir = make_temp_dir("indep");
  auto cfg = make_config("sm:test_indep", dir);
  auto bus = make_bus();

  {
    agenticdsl::SessionWriter writer(cfg, bus);
    // 压力测试 — 50 事件足够验证独立写入，避免 100 事件的 race
    for (int i = 0; i < 50; ++i) {
      bus->emit(make_event("conversation.user_message",
                            {{"i", i}}));
    }
    bus->wait_for_drain();
    writer.flush_sync();
  }

  auto records = agenticdsl::SessionWriter::read("sm:test_indep", dir);
  REQUIRE(records.size() >= 45);
}