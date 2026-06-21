// src/common/llm/factory.h
// Sprint 6 P2-7: LLM 工厂函数
#pragma once

#include "agenticdsl/contract/iprovider_factory.h"

#include <memory>

namespace agenticdsl::llm {

std::unique_ptr<IProviderFactory> create_provider_factory();

}  // namespace agenticdsl::llm

