#ifndef AGENTICDSL_LLM_MOCK_PROVIDER_FACTORY_H
#define AGENTICDSL_LLM_MOCK_PROVIDER_FACTORY_H

// 文件头注释
// 功能描述：MockProviderFactory — 创建 MockLLMProvider 实例
//          Phase 1 P1 引擎解耦 (ADR-0019 §1.4 退出标准) — T1.3
//          包装 MockLLMProvider, 屏蔽 mock_provider.h 的 include 依赖
// 设计依据：ADR-0005 (LLM 后端配置与工厂) §3 + openspec/.../T1
// 作者：AgenticDSL Phase 1 P1.T1
// 最后修改日期：2026-06-18

#include "agenticdsl/contract/iprovider_factory.h"

namespace agenticdsl {

/**
 * @brief MockLLMProvider 工厂
 *
 * 实现 IProviderFactory::create(), 返回 MockLLMProvider 实例。
 * config 参数被忽略 (Mock 实现无需 LLMConfig)。
 * 线程安全: create() 可并发调用 (MockLLMProvider 自身不保证线程安全,
 *           但 factory 自身无状态)。
 */
class MockProviderFactory : public IProviderFactory {
 public:
  MockProviderFactory() = default;
  ~MockProviderFactory() override = default;

  std::unique_ptr<ILLMProvider> create(const LLMConfig& config) override;
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_LLM_MOCK_PROVIDER_FACTORY_H
