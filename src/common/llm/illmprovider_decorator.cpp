// illmprovider_decorator.cpp
// 文件头注释
// 功能描述：ILLMProviderDecorator 实现 — final 转发层 + wrap_chain 静态工厂
//          转发到 inner_ 后调用 protected 钩子, 子类通过 override 钩子注入逻辑
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/specs/
//          illmprovider-decorator/spec.md (REQ-IPD-001)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09

#include "agenticdsl/contract/i_llm_provider_decorator.h"

#include <utility>  // std::move

namespace agenticdsl {

// =====================================================================
// 构造 / 转发层
// =====================================================================

ILLMProviderDecorator::ILLMProviderDecorator(std::unique_ptr<ILLMProvider> inner)
    : inner_(std::move(inner)) {}

Result<GenerationResult, LLMError> ILLMProviderDecorator::generate(
    const GenerationRequest& req, std::stop_token token) {
  // 1. 转发到 inner provider (Result 是 move-only, 用 auto 持有)
  auto inner_result = inner_->generate(req, token);
  // 2. 调用子类钩子 (默认 pass-through)
  return decorate_generate(req, std::move(inner_result));
}

std::unique_ptr<IGenerationStream>
ILLMProviderDecorator::generate_stream(const GenerationRequest& req,
                                        std::stop_token token) {
  // 1. 转发到 inner provider
  auto inner_stream = inner_->generate_stream(req, token);
  // 2. 调用子类钩子 (子类可返回 TrackingStream 包装)
  return decorate_generate_stream(req, std::move(inner_stream));
}

std::vector<ILLMProvider::ModelInfo>
ILLMProviderDecorator::available_models() const {
  // const 方法: decorate_available_models 接收 vector 值,返回装饰后的
  return decorate_available_models(inner_->available_models());
}

// =====================================================================
// wrap_chain 静态工厂
// =====================================================================

std::unique_ptr<ILLMProvider> ILLMProviderDecorator::wrap_chain(
    std::unique_ptr<ILLMProvider> innermost,
    std::vector<std::function<std::unique_ptr<ILLMProvider>(
        std::unique_ptr<ILLMProvider>)>> decorators) {
  // REQ-IPD-001 §Scenario "装饰器链深度限制":
  //   最大层数 ≤ 4 (含 inner_ 在内, 即最多 3 个装饰器 + 1 个 inner_ = 4 层)
  //   超出层数 MUST 抛 std::runtime_error("decorator chain too deep")
  constexpr std::size_t MAX_CHAIN_DEPTH = 4;  // innermost + 3 decorators
  if (decorators.size() > MAX_CHAIN_DEPTH - 1) {
    throw DecoratorChainTooDeep(static_cast<int>(decorators.size()));
  }

  // 应用顺序: decorators[0] 是最外层 (最后应用,最终被调用方持有)
  //          decorators[N-1] 是最内层装饰器 (最先应用,直接包装 innermost)
  // 反向遍历: 先应用 decorators[N-1] 包装 innermost, 逐层向外
  std::unique_ptr<ILLMProvider> current = std::move(innermost);
  for (auto it = decorators.rbegin(); it != decorators.rend(); ++it) {
    current = (*it)(std::move(current));
  }
  return current;
}

}  // namespace agenticdsl
