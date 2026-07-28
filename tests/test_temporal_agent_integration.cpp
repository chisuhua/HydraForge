// tests/test_temporal_agent_integration.cpp
// 功能描述：Temporal Agent Plugin 端到端集成测试
//          需要 Temporal dev server (temporalite / temporal-cli dev server)
//          无 dev server 时自动 SKIP (#ifdef TEMPORAL_DEV_SERVER)
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §5.3
// 作者：pkgm-temporal-agent change
// 最后修改日期：2026-07-27

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/plugin/plugin_info.h"
#include "common/policy/execution_policy.h"
#include "temporal_client.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <unordered_map>

using json = nlohmann::json;
using namespace pdk_temporal_agent;

namespace {

class MockToolRegistry : public ::agenticdsl::IToolRegistry {
 public:
  std::vector<std::string> registered_tools;
  std::unordered_map<std::string, ::agenticdsl::ToolMetadata> tool_metas;
  std::unordered_map<std::string, ToolFunc> tool_funcs;

  void register_tool_function(
      std::string name,
      ::agenticdsl::ToolMetadata meta,
      ToolFunc fn) override {
    registered_tools.push_back(name);
    tool_metas[name] = meta;
    tool_funcs[name] = fn;
  }

  bool has_tool(const std::string& name) const override {
    return tool_funcs.find(name) != tool_funcs.end();
  }

  json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) override {
    auto it = tool_funcs.find(name);
    if (it == tool_funcs.end()) {
      return {{"error", "tool not found"}, {"tool", name}};
    }
    return it->second(args);
  }

  std::vector<std::string> list_tools() const override { return registered_tools; }
  void register_llm_tool(std::string, std::unique_ptr<::agenticdsl::ILLMTool>,
                         const ::agenticdsl::LLMParams&) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const ::agenticdsl::LLMParams& get_llm_params(const std::string&) const override {
    static const ::agenticdsl::LLMParams kEmpty{};
    return kEmpty;
  }
  json call_llm_tool(const std::string&, const std::string&,
                     const ::agenticdsl::LLMParams&) override { return {}; }
  void set_cost_callback(::agenticdsl::IToolRegistry::CostCallback) override {}
};

}  // namespace

extern "C" const hydraforge::PluginInfo pdk_plugin_info;
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry);

// === 无 Temporal dev server 的模拟集成测试 (始终运行) ===

TEST_CASE("temporal_agent integration: start + complete via InMemory backend",
          "[temporal_agent][integration]") {
  auto backend = std::make_unique<InMemoryTemporalBackend>();
  auto* raw = backend.get();
  auto& client = TemporalClient::instance();
  client.set_backend(std::move(backend));
  client.connect("localhost:7233");

  client.start_workflow_async("DelayWorkflow", "tq", "{}", "wf-int-001");

  raw->complete_workflow("wf-int-001", R"({"result":"completed"})"_json);

  auto result = client.poll("wf-int-001", 500);
  REQUIRE(result.status == WorkflowStatus::Completed);
  REQUIRE(result.result["result"] == "completed");
  REQUIRE(result.history_size_bytes > 0);
  REQUIRE(result.event_count >= 2);

  client.shutdown();
}

TEST_CASE("temporal_agent integration: idempotency same workflow_id",
          "[temporal_agent][integration]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  auto r1 = client.start_workflow_async("Wf", "tq", "{}", "wf-idem-int-001");
  auto r2 = client.start_workflow_async("Wf", "tq", "{}", "wf-idem-int-001");

  REQUIRE(r1.workflow_id == r2.workflow_id);
  REQUIRE(r1.run_id == r2.run_id);
  REQUIRE(r1.status == r2.status);

  client.shutdown();
}

TEST_CASE("temporal_agent integration: not found error scenario",
          "[temporal_agent][integration]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  REQUIRE_THROWS_AS(client.poll("nonexistent-wf-id", 100), TemporalError);

  try {
    client.poll("nonexistent-wf-id", 100);
  } catch (const TemporalError& e) {
    REQUIRE(e.code == GrpcError::NotFound);
  }

  client.shutdown();
}

TEST_CASE("temporal_agent integration: signal increases history size",
          "[temporal_agent][integration]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  client.start_workflow_async("Wf", "tq", "{}", "wf-sig-int-001");
  auto before = client.query("wf-sig-int-001");

  bool ok = client.signal("wf-sig-int-001", "cancel_signal", "{}");
  REQUIRE(ok);

  auto after = client.query("wf-sig-int-001");
  REQUIRE(after.event_count > before.event_count);
  REQUIRE(after.history_size_bytes > before.history_size_bytes);

  client.shutdown();
}

