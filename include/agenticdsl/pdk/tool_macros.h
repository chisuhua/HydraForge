// include/agenticdsl/pdk/tool_macros.h
// 文件头注释
// 功能描述：DECLARE_TOOL 宏 — 工具注册脚手架 (ADR-0021 §3.1)。
//          宏展开为 ToolSpec 元数据 + 错误处理包装的 handler 函数, 自动捕获
//          std::exception 并返回 nlohmann::json 错误对象, 开发者只写 5 行领域逻辑。
//          PDK 静态链接到插件, Runtime 零感知 (P3 静态链接)。
// 设计依据：ADR-0021 §3.1 + ADR-0004 V2 ToolRegistry 安全 + ADR-0023
//          + openspec/changes/2026-07-07-pdk-skeleton
// 作者：AgenticDSL Phase 1 Sprint 4 → Sprint 16 C6
// 最后修改日期：2026-06-19

#pragma once

#include <cstring>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <vector>

// C6: ToolMetadata 类型定义位于 src/common/policy/execution_policy.h,
//     通过相同路径格式引用 (同 iexecution_policy.h 做法)。
#include "common/policy/execution_policy.h"

namespace hydraforge::pdk {

/** @brief 工具参数 Schema (MVP, Sprint 4) */
struct ToolParam {
std::string name;
std::string type; // "string" | "int" | "json" | "bool"
bool required = false;
};

/** @brief 工具权限声明 (MVP metadata only)
 *
 *  声明工具运行时的权限需求, 仅作为元数据存储, 后续由安全层集成。
 */
struct ToolPermissions {
std::vector<std::string> readonly_paths;
std::vector<std::string> write_paths;
bool network = false;
};

/** @brief 工具元数据 Schema (C6 升级: 添加 metadata 字段) */
struct ToolSpec {
std::string name;
std::string description;
std::vector<ToolParam> params;
ToolPermissions permissions;
::agenticdsl::ToolMetadata metadata; // C6: V2 安全元数据
};

// ---- C6 helper: string → ApprovalPolicy ----

/** @brief 将审批策略字符串 ("always"/"yolo"/"plan"/"agent") 转为 ApprovalPolicy 结构体 */
inline ::agenticdsl::ApprovalPolicy make_approval(const char* policy_str) {
using AP = ::agenticdsl::ApprovalPolicy;
if (std::strcmp(policy_str, "always") == 0) return AP{true, true, true, true};
if (std::strcmp(policy_str, "yolo") == 0)   return AP{false, false, true, false};
if (std::strcmp(policy_str, "plan") == 0)    return AP{true, false, false, false};
// 默认: "agent" — plan + agent 需审批, yolo 不审
return AP{true, true, false, false};
}

// ---- C6 DECLARE_TOOL 宏 (4 参数 + 元数据) ----

/** @brief DECLARE_TOOL 宏 — C6 工具注册脚手架 (4 参数)
 *
 * 展开为:
 *  1. inline ToolSpec tool_spec_##name = { #name, description, {}, {}, ToolMetadata{...} };
 *  2. inline nlohmann::json tool_handler_##name(const nlohmann::json& __pdk_args)
 *     { try { __VA_ARGS__ } catch (std::exception& e) { return json{{"error", e.what()}}; } }
 *
 * 使用方式 (body 含 return 语句, 无尾部 {}):
 * DECLARE_TOOL(name, description, category, approval_policy,
 *   return __pdk_args;
 * )
 *
 * category: ReadOnly / WriteFile / Execute / Network / StateModify
 * approval_policy: "always" / "yolo" / "plan" / "agent"
 *
 * @param name 工具名 (用于注册+调用, 必须唯一)
 * @param description 工具描述 (用于 LLM 提示工程)
 * @param category 工具安全分类 (enum 成员名: ReadOnly / WriteFile / Execute / Network / StateModify)
 * @param approval_policy 审批策略字符串 ("always" / "yolo" / "plan" / "agent")
 * @param ... 领域逻辑 (必须含 return 语句, try-catch 包装后异常返回 json error)
 */
#define DECLARE_TOOL(name, description, category, approval_policy, ...) \
inline ::hydraforge::pdk::ToolSpec tool_spec_##name = { \
  #name /* name */, \
  description /* description */, \
  {} /* params */, \
  {} /* permissions */, \
  ::agenticdsl::ToolMetadata{ /* metadata */ \
    #name /* name */, \
    description /* description */, \
    "plugin" /* domain */, \
    ::agenticdsl::ToolCategory::category /* category */, \
    ::agenticdsl::LayerProfile::Workflow /* min_layer (default) */, \
    ::hydraforge::pdk::make_approval(approval_policy) /* approval */ \
  } \
}; \
inline nlohmann::json tool_handler_##name(const nlohmann::json& __pdk_args) { \
  try { \
    __VA_ARGS__ \
  } catch (const std::exception& __pdk_e) { \
    return nlohmann::json{{"error", __pdk_e.what()}}; \
  } \
}

} // namespace hydraforge::pdk