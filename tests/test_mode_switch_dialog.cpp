// tests/test_mode_switch_dialog.cpp
// 功能描述：ModeSwitchDialog 单元测试 (C3 Sprint 13)
// 设计依据：ADR-0031 §决策 6 (YOLO 切换需用户确认)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#include "catch_amalgamated.hpp"

#include "common/policy/mode_switch_dialog.h"

using namespace agenticdsl;

auto mock_true_input = [](const std::string&) { return true; };
auto mock_false_input = [](const std::string&) { return false; };

TEST_CASE("yolo_switch_requires_confirmation", "[policy][stage3]") {
  SECTION("Agent->Yolo confirm_yolo_switch returns false when user rejects") {
    REQUIRE_FALSE(confirm_yolo_switch("agent", mock_false_input));
  }

  SECTION("Agent->Yolo confirm_yolo_switch returns true when user accepts") {
    REQUIRE(confirm_yolo_switch("agent", mock_true_input));
  }

  SECTION("Plan->Yolo confirm_yolo_switch returns false when user rejects") {
    REQUIRE_FALSE(confirm_yolo_switch("plan", mock_false_input));
  }
}

TEST_CASE("plan_to_agent_silent", "[policy][stage3]") {
  SECTION("Plan->Agent switch does NOT require YOLO confirmation") {
    REQUIRE_FALSE(requires_yolo_confirmation("plan", "agent"));
  }

  SECTION("Agent->Plan switch does NOT require YOLO confirmation") {
    REQUIRE_FALSE(requires_yolo_confirmation("agent", "plan"));
  }

  SECTION("Plan->Yolo switch DOES require YOLO confirmation") {
    REQUIRE(requires_yolo_confirmation("plan", "yolo"));
  }

  SECTION("Yolo->Agent switch DOES require YOLO confirmation") {
    REQUIRE(requires_yolo_confirmation("yolo", "agent"));
  }

  SECTION("Agent->Yolo switch DOES require YOLO confirmation") {
    REQUIRE(requires_yolo_confirmation("agent", "yolo"));
  }

  SECTION("Yolo->Plan switch DOES require YOLO confirmation") {
    REQUIRE(requires_yolo_confirmation("yolo", "plan"));
  }
}