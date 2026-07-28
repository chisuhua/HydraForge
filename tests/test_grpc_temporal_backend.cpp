// tests/test_grpc_temporal_backend.cpp
// 功能描述：GrpcTemporalBackend 测试 - 条件编译 + InMemory fallback 委托验证
//          默认 TEMPORAL_ENABLE_GRPC=OFF, 所有方法委托到 InMemoryTemporalBackend。
//          gRPC 路径需安装 dev 包后 -DTEMPORAL_ENABLE_GRPC=ON 编译。
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.6
//          .rddf/plans/pkgm-temporal-agent.md Task 5
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "catch_amalgamated.hpp"

#include "grpc_temporal_backend.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace pdk_temporal_agent;

TEST_CASE("GrpcTemporalBackend: header compiles and class is instantiable",
          "[temporal_agent][grpc_backend]") {
  GrpcTemporalBackend backend("localhost:7233");
  REQUIRE(true);
}

TEST_CASE("GrpcTemporalBackend: connect sets connected state",
          "[temporal_agent][grpc_backend]") {
  GrpcTemporalBackend backend("localhost:7233");
  REQUIRE_FALSE(backend.is_connected());
  backend.connect();
  REQUIRE(backend.is_connected());
}

TEST_CASE("GrpcTemporalBackend: start_workflow_async delegates to fallback",
          "[temporal_agent][grpc_backend]") {
  GrpcTemporalBackend backend("localhost:7233");
  backend.connect();

  auto result = backend.start_workflow_async(
      "TestWorkflow", "task-queue", R"({"key":"value"})", "wf-grpc-001");

  REQUIRE(result.workflow_id == "wf-grpc-001");
  REQUIRE(!result.run_id.empty());
  REQUIRE(result.status == WorkflowStatus::Running);
}

TEST_CASE("GrpcTemporalBackend: poll and complete via fallback",
          "[temporal_agent][grpc_backend]") {
  GrpcTemporalBackend backend("localhost:7233");
  backend.connect();

  backend.start_workflow_async("Wf", "tq", "{}", "wf-grpc-002");
  backend.complete_workflow("wf-grpc-002", {{"done", true}});

  auto result = backend.poll("wf-grpc-002", 1000);
  REQUIRE(result.status == WorkflowStatus::Completed);
  REQUIRE(result.result["done"] == true);
}

TEST_CASE("GrpcTemporalBackend: signal + query + get_poll_count via fallback",
          "[temporal_agent][grpc_backend]") {
  GrpcTemporalBackend backend("localhost:7233");
  backend.connect();

  backend.start_workflow_async("Wf", "tq", "{}", "wf-grpc-003");

  REQUIRE(backend.signal("wf-grpc-003", "step_done", R"({"n":1})"));

  auto q = backend.query("wf-grpc-003");
  REQUIRE(q.workflow_id == "wf-grpc-003");
  REQUIRE(q.event_count >= 1);

  backend.poll("wf-grpc-003", 100);
  REQUIRE(backend.get_poll_count("wf-grpc-003") >= 1);
}
