// tests/test_dsl_extensions.cpp
// C6+C7 测试: exec: fork/join 等价性 + lint 警告/豁免/新文件 heuristic + 嵌套深度超限
// 设计依据: openspec/changes/from-roadmap-phase-6c-execution-dsl/specs/dsl-extensions/spec.md
// 作者: Solo Dev
// 日期: 2026-09-02

#include "catch_amalgamated.hpp"
#include "modules/parser/markdown_parser.h"
#include "modules/parser/declarative_style.h"
#include "agenticdsl/parser/node_factory.h"
#include "core/types/node.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>

namespace fs = std::filesystem;
using namespace agenticdsl;

// 辅助: 提取图的边集合 (from, to) 用于等价性对比
struct Edge {
  std::string from;
  std::string to;
  bool operator<(const Edge& other) const {
    if (from != other.from) return from < other.from;
    return to < other.to;
  }
  bool operator==(const Edge& other) const { return from == other.from && to == other.to; }
};

std::vector<Edge> extract_edges(const ParsedGraph& graph) {
  std::vector<Edge> edges;
  for (const auto& node : graph.nodes) {
    // next 边
    for (const auto& nxt : node->next) {
      edges.push_back({node->path, nxt});
    }
    // fork branches 边
    if (node->type == NodeType::FORK) {
      auto* f = static_cast<const ForkNode*>(node.get());
      for (const auto& b : f->branches) {
        edges.push_back({node->path, b});
      }
    }
    // join wait_for 边
    if (node->type == NodeType::JOIN) {
      auto* j = static_cast<const JoinNode*>(node.get());
      for (const auto& w : j->wait_for) {
        edges.push_back({w, node->path});
      }
    }
  }
  std::sort(edges.begin(), edges.end());
  return edges;
}

// 辅助: 解析 markdown 并获取主图
ParsedGraph parse_main_graph(const std::string& markdown) {
  MarkdownParser parser;
  auto graphs = parser.parse_from_string(markdown);
  for (auto& g : graphs) {
    if (g.path == "/main") return std::move(g);
  }
  if (!graphs.empty()) return std::move(graphs.front());
  return ParsedGraph{};
}

// 手写 fork/join 对照图构造器 (规范形式, 用于等价性对比)
ParsedGraph build_manual_fork_join(const std::string& base_path,
                                   const std::vector<std::string>& tools,
                                   const std::vector<std::string>& next_paths) {
  ParsedGraph g;
  g.path = "/main";
  const std::string fork_path = base_path + "_fork";
  const std::string join_path = base_path + "_join";
  std::vector<std::string> branch_paths;
  for (size_t i = 0; i < tools.size(); ++i) {
    branch_paths.push_back(base_path + "/branch_" + std::to_string(i));
  }

  // fork
  auto fork = std::make_unique<ForkNode>(fork_path, branch_paths, std::vector<std::string>{join_path});
  fork->metadata["exec_expanded"] = true;
  fork->metadata["exec_kind"] = "fork";
  g.nodes.push_back(std::move(fork));

  // children (tool_call, next 空 — 依靠 join wait_for)
  for (size_t i = 0; i < tools.size(); ++i) {
    std::string child_path = branch_paths[i];
    nlohmann::json tc = {
      {"type", "tool_call"},
      {"tool", tools[i]},
      {"arguments", nlohmann::json::object()},
      {"output_keys", nlohmann::json::array({tools[i] + "_result"})}
    };
    auto child = NodeFactoryRegistry::global().create("tool_call", child_path, tc);
    REQUIRE(child != nullptr);
    child->next = {}; // 空 next, join 通过 wait_for 等待
    g.nodes.push_back(std::move(child));
  }

  // join
  auto join = std::make_unique<JoinNode>(join_path, branch_paths, "error_on_conflict", next_paths);
  join->metadata["exec_expanded"] = true;
  join->metadata["exec_kind"] = "join";
  g.nodes.push_back(std::move(join));

  return g;
}

