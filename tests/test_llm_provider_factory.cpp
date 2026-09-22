// tests/test_llm_provider_factory.cpp
// 功能描述：Wave 3 Phase 1 D7 serving 集成测试 (ADR-0078 D7 Phase 1 最小版)
//          FinetuneBaseModelProvider stub + LLMProviderFactory::register_dynamic
//          注册 "agenticdsl-llama-3.1-70b-lora-v1" (AC-6)
// 设计依据：openspec/changes/wave-3-finetune-base-model-pilot-phase1/spec.md R5
//           + design.md D7-1/D7-2/D7-3/D7-4
// 作者：HydraForge Wave 3 Phase 1 ship
// 最后修改日期：2026-09-23

#include "catch_amalgamated.hpp"

#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_config.h"
#include "common/llm/llm_types.h"

#include <memory>
#include <string>

using agenticdsl::GenerationRequest;
using agenticdsl::ILLMProvider;
using agenticdsl::LLMConfig;
using agenticdsl::LLMError;
using agenticdsl::LLMProviderFactory;

namespace {

constexpr const char* kFinetuneModelName = "agenticdsl-llama-3.1-70b-lora-v1";

}  // namespace

// ============================================================
// 1. register_dynamic 注册 fine-tune provider 且实例化返回 stub
// ============================================================
TEST_CASE("register_dynamic registers fine-tune provider and instantiation returns stub",
          "[llm_provider_factory][wave3]") {
  LLMProviderFactory factory;

  // 构造自动注册 (design D7-3: factory 构造函数内 register_dynamic)
  REQUIRE(factory.has_dynamic(kFinetuneModelName));

  LLMConfig config;
  config.provider = kFinetuneModelName;
  auto provider = factory.create(config);
  REQUIRE(provider != nullptr);

  // available_models() 非空 (AC-6: 路由/工具能发现该模型)
  auto models = provider->available_models();
  REQUIRE_FALSE(models.empty());
  REQUIRE(models[0].name == kFinetuneModelName);

  // generate() 返回 failure "Phase 2 deferred" (fail-fast, 不静默空响应)
  GenerationRequest req("test prompt");
  auto result = provider->generate(req, std::stop_token{});
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error().message.find("Phase 2 deferred") != std::string::npos);
}
