// src/common/llm/finetune_provider.cpp
// 功能描述：FinetuneBaseModelProvider stub 3 虚函数实现 (Phase 1 最小版)
//          generate → failure "Phase 2 deferred" / generate_stream → nullptr
//          available_models → {kModelName} (非空, AC-6)
// 设计依据：design.md D7-2 + specs/wave-3-finetune-base-model/spec.md R5
// 作者：HydraForge Wave 3 Phase 1 ship
// 最后修改日期：2026-09-23

#include "common/llm/finetune_provider.h"

namespace agenticdsl {

namespace {
// generate_stream 返回 nullptr 是显式 Phase 1 边界 (无流式能力)
constexpr const char* kPhase2DeferredMsg =
    "Phase 2 deferred: finetune model inference not yet implemented";
}  // namespace

FinetuneBaseModelProvider::FinetuneBaseModelProvider(LLMConfig config)
    : config_(std::move(config)) {}

Result<GenerationResult, LLMError> FinetuneBaseModelProvider::generate(
    const GenerationRequest& /*req*/, std::stop_token /*token*/) {
  // fail-fast: 明确失败而非静默空响应 (design D7-2 设计原则).
  // Phase 1 不触发真实推理; Phase 2 替换为实际 fine-tune 推理.
  return Result<GenerationResult, LLMError>::failure(
      LLMError{LLMError::Code::Unknown, kPhase2DeferredMsg});
}

std::unique_ptr<IGenerationStream> FinetuneBaseModelProvider::generate_stream(
    const GenerationRequest& /*req*/, std::stop_token /*token*/) {
  // Phase 1 无流式支持 (spec R5: generate_stream 返回 nullptr)
  return nullptr;
}

std::vector<ILLMProvider::ModelInfo> FinetuneBaseModelProvider::available_models() const {
  // 非空: 让路由/工具能发现该模型 (AC-6 要求)
  return {ILLMProvider::ModelInfo{
      kModelName,
      {ILLMProvider::ModelCapability::Chat, ILLMProvider::ModelCapability::ToolUse},
      config_.n_ctx,
      "finetune"}};
}

}  // namespace agenticdsl
