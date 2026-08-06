#include "commands/clone_command.h"
#include "commands/command_globals.h"
#include <common/tools/tool_coordinator.h>
#include <core/session_manager.h>
#include <string>

namespace pdk_chat_demo {

hydraforge::pdk::CommandSpec make_clone_command_spec() {
  hydraforge::pdk::CommandSpec spec;
  spec.name = "/clone";
  spec.description = "clone the current session to a new session id";
  spec.usage = "/clone [branch_id]";
  spec.plugin_origin = "pdk_chat_demo";
  spec.handler = [](agenticdsl::ToolCallContext& tctx) -> std::string {
    if (g_command_coordinator == nullptr || g_session_manager == nullptr) {
      return "error: not initialized";
    }
    std::string branch_id;
    {
      auto pos = g_current_command_input.find(' ');
      if (pos != std::string::npos) {
        branch_id = g_current_command_input.substr(pos + 1);
      }
    }
    if (branch_id.empty()) {
      branch_id = g_session_manager->current_branch();
    }
    agenticdsl::ToolMetadata meta;
    meta.name = "session/clone";
    meta.description = "clone current session";
    meta.domain = "plugin";
    tctx.session_id = tctx.session_id.empty() ? "main" : tctx.session_id;
    tctx.caller_layer = "workflow";
    auto r = g_command_coordinator->execute(meta, tctx,
                                              {{"branch_id", branch_id}});
    if (!r.ok) return "error: " + r.meta.dump();
    auto new_session = r.data.value("session_id", "");
    return "Cloned to session " + new_session + " (use --session " + new_session +
           " to switch)";
  };
  return spec;
}

}  // namespace pdk_chat_demo