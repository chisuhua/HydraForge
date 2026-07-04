// tests/test_parser.cpp
#include "catch_amalgamated.hpp"
#include "modules/parser/markdown_parser.h"
#include "core/types/node.h"
#include "common/utils/parser_utils.h"
#include "common/utils/yaml_json.h"
#include "agenticdsl/parser/node_factory.h"
#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <yaml-cpp/yaml.h>

using namespace agenticdsl;

// Helper: parse YAML string → nlohmann::json via yaml-cpp
static nlohmann::json parse_yaml_str(const std::string& yaml_str) {
    YAML::Node node = YAML::Load(yaml_str);
    return yaml_to_json(node);
}

// Test 1: Extract pathed blocks correctly (with multi-line YAML)
TEST_CASE("Parse Pathed Blocks", "[parser][utils]") {
    std::string markdown = R"(
Some intro text.

### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/ask"]
# --- END AgenticDSL ---
```

More text.

### AgenticDSL `/main/ask`
```yaml
# --- BEGIN AgenticDSL ---
type: llm_call
prompt_template: "Hello {{ user.name }}!"
output_keys: "greeting"
next: "/main/end"
# --- END AgenticDSL ---
```

### AgenticDSL `/resources/db`
```yaml
# --- BEGIN AgenticDSL ---
type: resource
resource_type: postgres
uri: "postgresql://localhost/test"
scope: global
# --- END AgenticDSL ---
```
)";

    auto blocks = extract_pathed_blocks(markdown);
    REQUIRE(blocks.size() == 3);

    REQUIRE(blocks[0].first == "/main");
    REQUIRE(blocks[1].first == "/main/ask");
    REQUIRE(blocks[2].first == "/resources/db");

    // Check YAML content is extracted (not empty)
    REQUIRE_FALSE(blocks[0].second.empty());
    REQUIRE_FALSE(blocks[1].second.empty());
    REQUIRE_FALSE(blocks[2].second.empty());
}

// Test 2: Parse single llm_call node (backward compatibility - creates DSLNode)
TEST_CASE("Parse llm_call syntax backward compat (creates DSLNode)", "[parser]") {
    std::string yaml = R"(
type: llm_call
prompt_template: "Summarize: {{ input }}"
output_keys: "summary"
next: "/main/end"
metadata:
  description: "Summarization step"
)";
    auto json_doc = parse_yaml_str(yaml);
    MarkdownParser parser;
    auto node = parser.create_node_from_json("/main/summarize", json_doc);

    REQUIRE(node != nullptr);
    REQUIRE(node->type == NodeType::DSL_CALL);
    REQUIRE(node->path == "/main/summarize");
    REQUIRE(node->next.size() == 1);
    REQUIRE(node->next[0] == "/main/end");
    REQUIRE(node->metadata["description"] == "Summarization step");

    // Cast to DSLNode (llm_call creates DSLNode with default llm_tool_name)
    auto* dsl_node = dynamic_cast<DSLNode*>(node.get());
    REQUIRE(dsl_node != nullptr);
    REQUIRE(dsl_node->prompt_template == "Summarize: {{ input }}");
    REQUIRE(dsl_node->output_keys == std::vector<std::string>{"summary"});
    REQUIRE(dsl_node->llm_tool_name == "llama-default"); // Default from backward compat
}

// Test 3: Parse dsl_call node type with all fields
TEST_CASE("Parse DSLNode dsl_call type", "[parser]") {
    std::string yaml = R"(
type: dsl_call
prompt_template: "Generate response: {{ input }}"
llm_tool_name: "llama-7b"
llm_params:
  temperature: 0.5
  max_tokens: 256
  top_p: 0.9
  n_ctx: 4096
  n_threads: 8
  model: "llama-2-7b"
output_keys: "response"
next: "/main/end"
metadata:
  description: "LLM generation step"
)";
    auto json_doc = parse_yaml_str(yaml);
    MarkdownParser parser;
    auto node = parser.create_node_from_json("/main/generate", json_doc);

    REQUIRE(node != nullptr);
    REQUIRE(node->type == NodeType::DSL_CALL);
    REQUIRE(node->path == "/main/generate");
    REQUIRE(node->next.size() == 1);
    REQUIRE(node->next[0] == "/main/end");
    REQUIRE(node->metadata["description"] == "LLM generation step");

    auto* dsl_node = dynamic_cast<DSLNode*>(node.get());
    REQUIRE(dsl_node != nullptr);
    REQUIRE(dsl_node->prompt_template == "Generate response: {{ input }}");
    REQUIRE(dsl_node->llm_tool_name == "llama-7b");
    REQUIRE(dsl_node->output_keys == std::vector<std::string>{"response"});
    
    // Verify LLM params
    REQUIRE(dsl_node->llm_params.temperature == 0.5f);
    REQUIRE(dsl_node->llm_params.max_tokens == 256);
    REQUIRE(dsl_node->llm_params.top_p == 0.9f);
    REQUIRE(dsl_node->llm_params.n_ctx == 4096);
    REQUIRE(dsl_node->llm_params.n_threads == 8);
    REQUIRE(dsl_node->llm_params.model == "llama-2-7b");
}

// Test 4: Parse dsl_call with minimal params
TEST_CASE("Parse DSLNode minimal params", "[parser]") {
    std::string yaml = R"(
type: dsl_call
prompt_template: "Simple prompt"
llm_tool_name: "gpt-4"
output_keys: "result"
)";
    auto json_doc = parse_yaml_str(yaml);
    MarkdownParser parser;
    auto node = parser.create_node_from_json("/main/simple", json_doc);

    auto* dsl_node = dynamic_cast<DSLNode*>(node.get());
    REQUIRE(dsl_node != nullptr);
    REQUIRE(dsl_node->llm_tool_name == "gpt-4");
    
    // Default LLM params (Track 0.1 M1.3: 合并 LLMConfig 后 max_tokens 默认值从 512 调整为 2048)
    REQUIRE(dsl_node->llm_params.temperature == 0.7f); // Default
    REQUIRE(dsl_node->llm_params.max_tokens == 2048);   // Default
}

// Test 5: Parse tool_call with arguments and array output_keys
TEST_CASE("Parse ToolCallNode", "[parser]") {
    std::string yaml = R"(
type: tool_call
tool: http_get
arguments:
  url: "https://api.example.com?q={{ query }}"
  headers: "{{ default(headers, '{}') }}"
output_keys: ["status", "body"]
next: ["/main/process"]
)";
    auto json_doc = parse_yaml_str(yaml);
    MarkdownParser parser;
    auto node = parser.create_node_from_json("/main/fetch", json_doc);

    REQUIRE(node != nullptr);
    REQUIRE(node->type == NodeType::TOOL_CALL);

    auto* tool_node = dynamic_cast<ToolCallNode*>(node.get());
    REQUIRE(tool_node != nullptr);
    REQUIRE(tool_node->tool_name == "http_get");
    REQUIRE(tool_node->arguments.at("url") == "https://api.example.com?q={{ query }}");
    REQUIRE(tool_node->output_keys == std::vector<std::string>{"status", "body"});
    REQUIRE(tool_node->next.size() == 1);
    REQUIRE(tool_node->next[0] == "/main/process");
}

// Test 6: Parse resource node
TEST_CASE("Parse ResourceNode", "[parser]") {
    std::string yaml = R"(
type: resource
resource_type: file
uri: "/data/cache.json"
scope: global
metadata:
  tags: ["cache", "temp"]
)";
    auto json_doc = parse_yaml_str(yaml);
    MarkdownParser parser;
    auto node = parser.create_node_from_json("/resources/cache", json_doc);

    REQUIRE(node != nullptr);
    REQUIRE(node->type == NodeType::RESOURCE);
    REQUIRE(node->metadata["tags"] == nlohmann::json::array({"cache", "temp"}));

    auto* res_node = dynamic_cast<ResourceNode*>(node.get());
    REQUIRE(res_node != nullptr);
    REQUIRE(res_node->resource_type == ResourceType::FILE);
    REQUIRE(res_node->uri == "/data/cache.json");
    REQUIRE(res_node->scope == "global");
}

// Test 7: Parse subgraph (/main)
TEST_CASE("Parse Subgraph", "[parser]") {
    std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
metadata:
  description: "Main workflow"
nodes:
  - id: start
    type: start
    next: ["/main/ask"]
  - id: ask
    type: llm_call
    prompt_template: "What do you need?"
    output_keys: "user_request"
    next: ["/main/end"]
# --- END AgenticDSL ---
```
)";

    MarkdownParser parser;
    auto graphs = parser.parse_from_string(markdown);
    REQUIRE(graphs.size() == 1);

    auto& graph = graphs[0];
    REQUIRE(graph.path == "/main");
    REQUIRE(graph.metadata["description"] == "Main workflow");
    REQUIRE(graph.nodes.size() == 2);

    REQUIRE(graph.nodes[0]->type == NodeType::START);
    REQUIRE(graph.nodes[0]->path == "/main/start");

    REQUIRE(graph.nodes[1]->type == NodeType::DSL_CALL);
    REQUIRE(graph.nodes[1]->path == "/main/ask");
}

