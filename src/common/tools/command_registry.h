// src/common/tools/command_registry.h
// 功能描述：CommandRegistry — ICommandRegistry L1 默认实现 (ADR-0070 §决策 2/3/5)。
//          内置 /help 自动生成 + /exit 保留字保护 + 命名冲突诊断。
// 作者：AgenticDSL / adr-0070-declare-command
// 最后修改日期：2026-08-04
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "agenticdsl/contract/icommand_registry.h"

namespace agenticdsl {

class ToolCoordinator;  // 前向声明, 避免 include tool_coordinator.h 引入间接依赖

class CommandRegistry : public ICommandRegistry {
 public:
  /** @param coordinator ToolCoordinator 非 owning 指针 (构造时注入, 可为 null)
   *                     仅委托工具命令使用; null 时委托命令返回友好错误 */
  explicit CommandRegistry(ToolCoordinator* coordinator = nullptr);

  bool register_command(const hydraforge::pdk::CommandSpec& spec) override;
  std::optional<hydraforge::pdk::CommandSpec> resolve_command(
      const std::string& name) const override;
  std::vector<hydraforge::pdk::CommandSpec> list_commands() const override;
  std::optional<std::string> has_conflict(
      const std::string& name, const std::string& plugin_origin) const override;

  /** @brief /help 文本: 由 list_commands() 自动生成 name + description + usage, 无特权显示差异 */
  std::string render_help() const;

  ToolCoordinator* coordinator() const { return coordinator_; }

 private:
  static constexpr const char* kReservedExit = "/exit";
  ToolCoordinator* coordinator_;  // non-owning
  std::map<std::string, hydraforge::pdk::CommandSpec> commands_;  // map = 字典序
};

}  // namespace agenticdsl
