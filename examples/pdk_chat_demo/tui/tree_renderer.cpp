#include "tui/tree_renderer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace pdk_chat_demo {

int get_terminal_width() {
  struct winsize ws {};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return 80;
  }
  return ws.ws_col;
}

static std::string format_branch_list(const std::vector<agenticdsl::BranchMeta>& branches,
                                       const std::string& current_leaf_id,
                                       agenticdsl::SessionManager& sm) {
  std::ostringstream out;
  out << "Branches:\n";
  for (const auto& bm : branches) {
    auto leaf = sm.get_branch_leaf(bm.branch_id);
    std::string marker = (leaf == current_leaf_id) ? "* " : "  ";
    out << marker << bm.branch_id << "  (" << bm.name << ", " << bm.created_at << ")\n";
  }
  return out.str();
}

static std::string format_branch_tree(const std::vector<agenticdsl::BranchMeta>& branches,
                                       const std::string& current_leaf_id,
                                       agenticdsl::SessionManager& sm) {
  std::ostringstream out;
  out << "Session tree:\n";
  auto nodes = sm.list_all_nodes();
  for (const auto& bm : branches) {
    std::string prefix = (bm.branch_id == sm.current_branch()) ? "* " : "  ";
    out << prefix << bm.branch_id << " " << bm.name << "\n";
    std::string leaf = sm.get_branch_leaf(bm.branch_id);
    for (const auto& node : nodes) {
      if (node.branch_id != bm.branch_id) continue;
      std::string node_marker = (node.id == current_leaf_id) ? " [current]" : "";
      out << "    ├── " << node.id.substr(0, 8) << node_marker << "\n";
    }
    (void)leaf;
  }
  return out.str();
}

std::string render_session_tree(agenticdsl::SessionManager& sm,
                                const std::string& current_leaf_id) {
  auto branches = sm.list_branches();
  if (branches.empty()) {
    return "(empty session tree)\n";
  }
  int width = get_terminal_width();
  if (width < 60) {
    return format_branch_list(branches, current_leaf_id, sm);
  }
  return format_branch_tree(branches, current_leaf_id, sm);
}

}  // namespace pdk_chat_demo
