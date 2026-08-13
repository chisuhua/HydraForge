#include "commands/compact_command.h"
#include "commands/command_globals.h"

namespace pdk_chat_demo {

hydraforge::pdk::CommandSpec make_compact_command_spec() {
  hydraforge::pdk::CommandSpec spec;
  spec.name = "/compact";
  spec.description = "compress the current session transcript";
  spec.usage = "/compact [max_tokens]";
  spec.plugin_origin = "pdk_chat_demo";
  spec.handler = [](agenticdsl::ToolCallContext&) -> std::string {
    // Placeholder: full wiring requires LayeredContext + compactor injection
    // Task 8 DSLEngine integration will register session/compact tool
    return "Compaction not yet wired (session/compact tool pending Task 8 DSLEngine integration)";
  };
  return spec;
}

}  // namespace pdk_chat_demo
