#include "tools/session_clone.h"
#include "commands/command_globals.h"
#include <core/session_manager.h>
#include <common/policy/execution_policy.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

namespace pdk_chat_demo {

void register_session_clone_tool(agenticdsl::IToolRegistry& registry) {
  agenticdsl::ToolMetadata meta;
  meta.name = "session/clone";
  meta.description = "Clone the current session to a new session id";
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
      auto branch_it = args.find("branch_id");
      (void)branch_it;
      try {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        std::ostringstream ss;
        ss << "clone-" << now;
        auto new_session_id = ss.str();
        fs::path src = g_session_manager->dir_ /
                       (g_session_manager->current_session_id_ +
                        std::string(agenticdsl::kSessionFileExt));
        fs::path dst = g_session_manager->dir_ /
                       (new_session_id + std::string(agenticdsl::kSessionFileExt));
        if (fs::exists(src)) {
          fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        }
        return nlohmann::json{{"ok", true}, {"data", {{"session_id", new_session_id}}}};
      } catch (const std::exception& e) {
        return nlohmann::json{{"ok", false}, {"error", e.what()}};
      }
    });
}

}  // namespace pdk_chat_demo
