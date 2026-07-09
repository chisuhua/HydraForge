// cost_tracking_decorator.h
// 文件头注释
// 功能描述：CostTrackingDecorator — ILLMProvider 装饰器子类
//          在 generate() / generate_stream() 完成后记录 token 消耗到 IBudgetController
//          修复 Phase 5 Budget Hole (3 处 LLM 直调路径零计费, REQ-IPD-002)
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/specs/
//          illmprovider-decorator/spec.md (REQ-IPD-002)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09
#ifndef AGENTICDSL_LLM_COST_TRACKING_DECORATOR_H
#define AGENTICDSL_LLM_COST_TRACKING_DECORATOR_H

#include "agenticdsl/contract/i_llm_provider_decorator.h"

#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace agenticdsl {

// 前向声明 IBudgetController (完整定义在 modules/budget/budget_controller.h)
class IBudgetController;

/**
 * @brief 成本跟踪装饰器 (REQ-IPD-002)
 *
 * 部署位置 (REQ-IPD-005): 最外层装饰器, 保证所有 LLM 调用都被计费
 *
 * 行为:
 *  - decorate_generate(): 若 inner 返回 success, 提取 prompt_tokens +
 *    completion_tokens 并调用 budget_->record_llm_call(); 若返回 error 不计费
 *  - decorate_generate_stream(): 包装 inner_stream, 在流结束 (next() 返回
 *    nullopt) 时统计累积 token. 精确统计依赖 MockLLMProvider 的测试 hook
 *    (last_stream_total_tokens()); 真实 provider 使用 req.params.max_tokens
 *    作为近似
 *
 * 线程安全: budget_ 通过 shared_ptr 共享, IBudgetController::record_llm_call
 *          实现需保证线程安全 (BudgetController 内部 mutex 保护)
 */
class CostTrackingDecorator : public ILLMProviderDecorator {
 public:
  /**
   * @param inner 被包装的 provider (owned)
   * @param budget 预算控制器 (shared, 允许多个装饰器共享同一 budget)
   */
  CostTrackingDecorator(std::unique_ptr<ILLMProvider> inner,
                        std::shared_ptr<IBudgetController> budget);
  ~CostTrackingDecorator() override = default;

 protected:
  // === 钩子实现 ===

  /// 同步 generate: 成功时记录 prompt_tokens + completion_tokens
  Result<GenerationResult, LLMError> decorate_generate(
      const GenerationRequest& req,
      Result<GenerationResult, LLMError> inner_result) override;

  /// 流式 generate_stream: 返回 TrackingStream 包装 inner_stream
  std::unique_ptr<IGenerationStream> decorate_generate_stream(
      const GenerationRequest& req,
      std::unique_ptr<IGenerationStream> inner_stream) override;

 private:
  std::shared_ptr<IBudgetController> budget_;

  /**
   * @brief 流式 token 跟踪包装器
   *
   * 包装 inner_stream, 透传 next()/is_active()/error(), 在流结束时
   * (next() 返回 nullopt) 调用 budget_->record_llm_call()
   *
   * token 统计策略:
   *  - 精确 (测试): dynamic_cast inner_ 到 MockLLMProvider, 调用其
   *    last_stream_total_tokens() hook
   *  - 近似 (生产): 使用 req.params.max_tokens 作为上界估计
   *
   * 析构兜底 (Phase 5 REQ-IPD-002 修复): 流对象被提前销毁 (异常 / 取消 /
   * 调用方中断) 时, next() 不会返回 nullopt, recorded_ 保持 false —
   * 由 ~TrackingStream 兜底完成计费, 避免 budget hole。
   */
  class TrackingStream : public IGenerationStream {
   public:
    TrackingStream(std::unique_ptr<IGenerationStream> inner,
                   std::shared_ptr<IBudgetController> budget,
                   std::string model_name,
                   int max_tokens_estimate);
    ~TrackingStream() override;

    std::optional<std::string> next(std::stop_token token) override;
    bool is_active() const override;
    std::optional<LLMError> error() const override;

   private:
    std::unique_ptr<IGenerationStream> inner_;
    std::shared_ptr<IBudgetController> budget_;
    std::string model_name_;
    int max_tokens_estimate_;
    bool recorded_ = false;  // 防止重复计费
  };
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_LLM_COST_TRACKING_DECORATOR_H
