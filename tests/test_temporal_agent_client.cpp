// tests/test_temporal_agent_client.cpp
// 功能描述：Temporal Agent 客户端测试 - mock gRPC 连接 + 错误码映射 + 幂等性
//          使用 InMemoryTemporalBackend (进程内模拟, 无需真实 Temporal 服务器)
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §5.2
// 作者：pkgm-temporal-agent change
// 最后修改日期：2026-07-27

#include "catch_amalgamated.hpp"

#include "temporal_client.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <thread>

using json = nlohmann::json;
using namespace pdk_temporal_agent;

TEST_CASE("temporal_client: singleton instance", "[temporal_agent][client]") {
  auto& c1 = TemporalClient::instance();
  auto& c2 = TemporalClient::instance();
  REQUIRE(&c1 == &c2);
}

TEST_CASE("temporal_client: connect and shutdown", "[temporal_agent][client]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");
  REQUIRE(client.is_connected() == true);
  client.shutdown();
  REQUIRE(client.is_connected() == false);
}

TEST_CASE("temporal_client: start_async returns Running status", "[temporal_agent][client]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  auto result = client.start_workflow_async(
      "ExampleWorkflow", "task-queue-1", R"({"key":"value"})", "wf-test-001");

  REQUIRE(result.workflow_id == "wf-test-001");
  REQUIRE(!result.run_id.empty());
  REQUIRE(result.status == WorkflowStatus::Running);
  REQUIRE(result.event_count >= 1);

  client.shutdown();
}

TEST_CASE("temporal_client: idempotency - same workflow_id returns same result",
          "[temporal_agent][client]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  auto r1 = client.start_workflow_async(
      "ExampleWorkflow", "task-queue-1", "{}", "wf-idem-001");

  auto r2 = client.start_workflow_async(
      "ExampleWorkflow", "task-queue-1", "{}", "wf-idem-001");

  REQUIRE(r1.workflow_id == r2.workflow_id);
  REQUIRE(r1.run_id == r2.run_id);
  REQUIRE(r1.status == r2.status);

  client.shutdown();
}

TEST_CASE("temporal_client: poll returns NotFound for unknown workflow",
          "[temporal_agent][client]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  REQUIRE_THROWS_AS(
      client.poll("nonexistent-workflow-id", 100),
      TemporalError);

  try {
    client.poll("nonexistent-workflow-id", 100);
  } catch (const TemporalError& e) {
    REQUIRE(e.code == GrpcError::NotFound);
  }

  client.shutdown();
}

TEST_CASE("temporal_client: query returns NotFound for unknown workflow",
          "[temporal_agent][client]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  REQUIRE_THROWS_AS(
      client.query("nonexistent-workflow-id"),
      TemporalError);

  try {
    client.query("nonexistent-workflow-id");
  } catch (const TemporalError& e) {
    REQUIRE(e.code == GrpcError::NotFound);
  }

  client.shutdown();
}

TEST_CASE("temporal_client: signal on unknown workflow throws NotFound",
          "[temporal_agent][client]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  REQUIRE_THROWS_AS(
      client.signal("nonexistent-wf", "cancel", "{}"),
      TemporalError);

  client.shutdown();
}

TEST_CASE("temporal_client: signal increments event_count",
          "[temporal_agent][client]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  client.start_workflow_async("Wf", "tq", "{}", "wf-sig-001");
  auto before = client.query("wf-sig-001");

  bool ok = client.signal("wf-sig-001", "update_signal", R"({"data":"x"})");
  REQUIRE(ok == true);

  auto after = client.query("wf-sig-001");
  REQUIRE(after.event_count > before.event_count);

  client.shutdown();
}

TEST_CASE("temporal_client: GrpcError to prefix mapping",
          "[temporal_agent][client]") {
  TemporalError not_found(GrpcError::NotFound, "wf missing");
  REQUIRE(std::string(not_found.what()) == "wf missing");
  REQUIRE(not_found.code == GrpcError::NotFound);

  TemporalError timeout(GrpcError::DeadlineExceeded, "timed out");
  REQUIRE(timeout.code == GrpcError::DeadlineExceeded);

  TemporalError unavail(GrpcError::Unavailable, "server down");
  REQUIRE(unavail.code == GrpcError::Unavailable);
}

