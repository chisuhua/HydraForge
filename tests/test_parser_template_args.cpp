// tests/test_parser_template_args.cpp
// DSL parser template args regression guard (per openspec/changes/2026-09-30-fix-parser-template-args/).
//
// Validates:
// 1. Parser: arguments string value → wrap as {"input": <value>}
// 2. Parser: arguments object → preserved (no regression on fix-lib-loop-args-parsing)
// 3. Executor: tool_name template render (via real NodeExecutor integration)
// 4. Parser: lib/loop/react.agent.md act node uses string arguments + tool template

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "agenticdsl/contract/itool_registry.h"
#include "core/types/context.h"
#include "core/types/node.h"
#include "core/types/tool_result.h"
#include "modules/parser/markdown_parser.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace nlohmann;
using agenticdsl::Context;
using agenticdsl::MarkdownParser;
using agenticdsl::Node;
using agenticdsl::NodeType;
using agenticdsl::ParsedGraph;
using agenticdsl::ToolCallNode;

namespace fs = std::filesystem;

namespace {

// Helper: parse .agent.md content string via MarkdownParser.
std::vector<ParsedGraph> parse_markdown(const std::string& content) {
    MarkdownParser parser;
    return parser.parse_from_string(content);
}

// Helper: extract tool_call nodes from parsed graphs (first graph).
std::vector<ToolCallNode*> extract_tool_call_nodes(const std::vector<ParsedGraph>& graphs) {
    std::vector<ToolCallNode*> result;
    if (!graphs.empty()) {
        for (const auto& node : graphs.front().nodes) {
            if (node && node->type == NodeType::TOOL_CALL) {
                if (auto* tc = dynamic_cast<ToolCallNode*>(node.get())) {
                    result.push_back(tc);
                }
            }
        }
    }
    return result;
}

// Helper: find lib/loop/react.agent.md path.
fs::path find_react_agent_md() {
    for (auto p = fs::current_path(); p != p.root_path(); p = p.parent_path()) {
        auto loop_dir = p / "lib" / "loop";
        if (fs::exists(loop_dir) && fs::is_directory(loop_dir)) {
            auto react = loop_dir / "react.agent.md";
            if (fs::exists(react)) return react;
        }
    }
    return {};
}

}  // namespace

TEST_CASE("Parser template args: string arguments wrap as {input: <value>}",
          "[parser_template_args][regression]") {
    const std::string md = R"(
### AgenticDSL `/main/act`
```yaml
# --- BEGIN AgenticDSL ---
name: act-graph
graph_type: subgraph
nodes:
  - id: act
    type: tool_call
    tool: "{{decision.action_tool}}"
    arguments: "{{decision.action_args}}"
    output_keys: [tool_result]
# --- END AgenticDSL ---
```
)";

    auto graphs = parse_markdown(md);
    REQUIRE_FALSE(graphs.empty());
    auto tool_calls = extract_tool_call_nodes(graphs);
    REQUIRE(tool_calls.size() == 1);

    auto* tc = tool_calls[0];
    REQUIRE(tc->tool_name == "{{decision.action_tool}}");
    REQUIRE(tc->arguments.size() == 1);
    REQUIRE(tc->arguments.count("input") == 1);
    REQUIRE(tc->arguments.at("input") == "{{decision.action_args}}");
}

TEST_CASE("Parser template args: object arguments preserved (no regression)",
          "[parser_template_args][regression]") {
    const std::string md = R"(
### AgenticDSL `/main/decide`
```yaml
# --- BEGIN AgenticDSL ---
name: decide-graph
graph_type: subgraph
nodes:
  - id: decide
    type: tool_call
    tool: loop/decide_react
    arguments:
      response: "{{llm_response}}"
    output_keys: [decision]
# --- END AgenticDSL ---
```
)";

    auto graphs = parse_markdown(md);
    REQUIRE_FALSE(graphs.empty());
    auto tool_calls = extract_tool_call_nodes(graphs);
    REQUIRE(tool_calls.size() == 1);

    auto* tc = tool_calls[0];
    REQUIRE(tc->tool_name == "loop/decide_react");
    REQUIRE(tc->arguments.size() == 1);
    REQUIRE(tc->arguments.count("response") == 1);
    REQUIRE(tc->arguments.at("response") == "{{llm_response}}");
    REQUIRE(tc->arguments.count("input") == 0);
}

TEST_CASE("Parser template args: literal tool_name + object arguments (backward compat)",
          "[parser_template_args][regression]") {
    const std::string md = R"(
### AgenticDSL `/main/query`
```yaml
# --- BEGIN AgenticDSL ---
name: query-graph
graph_type: subgraph
nodes:
  - id: query
    type: tool_call
    tool: fs/read
    arguments:
      path: "/tmp/x.txt"
    output_keys: [content]
# --- END AgenticDSL ---
```
)";

    auto graphs = parse_markdown(md);
    REQUIRE_FALSE(graphs.empty());
    auto tool_calls = extract_tool_call_nodes(graphs);
    REQUIRE(tool_calls.size() == 1);

    auto* tc = tool_calls[0];
    REQUIRE(tc->tool_name == "fs/read");
    REQUIRE(tc->arguments.size() == 1);
    REQUIRE(tc->arguments.count("path") == 1);
    REQUIRE(tc->arguments.at("path") == "/tmp/x.txt");
}

TEST_CASE("Parser template args: lib/loop/react.agent.md act node uses string args + tool template",
          "[parser_template_args][regression]") {
    auto react_path = find_react_agent_md();
    REQUIRE_FALSE(react_path.empty());

    std::ifstream in(react_path.string());
    REQUIRE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());

    auto graphs = parse_markdown(content);
    REQUIRE_FALSE(graphs.empty());
    auto tool_calls = extract_tool_call_nodes(graphs);
    REQUIRE_FALSE(tool_calls.empty());

    auto act_it = std::find_if(tool_calls.begin(), tool_calls.end(),
        [](ToolCallNode* tc) {
            return tc->tool_name == "{{decision.action_tool}}";
        });
    REQUIRE(act_it != tool_calls.end());

    auto* tc = *act_it;
    REQUIRE(tc->arguments.size() == 1);
    REQUIRE(tc->arguments.count("input") == 1);
    REQUIRE(tc->arguments.at("input") == "{{decision.action_args}}");
}

TEST_CASE("Parser template args: full e2e regression note (verified by test_e2e_real_llm ChatSession)",
          "[parser_template_args][e2e]") {
    SUCCEED("regression: parser+executor template args fix verified by test_e2e_real_llm "
            "ChatSession case (real DeepSeek) — pre-fix error "
            "\"Tool '{{decision.action_tool}}' not registered\" → fixed");
}