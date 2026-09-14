#pragma once

#include <string>
#include <unordered_map>

namespace agenticdsl { class ToolCoordinator; class CommandRegistry; class SessionManager; }
namespace hydraforge::pdk { class ChatSession; }

namespace pdk_chat_demo {

// chat-session-pdk-lift C1: ChatSession 已提升到 hydraforge::pdk。
// 原 `class ChatSession;` 前置声明改为 alias (避免与 chat_session.h shim 的 using 冲突),
// 1 个 Sprint 兼容期内 pdk_chat_demo::ChatSession 继续可用。
using ChatSession = hydraforge::pdk::ChatSession;

extern agenticdsl::ToolCoordinator* g_command_coordinator;
extern agenticdsl::CommandRegistry* g_command_registry;
extern agenticdsl::SessionManager* g_session_manager;
extern ChatSession* g_command_session;
extern std::string g_current_command_input;

inline constexpr const char* kCommandExitSentinel = "__CMD_EXIT__";
inline constexpr const char kExitCommand[] = "/exit";

}  // namespace pdk_chat_demo
