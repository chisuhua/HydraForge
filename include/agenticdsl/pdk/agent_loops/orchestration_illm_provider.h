// include/agenticdsl/pdk/agent_loops/orchestration_illm_provider.h
// 文件头注释
// 功能描述：OrchestrationILLMProvider — Dual Consumer Model (Phase 5 / ADR-0045 §2 修订)。
//          编排 Plugin 的 ILLMProvider 实现, 内部从 `internal_registry_.call_tool` 改为直连
//          `inference_provider_->generate()`, 服务 DSLEngine/NodeExecutor 外部消费者。
//          Agent 循环 (ReAct/PlanExecute/ForkJoin) 通过 engine_->get_llm_provider() 访问
//          raw ILLMProvider* (经 Decorator 链), 绕开本类包装 (REQ-ICC-003)。
// 设计依据：ADR-0045 §2 (Dual Consumer Model) + openspec/changes/phase5-illmprovider-call-chain-v2
//          design.md Decision 1 + ADR-0034 IModelRouter 集成。
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09

#pragma once

#include "agenticdsl/pdk/model_router.h"  // IModelRouter
#include "common/llm/llm_types.h"          // ILLMProvider / GenerationRequest / IGenerationStream

#include <memory>
#include <string>

namespace agenticdsl { class IInteractionBus; }  // contract 层

namespace agenticdsl {

/**
 * @brief OrchestrationILLMProvider — Dual Consumer Model (REQ-ICC-001)
 *
 * 责任: 路由 + 会话管理 + 元数据注入 (而非调用工具 dispatch)。
 * 直连路径: `inference_provider_->generate(req)` (无 internal_registry call_tool)。
 * 不经过 ToolCoordinator / approval pipeline (per ADR-0031 §决策 5)。
 */
class OrchestrationILLMProvider : public ILLMProvider {
 public:
  using IModelRouter = pdk::IModelRouter;  // alias for clarity

  /**
   * @param inference_provider raw inference plugin ILLMProvider (shared)
   * @param router            IModelRouter 注入 (可选, nullptr 时不路由)
   * @param bus               IInteractionBus (可选, nullptr 时不 emit)
   */
  OrchestrationILLMProvider(std::shared_ptr<ILLMProvider> inference_provider,
                            std::shared_ptr<IModelRouter> router = nullptr,
                            std::shared_ptr<IInteractionBus> bus = nullptr);

  ~OrchestrationILLMProvider() override = default;

  Result<GenerationResult, LLMError>
  generate(const GenerationRequest& req, std::stop_token token) override;

  std::unique_ptr<IGenerationStream>
  generate_stream(const GenerationRequest& req, std::stop_token token) override;

  std::vector<ModelInfo>
  available_models() const override;

  /// 测试/诊断: 获取底层 inference_provider
  ILLMProvider* inference_provider() const noexcept { return inference_provider_.get(); }

 private:
  /// 直连 generate 的核心; 调用方 prepare-and-forward
  /// ensure_session() 在 MVP 阶段为 no-op (会话由 DSLEngine 管理层注入, REQ-ICC-006)
  /// apply_per_request_config() 在 MVP 阶段为 identity (后续可注入 max_tokens 调整)
  Result<GenerationResult, LLMError>
  direct_generate(const GenerationRequest& req, std::stop_token token);

  std::shared_ptr<ILLMProvider> inference_provider_;
  std::shared_ptr<IModelRouter> router_;
  std::shared_ptr<IInteractionBus> bus_;
};

}  // namespace agenticdsl
