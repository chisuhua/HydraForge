// tests/test_tool_coordinator.cpp
// 功能描述：ToolCoordinator 单元测试 (C4 Sprint 14)
// 设计依据：ADR-0031 §决策 5 (Oracle session ses_0ed4408faffeLv8VfrC0s5PzW7)
// 作者：AgenticDSL Phase3 / Sprint 14 C4 ship
// 最后修改日期：2026-06-29
#include "catch_amalgamated.hpp"

#include <memory>
#include <unordered_map>

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/policy/iexecution_policy.h"
#include "common/policy/agent_mode_policy.h"
#include "common/policy/approval_callbacks.h"
#include "common/tools/tool_coordinator.h"

using namespace agenticdsl;

namespace {

// Mock IToolRegistry for testing (returns success json)
class MockToolRegistry : public IToolRegistry {
 public:
  nlohmann::json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) override {
    last_called_tool_ = name;
    last_called_args_ = args;
    return {{"mock_result", "ok"}};
  }

  bool has_tool(const std::string&) const override { return true; }
  std::vector<std::string> list_tools() const override { return {"mock_tool"}; }
  void register_tool_function(std::string, ToolMetadata, ToolFunc) override {}
  void register_llm_tool(std::string, std::unique_ptr<ILLMTool>,
                          const LLMParams&) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const LLMParams& get_llm_params(const std::string&) const override {
    static LLMParams p;
    return p;
  }
  nlohmann::json call_llm_tool(
      const std::string&, const std::string&, const LLMParams&) override {
    return nullptr;
  }
  void set_cost_callback(CostCallback) override {}

  std::string last_called_tool_;
  std::unordered_map<std::string, std::string> last_called_args_;
};

// Mock IInteractionBus for testing (records emit calls)
class MockInteractionBus : public IInteractionBus {
 public:
  void emit(const BusEvent& event) override {
    emit_log_.push_back(event.topic);
  }
  void emit(const std::string& event_type, const std::string&) override {
    emit_log_.push_back(event_type);
  }
  size_t subscribe(const std::string&,
                    std::function<void(const BusEvent&)>) override {
    return 0;
  }
  void unsubscribe(size_t) override {}

  std::vector<std::string> emit_log_;
};

// Helper: create ToolMetadata V2 with defaults
ToolMetadata make_meta(const std::string& name, ToolCategory category) {
  ToolMetadata m;
  m.name = name;
  m.description = "test tool";
  m.domain = "test";
  m.category = category;
  m.min_layer = LayerProfile::Workflow;
  return m;
}

// Helper: create ToolCallContext with caller_layer
ToolCallContext make_ctx(const std::string& caller_layer) {
  ToolCallContext ctx;
  ctx.session_id = "test_session";
  ctx.caller_layer = caller_layer;
  ctx.target_path = "/tmp/test";
  ctx.is_in_fleet_mode = false;
  ctx.call_count_this_session = 0;
  return ctx;
}

}  // namespace

// ===== 8 test cases =====

TEST_CASE("tool_coordinator_layer_workflow_allows_all", "[tool_coordinator][stage3]") {
  MockToolRegistry registry;
  MockInteractionBus bus;
  auto policy = std::make_shared<AgentModePolicy>();
  auto coordinator = std::make_unique<ToolCoordinator>(
      registry, policy, make_test_auto_callback(true),
      std::shared_ptr<IInteractionBus>(&bus, [](IInteractionBus*){}));

  for (auto cat : {ToolCategory::ReadOnly, ToolCategory::WriteFile,
                    ToolCategory::Execute, ToolCategory::Network,
                    ToolCategory::StateModify}) {
    auto meta = make_meta("test_tool", cat);
    auto ctx = make_ctx("workflow");
    auto result = coordinator->execute(meta, ctx, {});
    REQUIRE(result.ok);
  }
}

TEST_CASE("tool_coordinator_layer_thinking_only_readonly", "[tool_coordinator][stage3]") {
  MockToolRegistry registry;
  MockInteractionBus bus;
  auto policy = std::make_shared<AgentModePolicy>();
  auto coordinator = std::make_unique<ToolCoordinator>(
      registry, policy, make_test_auto_callback(true),
      std::shared_ptr<IInteractionBus>(&bus, [](IInteractionBus*){}));

  auto meta_ro = make_meta("test_tool", ToolCategory::ReadOnly);
  auto result_ro = coordinator->execute(meta_ro, make_ctx("thinking"), {});
  REQUIRE(result_ro.ok);

  auto meta_write = make_meta("test_tool", ToolCategory::WriteFile);
  auto result_write = coordinator->execute(meta_write, make_ctx("thinking"), {});
  REQUIRE_FALSE(result_write.ok);
}

