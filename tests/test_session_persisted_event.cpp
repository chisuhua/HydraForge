// tests/test_session_persisted_event.cpp
// 功能描述：SessionManager flush_append session.persisted 事件发射单元测试
// 设计依据：OpenSpec change session-manager-jsonl-v2 §1 (session.persisted 事件发射)
//          + ADR-0068 §决策 5 (EventBuilder 统一构造)
//          + spec/session-persisted-emission/spec.md 5 ADDED Requirements
// 作者：AgenticDSL Phase 5 / Session Manager JSONL v2 Sprint
// 最后修改日期：2026-08-05

#include "catch_amalgamated.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/session_manager.h"
#include "core/types/tool_result.h"
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;

using agenticdsl::SessionManager;
using agenticdsl::SessionNode;
using agenticdsl::IInteractionBus;
using agenticdsl::BusEvent;
using agenticdsl::EventBuilder;
using agenticdsl::ToolResult;

// RecordingBus — 记录所有 emit 事件的 IInteractionBus mock
// 用于验证 session.persisted 事件在 flush_append 路径上被正确发射
class RecordingBus : public IInteractionBus {
 public:
  struct CapturedEvent {
    std::string topic;
    nlohmann::json args;
    nlohmann::json meta;
  };
  std::vector<CapturedEvent> events;

  void emit(const BusEvent& event) override {
    CapturedEvent cap;
    cap.topic = event.topic;
    cap.args = event.payload.data;
    cap.meta = event.payload.meta;
    events.push_back(cap);
  }

  void emit(const std::string& /*event_type*/,
            const std::string& /*content*/) override {}
  size_t subscribe(
      const std::string& /*event_type*/,
      std::function<void(const BusEvent&)> /*callback*/) override {
    return 0;
  }
  void unsubscribe(size_t /*token*/) override {}
};

