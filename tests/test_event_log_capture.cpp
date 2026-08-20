// tests/test_event_log_capture.cpp
// 功能描述：EventLogWriter 集成测试 (ADR-0080 v1.1 D5)
//          验证 schema 字段 (causal_time + ts_wall)、JSONL 文件创建、
//          causal_time 持久化 (D2 v1.1)、session_id 路由
// 设计依据：ADR-0080 v1.1 amendment §决策 D2 / D5 / D10
// 作者：HydraForge v1.1 Distillation/Self-Evolution 修订 sprint
// 最后修改日期：2026-08-12

#include "catch_amalgamated.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "core/event_log.h"
#include "core/types/event_log_config.h"

namespace fs = std::filesystem;

namespace {

fs::path make_unique_temp_dir(const std::string& tag) {
  static std::atomic<std::uint64_t> counter{0};
  const auto n = counter.fetch_add(1);
  const auto pid = static_cast<uint64_t>(::getpid());
  const auto ts = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  std::ostringstream oss;
  oss << "event_log_test_" << tag << "_" << pid << "_" << ts << "_" << n;
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

// Mock IInteractionBus 仅用于测试（不依赖 InMemoryBus 完整头文件）
// 实现同步 fan-out：emit 后立即调用所有 subscribe("*") 的 callback
class TestBus : public agenticdsl::IInteractionBus {
 public:
  void emit(const agenticdsl::BusEvent& event) override {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(event);
    auto callbacks = subscribers_;
    for (auto& cb : callbacks) {
      cb.second(event);
    }
  }
  void emit(const std::string& event_type, const std::string& content) override {
    (void)event_type;
    (void)content;
  }
  size_t subscribe(const std::string& event_type,
                   std::function<void(const agenticdsl::BusEvent&)> callback) override {
    (void)event_type;
    std::lock_guard<std::mutex> lock(mutex_);
    size_t token = next_token_++;
    subscribers_[token] = std::move(callback);
    return token;
  }
  void unsubscribe(size_t token) override {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.erase(token);
  }

 private:
  mutable std::mutex mutex_;
  std::vector<agenticdsl::BusEvent> events_;
  std::map<size_t, std::function<void(const agenticdsl::BusEvent&)>> subscribers_;
  size_t next_token_ = 1;
};

size_t count_jsonl_lines(const fs::path& path) {
  std::ifstream in(path);
  if (!in.is_open()) return 0;
  size_t n = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) ++n;
  }
  return n;
}

bool file_contains(const fs::path& path, const std::string& substr) {
  std::ifstream in(path);
  if (!in.is_open()) return false;
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  return content.find(substr) != std::string::npos;
}

agenticdsl::BusEvent make_event(const std::string& topic,
                                uint64_t causal,
                                const std::string& session_id = "") {
  agenticdsl::BusEvent e;
  e.topic = topic;
  e.causal_time = causal;
  auto p = agenticdsl::ToolResult::success(
      nlohmann::json{{"k", "v"}}, nlohmann::json::object());
  if (!session_id.empty()) p.trace_id = session_id;
  e.payload = std::move(p);
  return e;
}

}  // namespace

TEST_CASE("EventLogWriter: creates per-agent JSONL file",
          "[event_log][writer]") {
  TempDirGuard tmp("creates_file");
  const std::string agent_id = "agent-writer-create";

  auto bus = std::make_shared<TestBus>();
  agenticdsl::EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = agent_id;
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(20);

  {
    agenticdsl::EventLogWriter writer(cfg, bus);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  const auto path = tmp.path / (agent_id + ".v1.jsonl");
  REQUIRE(fs::exists(path));
}

TEST_CASE("EventLogWriter: serialized events contain causal_time + ts_wall + v",
          "[event_log][writer][schema]") {
  TempDirGuard tmp("schema_fields");
  const std::string agent_id = "agent-schema";

  auto bus = std::make_shared<TestBus>();
  agenticdsl::EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = agent_id;
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(20);

  {
    agenticdsl::EventLogWriter writer(cfg, bus);
    bus->emit(make_event("llm.request", 100));
    bus->emit(make_event("llm.response", 101));
    bus->emit(make_event("tool.execution.start", 102));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    writer.flush_sync();
  }

  const auto path = tmp.path / (agent_id + ".v1.jsonl");
  REQUIRE(fs::exists(path));
  REQUIRE(count_jsonl_lines(path) == 3);
  REQUIRE(file_contains(path, "\"v\":1"));
  REQUIRE(file_contains(path, "\"causal_time\""));
  REQUIRE(file_contains(path, "\"ts_wall\""));
  REQUIRE(file_contains(path, "\"agent_id\":\"" + agent_id + "\""));
}

TEST_CASE("EventLogWriter: causal_time preserved through serialization",
          "[event_log][writer][causal_time]") {
  TempDirGuard tmp("causal_persist");
  const std::string agent_id = "agent-causal-rt";

  auto bus = std::make_shared<TestBus>();
  agenticdsl::EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = agent_id;
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(20);

  {
    agenticdsl::EventLogWriter writer(cfg, bus);
    for (uint64_t t = 200; t < 205; ++t) {
      bus->emit(make_event("test.event", t));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    writer.flush_sync();
  }

  const auto path = tmp.path / (agent_id + ".v1.jsonl");
  REQUIRE(fs::exists(path));
  REQUIRE(file_contains(path, "\"causal_time\":200"));
  REQUIRE(file_contains(path, "\"causal_time\":201"));
  REQUIRE(file_contains(path, "\"causal_time\":204"));
}

TEST_CASE("EventLogWriter: session_id captured from payload.trace_id",
          "[event_log][writer][session_id]") {
  TempDirGuard tmp("session_id");
  const std::string agent_id = "agent-session";

  auto bus = std::make_shared<TestBus>();
  agenticdsl::EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = agent_id;
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(20);

  {
    agenticdsl::EventLogWriter writer(cfg, bus);
    bus->emit(make_event("test.event", 300, "ses-abc-123"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    writer.flush_sync();
  }

  const auto path = tmp.path / (agent_id + ".v1.jsonl");
  REQUIRE(fs::exists(path));
  REQUIRE(file_contains(path, "\"session_id\":\"ses-abc-123\""));
}