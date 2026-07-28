// tests/test_temporal_agent_signal_callback.cpp
// 功能描述：WorkflowCallbackChannel 测试 - Signal 双向通信 (long-poll)
//          验证 Workflow -> Agent 信号传递 + 多信号 + 异常隔离 + 停止清理
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.2
//          .rddf/plans/pkgm-temporal-agent.md Task 2
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "workflow_callback_channel.h"
#include "temporal_client.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using json = nlohmann::json;
using namespace pdk_temporal_agent;

// 辅助: 创建 backend + 启动 workflow (供信号测试)
static std::shared_ptr<InMemoryTemporalBackend> make_backend_with_wf(
    const std::string& wf_id) {
  auto backend = std::make_shared<InMemoryTemporalBackend>();
  backend->start_workflow_async("TestWorkflow", "task-queue", "{}", wf_id);
  return backend;
}

TEST_CASE("WorkflowCallbackChannel: receives Signal from Workflow (long-poll)",
          "[temporal_agent][signal]") {
  const std::string wf_id = "wf-signal-001";
  auto backend = make_backend_with_wf(wf_id);

  WorkflowCallbackChannel channel(wf_id);
  std::vector<json> received;
  channel.on_signal("ready_to_proceed", [&](const json& payload) {
    received.push_back(payload);
  });

  channel.start_polling(backend);
  backend->emit_signal(wf_id, "ready_to_proceed", {{"step", 1}});
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  channel.stop();

  REQUIRE(received.size() == 1);
  REQUIRE(received[0]["step"] == 1);
}

TEST_CASE("WorkflowCallbackChannel: multiple handlers for different signals",
          "[temporal_agent][signal]") {
  const std::string wf_id = "wf-signal-002";
  auto backend = make_backend_with_wf(wf_id);

  WorkflowCallbackChannel channel(wf_id);
  int proceed_count = 0;
  int cancel_count = 0;

  channel.on_signal("ready_to_proceed", [&](const json&) { ++proceed_count; });
  channel.on_signal("cancel", [&](const json&) { ++cancel_count; });

  channel.start_polling(backend);
  backend->emit_signal(wf_id, "ready_to_proceed", {{"v", 1}});
  backend->emit_signal(wf_id, "cancel", {{"reason", "user"}});
  backend->emit_signal(wf_id, "ready_to_proceed", {{"v", 2}});
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  channel.stop();

  REQUIRE(proceed_count == 2);
  REQUIRE(cancel_count == 1);
}

TEST_CASE("WorkflowCallbackChannel: stop terminates polling thread cleanly",
          "[temporal_agent][signal]") {
  const std::string wf_id = "wf-signal-003";
  auto backend = make_backend_with_wf(wf_id);

  WorkflowCallbackChannel channel(wf_id);
  channel.on_signal("noop", [](const json&) {});

  channel.start_polling(backend);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  channel.stop();

  // 析构不阻塞即证明 thread 已 join
  REQUIRE(true);
}

TEST_CASE("WorkflowCallbackChannel: handler exception does not crash poll loop",
          "[temporal_agent][signal]") {
  const std::string wf_id = "wf-signal-004";
  auto backend = make_backend_with_wf(wf_id);

  WorkflowCallbackChannel channel(wf_id);
  int ok_count = 0;

  channel.on_signal("bad", [](const json&) {
    throw std::runtime_error("handler boom");
  });
  channel.on_signal("good", [&](const json&) { ++ok_count; });

  channel.start_polling(backend);
  backend->emit_signal(wf_id, "bad", {});
  backend->emit_signal(wf_id, "good", {});
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  channel.stop();

  // bad handler 抛异常后, good 仍应被处理 (异常隔离)
  REQUIRE(ok_count == 1);
}

TEST_CASE("WorkflowCallbackChannel: multiple signals in sequence",
          "[temporal_agent][signal]") {
  const std::string wf_id = "wf-signal-005";
  auto backend = make_backend_with_wf(wf_id);

  WorkflowCallbackChannel channel(wf_id);
  std::vector<int> steps;

  channel.on_signal("step_done", [&](const json& payload) {
    steps.push_back(payload["n"].get<int>());
  });

  channel.start_polling(backend);
  for (int i = 1; i <= 5; ++i) {
    backend->emit_signal(wf_id, "step_done", {{"n", i}});
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  channel.stop();

  REQUIRE(steps.size() == 5);
  REQUIRE(steps[0] == 1);
  REQUIRE(steps[4] == 5);
}