TEST_CASE("temporal_client: workflow_status_str mapping",
          "[temporal_agent][client]") {
  REQUIRE(std::string(workflow_status_str(WorkflowStatus::Running)) == "RUNNING");
  REQUIRE(std::string(workflow_status_str(WorkflowStatus::Completed)) == "COMPLETED");
  REQUIRE(std::string(workflow_status_str(WorkflowStatus::Failed)) == "FAILED");
  REQUIRE(std::string(workflow_status_str(WorkflowStatus::Cancelled)) == "CANCELLED");
  REQUIRE(std::string(workflow_status_str(WorkflowStatus::TimedOut)) == "TIMED_OUT");
  REQUIRE(std::string(workflow_status_str(WorkflowStatus::Unknown)) == "UNKNOWN");
}

TEST_CASE("temporal_client: InMemory backend manual workflow completion",
          "[temporal_agent][client]") {
  auto backend = std::make_unique<InMemoryTemporalBackend>();
  auto* raw_backend = backend.get();
  auto& client = TemporalClient::instance();
  client.set_backend(std::move(backend));
  client.connect("localhost:7233");

  client.start_workflow_async("Wf", "tq", "{}", "wf-manual-001");

  raw_backend->complete_workflow("wf-manual-001", R"({"output":"done"})"_json);

  auto result = client.query("wf-manual-001");
  REQUIRE(result.status == WorkflowStatus::Completed);
  REQUIRE(result.result["output"] == "done");

  client.shutdown();
}

TEST_CASE("temporal_client: InMemory backend workflow failure",
          "[temporal_agent][client]") {
  auto backend = std::make_unique<InMemoryTemporalBackend>();
  auto* raw_backend = backend.get();
  auto& client = TemporalClient::instance();
  client.set_backend(std::move(backend));
  client.connect("localhost:7233");

  client.start_workflow_async("Wf", "tq", "{}", "wf-fail-001");

  raw_backend->fail_workflow("wf-fail-001", "activity timeout");

  auto result = client.query("wf-fail-001");
  REQUIRE(result.status == WorkflowStatus::Failed);
  REQUIRE(result.failure_reason == "activity timeout");

  client.shutdown();
}

TEST_CASE("temporal_client: event emitter fires on start_async",
          "[temporal_agent][client][events]") {
  auto& client = TemporalClient::instance();

  std::string captured_event;
  json captured_payload;
  int emit_count = 0;

  client.set_event_emitter([&](const std::string& event_type, const json& payload) {
    captured_event = event_type;
    captured_payload = payload;
    emit_count++;
  });

  client.connect("localhost:7233");
  client.start_workflow_async("Wf", "tq", "{}", "wf-evt-001");

  REQUIRE(emit_count >= 1);
  REQUIRE(captured_event == "temporal.workflow.start");
  REQUIRE(captured_payload["workflow_id"] == "wf-evt-001");

  client.set_event_emitter(nullptr);
  client.shutdown();
}

TEST_CASE("temporal_client: event emitter fires complete after backend completion",
          "[temporal_agent][client][events]") {
  auto backend = std::make_unique<InMemoryTemporalBackend>();
  auto* raw_backend = backend.get();
  auto& client = TemporalClient::instance();
  client.set_backend(std::move(backend));

  json last_payload;
  std::string last_event;
  int complete_count = 0;

  client.set_event_emitter([&](const std::string& event_type, const json& payload) {
    last_event = event_type;
    last_payload = payload;
    if (event_type == "temporal.workflow.complete") {
      complete_count++;
    }
  });

  client.connect("localhost:7233");

  client.start_workflow_async("Wf", "tq", "{}", "wf-evt-complete-001");
  raw_backend->complete_workflow("wf-evt-complete-001", R"({"done":true})"_json);

  auto result = client.poll("wf-evt-complete-001", 1000);

  REQUIRE(complete_count >= 1);
  REQUIRE(last_event == "temporal.workflow.complete");
  REQUIRE(last_payload["status"] == "COMPLETED");

  client.set_event_emitter(nullptr);
  client.shutdown();
}
