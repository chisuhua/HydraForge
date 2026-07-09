// rate_limit_decorator.h
// 文件头注释
// 功能描述：RateLimitDecorator — ILLMProvider 装饰器子类
//          在 generate() / generate_stream() 前检查 token bucket 配额 (多租户场景)
//          配额不足返回 Code::RateLimited; 成功后退还差额 (REQ-IPD-004)
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/specs/
//          illmprovider-decorator/spec.md (REQ-IPD-004)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09
#ifndef AGENTICDSL_LLM_RATE_LIMIT_DECORATOR_H
#define AGENTICDSL_LLM_RATE_LIMIT_DECORATOR_H

#include "agenticdsl/contract/i_llm_provider_decorator.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace agenticdsl {

/**
 * @brief 速率限制装饰器 (REQ-IPD-004)
 *
 * 部署位置 (REQ-IPD-005): 第 3 层 (CostTracking → Compliance → RateLimit → inner)
 * opt-in: 默认不启用, 多租户部署时显式启用
 *
 * 行为 (token bucket 算法):
 *  - decorate_generate():
 *    1. try_consume(max_tokens) 预扣配额
 *    2. 若配额不足, 返回 Result::failure(LLMError{Code::RateLimited, ...})
 *    3. 若配额充足, 转发到 inner_->generate()
 *    4. inner 成功后, 退还差额: tokens_remaining += (max_tokens - actual_tokens)
 *  - decorate_generate_stream():
 *    1. try_consume(max_tokens) 预扣
 *    2. 若不足, 返回失败的流 (error() 返回 RateLimited)
 *    3. 流结束后退还差额
 *
 * 线程安全: bucket_mutex_ 保护 tokens_remaining_ / last_refill_
 */
class RateLimitDecorator : public ILLMProviderDecorator {
 public:
  /**
   * @param inner 被包装的 provider (owned)
   * @param tenant_id 租户标识 (MVP 仅用于日志, Phase 6+ 用于多租户配额隔离)
   * @param tokens_per_minute 每分钟 token 配额上限
   */
  RateLimitDecorator(std::unique_ptr<ILLMProvider> inner,
                     std::string tenant_id,
                     int tokens_per_minute);
  ~RateLimitDecorator() override = default;

 protected:
  // === 钩子实现 ===

  /// 同步 generate pre-check: 预扣 max_tokens, 不足直接返回 RateLimited
  /// (不调用 inner_) — Phase 5 REQ-IPD-004 修复 (修复前在 decorate_generate
  /// post-check, 配额超限无法阻止底层调用)
  std::optional<LLMError> pre_check_generate(
      const GenerationRequest& req) override;

  /// 流式 generate_stream pre-check: 同上语义, 流版本
  std::optional<LLMError> pre_check_generate_stream(
      const GenerationRequest& req) override;

  /// 同步 generate: 仅负责根据成功 / 失败退还预扣的配额 (post-refund)
  Result<GenerationResult, LLMError> decorate_generate(
      const GenerationRequest& req,
      Result<GenerationResult, LLMError> inner_result) override;

  /// 流式 generate_stream: 包装 inner_stream 为 RateLimitStream (退款逻辑在
  /// RateLimitStream::next / ~RateLimitStream 中完成)
  std::unique_ptr<IGenerationStream> decorate_generate_stream(
      const GenerationRequest& req,
      std::unique_ptr<IGenerationStream> inner_stream) override;

 private:
  std::string tenant_id_;
  int tokens_per_minute_;

  // Token bucket 状态
  double tokens_remaining_;
  std::chrono::steady_clock::time_point last_refill_;
  std::mutex bucket_mutex_;

  /// 尝试消费 tokens, 不足返回 false (线程安全)
  bool try_consume(int tokens);

  /// 按时间流逝补充配额 (调用方需持锁)
  void refill();

  /**
   * @brief 流式速率限制包装器
   *
   * 预扣 max_tokens, 透传 next()/is_active()/error(), 流结束退还差额
   * (max_tokens - actual_tokens)
   */
  class RateLimitStream : public IGenerationStream {
   public:
    RateLimitStream(std::unique_ptr<IGenerationStream> inner,
                    RateLimitDecorator* owner,
                    int max_tokens);
    ~RateLimitStream() override;

    std::optional<std::string> next(std::stop_token token) override;
    bool is_active() const override;
    std::optional<LLMError> error() const override;

   private:
    std::unique_ptr<IGenerationStream> inner_;
    RateLimitDecorator* owner_;  // 非所有, 用于退还差额
    int max_tokens_;
    int actual_tokens_;       // 流式累积估算 token
    bool refunded_ = false;   // 防止重复退还
    std::optional<LLMError> injected_error_;  // 预扣失败时注入
  };
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_LLM_RATE_LIMIT_DECORATOR_H
