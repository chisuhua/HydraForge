#include "commands/help_command.h"
#include "commands/command_globals.h"
#include <common/tools/command_registry.h>

namespace pdk_chat_demo {

hydraforge::pdk::CommandSpec make_help_command_spec() {
  hydraforge::pdk::CommandSpec spec;
  spec.name = "/help";
  spec.description = "list available commands";
  spec.usage = "/help";
  spec.plugin_origin = "pdk_chat_demo";
  spec.handler = [](agenticdsl::ToolCallContext&) -> std::string {
    if (g_command_registry == nullptr) {
      return "error: CommandRegistry not injected";
    }
    return g_command_registry->render_help();
  };
  return spec;
}

}  // namespace pdk_chat_demo
