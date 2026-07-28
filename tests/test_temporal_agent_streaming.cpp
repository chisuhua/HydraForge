// tests/test_temporal_agent_streaming.cpp
// 功能描述：gRPC streaming 替代轮询测试
//          验证 stream_workflow_events 回调驱动 + poll_count 不增长 + 停止清理
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.3
//          .rddf/plans/pkgm-temporal-agent.md Task 3
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "temporal_client.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace pdk_temporal_agent;

static std::shared_ptr<InMemoryTemporalBackend> make_backend_with_wf(
    const std::string& wf_id) {
  auto backend = std::make_shared<InMemoryTemporalBackend>();
  backend->start_workflow_async("TestWorkflow", "task-queue", "{}", wf_id);
  return backend;
}

TEST_CASE("Streaming: callback fires on state change, poll_count stays 0",
          "[temporal_agent][streaming]") {
  const std::string wf_id = "wf-stream-001";
  auto backend = make_backend_with_wf(wf_id);

  std::atomic<int> callback_count{0};
  std::atomic<bool> stop_flag{false};

  std::thread stream_thread([&]() {
    backend->stream_workflow_events(wf_id, [&](const WorkflowResult& r) {
      callback_count.fetch_add(1, std::memory_order_relaxed);
      if (r.status == WorkflowStatus::Completed) {
        stop_flag.store(true, std::memory_order_relaxed);
      }
    }, stop_flag);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  backend->complete_workflow(wf_id, {{"output", "done"}});
  stream_thread.join();

  REQUIRE(callback_count.load() >= 1);
  REQUIRE(backend->get_poll_count(wf_id) == 0);
}

TEST_CASE("Streaming: receives multiple state transitions",
          "[temporal_agent][streaming]") {
  const std::string wf_id = "wf-stream-002";
  auto backend = make_backend_with_wf(wf_id);

  std::vector<WorkflowStatus> seen;
  std::atomic<bool> stop_flag{false};

  std::thread stream_thread([&]() {
    backend->stream_workflow_events(wf_id, [&](const WorkflowResult& r) {
      seen.push_back(r.status);
      if (r.status == WorkflowStatus::Completed) {
        stop_flag.store(true, std::memory_order_relaxed);
      }
    }, stop_flag);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  backend->advance_workflow(wf_id, WorkflowStatus::Running);
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  backend->complete_workflow(wf_id, {{"final", true}});

  stream_thread.join();

  REQUIRE_FALSE(seen.empty());
  REQUIRE(seen.back() == WorkflowStatus::Completed);
}

TEST_CASE("Streaming: stop flag terminates stream cleanly",
          "[temporal_agent][streaming]") {
  const std::string wf_id = "wf-stream-003";
  auto backend = make_backend_with_wf(wf_id);

  std::atomic<bool> stop_flag{false};

  auto start = std::chrono::steady_clock::now();
  std::thread stream_thread([&]() {
    backend->stream_workflow_events(wf_id, [](const WorkflowResult&) {},
                                     stop_flag);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop_flag.store(true, std::memory_order_relaxed);
  stream_thread.join();
  auto elapsed = std::chrono::steady_clock::now() - start;

  REQUIRE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < 500);
}

TEST_CASE("Streaming and poll can coexist on same workflow",
          "[temporal_agent][streaming]") {
  const std::string wf_id = "wf-stream-004";
  auto backend = make_backend_with_wf(wf_id);

  std::atomic<bool> stop_flag{false};
  std::atomic<int> stream_events{0};

  std::thread stream_thread([&]() {
    backend->stream_workflow_events(wf_id, [&](const WorkflowResult&) {
      stream_events.fetch_add(1, std::memory_order_relaxed);
    }, stop_flag);
  });

  backend->poll(wf_id, 100);
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  backend->complete_workflow(wf_id, {});
  stream_thread.join();

  REQUIRE(backend->get_poll_count(wf_id) >= 1);
  REQUIRE(stream_events.load() >= 1);
}
