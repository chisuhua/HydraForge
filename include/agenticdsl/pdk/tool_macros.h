// include/agenticdsl/pdk/tool_macros.h
// 文件头注释
// 功能描述：DECLARE_TOOL 宏 — 工具注册脚手架 (ADR-0021 §3.1)。
//          宏展开为 ToolSpec 元数据 + 错误处理包装的 handler 函数, 自动捕获
//          std::exception 并返回 nlohmann::json 错误对象, 开发者只写 5 行领域逻辑。
//          PDK 静态链接到插件, Runtime 零感知 (P3 静态链接)。
// 设计依据：ADR-0021 §3.1 + ADR-0004 ToolRegistry 安全模型 + ADR-0023 ToolResult 标准化
//          + openspec/changes/2026-07-07-pdk-skeleton
// 作者：AgenticDSL Phase 1 Sprint 4
// 最后修改日期：2026-06-19

#pragma once

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace hydraforge::pdk {

/**
 * @brief 工具参数 Schema (MVP, Sprint 4)
 *
 * 描述单个工具参数的元数据. 未来 Phase 2 可扩展为更丰富的 JSON Schema.
 */
struct ToolParam {
  std::string name;
  std::string type;        // "string" | "int" | "json" | "bool"
  bool required = false;
};

/**
 * @brief 工具权限声明 (MVP metadata only, Phase 2 集成 ADR-0004)
 *
 * 声明工具运行时的权限需求, Sprint 4 仅作为元数据存储, Phase 2/3 由 SafeExec::with_layer_profile 集成.
 */
struct ToolPermissions {
  std::vector<std::string> readonly_paths;
  std::vector<std::string> write_paths;
  bool network = false;
};

/**
 * @brief 工具元数据 (Schema)
 *
 * 用于 PDK DECLARE_TOOL 宏生成的 ToolSpec 实例.
 * Sprint 5 PluginLoader 通过 .so 反射获取 ToolSpec + handler 函数.
 */
struct ToolSpec {
  std::string name;
  std::string description;
  std::vector<ToolParam> params;
  ToolPermissions permissions;
};

/**
 * @brief DECLARE_TOOL 宏 — 工具注册脚手架
 *
 * 展开为:
 *   1. inline ToolSpec tool_spec_##name = { #name, description, {}, {} };
 *   2. inline nlohmann::json tool_handler_##name(const nlohmann::json& __pdk_args)
 *      { try { __VA_ARGS__ } catch (std::exception& e) { return json{{"error", e.what()}}; } }
 *
 * 用法示例 (无尾部 {}, 因为 __VA_ARGS__ 已含 return 语句):
 *   DECLARE_TOOL(echo_tool, "回显工具",
 *     return __pdk_args;
 *   )
 *
 * @param name        工具名 (用于注册 + 调用, 必须唯一)
 * @param description 工具描述 (用于 LLM 提示工程)
 * @param ...         领域逻辑 (必须含 return 语句, try-catch 包装后异常返回 json error)
 */
#define DECLARE_TOOL(name, description, ...) \
    inline ::hydraforge::pdk::ToolSpec tool_spec_##name = { \
        #name, description, {}, {} \
    }; \
    inline nlohmann::json tool_handler_##name(const nlohmann::json& __pdk_args) { \
        try { \
            __VA_ARGS__ \
        } catch (const std::exception& __pdk_e) { \
            return nlohmann::json{{"error", __pdk_e.what()}}; \
        } \
    }

} // namespace hydraforge::pdk