// tests/test_yaml_parser.cpp
// 功能描述: fix-markdown-parser-yaml 单元测试
//   - MarkdownParser fenced yaml 块检测 (detect_format)
//   - MarkdownParser fenced yaml 块提取 (parse_yaml_fenced_block)
// 注: DslValidator 测试在 examples/pdk_chat_demo/tests/ (example 侧代码)
// 作者: Sisyphus (fix-markdown-parser-yaml)
// 最后修改日期: 2026-08-04
#include <catch_amalgamated.hpp>

#include "parser/markdown_parser.h"

#include <string>

using agenticdsl::DslFormat;
using agenticdsl::MarkdownParser;

namespace {

// Test fixture: yaml fenced block with # --- BEGIN AgenticDSL --- marker
// 注: 使用 R"yaml(...)yaml" 避免内容中的 backtick 干扰 raw string 解析
const std::string kYamlAgentMd =
    "# Sample Agent\n\n"
    "Some description.\n\n"
    "```yaml\n"
    "# --- BEGIN AgenticDSL ---\n"
    "name: test_agent\n"
    "version: 1.0.0\n"
    "agent_loop: react\n"
    "## Nodes\n"
    "- id: start\n"
    "  type: start\n"
    "- id: end\n"
    "  type: end\n"
    "```\n";

// Test fixture: bold frontmatter (existing format)
const std::string kBoldAgentMd =
    "# Sample Agent\n\n"
    "**name**: test_agent\n"
    "**version**: 1.0.0\n"
    "**agent_loop**: react\n\n"
    "## Nodes\n"
    "```json\n"
    "[\n"
    "  {\"id\": \"start\", \"type\": \"start\"},\n"
    "  {\"id\": \"end\", \"type\": \"end\"}\n"
    "]\n"
    "```\n";

}  // namespace

TEST_CASE("MarkdownParser::detect_format returns kYamlFenced for yaml block",
          "[parser][yaml]") {
  MarkdownParser parser;
  REQUIRE(parser.detect_format(kYamlAgentMd) == DslFormat::kYamlFenced);
}

TEST_CASE("MarkdownParser::detect_format returns kBold for bold frontmatter",
          "[parser][yaml]") {
  MarkdownParser parser;
  REQUIRE(parser.detect_format(kBoldAgentMd) == DslFormat::kBold);
}

TEST_CASE("MarkdownParser::detect_format returns kNone for neither",
          "[parser][yaml]") {
  MarkdownParser parser;
  REQUIRE(parser.detect_format("# Just a title\n\nNo metadata here.")
              == DslFormat::kNone);
}

TEST_CASE("MarkdownParser::parse_yaml_fenced_block extracts yaml text",
          "[parser][yaml]") {
  MarkdownParser parser;
  std::string yaml_text = parser.parse_yaml_fenced_block(kYamlAgentMd);
  REQUIRE_FALSE(yaml_text.empty());
  REQUIRE(yaml_text.find("name: test_agent") != std::string::npos);
  REQUIRE(yaml_text.find("agent_loop: react") != std::string::npos);
}

TEST_CASE("MarkdownParser::parse_yaml_fenced_block returns empty for bold-only",
          "[parser][yaml]") {
  MarkdownParser parser;
  REQUIRE(parser.parse_yaml_fenced_block(kBoldAgentMd).empty());
}

TEST_CASE(
    "MarkdownParser::parse_yaml_fenced_block returns empty for missing marker",
    "[parser][yaml]") {
  MarkdownParser parser;
  const std::string no_marker = R"md(
```yaml
name: foo
```
)md";
  REQUIRE(parser.parse_yaml_fenced_block(no_marker).empty());
}
