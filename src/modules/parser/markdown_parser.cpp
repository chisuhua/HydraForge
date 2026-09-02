// modules/parser/src/markdown_parser.cpp
#include "parser/markdown_parser.h"
#include "parser/declarative_style.h"
#include "agenticdsl/parser/node_factory.h"
#include "common/utils/parser_utils.h"
#include "common/log/log.h"  // agenticdsl::log facade
#include "common/utils/yaml_json.h"
#include "common/utils/template_renderer.h"
#include "core/types/node.h" 
#include "core/types/resource.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <regex> // For parsing signature (if needed for output_schema)

namespace {

// C6: 从节点 JSON 提取 next 路径列表 (string 或 array), 供 exec: 展开复用
// 与 node_factory.cpp parse_context 的 next 语义一致
std::vector<std::string> extract_next_paths(const nlohmann::json& node_json) {
  std::vector<std::string> next_paths;
  if (node_json.contains("next")) {
    const auto& next = node_json["next"];
    if (next.is_string()) {
      next_paths.push_back(next.get<std::string>());
    } else if (next.is_array()) {
      for (const auto& np : next) {
        next_paths.push_back(np.get<std::string>());
      }
    }
  }
  return next_paths;
}

// C6: exec: declarative style 展开钩子 — 仅当节点 JSON 含 exec: key 时触发
// 返回 true 表示该节点已被展开 (生成 1..N 个节点追加到 graph.nodes)
bool try_expand_exec_node(const std::string& node_path,
                          const nlohmann::json& node_json,
                          agenticdsl::ParsedGraph& graph) {
  if (!agenticdsl::DeclarativeStyleParser::has_exec_key(node_json)) {
    return false;
  }
  agenticdsl::DeclarativeStyleParser parser;
  auto next_paths = extract_next_paths(node_json);
  auto metadata = node_json.value("metadata", nlohmann::json::object());
  auto result = parser.expand_exec_array(node_path, node_json["exec"], next_paths, metadata);
  for (auto& n : result.generated_nodes) {
    graph.nodes.push_back(std::move(n));
  }
  return true;
}

} // namespace

