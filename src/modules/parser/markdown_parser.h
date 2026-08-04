// modules/parser/include/parser/markdown_parser.h
#ifndef AGENTICDSL_MODULES_PARSER_MARKDOWN_PARSER_H
#define AGENTICDSL_MODULES_PARSER_MARKDOWN_PARSER_H

#include "core/types/node.h" // 引入 Node, NodePath, ParsedGraph
#include "core/types/budget.h" // 引入 ExecutionBudget
#include "agenticdsl/contract/iparser.h"  // ADR-0019 §1.4：实现 IParser 抽象接口
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>

namespace agenticdsl {

// DslFormat: 描述 .agent.md 文件的元数据格式 (fix-markdown-parser-yaml)
// - kNone:  未检测到有效元数据
// - kBold:  传统 **key**: value bold frontmatter
// - kYamlFenced: ```yaml ... ``` 代码块内含 # --- BEGIN AgenticDSL --- 标记
enum class DslFormat { kNone, kBold, kYamlFenced };

class MarkdownParser : public IParser {
public:
    std::vector<ParsedGraph> parse_from_string(const std::string& markdown_content);
    std::vector<ParsedGraph> parse_from_file(const std::string& file_path);

    ParsedGraph parse(const std::string& markdown) override;
    ParsedGraph parse_file(const std::filesystem::path& p) override;

    std::unique_ptr<Node> create_node_from_json(const NodePath& path, const nlohmann::json& node_json);

    // fix-markdown-parser-yaml: 格式检测与 yaml 块提取
    // detect_format: 扫描 markdown 内容, 返回主导元数据格式
    DslFormat detect_format(const std::string& content) const;
    // parse_yaml_fenced_block: 提取 ```yaml ... ``` 块内含 # --- BEGIN AgenticDSL --- 标记的内容
    // 返回块内文本（不含围栏标记），未找到返回空字符串
    std::string parse_yaml_fenced_block(const std::string& content) const;

private:
    void validate_nodes(const std::vector<std::unique_ptr<Node>>& nodes);
    // Helper to parse signature.outputs into JSON Schema
    std::optional<nlohmann::json> parse_output_schema_from_signature(const std::string& signature_str);
};

} // namespace agenticdsl

#endif // AGENTICDSL_MODULES_PARSER_MARKDOWN_PARSER_H