namespace {

// 生成一个测试用唯一临时目录（避免并行测试间目录冲突）
fs::path make_unique_temp_dir(const std::string& tag) {
  static std::atomic<uint64_t> counter{0};
  const auto n = counter.fetch_add(1);
  const auto pid = static_cast<uint64_t>(::getpid());
  const auto ts = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  std::ostringstream oss;
  oss << "session_persisted_test_" << tag << "_" << pid << "_" << ts << "_" << n;
  auto dir = fs::temp_directory_path() / oss.str();
  fs::create_directories(dir);
  return dir;
}

// RAII 临时目录清理器
struct TempDirGuard {
  fs::path path;
  explicit TempDirGuard(const std::string& tag) : path(make_unique_temp_dir(tag)) {}
  ~TempDirGuard() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

}  // namespace

TEST_CASE("session.persisted emitted on successful flush_append",
          "[session_manager][event][persisted]") {
  TempDirGuard tmp("emitted");
  SessionManager mgr(tmp.path);
  auto bus = std::make_shared<RecordingBus>();
  mgr.set_bus(bus);

  mgr.open("test_session_001");
  SessionNode node;
  node.id = mgr.next_node_id();
  node.parent_id = "";
  node.branch_id = "main";
  node.content = nlohmann::json{{"role", "user"}, {"content", "hello"}};
  mgr.flush_append(node);

  REQUIRE(bus->events.size() == 1);
  REQUIRE(bus->events[0].topic == "session.persisted");
  REQUIRE(bus->events[0].args.contains("session_id"));
  REQUIRE(bus->events[0].args.contains("node_id"));
  REQUIRE(bus->events[0].args.contains("branch_id"));
  REQUIRE(bus->events[0].args.contains("timestamp"));
  REQUIRE(bus->events[0].args["session_id"] == "test_session_001");
  REQUIRE(bus->events[0].args["node_id"] == node.id);
  REQUIRE(bus->events[0].args["branch_id"] == "main");
  REQUIRE(bus->events[0].args["timestamp"].is_number());
}

TEST_CASE("session.persisted NOT emitted on flush_append failure",
          "[session_manager][event][persisted][failure]") {
  TempDirGuard tmp("failure");
  SessionManager mgr(tmp.path);
  auto bus = std::make_shared<RecordingBus>();
  mgr.set_bus(bus);

  mgr.open("test_session_002");
  SessionNode ok_node;
  ok_node.id = mgr.next_node_id();
  ok_node.parent_id = "";
  ok_node.branch_id = "main";
  ok_node.content = nlohmann::json{{"role", "user"}};
  mgr.flush_append(ok_node);
  REQUIRE(bus->events.size() == 1);

  SessionManager mgr2(tmp.path);
  mgr2.set_bus(bus);
  SessionNode bad_node;
  bad_node.id = "no_open_invalid";
  bad_node.parent_id = "";
  bad_node.branch_id = "main";
  bad_node.content = nlohmann::json{{"k", "v"}};
  REQUIRE_THROWS(mgr2.flush_append(bad_node));
  REQUIRE(bus->events.size() == 1);
}

TEST_CASE("session.persisted skipped when bus_ is null",
          "[session_manager][event][persisted][null-bus]") {
  TempDirGuard tmp("nullbus");
  SessionManager mgr(tmp.path);  // bus_ 默认 nullptr
  mgr.open("test_session_003");
  SessionNode node;
  node.id = mgr.next_node_id();
  node.parent_id = "";
  node.branch_id = "main";
  node.content = nlohmann::json{{"role", "user"}, {"content", "hi"}};
  // 不抛异常即通过 — bus_==nullptr 时跳过 emit
  REQUIRE_NOTHROW(mgr.flush_append(node));
}

TEST_CASE("session.persisted payload contains all 4 ADR-0068 fields",
          "[session_manager][event][persisted][payload]") {
  TempDirGuard tmp("payload");
  SessionManager mgr(tmp.path);
  auto bus = std::make_shared<RecordingBus>();
  mgr.set_bus(bus);
  mgr.open("test_session_004");
  SessionNode node;
  node.id = mgr.next_node_id();
  node.parent_id = "";
  node.branch_id = "main";
  node.content = nlohmann::json{{"role", "user"}, {"content", "test"}};
  mgr.flush_append(node);

  REQUIRE(bus->events.size() == 1);
  const auto& args = bus->events[0].args;
  // 严格 4 字段, 不多不少
  REQUIRE(args.size() == 4);
  REQUIRE(args["session_id"].is_string());
  REQUIRE(args["node_id"].is_string());
  REQUIRE(args["branch_id"].is_string());
  REQUIRE(args["timestamp"].is_number());
  REQUIRE(args["timestamp"].get<uint64_t>() > 0);
}

TEST_CASE("session.persisted emitted exactly once per flush_append",
          "[session_manager][event][persisted][count]") {
  TempDirGuard tmp("count");
  SessionManager mgr(tmp.path);
  auto bus = std::make_shared<RecordingBus>();
  mgr.set_bus(bus);
  mgr.open("test_session_005");

  // 3 次 flush_append 应产生 3 次事件
  for (int i = 0; i < 3; ++i) {
    SessionNode node;
    node.id = mgr.next_node_id();
    node.parent_id = "";
    node.branch_id = "main";
    node.content = nlohmann::json{{"i", i}};
    mgr.flush_append(node);
  }
  REQUIRE(bus->events.size() == 3);
  for (const auto& ev : bus->events) {
    REQUIRE(ev.topic == "session.persisted");
  }
}

TEST_CASE("session.persisted event topic strictly 'session.persisted'",
          "[session_manager][event][persisted][topic]") {
  TempDirGuard tmp("topic");
  SessionManager mgr(tmp.path);
  auto bus = std::make_shared<RecordingBus>();
  mgr.set_bus(bus);
  mgr.open("test_session_006");
  SessionNode node;
  node.id = mgr.next_node_id();
  node.parent_id = "";
  node.branch_id = "main";
  node.content = nlohmann::json{{"k", "v"}};
  mgr.flush_append(node);

  REQUIRE(bus->events.size() == 1);
  // 严格字符串匹配, 不允许 'session.persisted.' / 'session_persisted' / 'session.before.persisted' 等变体
  REQUIRE(bus->events[0].topic == "session.persisted");
}
