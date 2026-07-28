// pdk/temporal_agent/tests/test_mock_client.cpp
// 功能描述：MockTemporalClient 状态机单元测试 (Task 2, TDD Step 1)。
//          覆盖:
//            1. 状态转换 CREATED -> RUNNING -> COMPLETED (基于延迟)
//            2. 幂等性: 重复 workflow_id 返回 idempotent_replay
//            3. 延迟模拟: set_simulated_latency + advance_time
//            4. signal 处理: 追加 signal 到 record
//            5. query 只读元数据
//            6. start_workflow_blocking 阻塞轮询直到完成
//            7. 不存在 workflow_id 的错误路径
//            8. FAILED 状态转换
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 2 Step 1
// 作者：pkm-temporal-demo-scaffold Task 2
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/itemporal_client.h"
#include "mock_client.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <string>
#include <thread>

using json = nlohmann::json;
using agenticdsl::pdk::ITemporalClient;
using agenticdsl::pdk::MockTemporalClient;
using agenticdsl::pdk::WorkflowState;

namespace {

// 辅助: 将 WorkflowState enum 转 string (用于 assertion 可读性)
const char* state_str(WorkflowState s) {
  switch (s) {
    case WorkflowState::CREATED:   return "CREATED";
    case WorkflowState::RUNNING:   return "RUNNING";
    case WorkflowState::COMPLETED: return "COMPLETED";
    case WorkflowState::FAILED:    return "FAILED";
  }
  return "UNKNOWN";
}

} // namespace

// ============================================================================
// Test 1: 状态转换 CREATED -> RUNNING -> COMPLETED (advance_time 触发完成)
// ============================================================================
TEST_CASE("MockTemporalClient: CREATED -> RUNNING -> COMPLETED transition",
          "[pdk][temporal][task2][state]") {
  MockTemporalClient client;
  auto start = client.start_workflow_async("wf-1", {{"task", "noop"}});
  REQUIRE(start["state"] == "RUNNING");
  REQUIRE(start["workflow_id"] == "wf-1");

  // 未推进时间 -> 仍 RUNNING
  auto poll_before = client.poll("wf-1");
  REQUIRE(poll_before["state"] == "RUNNING");

  // 推进时间超过延迟阈值 -> COMPLETED
  client.advance_time(std::chrono::milliseconds(100));
  auto poll_after = client.poll("wf-1");
  REQUIRE(poll_after["state"] == "COMPLETED");
  REQUIRE(poll_after["workflow_id"] == "wf-1");
}

// ============================================================================
// Test 2: 幂等性 - 重复 workflow_id 返回 idempotent_replay
// ============================================================================
TEST_CASE("MockTemporalClient: idempotency on duplicate workflow_id",
          "[pdk][temporal][task2][idempotency]") {
  MockTemporalClient client;

  auto first = client.start_workflow_async("dup", {{"task", "x"}});
  REQUIRE(first["state"] == "RUNNING");
  REQUIRE_FALSE(first.value("idempotent_replay", false));

  // 同 ID 第二次启动 -> 幂等重放
  auto second = client.start_workflow_async("dup", {{"task", "y"}});
  REQUIRE(second["idempotent_replay"] == true);
  REQUIRE(second["original_workflow_id"] == "dup");
  REQUIRE(second["state"] == "RUNNING");
}

// ============================================================================
// Test 3: 延迟模拟 - set_simulated_latency 控制完成时间
// ============================================================================
TEST_CASE("MockTemporalClient: simulated latency controls completion timing",
          "[pdk][temporal][task2][latency]") {
  MockTemporalClient client;
  client.set_simulated_latency(std::chrono::milliseconds(500));

  client.start_workflow_async("lat-wf", {{"task", "slow"}});
  REQUIRE(client.poll("lat-wf")["state"] == "RUNNING");

  // 499ms 仍 RUNNING
  client.advance_time(std::chrono::milliseconds(499));
  REQUIRE(client.poll("lat-wf")["state"] == "RUNNING");

  // 500ms -> COMPLETED
  client.advance_time(std::chrono::milliseconds(1));
  REQUIRE(client.poll("lat-wf")["state"] == "COMPLETED");
}