// Test 8: output_keys as string vs array
TEST_CASE("OutputKeys Parsing", "[parser]") {
    MarkdownParser parser;

    // String case
    {
        std::string yaml1 = R"(
type: llm_call
prompt_template: "Test"
output_keys: "result"
)";
        auto node1 = parser.create_node_from_json("/test1", parse_yaml_str(yaml1));
        auto* llm1 = dynamic_cast<DSLNode*>(node1.get());
        REQUIRE(llm1->output_keys == std::vector<std::string>{"result"});
    }

    // Array case
    {
        std::string yaml2 = R"(
type: tool_call
tool: mock_tool
output_keys: ["a", "b"]
arguments: {}
)";
        auto node2 = parser.create_node_from_json("/test2", parse_yaml_str(yaml2));
        auto* tool2 = dynamic_cast<ToolCallNode*>(node2.get());
        REQUIRE(tool2->output_keys == std::vector<std::string>{"a", "b"});
    }
}

// Test 9: Invalid path format
TEST_CASE("Invalid Path Format", "[parser]") {
    std::string markdown = R"(
### AgenticDSL `invalid_path`
```yaml
# --- BEGIN AgenticDSL ---
type: assign
assign:
  x: "1"
# --- END AgenticDSL ---
```
)";

    MarkdownParser parser;
    REQUIRE_THROWS_WITH(
        parser.parse_from_string(markdown),
        Catch::Matchers::ContainsSubstring("Invalid node path format")
    );
}

