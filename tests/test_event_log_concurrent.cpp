// tests/test_event_log_concurrent.cpp
// fix-tsan-residual-2026-09-15 regression guard: EventLogWriter concurrent flush_loop + flush_sync
// 关联: openspec/changes/fix-tsan-residual-2026-09-15/proposal.md (R1)
//       AGENTS.md 模式 #7 v2 — 1456752 SessionWriter 同模式首次应用
//
// 验证: 多线程并发灌入 buffer + 显式 flush_sync + 后台 flush_loop 不丢记录。
// Pre-fix: 无 file_mutex_ 时 flush_sync 可能见 buffer 空早返回，但 flush_loop
// 写入 file_ 未完成 → test read() 丢记录 (per SessionWriter fix 1456752 同 pattern)。

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "core/event_log.h"
#include "core/types/event_log_config.h"
#include "core/types/tool_result.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

class TempDirGuard {
 public:
  explicit TempDirGuard(const std::string& tag) : path_(fs::temp_directory_path() /
                                                       ("evlog_concurrent_" + tag +
                                                        "_" + std::to_string(::getpid()) +
                                                        "_" +
                                                        std::to_string(reinterpret_cast<std::uintptr_t>(this)))) {
    fs::create_directories(path_);
  }
  ~TempDirGuard() { fs::remove_all(path_); }
  fs::path path() const { return path_; }
 private:
  fs::path path_;
};

}  // namespace

TEST_CASE("EventLogWriter concurrent flush_loop + flush_sync loses no records",
          "[event_log][writer][concurrent][tsan]") {
  TempDirGuard tmp("concurrent_fuzz");
  const std::string agent_id = "agent-concurrent-fuzz";

  auto bus = std::make_shared<agenticdsl::InMemoryBus>();
  agenticdsl::EventLogConfig cfg;
  cfg.event_log_enabled = true;
  cfg.event_log_agent_id = agent_id;
  cfg.event_log_dir = tmp.path();
  // 短 interval 让 flush_loop 频繁触发, 加大与 flush_sync 并发概率
  cfg.flush_interval = std::chrono::milliseconds(5);

  constexpr int kThreads = 7;
  constexpr int kEventsPerThread = 50;
  constexpr int kTotalEvents = kThreads * kEventsPerThread;

  {
    agenticdsl::EventLogWriter writer(cfg, bus);

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
      threads.emplace_back([&, t]() {
        ready.fetch_add(1, std::memory_order_release);
        while (!go.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }
        for (int i = 0; i < kEventsPerThread; ++i) {
          agenticdsl::BusEvent ev;
          ev.topic = "concurrent.test";
          ev.causal_time = static_cast<std::uint64_t>(t * 1000 + i);
          ev.payload.data = {{"thread", t}, {"index", i}};
          // 通过 bus emit 触发 EventLogWriter 的 subscribe 回调 (on_bus_event 私有).
          // 这样测试覆盖与生产相同的 path: bus emit → on_bus_event → buffer push → notify.
          bus->emit(ev);
        }
      });
    }
    while (ready.load(std::memory_order_acquire) < kThreads) {
      std::this_thread::yield();
    }
    go.store(true, std::memory_order_release);

    // 主线程并发调 flush_sync 模拟外部消费者
    for (int sync_round = 0; sync_round < 10; ++sync_round) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      writer.flush_sync();
    }
    for (auto& th : threads) th.join();
    writer.flush_sync();  // 最终 flush
  }  // writer dtor calls stop() → flush_loop join + final flush_sync + file close

  // 验证 records 数 ≥ kTotalEvents
  const auto path = tmp.path() / (agent_id + ".v1.jsonl");
  REQUIRE(fs::exists(path));

  std::ifstream in(path);
  REQUIRE(in.is_open());
  int line_count = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) ++line_count;
  }
  // Pre-fix: ~14% records.size() < kTotalEvents (per SessionWriter case study baseline)
  // Post-fix: 必须 100% — 所有 events 都已 flush 到文件
  REQUIRE(line_count >= kTotalEvents);
}