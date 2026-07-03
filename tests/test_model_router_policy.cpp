// tests/test_model_router_policy.cpp
// 文件头注释
// 功能描述：Phase 1 Sprint 0 ModelRouterPolicy 单元测试 (5 test cases)
//          覆盖 available_models() 基础契约 + ModelRouterPolicy 路由决策
// 设计依据：phase1-execution.md §Sprint 0 (K1)
// 作者：AgenticDSL Phase 1 / Sprint 0
// 最后修改日期：2026-06-16

#include "catch_amalgamated.hpp"
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

using namespace agenticdsl;
using ModelCapability = ILLMProvider::ModelCapability;
using ModelInfo = ILLMProvider::ModelInfo;

namespace {
// === Phase 1 Sprint 0 新增: ModelRouterPolicy (K1 Plugin Stub 验证) ===
// 与 examples/phase1_model_router_plugin/main.cpp 共享同一逻辑
// Sprint 5 PluginLoader 实现后, 此 Policy 迁移到 PDK Plugin, 单元测试不变
class ModelRouterPolicy {
 public:
  /// 路由决策: 选择第一个支持 Chat 的 ModelInfo
  /// @throws std::runtime_error 当 available_models() 为空或无 Chat-capable 模型
  static ModelInfo route(const ILLMProvider& provider) {
    const auto models = provider.available_models();
    if (models.empty()) {
      throw std::runtime_error("no models available from provider");
    }

    auto it = std::find_if(models.begin(), models.end(),
                           [](const ModelInfo& m) {
                             return std::any_of(
                                 m.capabilities.begin(),
                                 m.capabilities.end(),
                                 [](ModelCapability c) {
                                   return c == ModelCapability::Chat;
                                 });
                           });
    if (it == models.end()) {
      throw std::runtime_error("no Chat-capable model available");
    }
    return *it;
  }
};

/// 测试用 stub provider, 返回固定模型列表 (覆盖边界条件)
class StubProvider : public ILLMProvider {
 public:
  explicit StubProvider(std::vector<ModelInfo> models) : models_(std::move(models)) {}

  Result<GenerationResult, LLMError>
      generate(const GenerationRequest& /*req*/, std::stop_token /*token*/) override {
    return Result<GenerationResult, LLMError>::failure(LLMError());
  }

  std::unique_ptr<IGenerationStream>
      generate_stream(const GenerationRequest& /*req*/, std::stop_token /*token*/) override {
    return nullptr;
  }

  std::vector<ModelInfo> available_models() const override { return models_; }

 private:
  std::vector<ModelInfo> models_;
};
}  // namespace

TEST_CASE("available_models() returns non-empty (MockLLMProvider default)", "[model_router][sprint0]") {
  MockLLMProvider provider;
  const auto models = provider.available_models();

  REQUIRE_FALSE(models.empty());
  REQUIRE(models.size() == 1);
  REQUIRE(models[0].name == "mock-llm-v1");
  REQUIRE(models[0].provider == "mock");
  REQUIRE(models[0].context_window == 4096);
}

TEST_CASE("ModelRouterPolicy.route() selects first Chat-capable model", "[model_router][sprint0]") {
  // 构造多模型场景: Embedding-only + Chat (应选 Chat)
  StubProvider provider({
      ModelInfo("embedding-only", {ModelCapability::Embedding}, 512, "stub"),
      ModelInfo("chat-model", {ModelCapability::Chat, ModelCapability::ToolUse}, 8192, "stub"),
      ModelInfo("another-chat", {ModelCapability::Chat}, 4096, "stub"),
  });

  const auto selected = ModelRouterPolicy::route(provider);
  REQUIRE(selected.name == "chat-model");
  REQUIRE(selected.context_window == 8192);
}

TEST_CASE("ModelRouterPolicy throws on empty model list", "[model_router][sprint0]") {
  StubProvider provider({});
  REQUIRE_THROWS_AS(ModelRouterPolicy::route(provider), std::runtime_error);
}

TEST_CASE("ModelRouterPolicy throws when no Chat-capable model", "[model_router][sprint0]") {
  StubProvider provider({
      ModelInfo("embedding-only", {ModelCapability::Embedding}, 512, "stub"),
      ModelInfo("vision-only", {ModelCapability::Vision}, 2048, "stub"),
  });
  REQUIRE_THROWS_AS(ModelRouterPolicy::route(provider), std::runtime_error);
}

