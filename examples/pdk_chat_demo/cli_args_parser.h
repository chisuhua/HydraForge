#pragma once
#include "cli_options.h"
#include <string>
#include <vector>

namespace pdk_chat_demo {
enum class CliValueKind { flag, string };
enum class CliDestination { mock, session_id, print, provider, offline, fork_node_id, session_name, system_prompt, append_system_prompt, allow_training_capture };
struct CliFlagSpec {
  std::string long_name;
  std::string short_name;
  CliValueKind value_kind;
  std::string value_name;
  std::string description;
  CliDestination destination;
};
const std::vector<CliFlagSpec>& cli_flag_declarations();
CliParseResult parse_cli_args(int argc, char* argv[]);
}  // namespace pdk_chat_demo
