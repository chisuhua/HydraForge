// tests/test_tool_coordinator_hooks.cpp
// 功能描述：ToolCoordinator pre/post hook 机制测试 (ADR-0069)
// 设计依据：ADR-0069 §决策 1-6 + ADR-0068 事件契约
// 作者：AgenticDSL Phase 6 / adr-0069-tool-coordinator-hooks change
// 最后修改日期：2026-08-04
#include "catch_amalgamated.hpp"
#include "agenticdsl/contract/itool_hook_registry.h"

TEST_CASE("itool_hook_registry_contract_compiles", "[tool_coordinator_hooks][stage1]") {
  agenticdsl::PreHookResult r;
  r.action = agenticdsl::PreHookResult::Deny;
  r.deny_reason = "blocked";
  REQUIRE(r.action == agenticdsl::PreHookResult::Deny);
  REQUIRE(r.deny_reason == "blocked");
}

#include "common/tools/tool_hook_registry.h"

TEST_CASE("tool_hook_registry_executes_matching_pre_hook", "[tool_coordinator_hooks][stage2]") {
  agenticdsl::ToolHookRegistry registry;
  bool called = false;
  registry.register_pre_hook(
      "*",
      [&](const auto&, const auto&, const auto&) {
        called = true;
        return agenticdsl::PreHookResult{};
      },
      0,
      agenticdsl::HookErrorPolicy::FailClosed);

  std::vector<std::string> warnings;
  agenticdsl::ToolMetadata meta;
  meta.name = "shell/exec";
  agenticdsl::ToolCallContext ctx;
  auto result = registry.apply_pre_hooks(meta, ctx, {}, warnings);

  REQUIRE(called);
  REQUIRE(result.action == agenticdsl::PreHookResult::Continue);
  REQUIRE(warnings.empty());
}

#include "common/tools/tool_coordinator.h"
#include "common/policy/agent_mode_policy.h"
#include "common/policy/approval_callbacks.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/policy/iexecution_policy.h"

using namespace agenticdsl;

namespace {

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
  nlohmann::json call_llm_tool(const std::string&, const std::string&,
                               const LLMParams&) override { return nullptr; }
  void set_cost_callback(CostCallback) override {}

  std::string last_called_tool_;
  std::unordered_map<std::string, std::string> last_called_args_;
};

class MockInteractionBus : public IInteractionBus {
 public:
  void emit(const BusEvent& event) override {
    emit_log_.push_back(event.topic);
    payloads_.push_back(event.payload);
  }
  void emit(const std::string& event_type, const std::string&) override {
    emit_log_.push_back(event_type);
  }
  size_t subscribe(const std::string&,
                   std::function<void(const BusEvent&)>) override { return 0; }
  void unsubscribe(size_t) override {}

  std::vector<std::string> emit_log_;
  std::vector<ToolResult> payloads_;
};

ToolMetadata make_meta(const std::string& name, ToolCategory category) {
  ToolMetadata m;
  m.name = name;
  m.description = "test tool";
  m.domain = "test";
  m.category = category;
  m.min_layer = LayerProfile::Workflow;
  return m;
}

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

TEST_CASE("tool_coordinator_pre_hook_deny_skips_call", "[tool_coordinator_hooks][stage3]") {
  MockToolRegistry registry;
  MockInteractionBus bus;
  ToolHookRegistry hooks;
  hooks.register_pre_hook(
      "shell/*",
      [](const ToolMetadata&, const ToolCallContext&,
         const std::unordered_map<std::string, std::string>&) {
        PreHookResult r;
        r.action = PreHookResult::Deny;
        r.deny_reason = "policy violation";
        return r;
      },
      0,
      HookErrorPolicy::FailClosed);

  auto policy = std::make_shared<AgentModePolicy>();
  auto coordinator = std::make_unique<ToolCoordinator>(
      registry, policy, make_test_auto_callback(true),
      std::shared_ptr<IInteractionBus>(&bus, [](IInteractionBus*) {}));
  coordinator->set_hook_registry(&hooks);

  auto result = coordinator->execute(make_meta("shell/exec", ToolCategory::Execute),
                                     make_ctx("workflow"), {});
  REQUIRE_FALSE(result.ok);
  REQUIRE(registry.last_called_tool_.empty());
  REQUIRE(bus.emit_log_.size() == 1);
  REQUIRE(bus.emit_log_[0] == "tool.audit.denied");
}
