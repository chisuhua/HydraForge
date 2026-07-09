// agenticdsl/contract/i_llm_provider_decorator.h
// 文件头注释
// 功能描述：ILLMProviderDecorator 抽象基类 — GoF Decorator 模式
//          包装 ILLMProvider, 通过 final 转发 + protected virtual 钩子
//          让子类 (CostTracking/Compliance/RateLimit) 注入正交关注点
//          Phase 5 Budget Hole 修复 (3 处 LLM 直调路径零计费, REQ-IPD-001)
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/specs/
//          illmprovider-decorator/spec.md (REQ-IPD-001)
//          + GoF Decorator Pattern + ADR-0031 §决策 5 (ToolCoordinator Option C)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09
#ifndef AGENTICDSL_CONTRACT_I_LLM_PROVIDER_DECORATOR_H
#define AGENTICDSL_CONTRACT_I_LLM_PROVIDER_DECORATOR_H

#include "common/llm/llm_types.h"  // ILLMProvider / Result / GenerationRequest / IGenerationStream / ModelInfo

#include <functional>  // std::function (wrap_chain 签名)
#include <memory>      // std::unique_ptr
#include <optional>    // std::optional<LLMError> (pre_check 钩子返回值)
#include <stdexcept>   // std::runtime_error
#include <vector>      // std::vector

namespace agenticdsl {

/**
 * @brief ILLMProvider 装饰器抽象基类 (GoF Decorator)
 *
 * 设计目标 (REQ-IPD-001):
 *  - 持有 `std::unique_ptr<ILLMProvider> inner_` 转发基础调用
 *  - `generate()` / `generate_stream()` / `available_models()` 标记 `final`
 *    防止子类绕过钩子,保证转发路径单一
 *  - protected virtual 钩子 `decorate_*` 提供子类扩展点,默认 pass-through
 *  - 静态工厂 `wrap_chain()` 限制链深度 ≤ 4 (含 inner_),超出抛异常
 *
 * 部署顺序 (REQ-IPD-005, 从外到内):
 *   1. CostTrackingDecorator (最外层, 保证所有 LLM 调用被计费)
 *   2. ComplianceDecorator (opt-in)
 *   3. RateLimitDecorator (opt-in)
 *   4. inner_ (推理 Plugin ILLMProvider)
 *
 * 用法示例:
 * @code
 *   auto inner = std::make_unique<MockLLMProvider>();
 *   auto chain = ILLMProviderDecorator::wrap_chain(
 *       std::move(inner),
 *       {[&](auto p){ return std::make_unique<CostTrackingDecorator>(std::move(p), budget); }}});
 *   auto result = chain->generate(req, token);
 * @endcode
 */
class ILLMProviderDecorator : public ILLMProvider {
 public:
  /**
   * @brief 构造装饰器,接管 inner provider 的所有权
   * @param inner 被包装的 ILLMProvider (不可为 nullptr, 调用方负责)
   */
  explicit ILLMProviderDecorator(std::unique_ptr<ILLMProvider> inner);
  ~ILLMProviderDecorator() override = default;

  // === final 转发层 ===
  // 标记 final 防止子类绕过钩子; 子类通过 override decorate_* 钩子注入逻辑

  Result<GenerationResult, LLMError> generate(
      const GenerationRequest& req, std::stop_token token) override final;

  std::unique_ptr<IGenerationStream> generate_stream(
      const GenerationRequest& req, std::stop_token token) override final;

  std::vector<ModelInfo> available_models() const override final;

  // === 静态工厂: 批量包装装饰器链 ===