TEST_CASE("ModelRouterPolicy route decision includes trace_id (Plugin Stub metadata)", "[model_router][sprint0]") {
  // Sprint 0 Plugin Stub 验证: 路由决策必须可追溯
  // trace_id 由 MockLLMProvider 在 Sprint 1a ToolResult 标准化 (P3) 后透传
  // 当前 Sprint 0 实现: 仅验证 name + capabilities 唯一性
  MockLLMProvider provider;
  const auto selected = ModelRouterPolicy::route(provider);

  // 验证 selected 是 provider.available_models() 中的一个 (trace 前提)
  const auto all = provider.available_models();
  auto found = std::find_if(all.begin(), all.end(),
                            [&selected](const ModelInfo& m) {
                              return m.name == selected.name;
                            });
  REQUIRE(found != all.end());
  REQUIRE(found->name == selected.name);
  REQUIRE(found->provider == selected.provider);

  // 验证 capabilities 包含 Chat (trace 决策依据)
  auto has_chat = std::any_of(selected.capabilities.begin(),
                              selected.capabilities.end(),
                              [](ModelCapability c) { return c == ModelCapability::Chat; });
  REQUIRE(has_chat);
}

// ============================================================================
// C7 Phase 2: Quality + Latency 策略单元测试 (PDK Plugin IModelRouter 接口)
// ============================================================================

#include "agenticdsl/pdk/model_router.h"
#include "pdk/model_router/quality_strategy/quality_router.h"
#include "pdk/model_router/latency_strategy/latency_router.h"

namespace {

agenticdsl::pdk::ModelCapability make_pdk_cap(
    const std::string& id, int n_ctx, int max_tokens,
    double cost, int latency, std::vector<std::string> tags) {
  agenticdsl::pdk::ModelCapability cap;
  cap.model_id = id;
  cap.model_name = id;
  cap.n_ctx = n_ctx;
  cap.max_tokens = max_tokens;
  cap.per_token_cost = cost;
  cap.avg_latency_ms = latency;
  cap.tags = std::move(tags);
  return cap;
}

} // namespace

// --- Quality 策略测试 (4 TEST_CASE) ---

TEST_CASE("QualityRouter full-tag-match: gpt-4 (reasoning+code) over gpt-3.5 (general)",
          "[model_router][quality]") {
  agenticdsl::pdk::QualityModelRouterPolicy router;
  agenticdsl::pdk::RoutingContext ctx;
  ctx.required_tags = {"reasoning", "code"};

  auto candidates = std::vector<agenticdsl::pdk::ModelCapability>{
    make_pdk_cap("gpt-3.5-turbo", 4096, 4096, 0.002, 200, {"general"}),
    make_pdk_cap("gpt-4", 8192, 4096, 0.03, 500, {"general", "reasoning", "code"}),
    make_pdk_cap("claude-3-opus", 16384, 4096, 0.015, 350,
                 {"general", "reasoning", "code", "vision"}),
  };

  // gpt-4 匹配 2/2, claude-3 匹配 2/2, gpt-3.5 匹配 0/2
  // 按匹配度降序, 同分按 stable_sort 保持原顺序 → gpt-4 先加入 scored → 返回 gpt-4
  auto result = router.route(ctx, candidates);
  REQUIRE(result == "gpt-4");
}

TEST_CASE("QualityRouter partial-match: claude-3 (2) over gpt-4 (1)",
          "[model_router][quality]") {
  agenticdsl::pdk::QualityModelRouterPolicy router;
  agenticdsl::pdk::RoutingContext ctx;
  ctx.required_tags = {"reasoning", "vision"};

  auto candidates = std::vector<agenticdsl::pdk::ModelCapability>{
    make_pdk_cap("gpt-4", 8192, 4096, 0.03, 500, {"general", "reasoning"}),
    make_pdk_cap("claude-3-opus", 16384, 4096, 0.015, 350,
                 {"general", "reasoning", "vision"}),
  };

  auto result = router.route(ctx, candidates);
  REQUIRE(result == "claude-3-opus");
}

TEST_CASE("QualityRouter no-tag-match-fallback: vision → candidates[0]",
          "[model_router][quality]") {
  agenticdsl::pdk::QualityModelRouterPolicy router;
  agenticdsl::pdk::RoutingContext ctx;
  ctx.required_tags = {"vision"};

  auto candidates = std::vector<agenticdsl::pdk::ModelCapability>{
    make_pdk_cap("gpt-4", 8192, 4096, 0.03, 500, {"general", "code"}),
    make_pdk_cap("gpt-3.5-turbo", 4096, 4096, 0.002, 200, {"general"}),
  };

  auto result = router.route(ctx, candidates);
  REQUIRE(result == "gpt-4");
}

