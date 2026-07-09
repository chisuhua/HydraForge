// src/common/llm/orchestration_illm_provider.cpp
// Phase 5 / REQ-ICC-001 Dual Consumer Model — 直连实现

#include "agenticdsl/contract/iinteraction_bus.h"  // IInteractionBus (用于可选 emit)
#include "agenticdsl/pdk/agent_loops/orchestration_illm_provider.h"

#include <stop_token>
#include <utility>

namespace agenticdsl {

OrchestrationILLMProvider::OrchestrationILLMProvider(
    std::shared_ptr<ILLMProvider> inference_provider,
    std::shared_ptr<IModelRouter> router,
    std::shared_ptr<IInteractionBus> bus)
    : inference_provider_(std::move(inference_provider)),
      router_(std::move(router)),
      bus_(std::move(bus)) {
  // inference_provider_ 非空是契约: 调用方必须保证 .get() != nullptr
  // 这里只防御 nullptr, 不抛 (避免破坏 EXPECT/TEST 路径中的快速失败)
}

Result<GenerationResult, LLMError>
OrchestrationILLMProvider::generate(const GenerationRequest& req,
                                    std::stop_token token) {
  if (!inference_provider_) {
    return Result<GenerationResult, LLMError>::failure(
        LLMError{LLMError::Code::InvalidRequest, "no inference provider"});
  }

  // === Step 1: 路由选择 (若 router_ 注入) ===
  // MVP: 路由结果未直接修改 req (后续可基于 selected model 调整 max_tokens 等)
  //       显式捕获 models list 以触发 Router 静默失败检查 (REQ-MR-003)
  if (router_) {
    auto models = inference_provider_->available_models();
    if (models.empty()) {
      return Result<GenerationResult, LLMError>::failure(
          LLMError{LLMError::Code::InvalidRequest, "no models available"});
    }
    // 调用 router 但结果不强制修改 req (router 可作为日志/审计信号)
    // Note: full router integration 是 ADR-0045 §2.2 — Phase 5 ship MVP 仅做钩子
    // (后续: 根据 selected model 设置 req.params.model)
  }

  // === Step 2: ensure_session() — MVP no-op ===
  // 会话由 DSLEngine::run() 在 LayeredContext 中管理 (REQ-ICC-006)
  // (后续: emit session_lifecycle event 到 bus_)

  // === Step 3: apply_per_request_config() — MVP identity ===
  // 后续可基于 router.selected 注入 max_tokens 调整

  // === Step 4: 直连 inference_provider_->generate() ===
  // REQ-ICC-002 Scenario 1: 不经过 internal_registry_ / ToolCoordinator / approval
  return direct_generate(req, token);
}

std::unique_ptr<IGenerationStream>
OrchestrationILLMProvider::generate_stream(const GenerationRequest& req,
                                           std::stop_token token) {
  if (!inference_provider_) {
    // 无 provider — 返回立即 inactive 流 (调用方 next() 立即得到 nullopt)
    // 与 CloudLLMAdapter::generate_stream 错误路径一致
    class EmptyStream : public IGenerationStream {
     public:
      std::optional<std::string> next(std::stop_token) override { return std::nullopt; }
      bool is_active() const override { return false; }
      std::optional<LLMError> error() const override {
        return LLMError{LLMError::Code::InvalidRequest, "no inference provider"};
      }
    };
    return std::make_unique<EmptyStream>();
  }

  // 直连: inference_provider_->generate_stream() 不经过 registry (REQ-ICC-002)
  return inference_provider_->generate_stream(req, token);
}

std::vector<ILLMProvider::ModelInfo>
OrchestrationILLMProvider::available_models() const {
  if (!inference_provider_) return {};
  // REQ-ICC-002 Scenario: available_models 透传 (路由 Layer 可见的模型列表)
  return inference_provider_->available_models();
}

Result<GenerationResult, LLMError>
OrchestrationILLMProvider::direct_generate(const GenerationRequest& req,
                                          std::stop_token token) {
  // 双保险: 即使 generate() 已检查过, 这里也再确保一次
  if (!inference_provider_) {
    return Result<GenerationResult, LLMError>::failure(
        LLMError{LLMError::Code::InvalidRequest, "inference provider is null"});
  }
  return inference_provider_->generate(req, token);
}

}  // namespace agenticdsl
