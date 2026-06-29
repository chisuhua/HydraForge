// tests/test_layer_profile.cpp
// 功能描述：LayerProfile 辅助函数单元测试 (C4 Sprint 14)
// 设计依据：ADR-0031 §决策 4 + ADR-0004 §8 (Layer x Category 矩阵)
// 作者：AgenticDSL Phase3 / Sprint 14 C4 ship
// 最后修改日期：2026-06-29
#include "catch_amalgamated.hpp"

#include <stdexcept>
#include <string>

#include "common/policy/layer_profile.h"

using namespace agenticdsl;

TEST_CASE("parse_layer_case_insensitive", "[layer_profile][stage3]") {
  // Lower, upper, mixed case all accepted
  REQUIRE(parse_layer("workflow") == LayerProfile::Workflow);
  REQUIRE(parse_layer("Workflow") == LayerProfile::Workflow);
  REQUIRE(parse_layer("WORKFLOW") == LayerProfile::Workflow);
  REQUIRE(parse_layer("WoRkFlOw") == LayerProfile::Workflow);

  REQUIRE(parse_layer("thinking") == LayerProfile::Thinking);
  REQUIRE(parse_layer("Thinking") == LayerProfile::Thinking);
  REQUIRE(parse_layer("THINKING") == LayerProfile::Thinking);

  REQUIRE(parse_layer("cognitive") == LayerProfile::Cognitive);
  REQUIRE(parse_layer("Cognitive") == LayerProfile::Cognitive);
  REQUIRE(parse_layer("COGNITIVE") == LayerProfile::Cognitive);
}

TEST_CASE("parse_layer_unknown_throws", "[layer_profile][stage3]") {
  REQUIRE_THROWS_AS(parse_layer("invalid"), std::invalid_argument);
  REQUIRE_THROWS_AS(parse_layer(""), std::invalid_argument);
  REQUIRE_THROWS_AS(parse_layer("workflow_typo"), std::invalid_argument);
  // Exception message should contain the invalid input for debugging
  try {
    parse_layer("bogus");
    FAIL("expected throw");
  } catch (const std::invalid_argument& e) {
    std::string msg = e.what();
    REQUIRE(msg.find("bogus") != std::string::npos);
  }
}

TEST_CASE("check_layer_permission_workflow_allows_all", "[layer_profile][stage3]") {
  // Workflow (L2) can call all categories
  REQUIRE(check_layer_permission(LayerProfile::Workflow, ToolCategory::ReadOnly));
  REQUIRE(check_layer_permission(LayerProfile::Workflow, ToolCategory::WriteFile));
  REQUIRE(check_layer_permission(LayerProfile::Workflow, ToolCategory::Execute));
  REQUIRE(check_layer_permission(LayerProfile::Workflow, ToolCategory::Network));
  REQUIRE(check_layer_permission(LayerProfile::Workflow, ToolCategory::StateModify));
}

TEST_CASE("check_layer_permission_thinking_readonly_only", "[layer_profile][stage3]") {
  // Thinking (L3) only ReadOnly allowed
  REQUIRE(check_layer_permission(LayerProfile::Thinking, ToolCategory::ReadOnly));
  REQUIRE_FALSE(check_layer_permission(LayerProfile::Thinking, ToolCategory::WriteFile));
  REQUIRE_FALSE(check_layer_permission(LayerProfile::Thinking, ToolCategory::Execute));
  REQUIRE_FALSE(check_layer_permission(LayerProfile::Thinking, ToolCategory::Network));
  REQUIRE_FALSE(check_layer_permission(LayerProfile::Thinking, ToolCategory::StateModify));
}

TEST_CASE("check_layer_permission_cognitive_denies_all", "[layer_profile][stage3]") {
  // Cognitive (L4) no tools allowed
  REQUIRE_FALSE(check_layer_permission(LayerProfile::Cognitive, ToolCategory::ReadOnly));
  REQUIRE_FALSE(check_layer_permission(LayerProfile::Cognitive, ToolCategory::WriteFile));
  REQUIRE_FALSE(check_layer_permission(LayerProfile::Cognitive, ToolCategory::Execute));
  REQUIRE_FALSE(check_layer_permission(LayerProfile::Cognitive, ToolCategory::Network));
  REQUIRE_FALSE(check_layer_permission(LayerProfile::Cognitive, ToolCategory::StateModify));
}

TEST_CASE("layer_profile_to_string_roundtrip", "[layer_profile][stage3]") {
  // LayerProfile -> string -> parse_layer roundtrip
  REQUIRE(parse_layer(to_string(LayerProfile::Workflow)) == LayerProfile::Workflow);
  REQUIRE(parse_layer(to_string(LayerProfile::Thinking)) == LayerProfile::Thinking);
  REQUIRE(parse_layer(to_string(LayerProfile::Cognitive)) == LayerProfile::Cognitive);

  // ToolCategory strings are direct (PascalCase)
  REQUIRE(to_string(ToolCategory::ReadOnly) == "ReadOnly");
  REQUIRE(to_string(ToolCategory::WriteFile) == "WriteFile");
  REQUIRE(to_string(ToolCategory::Execute) == "Execute");
  REQUIRE(to_string(ToolCategory::Network) == "Network");
  REQUIRE(to_string(ToolCategory::StateModify) == "StateModify");
}