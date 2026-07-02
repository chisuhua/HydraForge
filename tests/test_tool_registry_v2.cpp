#include "catch_amalgamated.hpp"
#include "common/tools/registry.h"
#include "common/policy/execution_policy.h"

using namespace agenticdsl;

TEST_CASE("register_with_meta", "[tool_registry][v2]") {
    ToolRegistry registry;
    
    ToolMetadata meta{
        "test_tool", 
        "Test tool description", 
        "test", 
        ToolCategory::ReadOnly, 
        LayerProfile::Thinking, 
        ApprovalPolicy{true, false, false, true}  // plan=true, agent=false, yolo=false, force=true
    };
    
    registry.register_tool_function("test_tool", meta, 
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
            return {{"result", "success"}};
        });
    
    auto metadata_list = registry.list_metadata();
    REQUIRE(metadata_list.size() == 4);
    
    bool found = false;
    for (const auto& [name, meta] : metadata_list) {
        if (name == "test_tool") {
            found = true;
            REQUIRE(meta.description == "Test tool description");
            REQUIRE(meta.domain == "test");
            REQUIRE(meta.category == ToolCategory::ReadOnly);
            REQUIRE(meta.min_layer == LayerProfile::Thinking);
            REQUIRE(meta.approval.requires_approval_in_plan == true);
            REQUIRE(meta.approval.requires_approval_in_agent == false);
            REQUIRE(meta.approval.requires_approval_in_yolo == false);
            REQUIRE(meta.approval.force_approval_always == true);
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("register_duplicate_name_throws", "[tool_registry][v2]") {
    ToolRegistry registry;
    
    ToolMetadata meta1{
        "test_tool", 
        "First tool", 
        "test", 
        ToolCategory::ReadOnly, 
        LayerProfile::Workflow, 
        ApprovalPolicy{true, true, false, false}
    };
    
    registry.register_tool_function("test_tool", meta1, 
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
            return {{"result", "first"}};
        });
    
    ToolMetadata meta2{
        "test_tool", 
        "Second tool", 
        "test", 
        ToolCategory::ReadOnly, 
        LayerProfile::Workflow, 
        ApprovalPolicy{true, true, false, false}
    };
    
    REQUIRE_THROWS_AS(
        registry.register_tool_function("test_tool", meta2, 
            [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
                return {{"result", "second"}};
            }),
        std::invalid_argument);
}

TEST_CASE("register_dangerous_no_approval_throws", "[tool_registry][v2]") {
    ToolRegistry registry;
    
    // Execute category with no plan or agent approval (yolo=false too)
    ToolMetadata meta{
        "dangerous_tool", 
        "Dangerous execute tool", 
        "test", 
        ToolCategory::Execute, 
        LayerProfile::Workflow, 
        ApprovalPolicy{false, false, false, false}  // no plan, no agent, no yolo, no force
    };
    
    REQUIRE_THROWS_AS(
        registry.register_tool_function("dangerous_tool", meta, 
            [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
                return {{"result", "success"}};
            }),
        std::invalid_argument);
}

TEST_CASE("register_min_layer_not_in_allowed_throws", "[tool_registry][v2]") {
    ToolRegistry registry;
    
    // min_layer=Workflow but allowed_layers only has Cognitive and Thinking
    ToolMetadata meta{
        "test_tool", 
        "Test tool", 
        "test", 
        ToolCategory::ReadOnly, 
        LayerProfile::Workflow,  // min_layer
        ApprovalPolicy{true, true, false, false},
        std::vector<LayerProfile>{LayerProfile::Cognitive, LayerProfile::Thinking},  // allowed_layers
        0.0,
        30000
    };
    
    REQUIRE_THROWS_AS(
        registry.register_tool_function("test_tool", meta, 
            [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
                return {{"result", "success"}};
            }),
        std::invalid_argument);
}

TEST_CASE("register_empty_allowed_layers_ok", "[tool_registry][v2]") {
    ToolRegistry registry;
    
    // Empty allowed_layers should be ok for any category
    ToolMetadata meta{
        "test_tool", 
        "Test tool", 
        "test", 
        ToolCategory::Execute, 
        LayerProfile::Cognitive, 
        ApprovalPolicy{true, true, false, false},
        std::vector<LayerProfile>{},  // empty allowed_layers
        0.0,
        30000
    };
    
    // Should not throw
    registry.register_tool_function("test_tool", meta, 
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
            return {{"result", "success"}};
        });
    
    auto metadata_list = registry.list_metadata();
    REQUIRE(metadata_list.size() == 4);
}

