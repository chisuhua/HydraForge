// rate_limit_decorator.cpp
// 文件头注释
// 功能描述：RateLimitDecorator 实现 — token bucket 配额检查
//          同步路径: try_consume(max_tokens), 不足返回 RateLimited, 成功退还差额
//          流式路径: 预扣 + 流结束退还
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/specs/
//          illmprovider-decorator/spec.md (REQ-IPD-004)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09

#include "rate_limit_decorator.h"

#include <chrono>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace agenticdsl {

// =====================================================================
// RateLimitDecorator 实现
// =====================================================================

RateLimitDecorator::RateLimitDecorator(
    std::unique_ptr<ILLMProvider> inner,
    std::string tenant_id,
    int tokens_per_minute)
    : ILLMProviderDecorator(std::move(inner)),
      tenant_id_(std::move(tenant_id)),
      tokens_per_minute_(tokens_per_minute),
      // 初始满桶 (避免冷启动拒绝)
      tokens_remaining_(static_cast<double>(tokens_per_minute)),
      last_refill_(std::chrono::steady_clock::now()) {}

void RateLimitDecorator::refill() {
  // 调用方需持 bucket_mutex_
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration<double>(now - last_refill_);
  // 每秒补充 tokens_per_minute / 60.0 个 token
  const double replenished = elapsed.count() * tokens_per_minute_ / 60.0;
  tokens_remaining_ = std::min(
      tokens_remaining_ + replenished,
      static_cast<double>(tokens_per_minute_));
  last_refill_ = now;
}

bool RateLimitDecorator::try_consume(int tokens) {
  if (tokens < 0) {
    tokens = 0;  // 防御性
  }
  std::lock_guard<std::mutex> lock(bucket_mutex_);
  refill();
  if (tokens_remaining_ >= tokens) {
    tokens_remaining_ -= tokens;
    return true;
  }
  return false;
}

Result<GenerationResult, LLMError> RateLimitDecorator::decorate_generate(
    const GenerationRequest& req,
    Result<GenerationResult, LLMError> inner_result) {
  // Phase 5 修复: 预扣已迁移到 pre_check_generate (在基类调用 inner 之前)。
  // 本方法仅负责在 inner 调用完之后, 根据成功 / 失败退还预扣的配额。
  const int max_tokens = req.params.max_tokens;
  if (max_tokens <= 0) {
    return inner_result;
  }

  int refund = 0;
  if (inner_result.has_value()) {
    // 成功: 退还差额 (max_tokens - actual_tokens)
    const auto& result = inner_result.value();
    const int actual_tokens = result.prompt_tokens + result.completion_tokens;
    refund = max_tokens - actual_tokens;
  } else {
    // 失败: 全额退还, 避免失败请求消耗配额
    refund = max_tokens;
  }

  if (refund > 0) {
    std::lock_guard<std::mutex> lock(bucket_mutex_);
    // 退还不超过 max_tokens (防止溢出)
    tokens_remaining_ = std::min(
        tokens_remaining_ + refund,
        static_cast<double>(tokens_per_minute_));
  }

  return inner_result;
}

std::optional<LLMError> RateLimitDecorator::pre_check_generate(
    const GenerationRequest& req) {
  const int max_tokens = req.params.max_tokens;
  if (max_tokens > 0 && !try_consume(max_tokens)) {
    return LLMError(LLMError::Code::RateLimited,
                    "tenant " + tenant_id_ + " quota exceeded");
  }
  return std::nullopt;
}

std::optional<LLMError> RateLimitDecorator::pre_check_generate_stream(
    const GenerationRequest& req) {
  const int max_tokens = req.params.max_tokens;
  if (max_tokens > 0 && !try_consume(max_tokens)) {
    return LLMError(LLMError::Code::RateLimited,
                    "tenant " + tenant_id_ + " quota exceeded");
  }
  return std::nullopt;
}

std::unique_ptr<IGenerationStream>
RateLimitDecorator::decorate_generate_stream(
    const GenerationRequest& req,
    std::unique_ptr<IGenerationStream> inner_stream) {
  // Phase 5 修复: 预扣已迁移到 pre_check_generate_stream.
  // 基类保证 inner_stream 非 null (pre-check 失败时已直接返回 ErrorStream).
  // RateLimitStream 的 nullptr 重载保留为防御性 dead-branch (Phase 5 后无入口触发).
  return std::make_unique<RateLimitStream>(
      std::move(inner_stream), this, req.params.max_tokens);
}

// =====================================================================
// RateLimitStream 实现
// =====================================================================

RateLimitDecorator::RateLimitStream::RateLimitStream(
    std::unique_ptr<IGenerationStream> inner,
    RateLimitDecorator* owner,
    int max_tokens)
    : inner_(std::move(inner)),
      owner_(owner),
      max_tokens_(max_tokens),
      actual_tokens_(0) {
  if (inner_ == nullptr) {
    // 预扣失败场景: 注入 RateLimited 错误
    injected_error_ = LLMError(
        LLMError::Code::RateLimited, "tenant quota exceeded");
  }
}

RateLimitDecorator::RateLimitStream::~RateLimitStream() {
  // 析构时若未退还, 补一次退还保证配额正确
  if (!refunded_ && owner_ != nullptr && max_tokens_ > 0) {
    refunded_ = true;
    // 流被提前销毁视为中断, 按已消耗 token 扣减 (不退还)
    // 退还差额 = max_tokens - actual_tokens
    const int refund = max_tokens_ - actual_tokens_;
    if (refund > 0) {
      std::lock_guard<std::mutex> lock(owner_->bucket_mutex_);
      owner_->tokens_remaining_ = std::min(
          owner_->tokens_remaining_ + refund,
          static_cast<double>(owner_->tokens_per_minute_));
    }
  }
}

std::optional<std::string>
RateLimitDecorator::RateLimitStream::next(std::stop_token token) {
  // 预扣失败场景: 直接返回 nullopt (流结束)
  if (inner_ == nullptr) {
    return std::nullopt;
  }

  auto chunk = inner_->next(token);
  if (chunk.has_value()) {
    // 累积估算 token (MVP: 简单字符数 / 4 估算, 真实 provider 应有 per-chunk token)
    // 这里仅用于退还差额估算, 不影响计费 (计费由 CostTracking 负责)
    actual_tokens_ += static_cast<int>(chunk->size() / 4);
  } else if (!refunded_ && owner_ != nullptr && max_tokens_ > 0) {
    // 流结束, 退还差额
    refunded_ = true;
    const int refund = max_tokens_ - actual_tokens_;
    if (refund > 0) {
      std::lock_guard<std::mutex> lock(owner_->bucket_mutex_);
      owner_->tokens_remaining_ = std::min(
          owner_->tokens_remaining_ + refund,
          static_cast<double>(owner_->tokens_per_minute_));
    }
  }
  return chunk;
}

bool RateLimitDecorator::RateLimitStream::is_active() const {
  if (inner_ == nullptr) {
    return false;  // 预扣失败, 流不活跃
  }
  return inner_->is_active();
}

std::optional<LLMError>
RateLimitDecorator::RateLimitStream::error() const {
  if (inner_ == nullptr) {
    return injected_error_;
  }
  return inner_->error();
}

}  // namespace agenticdsl
