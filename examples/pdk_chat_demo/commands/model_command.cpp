#include "commands/model_command.h"
#include "commands/command_globals.h"
#include "chat_session.h"
#include <sstream>
#include <string>

namespace pdk_chat_demo {

hydraforge::pdk::CommandSpec make_model_command_spec() {
  hydraforge::pdk::CommandSpec spec;
  spec.name = "/model";
  spec.description = "Switch LLM provider/model for next turn (Wave 3-A Phase C)";
  spec.usage = "/model <provider_name>[/<model_name>]";
  spec.plugin_origin = "pdk_chat_demo";
  spec.handler = [](agenticdsl::ToolCallContext& /*tctx*/) -> std::string {
    if (g_command_session == nullptr) {
      return "error: ChatSession not injected";
    }
    std::string provider_name;
    {
      auto pos = g_current_command_input.find(' ');
      if (pos != std::string::npos) {
        provider_name = g_current_command_input.substr(pos + 1);
      }
    }
    if (provider_name.empty()) {
      return "Usage: /model <provider_name>[/<model_name>]";
    }
    if (g_command_session->request_model_switch(provider_name)) {
      return "Model switched to " + provider_name + " (next turn)";
    }
    return "error: model switch rejected (e.g., mock mode + non-mock provider)";
  };
  return spec;
}

}  // namespace pdk_chat_demo
