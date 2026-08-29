#include "cli_args_parser.h"
#include <cxxopts.hpp>
#include <exception>
#include <iostream>

namespace pdk_chat_demo {
const std::vector<CliFlagSpec>& cli_flag_declarations() {
  static const std::vector<CliFlagSpec> table = {
    {"mock", "", CliValueKind::flag, "", "Use MockLLMProvider without network requests", CliDestination::mock},
    {"session", "", CliValueKind::string, "ID", "Load the selected persisted session", CliDestination::session_id},
    {"print", "p", CliValueKind::flag, "", "Enable print mode intent", CliDestination::print},
    {"provider", "", CliValueKind::string, "NAME", "Override the configured provider for this run", CliDestination::provider},
    {"offline", "", CliValueKind::flag, "", "Enable offline startup intent independently of mock", CliDestination::offline},
    {"fork", "", CliValueKind::string, "NODE_ID", "Fork a new branch from the named session node on startup", CliDestination::fork_node_id},
    {"name", "", CliValueKind::string, "SESSION_NAME", "Persist a human-readable name for the new session (ignored when --session loads an existing session)", CliDestination::session_name},
    {"system-prompt", "", CliValueKind::string, "TEXT", "Replace the default system prompt with TEXT (overwrites)", CliDestination::system_prompt},
    {"append-system-prompt", "", CliValueKind::string, "TEXT", "Append TEXT after the default system prompt, separated by one newline", CliDestination::append_system_prompt},
    {"allow-training-capture", "", CliValueKind::flag, "",
     "Enable Training-mode distillation capture (requires real LLM provider, rejected in mock mode)",
     CliDestination::allow_training_capture},
  };
  return table;
}

CliParseResult parse_cli_args(int argc, char* argv[]) {
  CliParseResult result;
  cxxopts::Options options("pdk_chat_demo", "HydraForge PDK chat demo");
  for (const auto& spec : cli_flag_declarations()) {
    const std::string spelling = spec.short_name.empty() ? spec.long_name : spec.short_name + "," + spec.long_name;
    if (spec.value_kind == CliValueKind::string)
      options.add_options()(spelling, spec.description, cxxopts::value(), spec.value_name);
    else
      options.add_options()(spelling, spec.description);
  }
  options.add_options()("help", "Show generated usage");
  try {
    const auto parsed = options.parse(argc, argv);
    result.help = options.help();
    result.show_help = parsed.count("help") != 0;
    if (!result.show_help) {
      result.options.mock = parsed["mock"].as<bool>();
      result.options.print = parsed["print"].as<bool>();
      result.options.offline = parsed["offline"].as<bool>();
      result.options.allow_training_capture = parsed["allow-training-capture"].as<bool>();
      if (parsed.count("session")) result.options.session_id = parsed["session"].as<std::string>();
      if (parsed.count("provider")) result.options.provider = parsed["provider"].as<std::string>();
      if (parsed.count("fork")) result.options.fork_node_id = parsed["fork"].as<std::string>();
      if (parsed.count("name")) result.options.session_name = parsed["name"].as<std::string>();
      if (parsed.count("system-prompt")) result.options.system_prompt = parsed["system-prompt"].as<std::string>();
      if (parsed.count("append-system-prompt")) result.options.append_system_prompt = parsed["append-system-prompt"].as<std::string>();
    }
    result.ok = true;
  } catch (const std::exception& error) {
    result.error = std::string(error.what()) + ". Use --help for usage.";
    result.help = options.help();
  }
  return result;
}
}  // namespace pdk_chat_demo
