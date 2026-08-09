#pragma once

#include <string>
#include <unordered_map>

namespace agenticdsl { class ToolCoordinator; class CommandRegistry; class SessionManager; }

namespace pdk_chat_demo {

class ChatSession;

extern agenticdsl::ToolCoordinator* g_command_coordinator;
extern agenticdsl::CommandRegistry* g_command_registry;
extern agenticdsl::SessionManager* g_session_manager;
extern ChatSession* g_command_session;
extern std::string g_current_command_input;

inline constexpr const char* kCommandExitSentinel = "__CMD_EXIT__";
inline constexpr const char kExitCommand[] = "/exit";

}  // namespace pdk_chat_demo
