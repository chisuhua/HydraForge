// include/agenticdsl/contract/icommand_registry.h
// 功能描述：ICommandRegistry L3 契约 — 命令注册/解析/列举/冲突检测 (ADR-0070 §决策 2)。
//          仅依赖值类型 (CommandSpec) 与前向声明, 不 include tool_registry.h 完整定义,
//          避免循环 include (ADR-0070 §Risk 5)。
// 作者：AgenticDSL / adr-0070-declare-command
// 最后修改日期：2026-08-04
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "agenticdsl/pdk/command_macros.h"  // CommandSpec

namespace agenticdsl {

class ICommandRegistry {
 public:
  virtual ~ICommandRegistry() = default;

  /** @brief 注册命令。同名第二次注册返回 false (冲突, 含双方 plugin_origin)。/exit 保留字拒绝。 */
  virtual bool register_command(const hydraforge::pdk::CommandSpec& spec) = 0;

  /** @brief 按 `/` 前缀精确解析命令, 未注册返回 std::nullopt */
  virtual std::optional<hydraforge::pdk::CommandSpec> resolve_command(
      const std::string& name) const = 0;

  /** @brief 返回全部已注册命令 (内置 + plugin), 按名称字典序排列 */
  virtual std::vector<hydraforge::pdk::CommandSpec> list_commands() const = 0;

  /** @brief 冲突检测: 无冲突返回 nullopt; 有冲突返回诊断字符串 (双方 plugin_origin) */
  virtual std::optional<std::string> has_conflict(
      const std::string& name, const std::string& plugin_origin) const = 0;
};

}  // namespace agenticdsl
