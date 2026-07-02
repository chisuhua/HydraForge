// tests/test_layer_profile_matrix.cpp
// C6 Sprint 16: Layer × ToolCategory 权限矩阵注册时检查

#include "catch_amalgamated.hpp"

#include "common/policy/execution_policy.h"
#include "common/policy/layer_profile.h"

using namespace agenticdsl;

namespace {

ToolMetadata make_meta(ToolCategory category, std::vector<LayerProfile> allowed) {
    ToolMetadata m;
    m.name = "test_tool";
    m.description = "test";
    m.domain = "test";
    m.category = category;
    m.min_layer = LayerProfile::Workflow;
    m.approval = ApprovalPolicy{true, true, false, false};
    m.allowed_layers = std::move(allowed);
    return m;
}

} // namespace

TEST_CASE("Layer × Category matrix: Cognitive allows ReadOnly", "[layer_profile][matrix][c6]") {
    ToolMetadata meta = make_meta(ToolCategory::ReadOnly, {LayerProfile::Cognitive});
    REQUIRE_NOTHROW(check_registration_permission(meta));
}

TEST_CASE("Layer × Category matrix: Cognitive rejects WriteFile", "[layer_profile][matrix][c6]") {
    ToolMetadata meta = make_meta(ToolCategory::WriteFile, {LayerProfile::Cognitive});
    REQUIRE_THROWS_AS(check_registration_permission(meta), std::invalid_argument);
}

TEST_CASE("Layer × Category matrix: Cognitive rejects Network", "[layer_profile][matrix][c6]") {
    ToolMetadata meta = make_meta(ToolCategory::Network, {LayerProfile::Cognitive});
    REQUIRE_THROWS_AS(check_registration_permission(meta), std::invalid_argument);
}

TEST_CASE("Layer × Category matrix: Thinking allows ReadOnly + WriteFile", "[layer_profile][matrix][c6]") {
    ToolMetadata meta_ro = make_meta(ToolCategory::ReadOnly, {LayerProfile::Thinking});
    ToolMetadata meta_wf = make_meta(ToolCategory::WriteFile, {LayerProfile::Thinking});
    REQUIRE_NOTHROW(check_registration_permission(meta_ro));
    REQUIRE_NOTHROW(check_registration_permission(meta_wf));
}

TEST_CASE("Layer × Category matrix: Thinking rejects Execute", "[layer_profile][matrix][c6]") {
    ToolMetadata meta = make_meta(ToolCategory::Execute, {LayerProfile::Thinking});
    REQUIRE_THROWS_AS(check_registration_permission(meta), std::invalid_argument);
}

TEST_CASE("Layer × Category matrix: Workflow allows all categories", "[layer_profile][matrix][c6]") {
    for (auto cat : {ToolCategory::ReadOnly, ToolCategory::WriteFile,
                     ToolCategory::Execute, ToolCategory::Network,
                     ToolCategory::StateModify}) {
        ToolMetadata meta = make_meta(cat, {LayerProfile::Workflow});
        REQUIRE_NOTHROW(check_registration_permission(meta));
    }
}

TEST_CASE("Layer × Category matrix: empty allowed_layers skips check", "[layer_profile][matrix][c6]") {
    ToolMetadata meta = make_meta(ToolCategory::Network, {});
    REQUIRE_NOTHROW(check_registration_permission(meta));
}

TEST_CASE("Layer × Category matrix: mixed allowed layers any-incompatible rejects", "[layer_profile][matrix][c6]") {
    ToolMetadata meta = make_meta(ToolCategory::WriteFile,
        {LayerProfile::Workflow, LayerProfile::Cognitive});
    REQUIRE_THROWS_AS(check_registration_permission(meta), std::invalid_argument);
}