// ============================================================================
// Test 4: signal 处理 - 追加 signal payload 到 record
// ============================================================================
TEST_CASE("MockTemporalClient: signal appends to workflow record",
          "[pdk][temporal][task2][signal]") {
  MockTemporalClient client;
  client.start_workflow_async("sig-wf", {{"task", "wait_signal"}});

  auto sig = client.signal("sig-wf", "go", {{"value", 42}});
  REQUIRE(sig["workflow_id"] == "sig-wf");
  REQUIRE(sig["signal_name"] == "go");
  REQUIRE(sig["ack"] == true);

  // query 验证 signal 已记录
  auto q = client.query("sig-wf", "signals");
  REQUIRE(q["count"] == 1);
  REQUIRE(q["signals"][0]["name"] == "go");
  REQUIRE(q["signals"][0]["payload"]["value"] == 42);
}

// ============================================================================
// Test 5: query 只读元数据
// ============================================================================
TEST_CASE("MockTemporalClient: query returns readonly metadata",
          "[pdk][temporal][task2][query]") {
  MockTemporalClient client;
  client.start_workflow_async("q-wf", {{"task", "inspect"}, {"tag", "test"}});

  auto q = client.query("q-wf", "status");
  REQUIRE(q["workflow_id"] == "q-wf");
  REQUIRE(q["state"] == "RUNNING");
  REQUIRE(q["args"]["task"] == "inspect");
  // query 不改变状态
  REQUIRE(client.poll("q-wf")["state"] == "RUNNING");
}

// ============================================================================
// Test 6: start_workflow_blocking 阻塞直到完成
// ============================================================================
TEST_CASE("MockTemporalClient: start_workflow_blocking returns COMPLETED",
          "[pdk][temporal][task2][blocking]") {
  MockTemporalClient client;
  client.set_simulated_latency(std::chrono::milliseconds(50));

  auto result = client.start_workflow_blocking("block-wf", {{"task", "sync"}});
  REQUIRE(result["state"] == "COMPLETED");
  REQUIRE(result["workflow_id"] == "block-wf");
}

// ============================================================================
// Test 7: 不存在的 workflow_id 错误路径
// ============================================================================
TEST_CASE("MockTemporalClient: unknown workflow_id returns error",
          "[pdk][temporal][task2][error]") {
  MockTemporalClient client;

  auto poll = client.poll("nonexistent");
  REQUIRE(poll["error"] == "workflow_not_found");
  REQUIRE(poll["workflow_id"] == "nonexistent");

  auto sig = client.signal("nonexistent", "go", json::object());
  REQUIRE(sig["error"] == "workflow_not_found");

  auto q = client.query("nonexistent", "status");
  REQUIRE(q["error"] == "workflow_not_found");
}

// ============================================================================
// Test 8: FAILED 状态转换 (args 含 fail=true)
// ============================================================================
TEST_CASE("MockTemporalClient: FAILED state transition",
          "[pdk][temporal][task2][failed]") {
  MockTemporalClient client;
  client.start_workflow_async("fail-wf", {{"task", "doom"}, {"fail", true}});

  client.advance_time(std::chrono::milliseconds(100));
  auto poll = client.poll("fail-wf");
  REQUIRE(poll["state"] == "FAILED");
}

// ============================================================================
// Test 9: 多 workflow 并发管理
// ============================================================================
TEST_CASE("MockTemporalClient: manages multiple workflows independently",
          "[pdk][temporal][task2][multi]") {
  MockTemporalClient client;
  client.set_simulated_latency(std::chrono::milliseconds(200));

  client.start_workflow_async("wf-a", {{"task", "a"}});
  client.start_workflow_async("wf-b", {{"task", "b"}});

  // 仅推进部分时间, 两 wf 都 RUNNING
  client.advance_time(std::chrono::milliseconds(100));
  REQUIRE(client.poll("wf-a")["state"] == "RUNNING");
  REQUIRE(client.poll("wf-b")["state"] == "RUNNING");

  // 再推进 -> 都 COMPLETED
  client.advance_time(std::chrono::milliseconds(100));
  REQUIRE(client.poll("wf-a")["state"] == "COMPLETED");
  REQUIRE(client.poll("wf-b")["state"] == "COMPLETED");
}

// ============================================================================
// Test 10: 多态接口 - MockTemporalClient 通过 ITemporalClient& 调用
// ============================================================================
TEST_CASE("MockTemporalClient: accessible via ITemporalClient& polymorphic",
          "[pdk][temporal][task2][polymorphic]") {
  MockTemporalClient mock;
  ITemporalClient& client = mock;

  client.start_workflow_async("poly-wf", {{"task", "x"}});
  auto poll = client.poll("poly-wf");
  REQUIRE(poll["state"] == "RUNNING");

  mock.advance_time(std::chrono::milliseconds(100));
  auto poll2 = client.poll("poly-wf");
  REQUIRE(poll2["state"] == "COMPLETED");
}
