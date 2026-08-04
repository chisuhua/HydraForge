// dsl_validator.cpp - PDK Chat Demo DSL Schema 校验器实现
// 关联: examples/pdk_chat_demo/dsl_validator.h
//       openspec/changes/pdk-chat-demo-v1-recap/design.md §T2
//       openspec/changes/fix-markdown-parser-yaml/ (yaml fenced block 支持)
// 作者: Sisyphus (OhMyOpenCode), 2026-07-27
// 更新: 2026-07-30 — 接受可选 IToolRegistry*, 启用 MISSING_TOOL_DEPENDENCY 检查
// 更新: 2026-08-04 — fix-markdown-parser-yaml: 双格式自动检测 (bold + yaml fenced)

#include "dsl_validator.h"

#include <nlohmann/json.hpp>
#include <regex>
#include <set>
#include <stdexcept>

#include <agenticdsl/contract/itool_registry.h>

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
// fix-markdown-parser-yaml: yaml fenced block 解析 (双格式支持)
// 扫描 ```yaml 代码块, 首行含 # --- BEGIN AgenticDSL --- 标记
// 返回块内纯 yaml 文本 (不含围栏), 未找到返回空
// ============================================================
static std::string extract_yaml_fenced_block(const std::string& content) {
  static const std::string fence_open = "```yaml";
  static const std::string begin_marker = "# --- BEGIN AgenticDSL ---";
  static const std::string fence_close = "```";

  size_t pos = 0;
  while (pos < content.size()) {
    size_t fence_pos = content.find(fence_open, pos);
    if (fence_pos == std::string::npos) return "";

    size_t after_fence = fence_pos + fence_open.size();
    size_t line_end = content.find('\n', after_fence);
    if (line_end == std::string::npos) return "";
    std::string first_line = content.substr(after_fence, line_end - after_fence);
    // trim 前导空白
    size_t first_nonspace = first_line.find_first_not_of(" \t\r");
    std::string trimmed = (first_nonspace == std::string::npos)
        ? "" : first_line.substr(first_nonspace);

    if (trimmed == begin_marker) {
      size_t content_start = line_end + 1;
      size_t close_pos = content.find(fence_close, content_start);
      if (close_pos == std::string::npos) return "";
      return content.substr(content_start, close_pos - content_start);
    }
    pos = line_end + 1;
  }
  return "";
}

// 从 yaml 文本提取 frontmatter 字段值 (如 name/version/agent_loop)
// yaml 顶层为 key: value 格式; 返回 value 的字符串表示
static std::string yaml_field_value(const std::string& yaml_text,
                                    const std::string& key) {
  // 简单行扫描: 匹配 "key: value" 或 "key: value # comment"
  std::regex pattern("^\\s*" + key + "\\s*:\\s*([^#\\n]+?)\\s*(?:#.*)?$",
                      std::regex::multiline);
  std::smatch match;
  if (std::regex_search(yaml_text, match, pattern)) {
    std::string val = match[1].str();
    // 去除首尾引号
    if (val.size() >= 2 &&
        ((val.front() == '"' && val.back() == '"') ||
         (val.front() == '\'' && val.back() == '\''))) {
      val = val.substr(1, val.size() - 2);
    }
    return val;
  }
  return "";
}

// 从 yaml 文本提取 Nodes 列表 JSON
// 查找 "nodes:" 键, 后续列表项转为 nlohmann::json 数组
static std::string yaml_nodes_json(const std::string& yaml_text) {
  // 找 nodes: 起始位置
  size_t nodes_pos = yaml_text.find("\nnodes:");
  if (nodes_pos == std::string::npos) {
    // 也尝试文件开头就是 nodes:
    nodes_pos = yaml_text.find("nodes:");
    if (nodes_pos == std::string::npos) return "";
    if (nodes_pos > 0 && yaml_text[nodes_pos - 1] != '\n') {
      // "nodes:" 不是行首, 可能是别处字符串含
      // 重置为第一处行首
      nodes_pos = yaml_text.find("\nnodes:");
      if (nodes_pos == std::string::npos) return "";
    }
  } else {
    nodes_pos += 1;  // 跳过 \n
  }

  // 提取从 nodes: 到文件末尾的列表部分
  size_t list_start = yaml_text.find(':', nodes_pos);
  if (list_start == std::string::npos) return "";
  list_start += 1;  // 跳过 :

  // 跳到下一个非空白字符（换行+缩进）
  size_t content_start = yaml_text.find_first_not_of(" \t\r\n", list_start);
  if (content_start == std::string::npos) return "";

  // 找到 nodes 列表结束位置（下一个顶层 key，或文件末尾）
  // 简化: 取到文件末尾 (yaml 顶层 nodes 通常是最后一个字段)
  return yaml_text.substr(content_start);
}

// ============================================================
// 主校验入口
// ============================================================
ValidationResult DslValidator::validate(const std::string& markdown_content,
                                       const agenticdsl::IToolRegistry* registry) {
  ValidationResult result;

  // ----------------------------------------------------------
  // fix-markdown-parser-yaml: 双格式检测
  // 优先 yaml fenced 块 (生产 .agent.md 格式), 回退 bold (**key**: value)
  // ----------------------------------------------------------
  std::string yaml_block = extract_yaml_fenced_block(markdown_content);
  bool use_yaml_format = !yaml_block.empty();

  // ----------------------------------------------------------
  // 1. 必填字段检查
  // ----------------------------------------------------------
  for (const auto& field : REQUIRED_FIELDS) {
    std::string value = use_yaml_format
        ? yaml_field_value(yaml_block, field)
        : extract_frontmatter_value(markdown_content, field);
    if (value.empty()) {
      result.add_error("MISSING_REQUIRED_FIELD", "frontmatter",
                       "missing required field: " + field);
    }
  }

  // ----------------------------------------------------------
  // 2. Nodes 节存在性 (双格式: yaml 路径取 yaml 块后 nodes 列表, bold 路径取 ## Nodes JSON)
  // ----------------------------------------------------------
  std::string nodes_json = use_yaml_format
      ? yaml_nodes_json(yaml_block)
      : extract_nodes_json(markdown_content);
  if (nodes_json.empty()) {
    result.add_error(use_yaml_format ? "MISSING_SECTION" : "MISSING_SECTION",
                     use_yaml_format ? "yaml nodes" : "## Nodes",
                     use_yaml_format
                         ? "missing 'nodes:' list in yaml block"
                         : "missing '## Nodes' section or JSON code block");
    return result;
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
      } else if (registry != nullptr && !registry->has_tool(node["tool_name"].get<std::string>())) {
        // registry 提供时，查 ToolRegistry 实际注册表，未注册则拒绝
        result.add_error("MISSING_TOOL_DEPENDENCY", node_path,
                         "call_tool references unregistered tool '" +
                             node["tool_name"].get<std::string>() + "'");
      }
    }
  }

  return result;
}

}  // namespace pdk_chat_demo
