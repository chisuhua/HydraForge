// tests/test_model_router_provider.cpp
// 功能描述：MockLLMProvider available_models 测试 hook (C7 Phase 1 MVP)。
//          2 个 TEST_CASE:
//            1. 默认构造函数返回 mock 模型
//            2. set_available_models 后返回注入的模型列表
// 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/design.md Decision 5
// 作者：C7 Phase 1 MVP
// 最后修改日期：2026-07-02

#include "catch_amalgamated.hpp"
#include "common/llm/mock_provider.h"

#include <vector>

using agenticdsl::MockLLMProvider;
using agenticdsl::ILLMProvider;

TEST_CASE("MockLLMProvider default available_models returns mock model", "[model_router][provider]") {
    MockLLMProvider provider;
    auto models = provider.available_models();
    REQUIRE_FALSE(models.empty());
    REQUIRE(models.size() == 1);
    REQUIRE(models[0].name == "mock-llm-v1");
}

TEST_CASE("MockLLMProvider set_available_models replaces models list", "[model_router][provider]") {
    MockLLMProvider provider;

    std::vector<ILLMProvider::ModelInfo> test_models = {
        ILLMProvider::ModelInfo("gpt-4", {ILLMProvider::ModelCapability::Chat}, 8192, "openai"),
        ILLMProvider::ModelInfo("claude-3", {ILLMProvider::ModelCapability::Chat, ILLMProvider::ModelCapability::Vision}, 16384, "anthropic"),
    };

    provider.set_available_models(test_models);
    auto models = provider.available_models();

    REQUIRE(models.size() == 2);
    REQUIRE(models[0].name == "gpt-4");
    REQUIRE(models[0].context_window == 8192);
    REQUIRE(models[0].provider == "openai");
    REQUIRE(models[1].name == "claude-3");
    REQUIRE(models[1].capabilities.size() == 2);
    REQUIRE(models[1].context_window == 16384);
}