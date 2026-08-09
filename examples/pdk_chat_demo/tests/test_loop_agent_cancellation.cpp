// tests/test_loop_agent_cancellation.cpp
// Phase B Step 3: loop_agent cancellation chain integration tests
// Tests: cancellation_id parsing, NodeExecutor token forwarding, ToolCoordinator short-circuit

#include "catch_amalgamated.hpp"

#include <memory>
#include <unordered_map>
#include <stop_token>

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/policy/iexecution_policy.h"
#include "common/policy/agent_mode_policy.h"
#include "common/policy/approval_callbacks.h"
#include "common/tools/tool_coordinator.h"

using namespace agenticdsl;

namespace {

class MockToolRegistry : public IToolRegistry {
 public:
  nlohmann::json call_tool(
      const std::string&,
      const std::unordered_map<std::string, std::string>&) override {
    return {{"mock_result", "ok"}};
  }
  bool has_tool(const std::string&) const override { return true; }
  std::vector<std::string> list_tools() const override { return {"test_tool"}; }
  void register_tool_function(std::string, ToolMetadata, ToolFunc) override {}
  void register_llm_tool(std::string, std::unique_ptr<ILLMTool>,
                         const LLMParams&) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const LLMParams& get_llm_params(const std::string&) const override {
    static LLMParams p;
    return p;
  }
  nlohmann::json call_llm_tool(const std::string&, const std::string&,
                               const LLMParams&) override {
    return nullptr;
  }
  void set_cost_callback(CostCallback) override {}
};

}  // namespace

TEST_CASE("ToolCoordinator short-circuits on cancelled token + emits audit event",
          "[tool_coordinator][cancellation]") {
  MockToolRegistry registry;
  auto bus = std::make_shared<InMemoryBus>();
  auto policy = std::make_shared<AgentModePolicy>();
  ToolCoordinator coord(registry, policy, make_test_auto_callback(true), bus);

  std::vector<BusEvent> events;
  bus->subscribe("tool.audit.denied",
                 [&](const BusEvent& ev) { events.push_back(ev); });

  ToolMetadata meta;
  meta.name = "test_tool";
  meta.category = ToolCategory::ReadOnly;
  meta.min_layer = LayerProfile::Workflow;
  meta.allowed_layers = {LayerProfile::Workflow};

  ToolCallContext ctx;
  ctx.caller_layer = "workflow";
  ctx.session_id = "sess_cancel";

  // Create a cancelled stop_token
  auto source = std::make_shared<std::stop_source>();
  source->request_stop();
  std::stop_token cancelled_token = source->get_token();
  REQUIRE(cancelled_token.stop_requested());

  std::unordered_map<std::string, std::string> args{{"key", "value"}};

  auto result = coord.execute(meta, ctx, args, cancelled_token);

  // Verify short-circuit behavior
  REQUIRE(result.ok == false);
  REQUIRE(result.error_code.has_value());
  REQUIRE(result.error_code.value() == ErrorCode::PermissionDenied);
  REQUIRE(result.meta["error_message"] == "cancelled");

  // Verify audit event was emitted
  bus->wait_for_drain();
  REQUIRE(events.size() == 1);
  REQUIRE(events[0].topic == "tool.audit.denied");
  REQUIRE(events[0].payload.data["reason"] == "cancelled");
  REQUIRE(events[0].payload.data["tool"] == "test_tool");
}

TEST_CASE("ToolCoordinator accepts non-cancelled token and proceeds",
          "[tool_coordinator][cancellation]") {
  MockToolRegistry registry;
  auto bus = std::make_shared<InMemoryBus>();
  auto policy = std::make_shared<AgentModePolicy>();
  ToolCoordinator coord(registry, policy, make_test_auto_callback(true), bus);

  std::vector<BusEvent> events;
  bus->subscribe("tool.execution.start",
                 [&](const BusEvent& ev) { events.push_back(ev); });
  bus->subscribe("tool.execution.end",
                 [&](const BusEvent& ev) { events.push_back(ev); });

  ToolMetadata meta;
  meta.name = "test_tool";
  meta.category = ToolCategory::ReadOnly;
  meta.min_layer = LayerProfile::Workflow;
  meta.allowed_layers = {LayerProfile::Workflow};

  ToolCallContext ctx;
  ctx.caller_layer = "workflow";
  ctx.session_id = "sess_normal";

  // Create a non-cancelled stop_token
  auto source = std::make_shared<std::stop_source>();
  std::stop_token live_token = source->get_token();
  REQUIRE_FALSE(live_token.stop_requested());

  std::unordered_map<std::string, std::string> args{{"key", "value"}};

  auto result = coord.execute(meta, ctx, args, live_token);

  // Verify normal execution
  REQUIRE(result.ok == true);
  bus->wait_for_drain();
  REQUIRE(events.size() == 2);
  REQUIRE(events[0].topic == "tool.execution.start");
  REQUIRE(events[1].topic == "tool.execution.end");
}

TEST_CASE("ToolCoordinator with empty token (default) proceeds normally",
          "[tool_coordinator][cancellation]") {
  MockToolRegistry registry;
  auto bus = std::make_shared<InMemoryBus>();
  auto policy = std::make_shared<AgentModePolicy>();
  ToolCoordinator coord(registry, policy, make_test_auto_callback(true), bus);

  ToolMetadata meta;
  meta.name = "test_tool";
  meta.category = ToolCategory::ReadOnly;
  meta.min_layer = LayerProfile::Workflow;
  meta.allowed_layers = {LayerProfile::Workflow};

  ToolCallContext ctx;
  ctx.caller_layer = "workflow";
  ctx.session_id = "sess_default";

  std::unordered_map<std::string, std::string> args{{"key", "value"}};

  // Call without passing token (uses default empty token)
  auto result = coord.execute(meta, ctx, args);

  // Verify normal execution with default empty token
  REQUIRE(result.ok == true);
}
