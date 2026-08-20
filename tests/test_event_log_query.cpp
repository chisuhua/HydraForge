// tests/test_event_log_query.cpp
// 功能描述：EventLogWriter::read() + query() 单元测试 (P4 event-log-query-api)
//          ≥10 cases: read 时间窗 / query glob / query time / max_count /
//          组合 / 空 / 损坏行 / 静态 read 不变
// 设计依据：openspec/changes/event-log-query-api (P4)
// 作者：HydraForge Sprint 22 P4 ship
// 最后修改日期：2026-08-20

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "core/event_log.h"
#include "core/types/event_log_config.h"
#include "test_helpers/mock_bus.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using agenticdsl::BusEvent;
using agenticdsl::EventLogConfig;
using agenticdsl::EventLogWriter;

namespace {

fs::path make_unique_temp_dir(const std::string& tag) {
  static std::atomic<std::uint64_t> counter{0};
  const auto n = counter.fetch_add(1);
  const auto pid = static_cast<uint64_t>(::getpid());
  const auto ts = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  std::ostringstream oss;
  oss << "event_log_query_test_" << tag << "_" << pid << "_" << ts << "_" << n;
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

BusEvent make_event(const std::string& topic, uint64_t causal_time,
                    const std::string& trace_id = "") {
  BusEvent e;
  e.topic = topic;
  e.causal_time = causal_time;
  e.payload.ok = true;
  if (!trace_id.empty()) {
    e.payload.trace_id = trace_id;
  }
  return e;
}

}  // namespace

// =====================================================================
// Test 1: member read() 按因果时间窗过滤
// =====================================================================
TEST_CASE("EventLogWriter::read filters by causal_time window",
          "[event_log][query][read][time_window]") {
  TempDirGuard tmp("read_window");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_a";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争

  EventLogWriter writer(cfg, bus);
  bus->emit(make_event("llm.request", 10));
  bus->emit(make_event("llm.request", 50));
  bus->emit(make_event("llm.request", 100));
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  auto in_range = writer.read("agent_a", 20, 80);
  REQUIRE(in_range.size() == 1);
  REQUIRE(in_range[0].causal_time == 50);

  auto all = writer.read("agent_a", 0, UINT64_MAX);
  REQUIRE(all.size() == 3);

  auto after = writer.read("agent_a", 60, UINT64_MAX);
  REQUIRE(after.size() == 1);
  REQUIRE(after[0].causal_time == 100);
}

// =====================================================================
// Test 2: query() 精确 topic 匹配
// =====================================================================
TEST_CASE("EventLogWriter::query exact topic match",
          "[event_log][query][exact]") {
  TempDirGuard tmp("query_exact");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_b";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  bus->emit(make_event("llm.request", 10));
  bus->emit(make_event("tool.call", 20));
  bus->emit(make_event("llm.response", 30));
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  EventLogWriter::QueryFilter filter;
  filter.topic_glob = "llm.request";
  filter.start_causal_time = 0;
  filter.end_causal_time = UINT64_MAX;
  auto result = writer.query("agent_b", filter, 100);
  REQUIRE(result.size() == 1);
  REQUIRE(result[0].topic == "llm.request");
}

// =====================================================================
// Test 3: query() glob topic 匹配 (前缀通配)
// =====================================================================
TEST_CASE("EventLogWriter::query glob topic match (llm.*)",
          "[event_log][query][glob]") {
  TempDirGuard tmp("query_glob");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_c";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  bus->emit(make_event("llm.request", 10));
  bus->emit(make_event("llm.response", 20));
  bus->emit(make_event("tool.call", 30));
  bus->emit(make_event("llm.error", 40));
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  EventLogWriter::QueryFilter filter;
  filter.topic_glob = "llm.*";
  filter.start_causal_time = 0;
  filter.end_causal_time = UINT64_MAX;
  auto result = writer.query("agent_c", filter, 100);
  REQUIRE(result.size() == 3);
  for (const auto& e : result) {
    REQUIRE(e.topic.substr(0, 4) == "llm.");
  }
}

// =====================================================================
// Test 4: query() 空 glob 匹配所有
// =====================================================================
TEST_CASE("EventLogWriter::query empty glob matches all",
          "[event_log][query][empty_glob]") {
  TempDirGuard tmp("query_all");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_d";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  bus->emit(make_event("a", 10));
  bus->emit(make_event("b", 20));
  bus->emit(make_event("c", 30));
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  EventLogWriter::QueryFilter filter;
  filter.topic_glob = "";
  filter.start_causal_time = 0;
  filter.end_causal_time = UINT64_MAX;
  auto result = writer.query("agent_d", filter, 100);
  REQUIRE(result.size() == 3);
}

// =====================================================================
// Test 5: query() max_count 限制
// =====================================================================
TEST_CASE("EventLogWriter::query max_count limits results",
          "[event_log][query][max_count]") {
  TempDirGuard tmp("query_max");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_e";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  for (uint64_t i = 1; i <= 10; ++i) {
    bus->emit(make_event("llm.request", i * 10));
  }
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  EventLogWriter::QueryFilter filter;
  filter.topic_glob = "*";
  filter.start_causal_time = 0;
  filter.end_causal_time = UINT64_MAX;
  auto result = writer.query("agent_e", filter, 3);
  REQUIRE(result.size() == 3);
}

