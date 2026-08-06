#include "tools/session_fork.h"
#include "commands/command_globals.h"
#include <core/session_manager.h>
#include <common/policy/execution_policy.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <sstream>
#include <string>
#include <unordered_map>

namespace pdk_chat_demo {

void register_session_fork_tool(agenticdsl::IToolRegistry& registry) {
  agenticdsl::ToolMetadata meta;
  meta.name = "session/fork";
  meta.description = "Fork a new branch from a session node";
  meta.category = agenticdsl::ToolCategory::StateModify;
  meta.min_layer = agenticdsl::LayerProfile::Workflow;
  meta.approval.requires_approval_in_plan = true;
  meta.approval.requires_approval_in_agent = true;
  meta.approval.requires_approval_in_yolo = false;
  meta.approval.force_approval_always = false;
  meta.allowed_layers = {agenticdsl::LayerProfile::Workflow};

  registry.register_tool_function(meta.name, meta,
    [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
      if (g_session_manager == nullptr) {
        return nlohmann::json{{"ok", false}, {"error", "SessionManager not injected"}};
      }
      auto node_it = args.find("node_id");
      std::string node_id = (node_it != args.end()) ? node_it->second : std::string("");
      if (node_id.empty()) {
        node_id = g_session_manager->get_branch_leaf(g_session_manager->current_branch());
      }
      auto name_it = args.find("branch_name");
      std::string branch_name = (name_it != args.end()) ? name_it->second : std::string("fork");
      try {
        auto branch_id = g_session_manager->fork(node_id, branch_name);
        return nlohmann::json{{"ok", true}, {"data", {{"branch_id", branch_id}}}};
      } catch (const std::exception& e) {
        return nlohmann::json{{"ok", false}, {"error", e.what()}};
      }
    });
}

}  // namespace pdk_chat_demo
