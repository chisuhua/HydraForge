// test_dsl_validator_yaml.cpp - YAML→JSON structured parsing tests
// 关联: openspec/changes/from-roadmap-phase-6b-platform/design.md §D1-D6
//       tasks.md §3.1-§3.9
// 作者: Sisyphus (OhMyOpenCode), 2026-08-16

#include "catch_amalgamated.hpp"
#include "dsl_validator.h"

#include <agenticdsl/contract/itool_registry.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace pdk_chat_demo;

namespace {

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

std::string read_fixture(const std::string& filename) {
  const std::vector<std::string> candidates = {
      "examples/pdk_chat_demo/tests/fixtures/" + filename,
      "../examples/pdk_chat_demo/tests/fixtures/" + filename,
      "../../examples/pdk_chat_demo/tests/fixtures/" + filename,
      "../../../examples/pdk_chat_demo/tests/fixtures/" + filename,
      "../../../../examples/pdk_chat_demo/tests/fixtures/" + filename,
  };
  for (const auto& p : candidates) {
    std::ifstream f(p);
    if (f) {
      std::stringstream ss;
      ss << f.rdbuf();
      return ss.str();
    }
  }
  throw std::runtime_error("fixture not found: " + filename);
}

}  // namespace

TEST_CASE("dsl_validator_yaml: valid YAML produces no errors",
          "[dsl_validator][yaml][regression]") {
  DslValidator v;
  std::string md = read_fixture("react_golden.agent.md");
  auto result = v.validate(md);
  REQUIRE(result.valid);
  REQUIRE(result.errors.empty());
}

TEST_CASE("dsl_validator_yaml: missing frontmatter field reports frontmatter.<field>",
          "[dsl_validator][yaml]") {
  DslValidator v;
  const std::string md = R"(```yaml
# --- BEGIN AgenticDSL ---
name: missing-version-demo
# version: 0.1.0    ← 故意省略
agent_loop: react
nodes:
  - id: n1
    type: start
```
)";
  auto result = v.validate(md);
  REQUIRE_FALSE(result.valid);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "MISSING_REQUIRED_FIELD" &&
        e.node_path.find("frontmatter.version") != std::string::npos) {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("dsl_validator_yaml: node missing id reports node[N].id",
          "[dsl_validator][yaml]") {
  DslValidator v;
  const std::string md = R"(```yaml
# --- BEGIN AgenticDSL ---
name: missing-node-id
version: 0.1.0
agent_loop: react
nodes:
  - type: start      # ← 缺 id
  - id: n2
    type: end
```
)";
  auto result = v.validate(md);
  REQUIRE_FALSE(result.valid);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "MISSING_REQUIRED_FIELD" &&
        e.node_path == "node[0].id") {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("dsl_validator_yaml: invalid node type reports node[N].type",
          "[dsl_validator][yaml]") {
  DslValidator v;
  const std::string md = R"(```yaml
# --- BEGIN AgenticDSL ---
name: bad-type-demo
version: 0.1.0
agent_loop: react
nodes:
  - id: n1
    type: bogus_type    # ← 不在白名单
```
)";
  auto result = v.validate(md);
  REQUIRE_FALSE(result.valid);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "INVALID_NODE_TYPE" &&
        e.node_path == "node[0].type") {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("dsl_validator_yaml: call_tool to unregistered tool",
          "[dsl_validator][yaml][tools]") {
  DslValidator v;
  const std::string md = R"(```yaml
# --- BEGIN AgenticDSL ---
name: tool-dep-demo
version: 0.1.0
agent_loop: react
nodes:
  - id: n1
    type: call_tool
    tool_name: nonexistent_tool   # ← 未注册
```
)";
  MockToolRegistry registry({"some_other_tool"});
  auto result = v.validate(md, &registry);
  REQUIRE_FALSE(result.valid);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "MISSING_TOOL_DEPENDENCY" &&
        e.node_path == "node[0].tool_name" &&
        e.message.find("nonexistent_tool") != std::string::npos) {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("dsl_validator_yaml: invalid yaml syntax reports yaml_block[L:C]",
          "[dsl_validator][yaml][invalid]") {
  DslValidator v;
  std::string md = read_fixture("invalid_yaml.agent.md");
  auto result = v.validate(md);
  REQUIRE_FALSE(result.valid);
  bool found = false;
  for (const auto& e : result.errors) {
    if (e.type == "INVALID_YAML" &&
        e.node_path.find("yaml_block[") == 0 &&
        e.node_path.find(":") != std::string::npos) {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("dsl_validator_yaml: LF and CRLF produce equivalent results",
          "[dsl_validator][yaml][crlf]") {
  DslValidator v;
  std::string lf_md = read_fixture("react_golden.agent.md");
  std::string crlf_md = read_fixture("react_golden_crlf.agent.md");
  auto lf_result = v.validate(lf_md);
  auto crlf_result = v.validate(crlf_md);
  REQUIRE(lf_result.valid);
  REQUIRE(lf_result.errors.empty());
  REQUIRE(crlf_result.valid == lf_result.valid);
  REQUIRE(crlf_result.errors.size() == lf_result.errors.size());
  for (size_t i = 0; i < lf_result.errors.size(); ++i) {
    REQUIRE(crlf_result.errors[i].type == lf_result.errors[i].type);
    REQUIRE(crlf_result.errors[i].node_path == lf_result.errors[i].node_path);
  }
}

TEST_CASE("dsl_validator_yaml: validator is deterministic across repeated calls",
          "[dsl_validator][yaml][mock]") {
  DslValidator v;
  std::string md = read_fixture("react_golden.agent.md");
  auto r1 = v.validate(md);
  auto r2 = v.validate(md);
  REQUIRE(r1.valid == r2.valid);
  REQUIRE(r1.errors.size() == r2.errors.size());
  for (size_t i = 0; i < r1.errors.size(); ++i) {
    REQUIRE(r1.errors[i].type == r2.errors[i].type);
    REQUIRE(r1.errors[i].node_path == r2.errors[i].node_path);
  }
}