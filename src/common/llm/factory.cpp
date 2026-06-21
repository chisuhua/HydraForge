// src/common/llm/factory.cpp
// Sprint 6 P2-7: LLMProviderFactory 工厂实现
#include "common/llm/factory.h"
#include "common/llm/llm_provider_factory.h"

namespace agenticdsl::llm {

std::unique_ptr<IProviderFactory> create_provider_factory() {
  return std::make_unique<LLMProviderFactory>();
}

}  // namespace agenticdsl::llm
