#include <catch_amalgamated.hpp>

#include <common/tools/command_registry.h>
#include <common/tools/tool_coordinator.h>
#include <common/tools/registry.h>
#include <common/policy/agent_mode_policy.h>
#include <common/policy/approval_callbacks.h>
#include "commands/command_globals.h"
#include "commands/model_command.h"
#include "tools/provider_switch_stub.h"

using agenticdsl::AgentModePolicy;
using agenticdsl::ApprovalCallback;
using agenticdsl::CommandRegistry;
using agenticdsl::IExecutionPolicy;
using agenticdsl::make_test_auto_callback;
using agenticdsl::ToolCallContext;
using agenticdsl::ToolCoordinator;
using agenticdsl::ToolMetadata;
using agenticdsl::ToolRegistry;
using hydraforge::pdk::CommandSpec;

TEST_CASE("/model command is registered and /help lists it", "[chat-slash-cmd]") {
  ToolRegistry registry;
  pdk_chat_demo::register_provider_switch_stub_tool(registry);
  auto policy = std::make_shared<AgentModePolicy>();
  ApprovalCallback cb = make_test_auto_callback(true);
  ToolCoordinator coord(registry, policy, cb);
  pdk_chat_demo::g_command_coordinator = &coord;
  CommandRegistry reg(&coord);
  REQUIRE(reg.register_command(pdk_chat_demo::make_model_command_spec()));
  auto help = reg.render_help();
  REQUIRE(help.find("/model") != std::string::npos);
}

TEST_CASE("/model command returns stub message via ToolCoordinator", "[chat-slash-cmd]") {
  ToolRegistry registry;
  pdk_chat_demo::register_provider_switch_stub_tool(registry);
  auto policy = std::make_shared<AgentModePolicy>();
  ApprovalCallback cb = make_test_auto_callback(true);
  ToolCoordinator coord(registry, policy, cb);
  pdk_chat_demo::g_command_coordinator = &coord;
  CommandRegistry reg(&coord);
  reg.register_command(pdk_chat_demo::make_model_command_spec());

  auto spec = reg.resolve_command("/model");
  REQUIRE(spec.has_value());
  pdk_chat_demo::g_current_command_input = "/model deepseek-v4-pro";
  ToolCallContext tctx;
  auto output = spec->handler(tctx);
  REQUIRE(output.find("Wave 1 stub") != std::string::npos);
}

TEST_CASE("/model with no provider returns usage hint", "[chat-slash-cmd]") {
  ToolRegistry registry;
  pdk_chat_demo::register_provider_switch_stub_tool(registry);
  auto policy = std::make_shared<AgentModePolicy>();
  ApprovalCallback cb = make_test_auto_callback(true);
  ToolCoordinator coord(registry, policy, cb);
  pdk_chat_demo::g_command_coordinator = &coord;
  CommandRegistry reg(&coord);
  reg.register_command(pdk_chat_demo::make_model_command_spec());

  auto spec = reg.resolve_command("/model");
  REQUIRE(spec.has_value());
  pdk_chat_demo::g_current_command_input = "/model";
  ToolCallContext tctx;
  auto output = spec->handler(tctx);
  REQUIRE(output.find("usage: /model") != std::string::npos);
}
