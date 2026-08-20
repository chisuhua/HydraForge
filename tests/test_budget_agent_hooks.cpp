// tests/test_budget_agent_hooks.cpp
// 功能描述：budget_agent FailClosed pre-hook 集成测试 (ADR-0069 §决策 3)
// 设计依据：ADR-0069 + pdk/budget_agent 预算超限降级
// 作者：AgenticDSL Phase 6 / adr-0069-tool-coordinator-hooks change
// 最后修改日期：2026-08-20 (P12: 迁移本地 MockBus → canonical test::MockBus)
#include "catch_amalgamated.hpp"

#include <dlfcn.h>
#include <filesystem>
#include <memory>
#include <stdexcept>

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/itool_hook_registry.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/policy/agent_mode_policy.h"
#include "common/policy/approval_callbacks.h"
#include "common/tools/registry.h"
#include "common/tools/tool_coordinator.h"
#include "common/tools/tool_hook_registry.h"
#include "test_helpers/mock_bus.h"

using namespace agenticdsl;

namespace {

ToolMetadata make_meta(const std::string& name, ToolCategory category) {
  ToolMetadata m;
  m.name = name;
  m.description = "test";
  m.domain = "test";
  m.category = category;
  m.min_layer = LayerProfile::Workflow;
  return m;
}

ToolCallContext make_ctx() {
  ToolCallContext ctx;
  ctx.session_id = "session-1";
  ctx.caller_layer = "workflow";
  ctx.target_path = "/tmp";
  ctx.is_in_fleet_mode = false;
  ctx.call_count_this_session = 0;
  return ctx;
}

}  // namespace

TEST_CASE("budget_agent_pre_hook_denies_when_over_budget", "[budget_agent][stage7]") {
  void* handle = dlopen(TEST_BUDGET_AGENT_SO_PATH, RTLD_NOW);
  REQUIRE(handle != nullptr);

  auto register_tools = reinterpret_cast<void (*)(IToolRegistry&)>(
      dlsym(handle, "pdk_register_tools"));
  auto register_hooks = reinterpret_cast<void (*)(IToolHookRegistry&)>(
      dlsym(handle, "pdk_register_hooks"));
  REQUIRE(register_tools != nullptr);
  REQUIRE(register_hooks != nullptr);

  // 所有持有 hook std::function 的对象必须在 dlclose 之前析构
  {
    ToolRegistry registry;
    registry.register_tool(
        "test/echo",
        make_meta("test/echo", ToolCategory::ReadOnly),
        [](const std::unordered_map<std::string, std::string>&) {
          return nlohmann::json{{"ok", true}};
        });

    ToolHookRegistry hooks;
    test::MockBus bus;

    register_tools(registry);
    register_hooks(hooks);

    auto policy = std::make_shared<AgentModePolicy>();
    ToolCoordinator coordinator(
        registry, policy, make_test_auto_callback(true),
        std::shared_ptr<IInteractionBus>(&bus, [](IInteractionBus*) {}));
    coordinator.set_hook_registry(&hooks);

    // set limit to 0 to exhaust budget
    coordinator.execute(make_meta("budget/set_limit", ToolCategory::Execute),
                        make_ctx(), {{"limit_usd", "0"}});

    auto result = coordinator.execute(make_meta("test/echo", ToolCategory::ReadOnly),
                                      make_ctx(), {});
    REQUIRE_FALSE(result.ok);
    REQUIRE(bus.topics.back() == "tool.audit.denied");
  }

  dlclose(handle);
}
