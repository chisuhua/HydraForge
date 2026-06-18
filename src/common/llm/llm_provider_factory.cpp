#include "common/llm/llm_provider_factory.h"

#include "common/llm/cloud_adapter.h"        // CloudLLMAdapter (OpenAI 兼容协议)
#include "common/llm/llama_adapter_provider.h"  // LlamaAdapterProvider (本地 llama.cpp)
#include "common/llm/llm_config.h"          // LLMConfig
#include "common/llm/llm_types.h"            // ILLMProvider
#include "common/llm/mock_provider_factory.h"  // MockProviderFactory

#include <stdexcept>

namespace agenticdsl {

// 内置 CloudProviderFactory (P1.T1: 避免 YAGNI 多文件)
class CloudProviderFactory : public IProviderFactory {
 public:
  std::unique_ptr<ILLMProvider> create(const LLMConfig& config) override {
    return std::make_unique<CloudLLMAdapter>(config);
  }
};

// 内置 LlamaProviderFactory (P1.T1: LLMConfig → LlamaAdapter::Config 字段映射)
class LlamaProviderFactory : public IProviderFactory {
 public:
  std::unique_ptr<ILLMProvider> create(const LLMConfig& config) override {
    // LlamaAdapter::Config 实际字段: api_url/api_endpoint/api_key/model/n_ctx/n_threads/temperature/n_predict
    // (无 model_path / min_p 字段)
    LlamaAdapter::Config llama_config;
    llama_config.api_url = config.api_url;
    llama_config.api_endpoint = config.api_endpoint;
    llama_config.api_key = config.resolve_api_key();  // 优先 env > file > 字段
    llama_config.model = config.model;
    llama_config.n_ctx = config.n_ctx;
    llama_config.n_threads = config.n_threads;
    llama_config.temperature = config.temperature;
    llama_config.n_predict = config.max_tokens;       // max_tokens → n_predict
    return std::make_unique<LlamaAdapterProvider>(llama_config);
  }
};

LLMProviderFactory::LLMProviderFactory()
    : mock_factory(std::make_unique<MockProviderFactory>()),
      cloud_factory(std::make_unique<CloudProviderFactory>()),
      llama_factory(std::make_unique<LlamaProviderFactory>()) {}

std::unique_ptr<ILLMProvider> LLMProviderFactory::create(const LLMConfig& config) {
  const std::string& backend = config.provider;

  if (backend == "mock" || backend.empty()) {
    return mock_factory->create(config);
  }

  // OpenAI 兼容协议: openai / anthropic / deepseek / qwen / moonshot / custom
  if (backend == "openai" || backend == "anthropic" ||
      backend == "deepseek" || backend == "qwen" ||
      backend == "moonshot" || backend == "custom") {
    return cloud_factory->create(config);
  }

  // 本地 llama.cpp
  if (backend == "local" || backend == "llama") {
    return llama_factory->create(config);
  }

  return nullptr;
}

}  // namespace agenticdsl