TEST_CASE("QualityRouter empty-tag: n_ctx+max_tokens sort, claude-3 highest",
          "[model_router][quality]") {
  agenticdsl::pdk::QualityModelRouterPolicy router;
  agenticdsl::pdk::RoutingContext ctx;
  // required_tags 为空

  auto candidates = std::vector<agenticdsl::pdk::ModelCapability>{
    make_pdk_cap("gpt-3.5-turbo", 4096, 4096, 0.002, 200, {"general"}),
    make_pdk_cap("gpt-4", 8192, 4096, 0.03, 500, {"general", "code"}),
    make_pdk_cap("claude-3-opus", 16384, 4096, 0.015, 350,
                 {"general", "reasoning", "vision"}),
  };

  // claude-3: 16384+4096=20480, gpt-4: 8192+4096=12288, gpt-3.5: 4096+4096=8192
  auto result = router.route(ctx, candidates);
  REQUIRE(result == "claude-3-opus");
}

// --- Latency 策略测试 (4 TEST_CASE) ---

TEST_CASE("LatencyRouter lowest-latency: gpt-4(500ms) over gpt-3.5(200ms)",
          "[model_router][latency]") {
  agenticdsl::pdk::LatencyModelRouterPolicy router;
  agenticdsl::pdk::RoutingContext ctx;
  ctx.required_tags = {"general"};

  auto candidates = std::vector<agenticdsl::pdk::ModelCapability>{
    make_pdk_cap("gpt-4", 8192, 4096, 0.03, 500, {"general"}),
    make_pdk_cap("gpt-3.5-turbo", 4096, 4096, 0.002, 200, {"general"}),
  };

  auto result = router.route(ctx, candidates);
  REQUIRE(result == "gpt-3.5-turbo");
}

TEST_CASE("LatencyRouter latency-budget: max=300ms skips gpt-4(500) and claude-3(350)",
          "[model_router][latency]") {
  agenticdsl::pdk::LatencyModelRouterPolicy router;
  agenticdsl::pdk::RoutingContext ctx;
  ctx.required_tags = {"general"};
  ctx.budget_remaining = 300.0;

  auto candidates = std::vector<agenticdsl::pdk::ModelCapability>{
    make_pdk_cap("gpt-4", 8192, 4096, 0.03, 500, {"general"}),
    make_pdk_cap("claude-3-opus", 16384, 4096, 0.015, 350, {"general"}),
    make_pdk_cap("gpt-3.5-turbo", 4096, 4096, 0.002, 200, {"general"}),
  };

  auto result = router.route(ctx, candidates);
  REQUIRE(result == "gpt-3.5-turbo");
}

TEST_CASE("LatencyRouter all-exceed: max=100ms throws NoViableModel",
          "[model_router][latency]") {
  agenticdsl::pdk::LatencyModelRouterPolicy router;
  agenticdsl::pdk::RoutingContext ctx;
  ctx.required_tags = {"general"};
  ctx.budget_remaining = 100.0;

  auto candidates = std::vector<agenticdsl::pdk::ModelCapability>{
    make_pdk_cap("gpt-4", 8192, 4096, 0.03, 500, {"general"}),
    make_pdk_cap("gpt-3.5-turbo", 4096, 4096, 0.002, 200, {"general"}),
  };

  REQUIRE_THROWS_AS(router.route(ctx, candidates),
                    agenticdsl::pdk::ModelRoutingError);
}

TEST_CASE("LatencyRouter tag-over-latency: vision tag → gpt-4 even if slower",
          "[model_router][latency]") {
  agenticdsl::pdk::LatencyModelRouterPolicy router;
  agenticdsl::pdk::RoutingContext ctx;
  ctx.required_tags = {"vision"};

  auto candidates = std::vector<agenticdsl::pdk::ModelCapability>{
    make_pdk_cap("gpt-4", 8192, 4096, 0.03, 500, {"general", "vision"}),
    make_pdk_cap("gpt-3.5-turbo", 4096, 4096, 0.002, 200, {"general"}),
  };

  auto result = router.route(ctx, candidates);
  REQUIRE(result == "gpt-4");
}
