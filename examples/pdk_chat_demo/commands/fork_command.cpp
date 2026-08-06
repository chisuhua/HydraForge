#include "commands/fork_command.h"
#include "commands/command_globals.h"
#include <common/tools/tool_coordinator.h>
#include <core/session_manager.h>
#include <chrono>
#include <sstream>
#include <string>

namespace pdk_chat_demo {

hydraforge::pdk::CommandSpec make_fork_command_spec() {
  hydraforge::pdk::CommandSpec spec;
  spec.name = "/fork";
  spec.description = "fork a new branch from current or given node";
  spec.usage = "/fork [node_id]";
  spec.plugin_origin = "pdk_chat_demo";
  spec.handler = [](agenticdsl::ToolCallContext& tctx) -> std::string {
    if (g_command_coordinator == nullptr || g_session_manager == nullptr) {
      return "error: not initialized";
    }
    std::string node_id;
    {
      auto pos = g_current_command_input.find(' ');
      if (pos != std::string::npos) {
        node_id = g_current_command_input.substr(pos + 1);
      }
    }
    if (node_id.empty()) {
      node_id = g_session_manager->get_branch_leaf(g_session_manager->current_branch());
    }
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::ostringstream ss;
    ss << "fork-" << now;
    agenticdsl::ToolMetadata meta;
    meta.name = "session/fork";
    meta.description = "fork a new branch";
    meta.domain = "plugin";
    tctx.session_id = tctx.session_id.empty() ? "main" : tctx.session_id;
    tctx.caller_layer = "workflow";
    auto r = g_command_coordinator->execute(meta, tctx,
                                              {{"node_id", node_id},
                                               {"branch_name", ss.str()}});
    if (!r.ok) return "error: " + r.meta.dump();
    auto branch_id = r.data.value("branch_id", "");
    return "Forked to branch " + branch_id + " (auto-switched)";
  };
  return spec;
}

}  // namespace pdk_chat_demo
