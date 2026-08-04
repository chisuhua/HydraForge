// include/agenticdsl/pdk/command_macros.h
// 功能描述：DECLARE_COMMAND 宏 — 命令注册脚手架 (ADR-0070 §决策 2, ADR-0021 P1-P6)。
//          宏展开为 CommandSpec 静态实例 + 错误包装 handler, 静态链接, Runtime 零感知。
// 作者：AgenticDSL / adr-0070-declare-command
// 最后修改日期：2026-08-04
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "common/policy/execution_policy.h"  // ToolMetadata / ToolCallContext (与 tool_macros.h 同路径)

namespace agenticdsl {
class ToolCoordinator;  // 前向声明, 禁止在 CommandContext 暴露 IToolRegistry
}  // namespace agenticdsl

namespace hydraforge::pdk {

/** @brief 命令元数据 (ADR-0070 §决策 3) */
struct CommandSpec {
  std::string name;           // `/compact`, 以 `/` 开头
  std::string description;    // 非空
  std::string usage;          // `/compact [max_tokens]`
  std::string plugin_origin;  // 宏自动填充 (__FILE__)
  std::function<std::string(agenticdsl::ToolCallContext&)> handler;  // 错误包装 handler
};

/** @brief 命令执行上下文 — 仅暴露治理路径入口 (ADR-0070 §决策 5) */
struct CommandContext {
  std::string user_input;                                // 原始 `/xxx args` 输入
  agenticdsl::ToolCallContext tool_ctx{};                // 组装后的工具调用上下文
  agenticdsl::ToolCoordinator* tool_coordinator = nullptr;  // 唯一工具入口, 禁止 IToolRegistry&
};

}  // namespace hydraforge::pdk
