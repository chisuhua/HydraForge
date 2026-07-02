// tests/test_cost_router_plugin.cpp
// 功能描述：CostModelRouterPolicy 路由策略单元测试 (C7 Phase 1 MVP)。
//          4 个 TEST_CASE 覆盖:
//            1. cheapest-viable: 返回 per_token_cost 最低的 tag-matching 模型
//            2. budget-exceeded: 全模型超预算时 throw NoViableModel
//            3. capability-mismatch: 所有模型不满足 required_tags 时 throw NoViableModel
//            4. single-model: 仅 1 个候选模型时返回该模型
// 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/specs/model-router-plugin/spec.md
//           cost-strategy-end-to-end requirement (4 scenarios)
// 作者：C7 Phase 1 MVP
// 最后修改日期：2026-07-02

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/model_router.h"
#include "pdk/model_router/cost_strategy/cost_router.h"

#include <stdexcept>
#include <string>
#include <vector>

using agenticdsl::pdk::ModelCapability;
using agenticdsl::pdk::RoutingContext;
using agenticdsl::pdk::ModelRoutingError;
using agenticdsl::pdk::CostModelRouterPolicy;

namespace {

// 测试 fixture: 构造 3 个候选模型
std::vector<ModelCapability> make_test_candidates() {
  return {
    {"gpt-4", "GPT-4", 8192, 4096, true, true, 0.03, 500,
     {"general", "reasoning", "vision", "code"}},
    {"gpt-3.5-turbo", "GPT-3.5", 4096, 4096, true, false, 0.002, 200,
     {"general", "fast"}},
    {"claude-3", "Claude 3", 16384, 4096, true, true, 0.015, 350,
     {"general", "reasoning", "code"}}
  };
}

// 测试 fixture: 构造包含 vision tag 的单个模型
std::vector<ModelCapability> make_vision_only_candidate() {
  return {
    {"gemini-vision", "Gemini Vision", 4096, 2048, true, false, 0.05, 800,
     {"vision"}}
  };
}

// 测试 fixture: 构造全高成本模型
std::vector<ModelCapability> make_expensive_candidates() {
  return {
    {"gpt-4", "GPT-4", 8192, 4096, true, true, 0.03, 500,
     {"general"}},
    {"claude-3", "Claude 3", 16384, 4096, true, true, 0.015, 350,
     {"general"}}
  };
}

} // namespace

TEST_CASE("CostModelRouter returns cheapest viable model", "[model_router][cost][cheapest]") {
  CostModelRouterPolicy router;
  auto candidates = make_test_candidates();

  RoutingContext ctx;
  ctx.task_type = "completion";
  ctx.required_tags = {"general"};

  auto model_id = router.route(ctx, candidates);
  REQUIRE(model_id == "gpt-3.5-turbo");
  // 验证: gpt-3.5-turbo per_token_cost=0.002 是最低的 tag-matching 模型
}

TEST_CASE("CostModelRouter throws NoViableModel when all exceed budget", "[model_router][cost][budget]") {
  CostModelRouterPolicy router;
  auto candidates = make_expensive_candidates();

  RoutingContext ctx;
  ctx.task_type = "completion";
  ctx.required_tags = {"general"};
  ctx.budget_remaining = 0.001; // 低于所有模型成本 (最低 0.015)

  REQUIRE_THROWS_AS(router.route(ctx, candidates), ModelRoutingError);
  try {
    router.route(ctx, candidates);
    REQUIRE(false);
  } catch (const ModelRoutingError& e) {
    REQUIRE(e.code == ModelRoutingError::Code::NoViableModel);
    std::string what_str(e.what());
    REQUIRE(what_str.find("[NoViableModel]") != std::string::npos);
  }
}

TEST_CASE("CostModelRouter throws NoViableModel when no model matches required tags", "[model_router][cost][tag-mismatch]") {
  CostModelRouterPolicy router;
  auto candidates = make_test_candidates();

  RoutingContext ctx;
  ctx.task_type = "completion";
  ctx.required_tags = {"audio"}; // 没有任何模型有 audio tag (gpt-4/gpt-3.5/claude-3 都没有)

  REQUIRE_THROWS_AS(router.route(ctx, candidates), ModelRoutingError);
  try {
    router.route(ctx, candidates);
    REQUIRE(false);
  } catch (const ModelRoutingError& e) {
    REQUIRE(e.code == ModelRoutingError::Code::NoViableModel);
  }
}

TEST_CASE("CostModelRouter returns single model when only one candidate", "[model_router][cost][single]") {
  CostModelRouterPolicy router;
  auto candidates = make_vision_only_candidate();

  RoutingContext ctx;
  ctx.task_type = "completion";
  ctx.required_tags = {"vision"};

  auto model_id = router.route(ctx, candidates);
  REQUIRE(model_id == "gemini-vision");
}
