#pragma once
#include <string>

namespace pdk_chat_demo {
struct CliOptions {
  bool mock = false;
  bool print = false;
  bool offline = false;
  std::string session_id;
  std::string provider;
  std::string fork_node_id;
  std::string session_name;
  std::string system_prompt;
  std::string append_system_prompt;
};
struct CliParseResult {
  bool ok = false;
  bool show_help = false;
  CliOptions options;
  std::string help;
  std::string error;
};
}  // namespace pdk_chat_demo
