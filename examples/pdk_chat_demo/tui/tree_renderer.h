#pragma once

#include <core/session_manager.h>
#include <string>
#include <vector>

namespace pdk_chat_demo {

int get_terminal_width();
std::string render_session_tree(agenticdsl::SessionManager& sm,
                                const std::string& current_leaf_id);

}  // namespace pdk_chat_demo
