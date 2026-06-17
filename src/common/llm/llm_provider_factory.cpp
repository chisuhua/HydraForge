#include "common/llm/llm_provider_factory.h"

#include "common/llm/cloud_adapter.h"        // CloudLLMAdapter (OpenAI 兼容协议)
#include "common/llm/llama_adapter_provider.h"  // LlamaAdapterProvider (本地 llama.cpp)
#include "common/llm/llm_config.h"          // LLMConfig
#include "common/llm/llm_types.h"            // ILLMProvider
#include "common/llm/mock_provider_factory.h"  // MockProviderFactory

#include <stdexcept>

namespace agenticdsl {

// === 内置 CloudProviderFactory (P1.T1 同步实现, 避免 YAGNI 多文件) ===
// 前移: Impl 构造时需要这些类型
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
    // min_p 不存在于 LlamaAdapter::Config, 跳过 (LLMConfig.min_p 默认 0.05f, 仅供云端使用)
    return std::make_unique<LlamaAdapterProvider>(llama_config);
  }
};

// PIMPL 实现 (前向类已定义, 现在可安全使用)
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

}  // namespace agenticdsl
