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
      if (parsed.count("session")) result.options.session_id = parsed["session"].as<std::string>();
      if (parsed.count("provider")) result.options.provider = parsed["provider"].as<std::string>();
    }
    result.ok = true;
  } catch (const std::exception& error) {
    result.error = std::string(error.what()) + ". Use --help for usage.";
    result.help = options.help();
  }
  return result;
}
}  // namespace pdk_chat_demo