// Test 10: Missing required field (e.g., output_keys)
TEST_CASE("Missing Required Field", "[parser]") {
    std::string yaml = R"(
type: llm_call
prompt_template: "Test"
# output_keys missing!
)";
    MarkdownParser parser;
    REQUIRE_THROWS_WITH(
        parser.create_node_from_json("/bad", parse_yaml_str(yaml)),
        Catch::Matchers::ContainsSubstring("Missing 'output_keys'")
    );
}

// Test 11: Parse signature and permissions from subgraph
TEST_CASE("Parse signature and permissions from subgraph", "[parser][stage3]") {
    std::string markdown = R"(
### AgenticDSL `/lib/math/add`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
signature: "(a: number, b: number) -> sum: number"
permissions: ["file:read"]
nodes:
  - id: add
    type: tool_call
    tool: calculate
    arguments: {a: "1", b: "2", op: "+"}
    output_keys: ["sum"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```
)";
    agenticdsl::MarkdownParser parser;
    auto graphs = parser.parse_from_string(markdown);
    REQUIRE(graphs.size() == 1);
    auto& g = graphs[0];
    REQUIRE(g.path == "/lib/math/add");
    REQUIRE(g.signature == "(a: number, b: number) -> sum: number");
    REQUIRE(g.permissions == std::vector<std::string>{"file:read"});
    REQUIRE(g.is_standard_library == true);
}

// Test 12: Parse signature and permissions from single node
TEST_CASE("Parse signature and permissions from single node", "[parser][stage3]") {
    std::string markdown = R"(
### AgenticDSL `/main/tool`
```yaml
# --- BEGIN AgenticDSL ---
type: tool_call
tool: web_search
signature: "(query: string) -> results"
permissions: ["network"]
arguments: {query: "test"}
output_keys: ["out"]
next: ["/main/end"]
# --- END AgenticDSL ---
```
)";
    agenticdsl::MarkdownParser parser;
    auto graphs = parser.parse_from_string(markdown);
    REQUIRE(graphs.size() == 1);
    auto* node = dynamic_cast<agenticdsl::ToolCallNode*>(graphs[0].nodes[0].get());
    REQUIRE(node->signature == "(query: string) -> results");
    REQUIRE(node->permissions == std::vector<std::string>{"network"});
}

// =====================================================================
// Sprint 7 Day 4: NodeFactoryRegistry 测试补齐 (spec parser-test-coverage)
// =====================================================================

// Test 13: NodeFactoryRegistry 注册 12 个 NodeType (C12: 11→12, 新增 yield)
TEST_CASE("factory_registry_registers_all_types", "[parser][day4]") {
    auto& registry = agenticdsl::NodeFactoryRegistry::global();
    // Sprint 6 实际注册 11 个 factory (start, end, assign, dsl_call, llm_call,
    // tool_call, resource, fork, join, generate_subgraph, assert), 与旧 if-else
    // 11 分支一一对应, 零类型丢失. spec 写 13 是笔误, Day 4 修正.
    // C12 Phase 5 Stage 1 Step 2: 新增 yield factory, 总数 11→12
    // OpenSpec change 2026-07-03-phase5-stage1-step2-yield-stream
    REQUIRE(registry.size() == 12);
}

// Test 14: create() 返回正确子类型 (type 检查)
TEST_CASE("factory_registry_creates_correct_subtype", "[parser][day4]") {
    // llm_call 工厂需要 prompt_template 和 output_keys (见 node_factory.cpp L135-145)
    nlohmann::json j{
        {"prompt_template", "x"},
        {"output_keys", "y"}
    };
    auto node = agenticdsl::NodeFactoryRegistry::global().create("llm_call", "/main/test", j);
    REQUIRE(node != nullptr);
    // llm_call 在内部映射为 DSLNode (llama-default llm_tool_name)
    REQUIRE(node->type == agenticdsl::NodeType::DSL_CALL);
}

// Test 15: 未注册类型通过 parser 返回 nullptr (旧行为, 修正 spec 描述 throw→nullptr)
TEST_CASE("factory_registry_unknown_type_returns_nullptr", "[parser][day4]") {
    nlohmann::json j{{"type", "NonExistentType"}, {"x", 1}};
    agenticdsl::MarkdownParser parser;
    auto node = parser.create_node_from_json("/main/test", j);
    REQUIRE(node == nullptr);
    REQUIRE_FALSE(agenticdsl::NodeFactoryRegistry::global().has_factory("NonExistentType"));
}

// Test 16: global() 是单例 (Meyers singleton, 同地址)
TEST_CASE("factory_registry_global_singleton", "[parser][day4]") {
    auto& a = agenticdsl::NodeFactoryRegistry::global();
    auto& b = agenticdsl::NodeFactoryRegistry::global();
    REQUIRE(&a == &b);
}

// Test 17: TSan 验证 - 4 线程并发 create() + 1 线程 register_factory()
// shared_mutex 保护: 读 (create) 共享, 写 (register) 独占, 0 竞争.
TEST_CASE("factory_registry_concurrent_access", "[parser][day4][tsan]") {
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    nlohmann::json j{
        {"prompt_template", "x"},
        {"output_keys", "y"}
    };
    // 4 个 reader 线程并发调用 create()
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            try {
                (void)agenticdsl::NodeFactoryRegistry::global().create("llm_call", "/main/t", j);
            } catch (...) {
                errors++;
            }
        });
    }
    // 1 个 writer 线程延迟 1ms 后注册新 factory (抢占 shared_mutex 写锁)
    std::thread register_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        try {
            agenticdsl::NodeFactoryRegistry::global().register_factory(
                "day4_test_key",
                [](const agenticdsl::NodePath&,
                   const nlohmann::json&) -> std::unique_ptr<agenticdsl::Node> {
                    return nullptr;  // stub - 仅用于触发 writer 路径
                });
        } catch (...) {
            errors++;
        }
    });
    for (auto& t : threads) t.join();
    register_thread.join();
    REQUIRE(errors.load() == 0);
}
