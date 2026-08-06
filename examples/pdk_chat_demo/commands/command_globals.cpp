#include "commands/command_globals.h"
#include <common/tools/command_registry.h>

namespace pdk_chat_demo {

agenticdsl::ToolCoordinator* g_command_coordinator = nullptr;
agenticdsl::CommandRegistry* g_command_registry = nullptr;
std::string g_current_command_input;

}  // namespace pdk_chat_demo
