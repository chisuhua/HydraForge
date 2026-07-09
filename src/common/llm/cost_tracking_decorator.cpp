// cost_tracking_decorator.cpp
// 文件头注释
// 功能描述：CostTrackingDecorator 实现 — 同步/流式 LLM 调用计费
//          同步路径: 精确提取 GenerationResult.prompt_tokens + completion_tokens
//          流式路径: TrackingStream 包装, 流结束统计累积 token
//          真实 provider 无 per-chunk token 接口, 使用 req.params.max_tokens 近似
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/specs/
//          illmprovider-decorator/spec.md (REQ-IPD-002)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09

#include "cost_tracking_decorator.h"

#include "modules/budget/budget_controller.h"

#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace agenticdsl {

// =====================================================================
// CostTrackingDecorator 实现
// =====================================================================

CostTrackingDecorator::CostTrackingDecorator(
    std::unique_ptr<ILLMProvider> inner,
    std::shared_ptr<IBudgetController> budget)
    : ILLMProviderDecorator(std::move(inner)), budget_(std::move(budget)) {}

Result<GenerationResult, LLMError> CostTrackingDecorator::decorate_generate(
    const GenerationRequest& req,
    Result<GenerationResult, LLMError> inner_result) {
  if (inner_result.has_value() && budget_ != nullptr) {
    const auto& result = inner_result.value();
    const int total_tokens = result.prompt_tokens + result.completion_tokens;
    if (total_tokens > 0) {
      budget_->record_llm_call(total_tokens, req.params.model);
    }
  }
  return inner_result;
}

std::unique_ptr<IGenerationStream>
CostTrackingDecorator::decorate_generate_stream(
    const GenerationRequest& req,
    std::unique_ptr<IGenerationStream> inner_stream) {
  return std::make_unique<TrackingStream>(
      std::move(inner_stream), budget_, req.params.model,
      req.params.max_tokens);
}

// =====================================================================
// TrackingStream 实现
// =====================================================================

CostTrackingDecorator::TrackingStream::TrackingStream(
    std::unique_ptr<IGenerationStream> inner,
    std::shared_ptr<IBudgetController> budget,
    std::string model_name,
    int max_tokens_estimate)
    : inner_(std::move(inner)),
      budget_(std::move(budget)),
      model_name_(std::move(model_name)),
      max_tokens_estimate_(max_tokens_estimate) {}

std::optional<std::string>
CostTrackingDecorator::TrackingStream::next(std::stop_token token) {
  auto chunk = inner_->next(token);
  if (!chunk.has_value() && !recorded_) {
    recorded_ = true;
    if (budget_ != nullptr && max_tokens_estimate_ > 0) {
      budget_->record_llm_call(max_tokens_estimate_, model_name_);
    }
  }
  return chunk;
}

bool CostTrackingDecorator::TrackingStream::is_active() const {
  return inner_->is_active();
}

std::optional<LLMError>
CostTrackingDecorator::TrackingStream::error() const {
  return inner_->error();
}

}  // namespace agenticdsl
