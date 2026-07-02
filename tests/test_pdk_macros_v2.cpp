#include "catch_amalgamated.hpp"
#include "agenticdsl/pdk/tool_macros.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Anonymous namespace to avoid symbol conflicts across translation units
namespace {
    DECLARE_TOOL(test_tool, "A test tool", ReadOnly, "agent",
        return json{{"result", "success"}};
    )
    
    DECLARE_TOOL(always_tool, "Always approve tool", ReadOnly, "always",
        return json{{"result", "success"}};
    )
    
    DECLARE_TOOL(yolo_tool, "Yolo tool", ReadOnly, "yolo",
        return json{{"result", "success"}};
    )
    
    DECLARE_TOOL(plan_tool, "Plan tool", ReadOnly, "plan",
        return json{{"result", "success"}};
    )
    
    DECLARE_TOOL(echo_tool, "Echo tool", ReadOnly, "agent",
        return json{{"echo", __pdk_args.at("message")}};
    )
} // anonymous namespace

TEST_CASE("declare_tool_v2_expands_with_meta", "[pdk][macros][v2]") {
    const auto& spec = tool_spec_test_tool;
    REQUIRE(spec.name == "test_tool");
    REQUIRE(spec.description == "A test tool");
    REQUIRE(spec.metadata.category == agenticdsl::ToolCategory::ReadOnly);
    REQUIRE(spec.metadata.description == "A test tool");
    REQUIRE(spec.metadata.name == "test_tool");
    REQUIRE(spec.metadata.domain == "plugin");  // default from macro
}

TEST_CASE("declare_tool_v2_policy_always", "[pdk][macros][v2]") {
    const auto& spec = tool_spec_always_tool;
    REQUIRE(spec.metadata.approval.requires_approval_in_plan == true);
    REQUIRE(spec.metadata.approval.requires_approval_in_agent == true);
    REQUIRE(spec.metadata.approval.requires_approval_in_yolo == true);
    REQUIRE(spec.metadata.approval.force_approval_always == true);
}

TEST_CASE("declare_tool_v2_policy_yolo", "[pdk][macros][v2]") {
    const auto& spec = tool_spec_yolo_tool;
    REQUIRE(spec.metadata.approval.requires_approval_in_plan == false);
    REQUIRE(spec.metadata.approval.requires_approval_in_agent == false);
    REQUIRE(spec.metadata.approval.requires_approval_in_yolo == true);
    REQUIRE(spec.metadata.approval.force_approval_always == false);
}

TEST_CASE("declare_tool_v2_policy_plan", "[pdk][macros][v2]") {
    const auto& spec = tool_spec_plan_tool;
    REQUIRE(spec.metadata.approval.requires_approval_in_plan == true);
    REQUIRE(spec.metadata.approval.requires_approval_in_agent == false);
    REQUIRE(spec.metadata.approval.requires_approval_in_yolo == false);
    REQUIRE(spec.metadata.approval.force_approval_always == false);
}

TEST_CASE("declare_tool_v2_handler_executes", "[pdk][macros][v2]") {
    // Call the generated handler
    json args = {{"message", "hello world"}};
    json result = tool_handler_echo_tool(args);
    
    REQUIRE(result.contains("echo"));
    REQUIRE(result["echo"] == "hello world");
}