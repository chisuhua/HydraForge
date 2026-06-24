#ifndef AGENTICDSL_LLM_LLM_PROVIDER_FACTORY_H
#define AGENTICDSL_LLM_LLM_PROVIDER_FACTORY_H

// 文件头注释
// 功能描述：LLMProviderFactory — 单一 backend_name 路由的 ILLMProvider 工厂
//          Phase 1 P1 引擎解耦 (ADR-0019 §1.4 退出标准) — T1.2
//          基于 LLMConfig::provider 字段路由到具体工厂实现
//          单一 backend_name 路由 (避免 v2 YAGNI: 不实现 CloudProviderFactory/LlamaProviderFactory 独立类)
// 设计依据：ADR-0005 (LLM 后端配置与工厂) §3 + openspec/.../T1
// 作者：AgenticDSL Phase 1 P1.T1
// 最后修改日期：2026-06-18

#include "agenticdsl/contract/iprovider_factory.h"

#include <memory>

namespace agenticdsl {

/**
 * @brief 路由式 ILLMProvider 工厂
 *
 * 路由表 (基于 config.provider):
 *   - "mock" / "" (空/未识别) → MockProviderFactory → MockLLMProvider
 *   - "openai" / "anthropic" / "deepseek" / "qwen" / "moonshot" / "custom" → CloudProviderFactory → CloudLLMAdapter
 *   - "local" / "llama" → LlamaProviderFactory → LlamaAdapterProvider
 *   - 其他 → 兜底返回 MockLLMProvider (确保 engine 等 caller 永不收到 nullptr)
 *
 * 设计说明:
 *   - 内部持有 3 个 backend factory (mock_factory / cloud_factory / llama_factory)
 *   - 完整类型成员 (PIMPL 不必要, 因为 backend factory 都是 unique_ptr<IProviderFactory>)
 *   - 线程安全: create() 可并发调用 (内部各 factory 自身保证)
 */
class LLMProviderFactory : public IProviderFactory {
 public:
  LLMProviderFactory();
  ~LLMProviderFactory() override = default;

  std::unique_ptr<ILLMProvider> create(const LLMConfig& config) override;

 private:
  std::unique_ptr<IProviderFactory> mock_factory;
  std::unique_ptr<IProviderFactory> cloud_factory;
  std::unique_ptr<IProviderFactory> llama_factory;
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_LLM_LLM_PROVIDER_FACTORY_H
