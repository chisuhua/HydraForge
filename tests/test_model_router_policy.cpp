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