// =====================================================================
// Test 6: query() 组合过滤 (topic glob + time window + max_count)
// =====================================================================
TEST_CASE("EventLogWriter::query combined filter",
          "[event_log][query][combined]") {
  TempDirGuard tmp("query_combined");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_f";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  for (uint64_t i = 1; i <= 20; ++i) {
    bus->emit(make_event(i % 2 == 0 ? "llm.request" : "tool.call", i * 10));
  }
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  EventLogWriter::QueryFilter filter;
  filter.topic_glob = "llm.*";
  filter.has_time_window = true;
  filter.start_causal_time = 50;
  filter.end_causal_time = 150;
  auto result = writer.query("agent_f", filter, 100);
  REQUIRE(result.size() == 5);  // causal_time 60, 80, 100, 120, 140 (5 events)
  for (const auto& e : result) {
    REQUIRE(e.topic == "llm.request");
    REQUIRE(e.causal_time >= 50);
    REQUIRE(e.causal_time <= 150);
  }
}

// =====================================================================
// Test 7: query() 空日志返回空
// =====================================================================
TEST_CASE("EventLogWriter::query empty log returns empty",
          "[event_log][query][empty_log]") {
  TempDirGuard tmp("query_empty");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_g";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  EventLogWriter::QueryFilter filter;
  filter.topic_glob = "*";
  auto result = writer.query("agent_g", filter, 100);
  REQUIRE(result.empty());
}

// =====================================================================
// Test 8: query() agent_id 不存在
// =====================================================================
TEST_CASE("EventLogWriter::query nonexistent agent returns empty",
          "[event_log][query][nonexistent_agent]") {
  TempDirGuard tmp("query_no_agent");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "real_agent";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  bus->emit(make_event("llm.request", 10));
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  EventLogWriter::QueryFilter filter;
  filter.topic_glob = "*";
  auto result = writer.query("nonexistent_agent", filter, 100);
  REQUIRE(result.empty());
}

// =====================================================================
// Test 9: format-damaged 行处理 (line-frame JSONL 容错)
// =====================================================================
TEST_CASE("EventLogWriter::read skips damaged JSONL lines",
          "[event_log][query][damaged_line]") {
  TempDirGuard tmp("damaged");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_h";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  bus->emit(make_event("llm.request", 10));
  bus->emit(make_event("llm.request", 20));
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  // 手动追加损坏行
  auto log_path = tmp.path / "agent_h.v1.jsonl";
  {
    std::ofstream out(log_path, std::ios::app);
    out << "{this is not valid json\n";
    out << "\n";
  }

  auto result = writer.read("agent_h", 0, UINT64_MAX);
  REQUIRE(result.size() == 2);
}

// =====================================================================
// Test 10: 现有 static read() 签名不变
// =====================================================================
TEST_CASE("EventLogWriter::read static signature unchanged",
          "[event_log][query][static_unchanged]") {
  TempDirGuard tmp("static_unchanged");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_i";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  bus->emit(make_event("llm.request", 10));
  bus->emit(make_event("llm.request", 20));
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  auto result = EventLogWriter::read("agent_i", tmp.path);
  REQUIRE(result.size() == 2);
}

// =====================================================================
// Test 11: query() time window filter (无 has_time_window 标志 → 不过滤)
// =====================================================================
TEST_CASE("EventLogWriter::query without time_window flag returns all time",
          "[event_log][query][no_time_filter]") {
  TempDirGuard tmp("no_time");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_j";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  bus->emit(make_event("llm.request", 10));
  bus->emit(make_event("llm.request", 1000));
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  EventLogWriter::QueryFilter filter;
  filter.topic_glob = "*";
  filter.has_time_window = false;
  filter.start_causal_time = 0;
  filter.end_causal_time = UINT64_MAX;
  auto result = writer.query("agent_j", filter, 100);
  REQUIRE(result.size() == 2);
}

// =====================================================================
// Test 12: query() glob 前缀匹配 (通配符位置在前缀)
// =====================================================================
TEST_CASE("EventLogWriter::query glob prefix matching",
          "[event_log][query][prefix]") {
  TempDirGuard tmp("prefix");
  auto bus = std::make_shared<agenticdsl::test::MockBus>();
  EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = "agent_k";
  cfg.event_log_dir = tmp.path;
  cfg.flush_interval = std::chrono::milliseconds(60000);  // 禁用后台 flush_thread 防止与 flush_sync 竞争
  EventLogWriter writer(cfg, bus);

  bus->emit(make_event("session.start", 10));
  bus->emit(make_event("session.end", 20));
  bus->emit(make_event("tool.call", 30));
  writer.stop();  // 替代 flush_sync(): stop 内部 join flush_thread 后再 flush_sync, 避免与后台线程竞争 file_

  EventLogWriter::QueryFilter filter;
  filter.topic_glob = "session.*";
  filter.start_causal_time = 0;
  filter.end_causal_time = UINT64_MAX;
  auto result = writer.query("agent_k", filter, 100);
  REQUIRE(result.size() == 2);
  for (const auto& e : result) {
    REQUIRE(e.topic.substr(0, 8) == "session.");
  }
}
