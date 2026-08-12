// test_dsl_validation.cpp - T2: DSL Schema 校验测试
// 关联: examples/pdk_chat_demo/dsl_validator.h
//       openspec/changes/pdk-chat-demo-v1-recap/design.md §T2
// 作者: Sisyphus (OhMyOpenCode), 2026-07-27
// 更新: 2026-07-30 — 添加 MISSING_TOOL_DEPENDENCY 测试 (T2 任务 6.5)

#include "catch_amalgamated.hpp"

#include "dsl_validator.h"

#include <agenticdsl/contract/itool_registry.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

using namespace pdk_chat_demo;

namespace {

// Minimal IToolRegistry mock for MISSING_TOOL_DEPENDENCY tests.
// Only has_tool() is exercised by DslValidator; the rest throw / no-op.
class MockToolRegistry : public ::agenticdsl::IToolRegistry {
 public:
  explicit MockToolRegistry(std::unordered_set<std::string> registered)
      : registered_(std::move(registered)) {}

  bool has_tool(const std::string& name) const override {
    return registered_.count(name) > 0;
  }

  nlohmann::json call_tool(
      const std::string&,
      const std::unordered_map<std::string, std::string>&) override {
    return {};
  }

  std::vector<std::string> list_tools() const override {
    return std::vector<std::string>(registered_.begin(), registered_.end());
  }

  void register_tool_function(std::string, ::agenticdsl::ToolMetadata,
                              ToolFunc) override {}

  void register_llm_tool(std::string, std::unique_ptr<::agenticdsl::ILLMTool>,
                         const ::agenticdsl::LLMParams&) override {}

  bool is_llm_tool(const std::string&) const override { return false; }
  const ::agenticdsl::LLMParams& get_llm_params(const std::string&) const override {
    static ::agenticdsl::LLMParams empty{};
    return empty;
  }

  nlohmann::json call_llm_tool(const std::string&, const std::string&,
                               const ::agenticdsl::LLMParams&) override {
    return {};
  }

  void set_cost_callback(CostCallback) override {}

 private:
  std::unordered_set<std::string> registered_;
};

}  // namespace

// ============================================================
// 合法最小 DSL fixture
// ============================================================
static const std::string VALID_MINIMAL_DSL = R"(# test-agent
- **name**: test-agent
- **version**: 1.0.0
- **agent_loop**: react

## Nodes
```json
[
  {"id": "start", "type": "start", "next": "end"},
  {"id": "end",   "type": "end"}
]
```
)";

// ============================================================
// 非法 DSL fixtures
// ============================================================
static const std::string DSL_NO_AGENT_LOOP = R"(# bad-agent
- **name**: bad-agent
- **version**: 1.0.0

## Nodes
```json
[
  {"id": "start", "type": "start", "next": "end"},
  {"id": "end",   "type": "end"}
]
```
)";

static const std::string DSL_NO_NAME = R"(# no-name
- **version**: 1.0.0
- **agent_loop**: react

## Nodes
```json
[
  {"id": "start", "type": "start", "next": "end"},
  {"id": "end",   "type": "end"}
]
```
)";

static const std::string DSL_NO_VERSION = R"(# no-version
- **name**: no-version
- **agent_loop**: react

## Nodes
```json
[
  {"id": "start", "type": "start", "next": "end"},
  {"id": "end",   "type": "end"}
]
```
)";

static const std::string DSL_UNKNOWN_NODE_TYPE = R"(# bad-type
- **name**: bad-type
- **version**: 1.0.0
- **agent_loop**: react

## Nodes
```json
[
  {"id": "start", "type": "start", "next": "bad1"},
  {"id": "bad1",  "type": "foobar",  "next": "end"},
  {"id": "end",   "type": "end"}
]
```
)";

static const std::string DSL_MALFORMED_JSON = R"(# bad-json
- **name**: bad-json
- **version**: 1.0.0
- **agent_loop**: react

## Nodes
```json
[
  {"id": "start", "type": "start", "next": "end",
  {"id": "end", "type": "end"
]
```
)";

static const std::string DSL_NO_NODES_SECTION = R"(# no-nodes
- **name**: no-nodes
- **version**: 1.0.0
- **agent_loop**: react

No Nodes section here.
)";

