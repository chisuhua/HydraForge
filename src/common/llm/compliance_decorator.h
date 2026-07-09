// compliance_decorator.h
// 文件头注释
// 功能描述：ComplianceDecorator — ILLMProvider 装饰器子类
//          在 generate() / generate_stream() 调用前后 emit compliance log events
//          到 IInteractionBus (MVP 仅日志, 不做 PII 检测, REQ-IPD-003)
//          payload 仅含 prompt/completion hash, 不含原始文本 (per ADR-0031 §决策 7)
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/specs/
//          illmprovider-decorator/spec.md (REQ-IPD-003)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09
#ifndef AGENTICDSL_LLM_COMPLIANCE_DECORATOR_H
#define AGENTICDSL_LLM_COMPLIANCE_DECORATOR_H

#include "agenticdsl/contract/i_llm_provider_decorator.h"

#include <memory>
#include <string>

namespace agenticdsl {

// 前向声明 IInteractionBus (完整定义在 include/agenticdsl/contract/iinteraction_bus.h)
class IInteractionBus;

/**
 * @brief 合规日志装饰器 (REQ-IPD-003)
 *
 * 部署位置 (REQ-IPD-005): 第 2 层 (CostTracking → Compliance → RateLimit → inner)
 * opt-in: 默认不启用, Phase 6+ 启用 PII 检测后再 opt-in
 *
 * 行为:
 *  - decorate_generate(): 计算 prompt_hash (std::hash<std::string>),
 *    emit "compliance.log" event (payload 含 {prompt_hash, model, tenant_id,
 *    timestamp}); 若 inner 返回 success, 再计算 completion_hash, emit 另一个
 *    "compliance.log" event (payload 含 {completion_hash, prompt_hash, model,
 *    timestamp})
 *  - decorate_generate_stream(): 包装 inner_stream, 在流结束 (next() 返回
 *    nullopt) 时累积 chunk 计算 completion_hash, emit 单个 "compliance.log"
 *    event
 *
 * 安全约束 (per ADR-0031 §决策 7):
 *  - payload MUST NOT 包含 req.prompt 原始文本 (避免 secret 泄露)
 *  - payload MUST NOT 包含 result.text 原始 completion
 *  - 仅存储 hash 值 (std::hash<std::string>{}(...))
 *
 * 线程安全: bus_ 通过 shared_ptr 共享, IInteractionBus 实现保证线程安全
 */
class ComplianceDecorator : public ILLMProviderDecorator {
 public:
  /**
   * @param inner 被包装的 provider (owned)
   * @param bus 交互总线 (shared, 允许多个装饰器共享同一 bus)
   */
  ComplianceDecorator(std::unique_ptr<ILLMProvider> inner,
                      std::shared_ptr<IInteractionBus> bus);
  ~ComplianceDecorator() override = default;

 protected:
  // === 钩子实现 ===

  /// 同步 generate: emit prompt_hash + completion_hash compliance.log events
  Result<GenerationResult, LLMError> decorate_generate(
      const GenerationRequest& req,
      Result<GenerationResult, LLMError> inner_result) override;

  /// 流式 generate_stream: 返回 ComplianceStream 包装 inner_stream
  std::unique_ptr<IGenerationStream> decorate_generate_stream(
      const GenerationRequest& req,
      std::unique_ptr<IGenerationStream> inner_stream) override;

 private:
  std::shared_ptr<IInteractionBus> bus_;

  /**
   * @brief 流式合规日志包装器
   *
   * 包装 inner_stream, 透传 next()/is_active()/error(), 累积所有 chunk,
   * 在流结束 (next() 返回 nullopt) 时计算 completion_hash 并 emit 单个
   * "compliance.log" event
   */
  class ComplianceStream : public IGenerationStream {
   public:
    ComplianceStream(std::unique_ptr<IGenerationStream> inner,
                     std::shared_ptr<IInteractionBus> bus,
                     std::size_t prompt_hash,
                     std::string model_name);
    ~ComplianceStream() override;

    std::optional<std::string> next(std::stop_token token) override;
    bool is_active() const override;
    std::optional<LLMError> error() const override;

   private:
    std::unique_ptr<IGenerationStream> inner_;
    std::shared_ptr<IInteractionBus> bus_;
    std::size_t prompt_hash_;
    std::string model_name_;
    std::string accumulated_;  // 累积所有 chunk 的 completion 文本
    bool emitted_ = false;     // 防止重复 emit
  };
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_LLM_COMPLIANCE_DECORATOR_H