// =====================================================================
// C6: exec: fork/join 等价性测试 (≥3 case)
// =====================================================================
TEST_CASE("C6 exec: expands to fork/join DAG equivalent to manual", "[parser][dsl-ext][C6]") {
  MarkdownParser parser;

  SECTION("exec: [tool_a, tool_b, tool_c] three branches") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: fan_out
    exec: [shell/exec, fs/read, custom/tool]
    next: [/main/after]
  - id: after
    type: assign
    assign:
      done: "true"
# --- END AgenticDSL ---
```)";

    auto graph = parse_main_graph(markdown);

    // 验证生成了 6 个节点: fork + 3 children + join + after
    REQUIRE(graph.nodes.size() == 6);

    // 验证手写对照图边集合一致
    auto manual = build_manual_fork_join("/main/fan_out",
                                          {"shell/exec", "fs/read", "custom/tool"},
                                          {"/main/after"});
    auto edges_manual = extract_edges(manual);
    auto edges_parsed = extract_edges(graph);
    REQUIRE(edges_parsed == edges_manual);
  }

  SECTION("exec: [single_tool] single-item optimization (no fork/join)") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: single
    exec: [shell/exec]
    next: [/main/after]
  - id: after
    type: assign
    assign:
      done: "true"
# --- END AgenticDSL ---
```)";

    auto graph = parse_main_graph(markdown);

    // 单元素优化: 只有 2 个节点 (tool_call + after), 无 fork/join
    REQUIRE(graph.nodes.size() == 2);
    REQUIRE(graph.nodes[0]->type == NodeType::TOOL_CALL);
    auto* tc = dynamic_cast<ToolCallNode*>(graph.nodes[0].get());
    REQUIRE(tc != nullptr);
    REQUIRE(tc->tool_name == "shell/exec");
    REQUIRE(graph.nodes[0]->next == std::vector<std::string>{"/main/after"});
  }

  SECTION("exec: with object child nodes (full node definitions)") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: complex_fan
    exec:
      - type: tool_call
        tool: fs/read
        arguments:
          path: "/tmp"
        output_keys: ["content"]
      - type: dsl_call
        prompt_template: "Summarize: {{ content }}"
        output_keys: ["summary"]
    next: [/main/after]
  - id: after
    type: assign
    assign:
      final: "done"
# --- END AgenticDSL ---
```)";

    auto graph = parse_main_graph(markdown);
    // fork + 2 children + join + after = 5 nodes
    REQUIRE(graph.nodes.size() == 5);

    // 验证生成的节点类型
    size_t fork_count = 0, join_count = 0, tool_count = 0, dsl_count = 0;
    for (const auto& n : graph.nodes) {
      if (n->type == NodeType::FORK) fork_count++;
      else if (n->type == NodeType::JOIN) join_count++;
      else if (n->type == NodeType::TOOL_CALL) tool_count++;
      else if (n->type == NodeType::DSL_CALL) dsl_count++;
    }
    REQUIRE(fork_count == 1);
    REQUIRE(join_count == 1);
    REQUIRE(tool_count == 1);
    REQUIRE(dsl_count == 1);
  }
}

// =====================================================================
// C6: 嵌套深度超限 ParseError
// =====================================================================
TEST_CASE("C6 exec: nesting depth limit ParseError", "[parser][dsl-ext][C6]") {
  MarkdownParser parser;

  SECTION("nested exec: [exec: [...]] throws ParseError") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: outer
    exec:
      - type: tool_call
        tool: fs/read
        arguments: {path: "/tmp"}
        output_keys: ["a"]
      - exec: [shell/exec, custom/tool]  # 嵌套 exec
    next: [/main/end]
# --- END AgenticDSL ---
```)";

    REQUIRE_THROWS_WITH(
        parser.parse_from_string(markdown),
        Catch::Matchers::ContainsSubstring("nesting exceeds max_exec_depth")
    );
  }
}

