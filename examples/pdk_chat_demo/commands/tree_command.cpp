#include "commands/tree_command.h"
#include "commands/command_globals.h"
#include <core/session_manager.h>
#include <tui/tree_renderer.h>

namespace pdk_chat_demo {

hydraforge::pdk::CommandSpec make_tree_command_spec() {
  hydraforge::pdk::CommandSpec spec;
  spec.name = "/tree";
  spec.description = "render the session branch tree";
  spec.usage = "/tree [node_id]";
  spec.plugin_origin = "pdk_chat_demo";
  spec.handler = [](agenticdsl::ToolCallContext&) -> std::string {
    if (g_session_manager == nullptr) {
      return "error: SessionManager not injected";
    }
    std::string arg;
    {
      auto pos = g_current_command_input.find(' ');
      if (pos != std::string::npos) {
        arg = g_current_command_input.substr(pos + 1);
      }
    }
    if (arg.empty()) {
      return render_session_tree(*g_session_manager,
                                 g_session_manager->get_branch_leaf(
                                     g_session_manager->current_branch()));
    }
    auto match = g_session_manager->get_node_by_short_id(arg);
    if (!match) {
      return "error: ambiguous or unknown node id";
    }
    (void)match;
    return "switched to leaf " + match->id;
  };
  return spec;
}

}  // namespace pdk_chat_demo
