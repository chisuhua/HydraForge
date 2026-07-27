// test_dsl_validation.cpp - T2: DSL Schema 校验测试
// 关联: examples/pdk_chat_demo/dsl_validator.h
//       openspec/changes/pdk-chat-demo-v1-recap/design.md §T2
// 作者: Sisyphus (OhMyOpenCode), 2026-07-27

#include "catch_amalgamated.hpp"

#include "dsl_validator.h"

using namespace pdk_chat_demo;

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
