#include "commands/compact_command.h"
#include "commands/command_globals.h"
#include <common/tools/tool_coordinator.h>

namespace pdk_chat_demo {

hydraforge::pdk::CommandSpec make_compact_command_spec() {
  hydraforge::pdk::CommandSpec spec;
  spec.name = "/compact";
  spec.description = "compress the current session transcript";
  spec.usage = "/compact [max_tokens]";
  spec.plugin_origin = "pdk_chat_demo";
  spec.handler = [](agenticdsl::ToolCallContext& tctx) -> std::string {
    if (g_command_coordinator == nullptr) {
      return "error: ToolCoordinator not injected";
    }
    agenticdsl::ToolMetadata meta;
    meta.name = "session/compact";
    meta.description = "LLM 压缩会话历史";
    meta.domain = "plugin";
    tctx.session_id = tctx.session_id.empty() ? "main" : tctx.session_id;
    tctx.caller_layer = "workflow";
    auto r = g_command_coordinator->execute(meta, tctx,
                                            {{"session_id", tctx.session_id},
                                             {"max_tokens", "4000"}});
    return r.ok ? ("compacted: " + r.data.dump())
                : ("error: " + r.meta.dump());
  };
  return spec;
}

}  // namespace pdk_chat_demo