  /**
   * @brief 包装装饰器链 (从内到外)
   * @param innermost 最内层 ILLMProvider (真实 provider)
   * @param decorators 装饰器构造函数列表; decorators[0] = 最外层
   * @return 链最外层装饰器 (调用方持有)
   * @throw DecoratorChainTooDeep 若 decorators.size() > 3 (含 inner 总层数 > 4)
   *
   * 应用顺序: decorators[0] 是最外层 (最后应用), decorators[N-1] 是最内层装饰器 (最先应用)
   * 实现: 反向遍历,先应用最内层装饰器包装 innermost, 逐层向外
   */
  static std::unique_ptr<ILLMProvider> wrap_chain(
      std::unique_ptr<ILLMProvider> innermost,
      std::vector<std::function<std::unique_ptr<ILLMProvider>(
          std::unique_ptr<ILLMProvider>)>> decorators);

  /**
   * @brief 装饰器链深度超限异常
   *
   * 触发条件: wrap_chain() 收到 decorators.size() > 3 (含 inner 总层数 > 4)
   * 设计依据: REQ-IPD-001 §Scenario "装饰器链深度限制"
   */
  class DecoratorChainTooDeep : public std::runtime_error {
   public:
    explicit DecoratorChainTooDeep(int depth)
        : std::runtime_error("decorator chain too deep"), depth_(depth) {}

    int depth() const noexcept { return depth_; }

   private:
    int depth_;
  };

 protected:
  // === 子类扩展钩子 (默认 pass-through) ===
  // 子类 override 这些方法注入 cost/compliance/ratelimit 逻辑

  /**
   * @brief 同步 generate pre-check 钩子
   * @param req 原始请求 (只读)
   * @return std::optional<LLMError>: nullopt = pass-through;
   *         设置值 = 基类不再调用 inner, 直接返回该错误
   *
   * 用例 (Phase 5 REQ-IPD-004): RateLimitDecorator 在 pre-check 中预扣配额,
   * 配额不足立即返回 RateLimited, 不会真实调用 inner provider (避免消耗底层资源)。
   */
  virtual std::optional<LLMError> pre_check_generate(
      const GenerationRequest& /*req*/) {
    return std::nullopt;
  }

  /**
   * @brief 流式 generate_stream pre-check 钩子
   * @return 同 pre_check_generate 语义
   */
  virtual std::optional<LLMError> pre_check_generate_stream(
      const GenerationRequest& /*req*/) {
    return std::nullopt;
  }

  /**
   * @brief 同步 generate 钩子
   * @param req 原始请求 (只读引用)
   * @param inner_result inner_->generate() 的返回值 (success 或 failure)
   * @return 装饰后的 Result (默认原样返回)
   *
   * 注意: 装饰器 MUST NOT 修改业务返回值 (REQ-IPD-002 §Scenario "同步 generate 计费")
   */
  virtual Result<GenerationResult, LLMError> decorate_generate(
      const GenerationRequest& req,
      Result<GenerationResult, LLMError> inner_result) {
    return inner_result;
  }

  /**
   * @brief 流式 generate_stream 钩子
   * @param req 原始请求 (只读引用)
   * @param inner_stream inner_->generate_stream() 返回的流所有权
   * @return 包装后的流 (默认原样返回, 子类可返回 TrackingStream 包装)
   */
  virtual std::unique_ptr<IGenerationStream> decorate_generate_stream(
      const GenerationRequest& req,
      std::unique_ptr<IGenerationStream> inner_stream) {
    return inner_stream;
  }

  /**
   * @brief available_models 钩子
   * @param inner_models inner_->available_models() 返回值
   * @return 装饰后的列表 (默认原样返回)
   *
   * 标记 const: available_models() 是查询方法, 不应修改装饰器状态
   */
  virtual std::vector<ModelInfo> decorate_available_models(
      std::vector<ModelInfo> inner_models) const {
    return inner_models;
  }

  /// 被包装的 inner provider (owned)
  std::unique_ptr<ILLMProvider> inner_;

 public:
  /// 测试/诊断 helper: 获取 inner provider 指针
  /// Phase 5 (REQ-ICC-008): tests 可用此 unwrap decorator 链访问底层 mock
  /// 生产代码不应使用此方法 (破坏装饰器封装)
  ILLMProvider* inner() const noexcept { return inner_.get(); }
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_CONTRACT_I_LLM_PROVIDER_DECORATOR_H
