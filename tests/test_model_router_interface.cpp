// tests/test_model_router_interface.cpp
// 功能描述：IModelRouter 接口契约测试 (C7 Phase 1 MVP)。
//          6 个 TEST_CASE:
//            1. RoutingContext fields existence
//            2. ModelCapability struct fields
//            3. IModelRouter abstract class existence
//            4. ModelRoutingError NoViableModel throw/catch
//            5. ModelRoutingError ProviderUnavailable throw/catch
//            6. ModelRoutingError what() 含错误码前缀
// 设计依据：openspecs/model-router-plugin/spec.md
// 作者：C7 Phase 1 MVP
// 最后修改日期：2026-07-02

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/model_router.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <optional>

using namespace agenticdsl::pdk;

TEST_CASE("ModelCapability struct has required fields", "[model_router][interface]") {
    ModelCapability cap{"gpt-4", "GPT-4", 8192, 4096,
                       true, true, 0.03, 500,
                       {"general", "reasoning", "code"}};
    REQUIRE(cap.model_id == "gpt-4");
    REQUIRE(cap.model_name == "GPT-4");
    REQUIRE(cap.n_ctx == 8192);
    REQUIRE(cap.max_tokens == 4096);
    REQUIRE(cap.supports_streaming == true);
    REQUIRE(cap.supports_function_call == true);
    REQUIRE(cap.per_token_cost == 0.03);
    REQUIRE(cap.avg_latency_ms == 500);
    REQUIRE(cap.tags == std::vector<std::string>{"general", "reasoning", "code"});
}

TEST_CASE("RoutingContext struct has required fields", "[model_router][interface]") {
    RoutingContext ctx;
    ctx.task_type = "completion";
    ctx.session_id = "ses_001";
    ctx.max_tokens = 2048;
    ctx.budget_remaining = 0.05;
    ctx.required_tags = {"general"};
    ctx.preferred_model = "gpt-4";
    ctx.is_fleet_mode = false;

    REQUIRE(ctx.task_type == "completion");
    REQUIRE(ctx.session_id == "ses_001");
    REQUIRE(ctx.max_tokens.value() == 2048);
    REQUIRE(ctx.budget_remaining.value() == 0.05);
    REQUIRE(ctx.required_tags == std::vector<std::string>{"general"});
    REQUIRE(ctx.preferred_model == "gpt-4");
    REQUIRE(ctx.is_fleet_mode == false);
}

TEST_CASE("RoutingContext optional fields default empty", "[model_router][interface]") {
    RoutingContext ctx;
    ctx.task_type = "code_generation";
    REQUIRE_FALSE(ctx.max_tokens.has_value());
    REQUIRE_FALSE(ctx.budget_remaining.has_value());
    REQUIRE(ctx.required_tags.empty());
    REQUIRE(ctx.preferred_model.empty());
    REQUIRE_FALSE(ctx.is_fleet_mode);
}

TEST_CASE("ModelRoutingError NoViableModel can be thrown and caught", "[model_router][interface]") {
    try {
        throw ModelRoutingError(ModelRoutingError::Code::NoViableModel,
                                "no model within budget");
        REQUIRE(false); // should not reach
    } catch (const ModelRoutingError& e) {
        REQUIRE(e.code == ModelRoutingError::Code::NoViableModel);
        std::string what_str(e.what());
        REQUIRE(what_str.find("[NoViableModel]") != std::string::npos);
        REQUIRE(what_str.find("no model within budget") != std::string::npos);
    }
}

TEST_CASE("ModelRoutingError ProviderUnavailable can be thrown and caught", "[model_router][interface]") {
    try {
        throw ModelRoutingError(ModelRoutingError::Code::ProviderUnavailable,
                                "cloud provider not reachable");
        REQUIRE(false);
    } catch (const ModelRoutingError& e) {
        REQUIRE(e.code == ModelRoutingError::Code::ProviderUnavailable);
        std::string what_str(e.what());
        REQUIRE(what_str.find("[ProviderUnavailable]") != std::string::npos);
    }
}

TEST_CASE("ModelRoutingError AmbiguousCapability can be thrown and caught", "[model_router][interface]") {
    try {
        throw ModelRoutingError(ModelRoutingError::Code::AmbiguousCapability,
                                "multiple models tie for same tag match");
        REQUIRE(false);
    } catch (const ModelRoutingError& e) {
        REQUIRE(e.code == ModelRoutingError::Code::AmbiguousCapability);
        std::string what_str(e.what());
        REQUIRE(what_str.find("[AmbiguousCapability]") != std::string::npos);
    }
}