namespace agenticdsl {

// Helper to parse output_keys (string or array) from JSON
inline std::vector<std::string> parse_output_keys(const nlohmann::json& node_json, const NodePath& path) {
    if (!node_json.contains("output_keys")) {
        throw std::runtime_error("Missing 'output_keys' in node: " + path);
    }
    const auto& ok = node_json["output_keys"];
    if (ok.is_string()) {
        return {ok.get<std::string>()};
    } else if (ok.is_array()) {
        std::vector<std::string> keys;
        for (const auto& k : ok) {
            keys.push_back(k.get<std::string>());
        }
        return keys;
    } else {
        throw std::runtime_error("'output_keys' must be string or array in node: " + path);
    }
}

// Parse ResourceType from string
inline ResourceType parse_resource_type(const std::string& type_str) {
    if (type_str == "file") return ResourceType::FILE;
    if (type_str == "postgres") return ResourceType::POSTGRES;
    if (type_str == "mysql") return ResourceType::MYSQL;
    if (type_str == "sqlite") return ResourceType::SQLITE;
    if (type_str == "api_endpoint") return ResourceType::API_ENDPOINT;
    if (type_str == "vector_store") return ResourceType::VECTOR_STORE;
    if (type_str == "custom") return ResourceType::CUSTOM;
    throw std::runtime_error("Unknown resource_type '" + type_str + "'");
}

std::vector<ParsedGraph> MarkdownParser::parse_from_string(const std::string& markdown_content) {
    std::vector<ParsedGraph> graphs;
    std::optional<ExecutionBudget> global_budget; // 临时存储 /__meta__ 中的预算
    auto pathed_blocks = extract_pathed_blocks(markdown_content);

    for (auto& [path, yaml_content] : pathed_blocks) {
        if (!is_valid_node_path(path)) {
            throw std::runtime_error("Invalid node path format: " + path);
        }

        try {
            YAML::Node yaml_root = YAML::Load(yaml_content);
            nlohmann::json json_doc = yaml_to_json(yaml_root);

            if (path == "/__meta__") {
                // 提取 execution_budget（如果存在）
                if (json_doc.contains("execution_budget")) {
                    const auto& bj = json_doc["execution_budget"];
                    ExecutionBudget budget;
                    if (bj.contains("max_nodes") && bj["max_nodes"].is_number_integer()) {
                        budget.max_nodes = bj["max_nodes"].get<int>();
                    }
                    if (bj.contains("max_llm_calls") && bj["max_llm_calls"].is_number_integer()) {
                        budget.max_llm_calls = bj["max_llm_calls"].get<int>();
                    }
                    if (bj.contains("max_duration_sec") && bj["max_duration_sec"].is_number_integer()) {
                        budget.max_duration_sec = bj["max_duration_sec"].get<int>();
                    }
                    if (bj.contains("max_subgraph_depth") && bj["max_subgraph_depth"].is_number_integer()) {
                        budget.max_subgraph_depth = bj["max_subgraph_depth"].get<int>();
                    }
                    if (bj.contains("max_snapshots") && bj["max_snapshots"].is_number_integer()) {
                        budget.max_snapshots = bj["max_snapshots"].get<int>();
                    }
                    if (bj.contains("snapshot_max_size_kb") && bj["snapshot_max_size_kb"].is_number_integer()) {
                        budget.snapshot_max_size_kb = bj["snapshot_max_size_kb"].get<size_t>();
                    }
                    global_budget = std::move(budget);
                }
                continue; // /__meta__ 不是可执行的子图，跳过
            }

            // Handle subgraph (e.g., /main, /lib/reasoning/example)
            if (json_doc.contains("graph_type") && json_doc["graph_type"] == "subgraph") {
                ParsedGraph graph;
                graph.path = path;
                graph.metadata = json_doc.value("metadata", nlohmann::json::object());
                // Also extract entry from root level if present
                if (json_doc.contains("entry")) {
                    graph.metadata["entry"] = json_doc["entry"];
                }

                if (json_doc.contains("signature")) {
                    graph.signature = json_doc["signature"].get<std::string>();
                    // v3.1: Parse output_schema from signature
                    graph.output_schema = parse_output_schema_from_signature(graph.signature.value());
                }
                if (json_doc.contains("permissions") && json_doc["permissions"].is_array()) {
                    for (const auto& p : json_doc["permissions"]) {
                        if (p.is_string()) {
                            graph.permissions.push_back(p.get<std::string>());
                        }
                    }
                }
                graph.is_standard_library = (path.rfind("/lib/", 0) == 0); // 以 /lib/ 开头

                if (json_doc.contains("nodes") && json_doc["nodes"].is_array()) {
                    for (const auto& node_json : json_doc["nodes"]) {
                        std::string id = node_json.value("id", "");
                        if (id.empty()) {
                            throw std::runtime_error("Node in subgraph '" + path + "' missing 'id'");
                        }
                        NodePath node_path = path + "/" + id;
                        // C6: exec: declarative style 分支钩子 — 仅当节点含 exec: key 时触发
                        if (try_expand_exec_node(node_path, node_json, graph)) {
                          continue; // 节点已展开，跳过常规 create_node_from_json
                        }
                        auto node = create_node_from_json(node_path, node_json);
                        if (node) {
                            graph.nodes.push_back(std::move(node));
                        }
                    }
                }
                graphs.push_back(std::move(graph));
                continue;
            }

            // Handle single node (standalone block representing one node)
            if (json_doc.contains("type")) {
                ParsedGraph graph;
                graph.path = path;
                graph.metadata = json_doc.value("metadata", nlohmann::json::object());
                // 单节点图也可有 signature
                if (json_doc.contains("signature")) {
                    graph.signature = json_doc["signature"].get<std::string>();
                    // v3.1: Parse output_schema from signature
                    graph.output_schema = parse_output_schema_from_signature(graph.signature.value());
                }
                if (json_doc.contains("permissions") && json_doc["permissions"].is_array()) {
                    for (const auto& p : json_doc["permissions"]) {
                        if (p.is_string()) {
                            graph.permissions.push_back(p.get<std::string>());
                        }
                    }
                }
                graph.is_standard_library = (path.rfind("/lib/", 0) == 0);

                // C6: exec: declarative style 分支钩子 — 单节点图同样支持 exec: 展开
                if (try_expand_exec_node(path, json_doc, graph)) {
                    graphs.push_back(std::move(graph));
                    continue;
                }
                auto node = create_node_from_json(path, json_doc);
                if (node) {
                    graph.nodes.push_back(std::move(node));
                    graphs.push_back(std::move(graph));
                }
                continue;
            }

        } catch (const YAML::ParserException& e) {
            throw std::runtime_error("YAML parse error in block '" + path + "': " + e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error("Error parsing block '" + path + "': " + std::string(e.what()));
        }
    }

    // 将 global_budget 注入到 graphs 中（例如附加到第一个图，或单独存储）
    // 方案：注入到第一个图（或任意图），TopoScheduler 会检查
    if (global_budget.has_value() && !graphs.empty()) {
        graphs[0].budget = std::move(global_budget);
    }

    return graphs;
}

std::vector<ParsedGraph> MarkdownParser::parse_from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_from_string(buffer.str());
}

// IParser overrides: 从 vector<ParsedGraph> 适配为单 ParsedGraph。
// 选取策略：优先 path == "/main" 的子图，否则取第一个非空结果。
ParsedGraph MarkdownParser::parse(const std::string& markdown) {
    auto graphs = parse_from_string(markdown);
    if (graphs.empty()) return ParsedGraph{};
    for (auto& g : graphs) {
        if (g.path == "/main") return std::move(g);
    }
    return std::move(graphs.front());
}

ParsedGraph MarkdownParser::parse_file(const std::filesystem::path& p) {
    auto graphs = parse_from_file(p.string());
    if (graphs.empty()) return ParsedGraph{};
    for (auto& g : graphs) {
        if (g.path == "/main") return std::move(g);
    }
    return std::move(graphs.front());
}

std::unique_ptr<Node> MarkdownParser::create_node_from_json(const NodePath& path, const nlohmann::json& node_json) {
    LOG_DEBUG("Parsing node at " << path << ": " << node_json.dump(2));
    std::string type_str = node_json.at("type").get<std::string>();

    if (!NodeFactoryRegistry::global().has_factory(type_str)) {
        // Unknown type: ignore (forward-compatible per spec)
        return nullptr;
    }
    return NodeFactoryRegistry::global().create(type_str, path, node_json);
}

void MarkdownParser::validate_nodes(const std::vector<std::unique_ptr<Node>>& nodes) {
    // Optional: implement validation (e.g., path uniqueness, next existence)
    // For v1, rely on executor-level validation
}

// v3.1: Helper to parse signature.outputs into JSON Schema
std::optional<nlohmann::json> MarkdownParser::parse_output_schema_from_signature(const std::string& signature_str) {
    // This is a simplified example. A full implementation would require a proper parser
    // for the signature format defined in the spec (e.g., "(input: string) -> {result: number}")
    // For now, we'll look for a pattern like " -> { ... }" and assume it's valid JSON schema.
    std::regex output_pattern(R"(\s*->\s*(\{.*\}))");
    std::smatch match;
    if (std::regex_search(signature_str, match, output_pattern)) {
        try {
            std::string schema_str = match[1].str();
            nlohmann::json schema = nlohmann::json::parse(schema_str);
            return schema;
        } catch (const std::exception& e) {
            // If parsing the output part fails, log a warning but don't fail the whole parse
            LOG_WARN("Could not parse output schema from signature '" << signature_str << "': " << e.what());
            // Return empty object or null if parsing fails
            return nlohmann::json::object();
        }
    }
    // If no output part is found, return nullopt
    return std::nullopt;
}

// fix-markdown-parser-yaml: 扫描 markdown 内容检测主导元数据格式
// 优先级: kYamlFenced > kBold > kNone (生产 .agent.md 多用 yaml fenced 块)
DslFormat MarkdownParser::detect_format(const std::string& content) const {
    if (content.find("```yaml") != std::string::npos &&
        content.find("# --- BEGIN AgenticDSL ---") != std::string::npos) {
        return DslFormat::kYamlFenced;
    }
    if (content.find("**") != std::string::npos) {
        return DslFormat::kBold;
    }
    return DslFormat::kNone;
}

// fix-markdown-parser-yaml: 提取 fenced yaml 代码块内含 # --- BEGIN AgenticDSL --- 标记的内容
// 行扫描: 找 ```yaml 起, 下一行匹配标记, 截取到 ``` 止
std::string MarkdownParser::parse_yaml_fenced_block(const std::string& content) const {
    const std::string fence_open = "```yaml";
    const std::string begin_marker = "# --- BEGIN AgenticDSL ---";
    const std::string fence_close = "```";

    size_t pos = 0;
    while (pos < content.size()) {
        size_t fence_pos = content.find(fence_open, pos);
        if (fence_pos == std::string::npos) return "";

        // 跳过 fence_open 自身
        size_t line_start = fence_pos + fence_open.size();
        // 跳过 fence_open 后的换行符 (若有), 定位到下一行开头
        if (line_start < content.size() && content[line_start] == '\n') {
            line_start += 1;
        } else if (line_start + 1 < content.size() &&
                   content[line_start] == '\r' && content[line_start + 1] == '\n') {
            line_start += 2;
        }

        // 读取下一行 (BEGIN marker 所在行)
        size_t line_end = content.find('\n', line_start);
        if (line_end == std::string::npos) return "";
        std::string first_line = content.substr(line_start, line_end - line_start);
        // trim 前导空白
        size_t first_nonspace = first_line.find_first_not_of(" \t\r");
        std::string trimmed_first = (first_nonspace == std::string::npos)
            ? "" : first_line.substr(first_nonspace);

        if (trimmed_first == begin_marker) {
            size_t content_start = line_end + 1;
            size_t close_pos = content.find(fence_close, content_start);
            if (close_pos == std::string::npos) return "";
            return content.substr(content_start, close_pos - content_start);
        }
        // 未匹配: 跳到 fence_open 后的下一行继续扫描
        pos = fence_pos + fence_open.size();
    }
    return "";
}

} // namespace agenticdsl
