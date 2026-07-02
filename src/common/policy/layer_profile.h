// src/common/policy/layer_profile.h
// 功能描述：LayerProfile 辅助函数 — parse_layer + check_layer_permission
// 设计依据：ADR-0031 §决策 4 (Oracle session ses_0ed4408faffeLv8VfrC0s5PzW7, 2026-06-29)
//          + ADR-0004 §8 (Layer × Category 矩阵)
// 作者：AgenticDSL Phase3 / Sprint 14 C4 ship
// 最后修改日期：2026-06-29
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "common/policy/execution_policy.h"  // LayerProfile + ToolCategory

namespace agenticdsl {

/**
 * @brief 解析 caller_layer 字符串为 LayerProfile enum
 *
 * @param s caller_layer 字符串 (例如 "workflow" / "Workflow" / "WORKFLOW")
 * @return LayerProfile (Workflow / Thinking / Cognitive)
 * @throw std::invalid_argument 当字符串不匹配任何 layer 时 (fail-fast)
 *
 * Case-insensitive, 接受任意大小写组合.
 */
LayerProfile parse_layer(std::string_view s);

/**
 * @brief 检查 caller_layer 是否可调用给定 category 的工具 (ADR-0004 §8 矩阵)
 *
 * @param caller 调用方 LayerProfile (从 ctx.caller_layer 解析)
 * @param category 工具 ToolCategory (从 meta.category)
 * @return true=允许, false=拒绝
 *
 * 矩阵 (复刻 ADR-0004 §8):
 * - Workflow (L2): all categories
 * - Thinking (L3): ReadOnly only
 * - Cognitive (L4): none (no tool calls)
 */
bool check_layer_permission(LayerProfile caller, ToolCategory category);

/**
 * @brief LayerProfile → string (用于 audit log / 错误信息)
 */
std::string to_string(LayerProfile layer);

/**
 * @brief ToolCategory → string (用于 audit log / 错误信息)
 */
std::string to_string(ToolCategory category);

/**
 * @brief 注册时验证 allowed_layers 与 ToolCategory 的兼容性 (C6 Sprint 16)
 *
 * 遍历 meta.allowed_layers, 对每个 layer 检查该 layer 是否允许 meta.category 类型的工具。
 * 若 allowed_layers 为空 (默认全允许), 跳过检查。
 * @throws std::invalid_argument 若 allowed_layers 包含不兼容的 layer
 */
void check_registration_permission(const ToolMetadata& meta);

}  // namespace agenticdsl