// dsl_validator.cpp - PDK Chat Demo DSL Schema 校验器实现
// 关联: examples/pdk_chat_demo/dsl_validator.h
//       openspec/changes/pdk-chat-demo-v1-recap/design.md §T2
// 作者: Sisyphus (OhMyOpenCode), 2026-07-27

#include "dsl_validator.h"

#include <nlohmann/json.hpp>
#include <regex>
#include <set>
#include <stdexcept>

namespace pdk_chat_demo {

// ============================================================
// 合法节点类型白名单
// ============================================================
static const std::set<std::string> VALID_NODE_TYPES = {
    "start", "end", "call_tool", "llm_generate", "condition",
    "fork", "join", "assign", "resource"
};

// ============================================================
// 必填 frontmatter 字段
// ============================================================
static const std::vector<std::string> REQUIRED_FIELDS = {
    "name", "version", "agent_loop"
};

// ============================================================
// 内部 helper
// ============================================================
std::string DslValidator::extract_frontmatter_value(const std::string& content,
                                                     const std::string& key) {
  // 匹配 **key**: value 格式（Markdown bold syntax）
  std::regex pattern(R"(\*\*)" + key + R"(\*\*:\s*(\S+))");
  std::smatch match;
  if (std::regex_search(content, match, pattern)) {
    return match[1].str();
  }
  return "";
}

std::string DslValidator::extract_nodes_json(const std::string& content) {
  // 找到 ## Nodes 节
  auto nodes_pos = content.find("## Nodes");
  if (nodes_pos == std::string::npos) {
    return "";
  }

  // 在 ## Nodes 之后找 ```json 代码块
  auto after_nodes = content.substr(nodes_pos);
  auto json_start = after_nodes.find("```json");
  if (json_start == std::string::npos) {
    // 尝试 ``` 无语言标识
    json_start = after_nodes.find("```");
    if (json_start == std::string::npos) {
      return "";
    }
  }

  auto code_start = after_nodes.find('\n', json_start);
  if (code_start == std::string::npos) {
    return "";
  }
  code_start += 1;

  auto code_end = after_nodes.find("```", code_start);
  if (code_end == std::string::npos) {
    return "";
  }

  return after_nodes.substr(code_start, code_end - code_start);
}

// ============================================================
// 主校验入口
// ============================================================
ValidationResult DslValidator::validate(const std::string& markdown_content) {
  ValidationResult result;

  // ----------------------------------------------------------
  // 1. 必填字段检查
  // ----------------------------------------------------------
  for (const auto& field : REQUIRED_FIELDS) {
    std::string value = extract_frontmatter_value(markdown_content, field);
    if (value.empty()) {
      result.add_error("MISSING_REQUIRED_FIELD", "frontmatter",
                       "missing required field: " + field);
    }
  }

  // ----------------------------------------------------------
  // 2. Nodes 节存在性
  // ----------------------------------------------------------
  std::string nodes_json = extract_nodes_json(markdown_content);
  if (nodes_json.empty()) {
    result.add_error("MISSING_SECTION", "## Nodes",
                     "missing '## Nodes' section or JSON code block");
    return result;  // 无法继续校验节点
  }

  // ----------------------------------------------------------
  // 3. JSON 解析
  // ----------------------------------------------------------
  nlohmann::json nodes;
  try {
    nodes = nlohmann::json::parse(nodes_json);
  } catch (const nlohmann::json::parse_error& e) {
    result.add_error("PARSE_ERROR", "## Nodes",
                     "invalid JSON in Nodes section: " + std::string(e.what()));
    return result;  // 无法继续校验
  }

  // 确保是数组
  if (!nodes.is_array()) {
    result.add_error("PARSE_ERROR", "## Nodes",
                     "Nodes section must be a JSON array, got: " +
                         std::string(nodes.type_name()));
    return result;
  }

  // ----------------------------------------------------------
  // 4. 逐节点校验
  // ----------------------------------------------------------
  for (size_t i = 0; i < nodes.size(); ++i) {
    const auto& node = nodes[i];
    std::string node_path = "node[" + std::to_string(i) + "]";

    // 4a. 必填字段 per-node
    if (!node.contains("id") || !node["id"].is_string() || node["id"].get<std::string>().empty()) {
      result.add_error("MISSING_REQUIRED_FIELD", node_path,
                       "node missing required field 'id'");
    }
    if (!node.contains("type") || !node["type"].is_string() || node["type"].get<std::string>().empty()) {
      result.add_error("MISSING_REQUIRED_FIELD", node_path,
                       "node missing required field 'type'");
      continue;  // 无 type 无法校验类型
    }

    // 4b. 节点类型白名单
    std::string node_type = node["type"].get<std::string>();
    if (VALID_NODE_TYPES.find(node_type) == VALID_NODE_TYPES.end()) {
      result.add_error("INVALID_NODE_TYPE", node_path,
                       "unknown node type '" + node_type + "'");
    }

    // 4c. call_tool 节点：tool_name 必须存在（if registry provided）
    //     demo-side 不依赖 ToolRegistry, 先做字符串存在性检查
    if (node_type == "call_tool") {
      if (!node.contains("tool_name") || !node["tool_name"].is_string() ||
          node["tool_name"].get<std::string>().empty()) {
        result.add_error("MISSING_REQUIRED_FIELD", node_path,
                         "call_tool node missing required field 'tool_name'");
      }
    }
  }

  return result;
}

}  // namespace pdk_chat_demo
