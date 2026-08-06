#include <catch_amalgamated.hpp>

#include <common/tools/command_registry.h>
#include <common/tools/tool_coordinator.h>
#include <common/tools/registry.h>
#include <common/policy/agent_mode_policy.h>
#include <common/policy/approval_callbacks.h>
#include <core/session_manager.h>
#include <filesystem>
#include "commands/command_globals.h"
#include "commands/tree_command.h"
#include "commands/fork_command.h"
#include "commands/clone_command.h"
#include "tools/session_fork.h"
#include "tools/session_clone.h"

namespace fs = std::filesystem;

using agenticdsl::AgentModePolicy;
using agenticdsl::ApprovalCallback;
using agenticdsl::CommandRegistry;
using agenticdsl::IExecutionPolicy;
using agenticdsl::SessionManager;
using agenticdsl::make_test_auto_callback;
using agenticdsl::ToolCoordinator;
using agenticdsl::ToolRegistry;

TEST_CASE("/tree /fork /clone commands are registered", "[session-tree]") {
  ToolRegistry registry;
  fs::path dir = fs::temp_directory_path() / "session_tree_test_cmds";
  fs::remove_all(dir);
  SessionManager sm(dir);
  sm.open("test");
  pdk_chat_demo::g_session_manager = &sm;

  pdk_chat_demo::register_session_fork_tool(registry);
  pdk_chat_demo::register_session_clone_tool(registry);

  auto policy = std::make_shared<AgentModePolicy>();
  ApprovalCallback cb = make_test_auto_callback(true);
  ToolCoordinator coord(registry, policy, cb);
  pdk_chat_demo::g_command_coordinator = &coord;
  CommandRegistry reg(&coord);

  REQUIRE(reg.register_command(pdk_chat_demo::make_tree_command_spec()));
  REQUIRE(reg.register_command(pdk_chat_demo::make_fork_command_spec()));
  REQUIRE(reg.register_command(pdk_chat_demo::make_clone_command_spec()));

  auto help = reg.render_help();
  REQUIRE(help.find("/tree") != std::string::npos);
  REQUIRE(help.find("/fork") != std::string::npos);
  REQUIRE(help.find("/clone") != std::string::npos);
}

TEST_CASE("/tree renders session tree", "[session-tree]") {
  fs::path dir = fs::temp_directory_path() / "session_tree_test_render";
  fs::remove_all(dir);
  SessionManager sm(dir);
  sm.open("test");
  pdk_chat_demo::g_session_manager = &sm;
  auto spec = pdk_chat_demo::make_tree_command_spec();
  agenticdsl::ToolCallContext tctx;
  pdk_chat_demo::g_current_command_input = "/tree";
  auto out = spec.handler(tctx);
  REQUIRE_FALSE(out.empty());
  REQUIRE(out.find("main") != std::string::npos);
}