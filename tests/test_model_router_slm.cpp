// tests/test_model_router_slm.cpp
// 功能描述：SLMModelRouterPolicy 单元测试 (T16, ADR-0061-04 SLM 路由优先)
//          ≥ 5 cases: SLM 桶优先 / fallback / required_tags / budget / 空 candidates
// 设计依据：openspec/changes/2026-08-24-adr-0061-04-slm-routing/
//          ADR-0061-04 SLM 路由优先 (NVIDIA Position Paper 2025)
//          capability-application-map-2026-08.md §八 T16
// 作者：HydraForge Sprint 23 T16 ship (Phase C opportunist)
// 最后修改日期：2026-08-24

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/model_router.h"
#include "pdk/model_router/slm_strategy/slm_router.h"

#include <string>
#include <vector>

using agenticdsl::pdk::ModelCapability;
using agenticdsl::pdk::ModelRoutingError;
using agenticdsl::pdk::RoutingContext;
using agenticdsl::pdk::SLMModelRouterPolicy;

namespace {

ModelCapability make_cap(const std::string& id, double cost,
                         std::vector<std::string> tags) {
  ModelCapability cap;
  cap.model_id = id;
  cap.model_name = id;
  cap.n_ctx = 4096;
  cap.max_tokens = 4096;
  cap.per_token_cost = cost;
  cap.avg_latency_ms = 200;
  cap.tags = std::move(tags);
  return cap;
}

}  // namespace

// ============================================================================
// Test 1: SLM 桶优先 — 带 fast tag 的模型被选中
// ============================================================================
TEST_CASE("SLMRouter prefers fast-tag model: gpt-3.5-turbo over gpt-4",
          "[model_router][slm][t16]") {
  SLMModelRouterPolicy router;
  RoutingContext ctx;
  ctx.required_tags = {"general"};

  auto candidates = std::vector<ModelCapability>{
      make_cap("gpt-4", 0.03, {"general", "reasoning"}),
      make_cap("gpt-3.5-turbo", 0.002, {"general", "fast"}),
  };

  REQUIRE(router.route(ctx, candidates) == "gpt-3.5-turbo");
}

// ============================================================================
// Test 2: SLM 桶内多候选按 per_token_cost 排序
// ============================================================================
TEST_CASE("SLMRouter sorts SLM bucket by cost ascending",
          "[model_router][slm][t16]") {
  SLMModelRouterPolicy router;
  RoutingContext ctx;
  ctx.required_tags = {"fast"};

  auto candidates = std::vector<ModelCapability>{
      make_cap("model-A", 0.005, {"fast"}),
      make_cap("model-B", 0.001, {"fast"}),
      make_cap("model-C", 0.003, {"fast"}),
  };

  // 桶内 cost 最低 → model-B
  REQUIRE(router.route(ctx, candidates) == "model-B");
}

// ============================================================================
// Test 3: 无 SLM 候选时 fallback 到非 SLM 桶
// ============================================================================
TEST_CASE("SLMRouter falls back to non-SLM bucket when no SLM candidate",
          "[model_router][slm][t16]") {
  SLMModelRouterPolicy router;
  RoutingContext ctx;
  ctx.required_tags = {"general"};

  auto candidates = std::vector<ModelCapability>{
      make_cap("gpt-4", 0.03, {"general", "reasoning"}),
      make_cap("claude-3-opus", 0.015, {"general", "reasoning", "code"}),
  };

  // 无 fast/slm tag → fallback → 非 SLM 桶最便宜 = gpt-4 (0.03) vs claude (0.015)
  // 等等: claude (0.015) < gpt-4 (0.03), 所以应返回 claude-3-opus
  REQUIRE(router.route(ctx, candidates) == "claude-3-opus");
}

// ============================================================================
// Test 4: required_tags 过滤 + SLM 桶交集
// ============================================================================
TEST_CASE("SLMRouter required_tags filter excludes non-matching SLM",
          "[model_router][slm][t16]") {
  SLMModelRouterPolicy router;
  RoutingContext ctx;
  ctx.required_tags = {"reasoning"};  // 只匹配 reasoning tag

  auto candidates = std::vector<ModelCapability>{
      make_cap("gpt-3.5-turbo", 0.002, {"general", "fast"}),     // 无 reasoning → 排除
      make_cap("gpt-4", 0.03, {"general", "reasoning"}),          // 匹配 → SLM? 无 fast/slm tag
      make_cap("claude-3-fast", 0.01, {"reasoning", "fast"}),    // 匹配 + SLM → 候选
  };

  // SLM 桶 = [claude-3-fast], 非 SLM 桶 = [gpt-4]
  // SLM 桶优先 → claude-3-fast
  REQUIRE(router.route(ctx, candidates) == "claude-3-fast");
}

// ============================================================================
// Test 5: budget_remaining 过滤
// ============================================================================
TEST_CASE("SLMRouter budget_remaining filter excludes expensive models",
          "[model_router][slm][t16]") {
  SLMModelRouterPolicy router;
  RoutingContext ctx;
  ctx.required_tags = {"general"};
  ctx.budget_remaining = 0.001;  // 仅允许 ≤ 0.001

  auto candidates = std::vector<ModelCapability>{
      make_cap("gpt-3.5-turbo", 0.002, {"general", "fast"}),  // 超预算
      make_cap("gpt-4", 0.03, {"general", "reasoning"}),       // 超预算
      make_cap("tiny-slm", 0.0005, {"general", "fast"}),      // 在预算 → SLM 候选
  };

  REQUIRE(router.route(ctx, candidates) == "tiny-slm");
}

// ============================================================================
// Test 6: budget 过紧导致无可行候选 → 抛异常
// ============================================================================
TEST_CASE("SLMRouter no viable model: empty after budget filter throws",
          "[model_router][slm][t16]") {
  SLMModelRouterPolicy router;
  RoutingContext ctx;
  ctx.required_tags = {"general"};
  ctx.budget_remaining = 0.0001;  // 极紧

  auto candidates = std::vector<ModelCapability>{
      make_cap("gpt-3.5-turbo", 0.002, {"general", "fast"}),
      make_cap("gpt-4", 0.03, {"general", "reasoning"}),
  };

  REQUIRE_THROWS_AS(router.route(ctx, candidates), ModelRoutingError);
}

// ============================================================================
// Test 7: 空 candidates 抛 NoViableModel
// ============================================================================
TEST_CASE("SLMRouter empty candidates throws NoViableModel",
          "[model_router][slm][t16]") {
  SLMModelRouterPolicy router;
  RoutingContext ctx;
  std::vector<ModelCapability> empty;

  REQUIRE_THROWS_AS(router.route(ctx, empty), ModelRoutingError);
}

// ============================================================================
// Test 8: name() 返回 "slm"
// ============================================================================
TEST_CASE("SLMRouter name returns slm", "[model_router][slm][t16]") {
  SLMModelRouterPolicy router;
  REQUIRE(router.name() == "slm");
}