static const std::string DSL_WITH_CALL_TOOL = R"(# tool-agent
- **name**: tool-agent
- **version**: 1.0.0
- **agent_loop**: react

## Nodes
```json
[
  {"id": "start",   "type": "start",      "next": "step1"},
  {"id": "step1",   "type": "call_tool",  "tool_name": "echo", "next": "end"},
  {"id": "end",     "type": "end"}
]
```
)";

// ============================================================
// yaml fenced block fixtures (fix-markdown-parser-yaml regression)
// ============================================================

// Production-style yaml fenced block with LF line endings
// Format: ```yaml\n# --- BEGIN AgenticDSL ---\n...fields...\nnodes:\n...\n```
static const std::string YAML_FENCED_DSL_LF =
    "# yaml-agent\n"
    "```yaml\n"
    "# --- BEGIN AgenticDSL ---\n"
    "name: yaml-agent\n"
    "version: 1.0.0\n"
    "agent_loop: react\n"
    "nodes:\n"
    "[\n"
    "  {\"id\":\"start\",\"type\":\"start\"},\n"
    "  {\"id\":\"end\",\"type\":\"end\"}\n"
    "]\n"
    "```\n";

// Production-style yaml fenced block with CRLF line endings
static const std::string YAML_FENCED_DSL_CRLF =
    "# yaml-agent-crlf\r\n"
    "```yaml\r\n"
    "# --- BEGIN AgenticDSL ---\r\n"
    "name: yaml-agent-crlf\r\n"
    "version: 1.0.0\r\n"
    "agent_loop: react\r\n"
    "nodes:\r\n"
    "[\r\n"
    "  {\"id\":\"start\",\"type\":\"start\"},\r\n"
    "  {\"id\":\"end\",\"type\":\"end\"}\r\n"
    "]\r\n"
    "```\r\n";

// yaml fenced block with missing begin marker (should fall back to bold format)
static const std::string YAML_FENCED_NO_MARKER =
    "# fallback-agent\n"
    "```yaml\n"
    "name: fallback-agent\n"
    "version: 1.0.0\n"
    "agent_loop: react\n"
    "nodes:\n"
    "[\n"
    "  {\"id\":\"start\",\"type\":\"start\"},\n"
    "  {\"id\":\"end\",\"type\":\"end\"}\n"
    "]\n"
    "```\n";

// ============================================================
// 测试用例
// ============================================================

TEST_CASE("valid minimal DSL passes validation", "[dsl_validator]") {
  DslValidator validator;
  auto result = validator.validate(VALID_MINIMAL_DSL);

  REQUIRE(result.valid == true);
  REQUIRE(result.errors.empty());
}

TEST_CASE("missing required field agent_loop → rejected", "[dsl_validator]") {
  DslValidator validator;
  auto result = validator.validate(DSL_NO_AGENT_LOOP);

  REQUIRE(result.valid == false);
  REQUIRE(result.errors.size() >= 1);

  bool found_agent_loop_err = false;
  for (const auto& e : result.errors) {
    if (e.type == "MISSING_REQUIRED_FIELD" &&
        e.message.find("agent_loop") != std::string::npos) {
      found_agent_loop_err = true;
    }
  }
  REQUIRE(found_agent_loop_err == true);
}

TEST_CASE("missing required field name → rejected", "[dsl_validator]") {
  DslValidator validator;
  auto result = validator.validate(DSL_NO_NAME);

  REQUIRE(result.valid == false);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "MISSING_REQUIRED_FIELD" &&
        e.message.find("name") != std::string::npos) {
      found = true;
    }
  }
  REQUIRE(found == true);
}

TEST_CASE("missing required field version → rejected", "[dsl_validator]") {
  DslValidator validator;
  auto result = validator.validate(DSL_NO_VERSION);

  REQUIRE(result.valid == false);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "MISSING_REQUIRED_FIELD" &&
        e.message.find("version") != std::string::npos) {
      found = true;
    }
  }
  REQUIRE(found == true);
}

TEST_CASE("unknown node type 'foobar' → rejected", "[dsl_validator]") {
  DslValidator validator;
  auto result = validator.validate(DSL_UNKNOWN_NODE_TYPE);

  REQUIRE(result.valid == false);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "INVALID_NODE_TYPE" &&
        e.message.find("foobar") != std::string::npos) {
      found = true;
    }
  }
  REQUIRE(found == true);
}

