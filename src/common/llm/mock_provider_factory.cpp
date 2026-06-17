#include "common/llm/mock_provider_factory.h"

#include "common/llm/mock_provider.h"  // MockLLMProvider 完整类型 (仅 .cpp 可见)

namespace agenticdsl {

std::unique_ptr<ILLMProvider> MockProviderFactory::create(const LLMConfig& /*config*/) {
  // Mock 实现忽略 config — MockLLMProvider 默认无参构造
  return std::make_unique<MockLLMProvider>();
}

}  // namespace agenticdsl