// =====================================================================
// C7: lint 警告/豁免/新文件 heuristic 测试
// 通过调用 dual_syntax_lint 可执行文件 (compile definition 注入路径) 测试
// =====================================================================
TEST_CASE("C7 dual_syntax_lint warnings and exemptions", "[parser][dsl-ext][C7]") {
  // 编译时通过 TEST_DUAL_SYNTAX_LINT_PATH 注入 (CMake target_compile_definitions)
  // 该宏定义为字符串字面量，直接使用
#ifdef TEST_DUAL_SYNTAX_LINT_PATH
  const char* lint_bin = TEST_DUAL_SYNTAX_LINT_PATH;
#else
  FAIL("TEST_DUAL_SYNTAX_LINT_PATH not defined at compile time");
  return;
#endif

  // 创建临时目录
  fs::path tmpdir = fs::temp_directory_path() / ("dual_syntax_lint_test_" + std::to_string(std::time(nullptr)));
  fs::create_directories(tmpdir);
  auto cleanup = [&]() { fs::remove_all(tmpdir); };

  auto run_lint = [&](const fs::path& file, bool include_historical = false,
                      const std::string& ship_ts = "2026-09-02") -> std::string {
    std::string cmd = std::string(lint_bin) + " " + file.string();
    if (include_historical) cmd += " --include-historical";
    cmd += " --ship-timestamp " + ship_ts;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "PIPE_ERROR";
    std::string result;
    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
    pclose(pipe);
    return result;
  };

  SECTION("legacy -> reference triggers warning") {
    // 新文件: mtime 现在 > ship_ts
    fs::path f = tmpdir / "new_file.agent.md";
    {
      std::ofstream of(f);
      of << R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: a
    type: tool_call
    tool: shell/exec
    next: [/main/b]
  - id: b
    type: assign
    assign:
      x: "-> output_name"   # legacy -> reference
# --- END AgenticDSL ---
```)";
    }
    // 设置 mtime 为未来 (新文件)
    auto now = fs::file_time_type::clock::now();
    fs::last_write_time(f, now + std::chrono::hours(1));

    std::string out = run_lint(f);
    REQUIRE(out.find("warning: legacy syntax") != std::string::npos);
    REQUIRE(out.find("-> output_name") != std::string::npos);
    REQUIRE(out.find("'$output_name'") != std::string::npos);
  }

  SECTION("lint:disable comment suppresses next line") {
    fs::path f = tmpdir / "disable_test.agent.md";
    {
      std::ofstream of(f);
      of << R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: a
    type: tool_call
    tool: shell/exec
    next: [/main/b]
  - id: b
    type: assign
    assign:
      # lint:disable dual-syntax
      x: "-> output_name"   # 应被抑制
# --- END AgenticDSL ---
```)";
    }
    auto now = fs::file_time_type::clock::now();
    fs::last_write_time(f, now + std::chrono::hours(1));

    std::string out = run_lint(f);
    REQUIRE(out.find("warning: legacy syntax") == std::string::npos);
  }

  SECTION("historical file skipped by default") {
    // 旧文件: mtime 设为 ship_ts 之前
    fs::path f = tmpdir / "historical.agent.md";
    {
      std::ofstream of(f);
      of << R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: a
    type: tool_call
    tool: shell/exec
    next: [/main/b]
  - id: b
    type: assign
    assign:
      x: "-> output_name"   # legacy
# --- END AgenticDSL ---
```)";
    }
    // 设置 mtime 为历史日期 (ship_ts 2026-09-02 之前)
    // file_clock 无 from_time_t, 用当前时间减一年模拟历史文件
    auto file_tp = std::chrono::file_clock::now() - std::chrono::hours(24*365);
    fs::last_write_time(f, file_tp);

    std::string out = run_lint(f); // 默认不 include-historical
    REQUIRE(out.find("warning: legacy syntax") == std::string::npos);

    // 加 --include-historical 应该报警
    std::string out2 = run_lint(f, true);
    REQUIRE(out2.find("warning: legacy syntax") != std::string::npos);
  }
}

// =====================================================================
// 向后兼容性: 现有 .agent.md 解析零变化
// =====================================================================
TEST_CASE("Backward compat: existing .agent.md files parse unchanged", "[parser][compat]") {
  // 遍历 examples/**/*.agent.md + lib/**/*.agent.md (相对 repo root, 非 ctest CWD)
  fs::path repo_root = fs::path(__FILE__).parent_path().parent_path();
  std::vector<fs::path> agent_files;
  for (const auto& dir : {repo_root / "examples", repo_root / "lib"}) {
    if (!fs::exists(dir)) continue;
    for (const auto& e : fs::recursive_directory_iterator(dir)) {
      if (e.path().extension() == ".md" && e.path().filename().string().find(".agent.md") != std::string::npos) {
        agent_files.push_back(e.path());
      }
    }
  }
  REQUIRE_FALSE(agent_files.empty());

  MarkdownParser parser;
  for (const auto& f : agent_files) {
    INFO("File: " << f.string());
    // 解析不应抛出 parse error (lint warning 可接受但 parser 不报错)
    REQUIRE_NOTHROW(parser.parse_from_file(f.string()));
  }
}

// =====================================================================
// W5: ADR-0072 D4 backend:/env_vars: 字段解析 (≥4 case)
// 设计依据: openspec/changes/adr-0072-d4-backend-parser/specs/adr-0072-backend-field/spec.md
// =====================================================================
TEST_CASE("W5 backend: docker field parsed into node.metadata", "[parser][dsl-ext][W5][backend]") {
  MarkdownParser parser;
  std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: docker_node
    type: tool_call
    tool: shell/exec
    arguments:
      cmd: "echo hello"
    output_keys: ["result"]
    backend: docker
# --- END AgenticDSL ---
```)";

  auto graph = parse_main_graph(markdown);
  REQUIRE(graph.nodes.size() == 1);
  REQUIRE(graph.nodes[0]->metadata.contains("backend"));
  REQUIRE(graph.nodes[0]->metadata["backend"] == "docker");
}

