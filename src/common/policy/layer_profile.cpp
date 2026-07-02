// src/common/policy/layer_profile.cpp
// 功能描述：LayerProfile 辅助函数实现
// 作者：AgenticDSL Phase3 / Sprint 14 C4 ship
// 最后修改日期：2026-06-29
#include "common/policy/layer_profile.h"

#include <algorithm>
#include <cctype>

namespace agenticdsl {

namespace {

// Case-insensitive string comparison (ASCII only, 不处理 unicode)
bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

LayerProfile parse_layer(std::string_view s) {
  if (iequals(s, "workflow")) return LayerProfile::Workflow;
  if (iequals(s, "thinking")) return LayerProfile::Thinking;
  if (iequals(s, "cognitive")) return LayerProfile::Cognitive;
  throw std::invalid_argument("Unknown caller_layer: '" + std::string(s) +
                              "' (expected: workflow/thinking/cognitive)");
}

bool check_layer_permission(LayerProfile caller, ToolCategory category) {
  // 复刻 ADR-0004 §8 矩阵
  switch (caller) {
    case LayerProfile::Workflow:
      // L2: 完全允许
      return true;
    case LayerProfile::Thinking:
      // L3: 只允许 ReadOnly
      return category == ToolCategory::ReadOnly;
    case LayerProfile::Cognitive:
      // L4: 禁止所有 tool_call
      return false;
  }
  return false;  // unreachable, defense-in-depth
}

std::string to_string(LayerProfile layer) {
  switch (layer) {
    case LayerProfile::Workflow: return "workflow";
    case LayerProfile::Thinking: return "thinking";
    case LayerProfile::Cognitive: return "cognitive";
  }
  return "unknown";  // unreachable
}

std::string to_string(ToolCategory category) {
  switch (category) {
    case ToolCategory::ReadOnly: return "ReadOnly";
    case ToolCategory::WriteFile: return "WriteFile";
    case ToolCategory::Execute: return "Execute";
    case ToolCategory::Network: return "Network";
    case ToolCategory::StateModify: return "StateModify";
  }
  return "Unknown";
}

/**
 * @brief 注册时验证 allowed_layers 与 ToolCategory 的兼容性 (C6 Sprint 16)
 *
 * 遍历 meta.allowed_layers, 对每个 layer 检查该 layer 是否允许 meta.category 类型的工具。
 * 若 allowed_layers 为空 (默认全允许), 跳过检查。
 * @throws std::invalid_argument 若 allowed_layers 包含不兼容的 layer
 */
void check_registration_permission(const ToolMetadata& meta) {
  if (meta.allowed_layers.empty()) return; // 空 = 全允许, 跳过

  for (const auto& layer : meta.allowed_layers) {
    // Check: does this layer allow this category?
    // Matrix from ADR-0004 §8:
    //   Cognitive  → {ReadOnly}
    //   Thinking   → {ReadOnly, WriteFile}
    //   Workflow   → {ReadOnly, WriteFile, Execute, Network, StateModify}
    bool allowed = false;
    switch (layer) {
      case LayerProfile::Cognitive:
        allowed = (meta.category == ToolCategory::ReadOnly);
        break;
      case LayerProfile::Thinking:
        allowed = (meta.category == ToolCategory::ReadOnly || 
                   meta.category == ToolCategory::WriteFile);
        break;
      case LayerProfile::Workflow:
        allowed = true; // all categories allowed
        break;
    }
    if (!allowed) {
      throw std::invalid_argument(
        "ToolMetadata registration: layer " + to_string(layer) +
        " does not permit category " + std::to_string(static_cast<int>(meta.category)) +
        " for tool '" + meta.name + "'");
    }
  }
}

}  // namespace agenticdsl