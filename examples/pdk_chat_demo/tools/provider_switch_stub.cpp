#include "tools/provider_switch_stub.h"
#include <common/tools/registry.h>
#include <common/policy/execution_policy.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace pdk_chat_demo {

void register_provider_switch_stub_tool(agenticdsl::IToolRegistry& registry) {
  agenticdsl::ToolMetadata meta;
  meta.name = "provider_switch_stub";
  meta.description = "Wave 1 stub for provider switch";
  meta.category = agenticdsl::ToolCategory::StateModify;
  meta.min_layer = agenticdsl::LayerProfile::Workflow;
  meta.approval.requires_approval_in_plan = true;
  meta.approval.requires_approval_in_agent = true;
  meta.approval.requires_approval_in_yolo = false;
  meta.approval.force_approval_always = false;
  meta.allowed_layers = {agenticdsl::LayerProfile::Workflow};

  registry.register_tool_function(meta.name, meta,
    [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
      auto it = args.find("provider_name");
      std::string provider_name = (it != args.end()) ? it->second : std::string("");
      if (provider_name.empty()) {
        return nlohmann::json{
          {"ok", true},
          {"data", {{"message", "usage: /model <provider_name> (Wave 1 stub - provider switch pending provider-dynamic-discovery)"}}}
        };
      }
      return nlohmann::json{
        {"ok", true},
        {"data", {{"message", "[Wave 1 stub] provider switch will activate after provider-dynamic-discovery ships (TBD: provider/switch tool registration + config persistence)"}}}
      };
    });
}

}  // namespace pdk_chat_demo
