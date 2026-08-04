// src/common/tools/command_registry.cpp
// 功能描述：CommandRegistry L1 实现 (ADR-0070 §决策 2/3/5) — 注册/冲突/保留字/`/help`/字典序。
// 作者：AgenticDSL / adr-0070-declare-command
// 最后修改日期：2026-08-04
#include "common/tools/command_registry.h"

namespace agenticdsl {

CommandRegistry::CommandRegistry(ToolCoordinator* coordinator)
    : coordinator_(coordinator) {}

bool CommandRegistry::register_command(const hydraforge::pdk::CommandSpec& spec) {
  if (spec.name == kReservedExit) return false;  // /exit 保留字保护 (ADR-0070 §决策 3)
  if (spec.name.empty() || spec.name.front() != '/') return false;
  if (spec.description.empty()) return false;
  if (auto conflict = has_conflict(spec.name, spec.plugin_origin)) return false;
  commands_[spec.name] = spec;
  return true;
}

std::optional<hydraforge::pdk::CommandSpec> CommandRegistry::resolve_command(
    const std::string& name) const {
  auto it = commands_.find(name);
  if (it == commands_.end()) return std::nullopt;
  return it->second;
}

std::vector<hydraforge::pdk::CommandSpec> CommandRegistry::list_commands() const {
  std::vector<hydraforge::pdk::CommandSpec> out;
  out.reserve(commands_.size());
  for (const auto& [name, spec] : commands_) out.push_back(spec);
  return out;
}

std::optional<std::string> CommandRegistry::has_conflict(
    const std::string& name, const std::string& plugin_origin) const {
  auto it = commands_.find(name);
  if (it == commands_.end()) return std::nullopt;
  // 同一 plugin 重复注册同一命令 → 幂等成功 (允许重载 plugin 初始化); 跨 plugin 冲突 → 拒绝
  if (it->second.plugin_origin == plugin_origin) return std::nullopt;
  return "[command conflict] '" + name + "' already registered by '" +
         it->second.plugin_origin + "', rejected for '" + plugin_origin + "'";
}

std::string CommandRegistry::render_help() const {
  std::string out = "Commands:\n";
  for (const auto& spec : list_commands()) {
    out += "  " + spec.name + " — " + spec.description + "\n";
    out += "      usage: " + spec.usage + "\n";
  }
  out += "  " + std::string(kReservedExit) + " — exit the session (reserved)\n";
  return out;
}

}  // namespace agenticdsl