TEST_CASE("tool_coordinator_layer_cognitive_denies_all", "[tool_coordinator][stage3]") {
  MockToolRegistry registry;
  MockInteractionBus bus;
  auto policy = std::make_shared<AgentModePolicy>();
  auto coordinator = std::make_unique<ToolCoordinator>(
      registry, policy, make_test_auto_callback(true),
      std::shared_ptr<IInteractionBus>(&bus, [](IInteractionBus*){}));

  auto meta = make_meta("test_tool", ToolCategory::ReadOnly);
  auto result = coordinator->execute(meta, make_ctx("cognitive"), {});
  REQUIRE_FALSE(result.ok);
}

TEST_CASE("tool_coordinator_audit_emit_order", "[tool_coordinator][stage3]") {
  MockToolRegistry registry;
  MockInteractionBus bus;
  auto policy = std::make_shared<AgentModePolicy>();
  auto coordinator = std::make_unique<ToolCoordinator>(
      registry, policy, make_test_auto_callback(true),
      std::shared_ptr<IInteractionBus>(&bus, [](IInteractionBus*){}));

  auto meta = make_meta("test_tool", ToolCategory::ReadOnly);
  coordinator->execute(meta, make_ctx("workflow"), {});

  REQUIRE(bus.emit_log_.size() == 2);
  REQUIRE(bus.emit_log_[0] == "tool.audit.invoked");
  REQUIRE(bus.emit_log_[1] == "tool.audit.completed");
}

TEST_CASE("tool_coordinator_audit_deny_on_layer", "[tool_coordinator][stage3]") {
  MockToolRegistry registry;
  MockInteractionBus bus;
  auto policy = std::make_shared<AgentModePolicy>();
  auto coordinator = std::make_unique<ToolCoordinator>(
      registry, policy, make_test_auto_callback(true),
      std::shared_ptr<IInteractionBus>(&bus, [](IInteractionBus*){}));

  auto meta = make_meta("test_tool", ToolCategory::WriteFile);
  auto result = coordinator->execute(meta, make_ctx("cognitive"), {});
  REQUIRE_FALSE(result.ok);
  REQUIRE(bus.emit_log_.size() == 1);
  REQUIRE(bus.emit_log_[0] == "tool.audit.denied");
}

TEST_CASE("tool_coordinator_null_bus_skips_audit", "[tool_coordinator][stage3]") {
  MockToolRegistry registry;
  auto policy = std::make_shared<AgentModePolicy>();
  auto coordinator = std::make_unique<ToolCoordinator>(
      registry, policy, make_test_auto_callback(true), nullptr);

  auto meta = make_meta("test_tool", ToolCategory::ReadOnly);
  auto result = coordinator->execute(meta, make_ctx("workflow"), {});
  REQUIRE(result.ok);
}

TEST_CASE("tool_coordinator_invalid_caller_layer_throws", "[tool_coordinator][stage3]") {
  MockToolRegistry registry;
  auto policy = std::make_shared<AgentModePolicy>();
  auto coordinator = std::make_unique<ToolCoordinator>(
      registry, policy, make_test_auto_callback(true), nullptr);

  auto meta = make_meta("test_tool", ToolCategory::ReadOnly);
  auto result = coordinator->execute(meta, make_ctx("invalid_layer_name"), {});
  REQUIRE_FALSE(result.ok);
}

TEST_CASE("tool_coordinator_unknown_tool_calls_registry", "[tool_coordinator][stage3]") {
  MockToolRegistry registry;
  auto policy = std::make_shared<AgentModePolicy>();
  auto coordinator = std::make_unique<ToolCoordinator>(
      registry, policy, make_test_auto_callback(true), nullptr);

  auto meta = make_meta("my_test_tool", ToolCategory::ReadOnly);
  std::unordered_map<std::string, std::string> args = {{"key1", "val1"}};
  coordinator->execute(meta, make_ctx("workflow"), args);

  REQUIRE(registry.last_called_tool_ == "my_test_tool");
  REQUIRE(registry.last_called_args_["key1"] == "val1");
}