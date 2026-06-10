#include "llama_adapter.h"
#include "http_adapter.h"
#include "llm_config.h"

#include <stdexcept>
#include <vector>
#include <cstdlib>

namespace agenticdsl {

LlamaAdapter::LlamaAdapter(const Config& config) : config_(config), loaded_(false) {
}

LlamaAdapter::~LlamaAdapter() = default;

std::string LlamaAdapter::generate(const std::string& prompt) {
    if (!is_loaded()) {
        throw std::runtime_error("LLM adapter not loaded");
    }

    // 构造统一 LLMConfig (HttpLLMAdapter 现在使用 ILLMProvider 接口)
    LLMConfig http_config;
    http_config.provider = "local";
    http_config.api_url = config_.api_url;
    http_config.api_endpoint = config_.api_endpoint;
    http_config.api_key = config_.api_key;
    http_config.model = config_.model;
    http_config.temperature = config_.temperature;
    http_config.max_tokens = config_.n_predict;
    http_config.n_ctx = config_.n_ctx;
    http_config.n_threads = config_.n_threads;

    HttpLLMAdapter http_adapter(http_config);

    GenerationRequest req(prompt);
    req.params = http_config;

    auto result = http_adapter.generate(req, std::stop_token{});
    if (!result.has_value()) {
        throw std::runtime_error("LLM generation failed: " +
                                 result.error().message);
    }
    return result.value().text;
}

bool LlamaAdapter::is_loaded() const {
    return loaded_;
}

}