TEST_CASE("temporal_agent integration: tool registration via pdk_register_tools",
          "[temporal_agent][integration]") {
  MockToolRegistry registry;
  pdk_register_tools(registry);

  REQUIRE(registry.has_tool("temporal/start_workflow"));
  REQUIRE(registry.has_tool("temporal/start_async"));
  REQUIRE(registry.has_tool("temporal/poll"));
  REQUIRE(registry.has_tool("temporal/signal"));
  REQUIRE(registry.has_tool("temporal/query"));
}

TEST_CASE("temporal_agent integration: call temporal/start_async tool via registry",
          "[temporal_agent][integration]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  MockToolRegistry registry;
  pdk_register_tools(registry);

  std::unordered_map<std::string, std::string> args = {
    {"workflow_type", "TestWorkflow"},
    {"task_queue", "test-queue"},
    {"input_json", R"({"test":true})"},
    {"workflow_id", "wf-tool-001"}
  };

  auto result = registry.call_tool("temporal/start_async", args);
  REQUIRE(result["workflow_id"] == "wf-tool-001");
  REQUIRE(result["status"] == "RUNNING");

  client.shutdown();
}

TEST_CASE("temporal_agent integration: call temporal/poll tool via registry",
          "[temporal_agent][integration]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  client.start_workflow_async("Wf", "tq", "{}", "wf-poll-tool-001");

  MockToolRegistry registry;
  pdk_register_tools(registry);

  std::unordered_map<std::string, std::string> args = {
    {"workflow_id", "wf-poll-tool-001"},
    {"timeout_ms", "200"}
  };

  auto result = registry.call_tool("temporal/poll", args);
  REQUIRE(result["workflow_id"] == "wf-poll-tool-001");
  REQUIRE(result["status"] == "RUNNING");

  client.shutdown();
}

TEST_CASE("temporal_agent integration: call temporal/query tool via registry",
          "[temporal_agent][integration]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  client.start_workflow_async("Wf", "tq", "{}", "wf-query-tool-001");

  MockToolRegistry registry;
  pdk_register_tools(registry);

  std::unordered_map<std::string, std::string> args = {
    {"workflow_id", "wf-query-tool-001"}
  };

  auto result = registry.call_tool("temporal/query", args);
  REQUIRE(result["workflow_id"] == "wf-query-tool-001");
  REQUIRE(result["event_count"] >= 1);

  client.shutdown();
}

TEST_CASE("temporal_agent integration: call temporal/signal tool via registry",
          "[temporal_agent][integration]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  client.start_workflow_async("Wf", "tq", "{}", "wf-signal-tool-001");

  MockToolRegistry registry;
  pdk_register_tools(registry);

  std::unordered_map<std::string, std::string> args = {
    {"workflow_id", "wf-signal-tool-001"},
    {"signal_name", "update"},
    {"input_json", R"({"val":42})"}
  };

  auto result = registry.call_tool("temporal/signal", args);
  REQUIRE(result["ok"] == true);
  REQUIRE(result["signal_name"] == "update");

  client.shutdown();
}

// === 真实 Temporal dev server 测试 (仅 #ifdef TEMPORAL_DEV_SERVER 时编译) ===

#ifdef TEMPORAL_DEV_SERVER

TEST_CASE("temporal_agent live: blocking workflow end-to-end", "[temporal_agent][live]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  auto result = client.start_workflow_blocking(
      "DelayWorkflow", "task-queue", R"({"delay_ms":1000})",
      "wf-live-block-001", 10000);

  REQUIRE(result.status == WorkflowStatus::Completed);
  REQUIRE(result.history_size_bytes > 0);
  REQUIRE(result.event_count > 1);

  client.shutdown();
}

TEST_CASE("temporal_agent live: async + poll to completion", "[temporal_agent][live]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  client.start_workflow_async(
      "DelayWorkflow", "task-queue", R"({"delay_ms":5000})",
      "wf-live-async-001");

  auto result = client.poll("wf-live-async-001", 15000);
  REQUIRE(result.status == WorkflowStatus::Completed);

  client.shutdown();
}

TEST_CASE("temporal_agent live: idempotency same workflow_id", "[temporal_agent][live]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  auto r1 = client.start_workflow_async(
      "DelayWorkflow", "task-queue", "{}", "wf-live-idem-001");

  auto r2 = client.start_workflow_async(
      "DelayWorkflow", "task-queue", "{}", "wf-live-idem-001");

  REQUIRE(r1.run_id == r2.run_id);

  client.shutdown();
}

TEST_CASE("temporal_agent live: not found error", "[temporal_agent][live]") {
  auto& client = TemporalClient::instance();
  client.connect("localhost:7233");

  try {
    client.poll("nonexistent-wf-id", 1000);
    FAIL("Expected TemporalError");
  } catch (const TemporalError& e) {
    REQUIRE(e.code == GrpcError::NotFound);
  }

  client.shutdown();
}

#endif  // TEMPORAL_DEV_SERVER
