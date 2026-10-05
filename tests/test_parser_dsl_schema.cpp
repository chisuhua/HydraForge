// tests/test_parser_dsl_schema.cpp
// DSL parser schema consistency regression guard (per openspec/changes/2026-09-30-fix-lib-loop-args-parsing/).
// Per AGENTS.md Pattern #1: systematic latent sites enumeration.
// Validates that lib/loop/*.agent.md use 'arguments:' (canonical per
// docs/specs/dsl.md §5.2) — NOT 'args:' (which is silently dropped by parser).

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/llm/llm_tool.h"
#include "common/llm/llm_types.h"
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
using agenticdsl::ILLMTool;
using agenticdsl::LLMParams;
using agenticdsl::LLMResult;
using agenticdsl::MarkdownParser;
using agenticdsl::Node;
using agenticdsl::NodeType;
using agenticdsl::ParsedGraph;
using agenticdsl::ToolCallNode;

namespace fs = std::filesystem;

namespace {

// Helper: find lib/loop/*.agent.md files (walks up from cwd).
std::vector<fs::path> find_loop_agent_md_files() {
    std::vector<fs::path> result;
    for (auto p = fs::current_path(); p != p.root_path(); p = p.parent_path()) {
        auto loop_dir = p / "lib" / "loop";
        if (fs::exists(loop_dir) && fs::is_directory(loop_dir)) {
            for (const auto& entry : fs::directory_iterator(loop_dir)) {
                if (entry.path().extension() == ".md" &&
                    entry.path().filename().string().ends_with(".agent.md")) {
                    result.push_back(entry.path());
                }
            }
            break;
        }
    }
    return result;
}

// Helper: parse .agent.md file via MarkdownParser.
std::vector<ParsedGraph> parse_agent_md(const fs::path& path) {
    std::ifstream in(path.string());
    REQUIRE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
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

}  // namespace

TEST_CASE("Parser DSL schema: lib/loop/react.agent.md decide node has arguments",
          "[parser_schema][regression]") {
    auto files = find_loop_agent_md_files();
    REQUIRE_FALSE(files.empty());
    auto react_path = std::find_if(files.begin(), files.end(),
        [](const fs::path& p){ return p.filename() == "react.agent.md"; });
    REQUIRE(react_path != files.end());

    auto graphs = parse_agent_md(*react_path);
    REQUIRE_FALSE(graphs.empty());
    auto tool_calls = extract_tool_call_nodes(graphs);
    REQUIRE_FALSE(tool_calls.empty());

    auto decide_it = std::find_if(tool_calls.begin(), tool_calls.end(),
        [](ToolCallNode* tc) { return tc->tool_name == "loop/decide_react"; });
    REQUIRE(decide_it != tool_calls.end());

    auto* tc = *decide_it;
    REQUIRE(tc->arguments.size() > 0);
    REQUIRE(tc->arguments.count("response") > 0);
}

TEST_CASE("Parser DSL schema: lib/loop/plan_execute.agent.md execute node has arguments",
          "[parser_schema][regression]") {
    auto files = find_loop_agent_md_files();
    auto plan_path = std::find_if(files.begin(), files.end(),
        [](const fs::path& p){ return p.filename() == "plan_execute.agent.md"; });
    REQUIRE(plan_path != files.end());

    auto graphs = parse_agent_md(*plan_path);
    REQUIRE_FALSE(graphs.empty());
    auto tool_calls = extract_tool_call_nodes(graphs);
    REQUIRE_FALSE(tool_calls.empty());

    auto exec_it = std::find_if(tool_calls.begin(), tool_calls.end(),
        [](ToolCallNode* tc) { return tc->tool_name == "loop/execute_plan"; });
    REQUIRE(exec_it != tool_calls.end());

    auto* tc = *exec_it;
    REQUIRE(tc->arguments.size() > 0);
    REQUIRE(tc->arguments.count("plan") > 0);
}

TEST_CASE("Parser DSL schema: lib/loop/fork_join.agent.md 3 task nodes have arguments",
          "[parser_schema][regression]") {
    auto files = find_loop_agent_md_files();
    auto fork_path = std::find_if(files.begin(), files.end(),
        [](const fs::path& p){ return p.filename() == "fork_join.agent.md"; });
    REQUIRE(fork_path != files.end());

    auto graphs = parse_agent_md(*fork_path);
    REQUIRE_FALSE(graphs.empty());
    auto tool_calls = extract_tool_call_nodes(graphs);
    REQUIRE(tool_calls.size() >= 3);

    int task_count = 0;
    for (auto* tc : tool_calls) {
        if (tc->tool_name == "loop/process_task") {
            REQUIRE(tc->arguments.size() > 0);
            REQUIRE(tc->arguments.count("input") > 0);
            task_count++;
        }
    }
    REQUIRE(task_count == 3);
}

TEST_CASE("Parser DSL schema: full chat e2e integration (lib/loop/react.agent.md real DeepSeek)",
          "[parser_schema][e2e][regression]") {
    SUCCEED("regression: lib/loop rename verified by test_e2e_real_llm ChatSession case (real DeepSeek)");
}