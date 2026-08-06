#include <catch_amalgamated.hpp>

#include <common/tools/command_registry.h>
#include <common/tools/tool_coordinator.h>
#include <common/tools/registry.h>
#include <common/policy/agent_mode_policy.h>
#include <common/policy/approval_callbacks.h>
#include "commands/command_globals.h"
#include "commands/help_command.h"
#include "commands/compact_command.h"
#include "commands/model_command.h"
#include "tools/provider_switch_stub.h"

using agenticdsl::AgentModePolicy;
using agenticdsl::ApprovalCallback;
using agenticdsl::CommandRegistry;
using agenticdsl::IExecutionPolicy;
using agenticdsl::make_test_auto_callback;
using agenticdsl::ToolCallContext;
using agenticdsl::ToolCoordinator;
using agenticdsl::ToolRegistry;

TEST_CASE("unregistered slash command is not resolvable", "[chat-slash-cmd]") {
  ToolRegistry registry;
  pdk_chat_demo::register_provider_switch_stub_tool(registry);
  auto policy = std::make_shared<AgentModePolicy>();
  ApprovalCallback cb = make_test_auto_callback(true);
  ToolCoordinator coord(registry, policy, cb);
  pdk_chat_demo::g_command_coordinator = &coord;
  CommandRegistry reg(&coord);
  reg.register_command(pdk_chat_demo::make_help_command_spec());
  reg.register_command(pdk_chat_demo::make_compact_command_spec());
  reg.register_command(pdk_chat_demo::make_model_command_spec());

  auto spec = reg.resolve_command("/unknown1");
  REQUIRE_FALSE(spec.has_value());
}
