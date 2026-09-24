#include "commands/compact_command.h"
#include "commands/command_globals.h"
#include <core/session_manager.h>

namespace pdk_chat_demo {

hydraforge::pdk::CommandSpec make_compact_command_spec() {
  hydraforge::pdk::CommandSpec spec;
  spec.name = "/compact";
  spec.description = "compress the current session transcript";
  spec.usage = "/compact [max_tokens]";
  spec.plugin_origin = "pdk_chat_demo";
  spec.handler = [](agenticdsl::ToolCallContext&) -> std::string {
    if (g_session_manager == nullptr) {
      return "SessionManager not injected";
    }
    const std::string sid = g_session_manager->current_session_id();
    if (sid.empty()) {
      return "no active session";
    }
    g_session_manager->compact();
    return "Compacted session " + sid;
  };
  return spec;
}

}  // namespace pdk_chat_demo
