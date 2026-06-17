#include "common/llm/llm_provider_factory.h"

#include "common/llm/cloud_adapter.h"        // CloudLLMAdapter (OpenAI 兼容协议)
#include "common/llm/llama_adapter_provider.h"  // LlamaAdapterProvider (本地 llama.cpp)
#include "common/llm/llm_config.h"          // LLMConfig
#include "common/llm/llm_types.h"            // ILLMProvider
#include "common/llm/mock_provider_factory.h"  // MockProviderFactory

#include <stdexcept>

namespace agenticdsl {

// PIMPL 实现
struct LLMProviderFactory::Impl {
  std::unique_ptr<IProviderFactory> mock_factory;
  std::unique_ptr<IProviderFactory> cloud_factory;
  std::unique_ptr<IProviderFactory> llama_factory;

  Impl()
      : mock_factory(std::make_unique<MockProviderFactory>()),
        cloud_factory(std::make_unique<CloudProviderFactory>()),
        llama_factory(std::make_unique<LlamaProviderFactory>()) {}
};

LLMProviderFactory::LLMProviderFactory() : impl_(std::make_unique<Impl>()) {}

std::unique_ptr<ILLMProvider> LLMProviderFactory::create(const LLMConfig& config) {
  // 路由表: 基于 config.provider
  const std::string& backend = config.provider;

  if (backend == "mock" || backend.empty()) {
    return impl_->mock_factory->create(config);
  }

  // OpenAI 兼容协议: openai / anthropic / deepseek / qwen / moonshot 等
  // 通过 config.api_url 区分具体 endpoint
  if (backend == "openai" || backend == "anthropic" ||
      backend == "deepseek" || backend == "qwen" ||
      backend == "moonshot" || backend == "custom") {
    return impl_->cloud_factory->create(config);
  }

  // 本地 llama.cpp
  if (backend == "local" || backend == "llama") {
    return impl_->llama_factory->create(config);
  }

  // 未识别的 provider — 返回 nullptr (调用方需检查)
  return nullptr;
}

// === 内置 CloudProviderFactory (P1.T1 同步实现, 避免 YAGNI 多文件) ===

class CloudProviderFactory : public IProviderFactory {
 public:
  std::unique_ptr<ILLMProvider> create(const LLMConfig& config) override {
    // CloudLLMAdapter 接受 LLMConfig (v2 关键认知: cloud_adapter.h:44)
    return std::make_unique<CloudLLMAdapter>(config);
  }
};

// === 内置 LlamaProviderFactory (P1.T1 同步实现) ===

class LlamaProviderFactory : public IProviderFactory {
 public:
  std::unique_ptr<ILLMProvider> create(const LLMConfig& config) override {
    // LlamaAdapterProvider 接受 LlamaAdapter::Config (与 LLMConfig 字段不同)
    // 需要从 LLMConfig 映射到 LlamaAdapter::Config
    LlamaAdapter::Config llama_config;
    llama_config.model_path = config.model;       // 模型路径 (LLMConfig.model 当 path 用)
    llama_config.n_ctx = config.n_ctx;
    llama_config.n_threads = config.n_threads;
    llama_config.temperature = config.temperature;
    llama_config.min_p = config.min_p;
    llama_config.n_predict = config.max_tokens;   // max_tokens → n_predict
    return std::make_unique<LlamaAdapterProvider>(llama_config);
  }
};

}  // namespace agenticdsl