TEST_CASE("register_default_tools_have_metadata", "[tool_registry][v2]") {
    ToolRegistry registry;
    
    auto metadata_list = registry.list_metadata();
    
    REQUIRE(metadata_list.size() >= 3);
    
    bool found_web_search = false;
    bool found_get_weather = false;
    bool found_calculate = false;
    
    for (const auto& [name, meta] : metadata_list) {
        if (name == "web_search") {
            found_web_search = true;
            REQUIRE(!meta.name.empty());
            REQUIRE(!meta.description.empty());
            REQUIRE(!meta.domain.empty());
        } else if (name == "get_weather") {
            found_get_weather = true;
            REQUIRE(!meta.name.empty());
            REQUIRE(!meta.description.empty());
            REQUIRE(!meta.domain.empty());
        } else if (name == "calculate") {
            found_calculate = true;
            REQUIRE(!meta.name.empty());
            REQUIRE(!meta.description.empty());
            REQUIRE(!meta.domain.empty());
        }
    }
    
    REQUIRE(found_web_search);
    REQUIRE(found_get_weather);
    REQUIRE(found_calculate);
}

TEST_CASE("list_metadata_returns_all", "[tool_registry][v2]") {
    ToolRegistry registry;
    
    // Register 3 new tools
    ToolMetadata meta1{"tool1", "Tool 1", "test", ToolCategory::ReadOnly, LayerProfile::Workflow, ApprovalPolicy{true, true, false, false}};
    ToolMetadata meta2{"tool2", "Tool 2", "test", ToolCategory::WriteFile, LayerProfile::Workflow, ApprovalPolicy{true, true, false, false}};
    ToolMetadata meta3{"tool3", "Tool 3", "test", ToolCategory::Execute, LayerProfile::Workflow, ApprovalPolicy{true, true, false, false}};
    
    registry.register_tool_function("tool1", meta1, 
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json { return {{"result", "1"}}; });
    registry.register_tool_function("tool2", meta2, 
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json { return {{"result", "2"}}; });
    registry.register_tool_function("tool3", meta3, 
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json { return {{"result", "3"}}; });
    
    auto metadata_list = registry.list_metadata();
    REQUIRE(metadata_list.size() == 6);
    
    // Check that our tools are in the list
    bool found_tool1 = false;
    bool found_tool2 = false;
    bool found_tool3 = false;
    
    for (const auto& [name, meta] : metadata_list) {
        if (name == "tool1") {
            found_tool1 = true;
            REQUIRE(meta.description == "Tool 1");
        } else if (name == "tool2") {
            found_tool2 = true;
            REQUIRE(meta.description == "Tool 2");
        } else if (name == "tool3") {
            found_tool3 = true;
            REQUIRE(meta.description == "Tool 3");
        }
    }
    
    REQUIRE(found_tool1);
    REQUIRE(found_tool2);
    REQUIRE(found_tool3);
}

TEST_CASE("register_network_no_plan_approval_throws", "[tool_registry][v2]") {
    ToolRegistry registry;
    
    // Network category with no plan or agent approval
    ToolMetadata meta{
        "network_tool", 
        "Network tool", 
        "test", 
        ToolCategory::Network, 
        LayerProfile::Workflow, 
        ApprovalPolicy{false, false, false, false}  // no plan, no agent, no yolo, no force
    };
    
    REQUIRE_THROWS_AS(
        registry.register_tool_function("network_tool", meta, 
            [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
                return {{"result", "success"}};
            }),
        std::invalid_argument);
}