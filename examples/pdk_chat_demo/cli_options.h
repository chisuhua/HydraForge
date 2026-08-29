#pragma once
#include <string>

namespace pdk_chat_demo {
struct CliOptions {
  bool mock = false;
  bool print = false;
  bool offline = false;
  bool allow_training_capture = false;  // Phase 2: --allow-training-capture (Training-mode distillation capture)
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
