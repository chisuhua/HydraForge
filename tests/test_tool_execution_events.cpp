// tests/test_tool_execution_events.cpp
// 功能描述：ToolCoordinator tool.execution.start/end 事件发射测试
// 设计依据：openspec/changes/adr-0068-event-emission-contract/design.md
//           + §3 ToolCoordinator 迁移任务 3.13
// 作者：AgenticDSL Phase 6a
// 最后修改日期：2026-08-03
#include "catch_amalgamated.hpp"

#include <memory>
#include <unordered_map>

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
  std::vector<std::string> list_tools() const override { return {"calculate"}; }
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

TEST_CASE("ToolCoordinator emits execution start/end pair",
          "[tool_coordinator][event]") {
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
  meta.name = "calculate";
  meta.category = ToolCategory::ReadOnly;
  meta.min_layer = LayerProfile::Workflow;
  meta.allowed_layers = {LayerProfile::Workflow};

  ToolCallContext ctx;
  ctx.caller_layer = "workflow";
  ctx.session_id = "sess_tc";
  std::unordered_map<std::string, std::string> args{{"a", "1"}, {"b", "2"},
                                                    {"op", "+"}};

  auto result = coord.execute(meta, ctx, args);
  bus->wait_for_drain();

  REQUIRE(events.size() == 2);
  REQUIRE(events[0].topic == "tool.execution.start");
  REQUIRE(events[0].payload.data["tool"] == "calculate");
  REQUIRE(events[0].payload.data["layer"] == "workflow");
  REQUIRE(events[1].topic == "tool.execution.end");
  REQUIRE(events[1].payload.data["tool"] == "calculate");
  REQUIRE(events[1].payload.data["ok"] == true);
  REQUIRE(events[1].payload.data.contains("duration_ms"));
  REQUIRE(result.ok == true);
}
