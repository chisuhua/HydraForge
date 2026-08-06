#include "commands/model_command.h"
#include "commands/command_globals.h"
#include <common/tools/tool_coordinator.h>
#include <sstream>
#include <string>

namespace pdk_chat_demo {

hydraforge::pdk::CommandSpec make_model_command_spec() {
  hydraforge::pdk::CommandSpec spec;
  spec.name = "/model";
  spec.description = "Switch LLM provider (Wave 1 stub)";
  spec.usage = "/model <provider_name>";
  spec.plugin_origin = "pdk_chat_demo";
  spec.handler = [](agenticdsl::ToolCallContext& tctx) -> std::string {
    if (g_command_coordinator == nullptr) {
      return "error: ToolCoordinator not injected";
    }
    std::string provider_name;
    {
      auto pos = g_current_command_input.find(' ');
      if (pos != std::string::npos) {
        provider_name = g_current_command_input.substr(pos + 1);
      }
    }
    agenticdsl::ToolMetadata meta;
    meta.name = "provider_switch_stub";
    meta.description = "Wave 1 stub for provider switch";
    meta.domain = "plugin";
    tctx.session_id = tctx.session_id.empty() ? "main" : tctx.session_id;
    tctx.caller_layer = "workflow";
    auto r = g_command_coordinator->execute(meta, tctx,
                                            {{"provider_name", provider_name}});
    if (!r.ok) return "error: " + r.meta.dump();
    if (r.data.contains("message") && r.data["message"].is_string()) {
      return r.data["message"].get<std::string>();
    }
    return r.data.dump();
  };
  return spec;
}

}  // namespace pdk_chat_demo
