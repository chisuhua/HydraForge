#include "commands/command_globals.h"
#include <common/tools/command_registry.h>
#include <core/session_manager.h>

#include "chat_session.h"

namespace pdk_chat_demo {

agenticdsl::ToolCoordinator* g_command_coordinator = nullptr;
agenticdsl::CommandRegistry* g_command_registry = nullptr;
agenticdsl::SessionManager* g_session_manager = nullptr;
ChatSession* g_command_session = nullptr;
std::string g_current_command_input;

}  // namespace pdk_chat_demo