TEST_CASE("W5 env: alias mapped to env_vars in metadata", "[parser][dsl-ext][W5][env-alias]") {
  MarkdownParser parser;
  std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: aliased_node
    type: tool_call
    tool: shell/exec
    arguments:
      cmd: "echo hi"
    output_keys: ["result"]
    env:
      DB_HOST: localhost
      API_TOKEN: secret_abc123
# --- END AgenticDSL ---
```)";

  auto graph = parse_main_graph(markdown);
  REQUIRE(graph.nodes.size() == 1);
  REQUIRE(graph.nodes[0]->metadata.contains("env_vars"));
  REQUIRE(graph.nodes[0]->metadata["env_vars"]["DB_HOST"] == "localhost");
  REQUIRE(graph.nodes[0]->metadata["env_vars"]["API_TOKEN"] == "secret_abc123");
}

TEST_CASE("W5 env_vars: takes priority over env: when both present", "[parser][dsl-ext][W5][env-priority]") {
  MarkdownParser parser;
  std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: priority_node
    type: tool_call
    tool: shell/exec
    arguments:
      cmd: "echo hi"
    output_keys: ["result"]
    env:
      KEY: old_string_value
    env_vars:
      KEY: new_string_value
# --- END AgenticDSL ---
```)";

  auto graph = parse_main_graph(markdown);
  REQUIRE(graph.nodes.size() == 1);
  REQUIRE(graph.nodes[0]->metadata["env_vars"]["KEY"] == "new_string_value");
}

TEST_CASE("W5 no backend/env fields → metadata backward compatible", "[parser][dsl-ext][W5][backward-compat]") {
  MarkdownParser parser;
  std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: plain_node
    type: tool_call
    tool: shell/exec
    arguments:
      cmd: "echo hi"
    output_keys: ["result"]
# --- END AgenticDSL ---
```)";

  auto graph = parse_main_graph(markdown);
  REQUIRE(graph.nodes.size() == 1);
  REQUIRE_FALSE(graph.nodes[0]->metadata.contains("backend"));
  REQUIRE_FALSE(graph.nodes[0]->metadata.contains("env_vars"));
}