TEST_CASE("malformed JSON in Nodes section → rejected", "[dsl_validator]") {
  DslValidator validator;
  auto result = validator.validate(DSL_MALFORMED_JSON);

  REQUIRE(result.valid == false);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "PARSE_ERROR") {
      found = true;
    }
  }
  REQUIRE(found == true);
}

TEST_CASE("missing ## Nodes section → rejected", "[dsl_validator]") {
  DslValidator validator;
  auto result = validator.validate(DSL_NO_NODES_SECTION);

  REQUIRE(result.valid == false);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "MISSING_SECTION") {
      found = true;
    }
  }
  REQUIRE(found == true);
}

TEST_CASE("valid DSL with call_tool using registered tool passes",
          "[dsl_validator]") {
  DslValidator validator;
  auto result = validator.validate(DSL_WITH_CALL_TOOL);

  REQUIRE(result.valid == true);
  REQUIRE(result.errors.empty());
}

TEST_CASE("multiple errors collected (not fail-fast)", "[dsl_validator]") {
  // 同时缺少 name + version + agent_loop
  std::string dsl_with_all_missing = R"(# all-bad
- **other**: x

## Nodes
```json
[
  {"id": "start", "type": "start", "next": "end"},
  {"id": "bad",   "type": "foobar2", "next": "end"},
  {"id": "end",   "type": "end"}
]
```
)";

  DslValidator validator;
  auto result = validator.validate(dsl_with_all_missing);

  // 应收集 name、version、agent_loop 三个缺失 + 一个非法节点类型
  REQUIRE(result.valid == false);
  REQUIRE(result.errors.size() >= 4);
}

// T2 任务 6.5: call_tool 引用未注册工具 → rejected (with ToolRegistry)
TEST_CASE("call_tool references unregistered tool → rejected",
          "[dsl_validator][tool_registry]") {
  MockToolRegistry registry({});  // 空注册表
  DslValidator validator;
  auto result = validator.validate(DSL_WITH_CALL_TOOL, &registry);

  REQUIRE(result.valid == false);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "MISSING_TOOL_DEPENDENCY" &&
        e.message.find("echo") != std::string::npos) {
      found = true;
    }
  }
  REQUIRE(found == true);
}

TEST_CASE("call_tool references registered tool → passes",
          "[dsl_validator][tool_registry]") {
  MockToolRegistry registry({"echo", "fs/read"});
  DslValidator validator;
  auto result = validator.validate(DSL_WITH_CALL_TOOL, &registry);

  REQUIRE(result.valid == true);
  REQUIRE(result.errors.empty());
}

TEST_CASE("without registry, call_tool only checks string presence",
          "[dsl_validator][tool_registry]") {
  DslValidator validator;
  // DSL_WITH_CALL_TOOL 中 tool_name="echo" 非空，无 registry 时应通过
  auto result = validator.validate(DSL_WITH_CALL_TOOL);
  REQUIRE(result.valid == true);
  REQUIRE(result.errors.empty());
}

TEST_CASE("yaml fenced block with LF (\\n) passes validation",
          "[dsl_validator][yaml_fenced]") {
  const auto result = DslValidator{}.validate(YAML_FENCED_DSL_LF);

  REQUIRE(result.valid);
  REQUIRE(result.errors.empty());
}

TEST_CASE("yaml fenced block with CRLF (\\r\\n) passes validation",
          "[dsl_validator][yaml_fenced]") {
  const auto result = DslValidator{}.validate(YAML_FENCED_DSL_CRLF);

  REQUIRE(result.valid);
  REQUIRE(result.errors.empty());
}

// fix-markdown-parser-yaml regression: yaml block without begin marker falls back
TEST_CASE("yaml fenced block without begin marker falls back to bold format",
          "[dsl_validator][yaml_fenced]") {
  DslValidator validator;
  auto result = validator.validate(YAML_FENCED_NO_MARKER);

  // 缺少 name/version/agent_loop 字段，因为没有 begin_marker 无法识别为 yaml 格式
  REQUIRE(result.valid == false);
  bool found_field_err = false;
  for (const auto& e : result.errors) {
    if (e.type == "MISSING_REQUIRED_FIELD") {
      found_field_err = true;
    }
  }
  REQUIRE(found_field_err == true);
}
