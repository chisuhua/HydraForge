#include "llama_adapter.h"
#include "http_adapter.h"
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

    HttpLLMAdapter http_adapter(LLMConfig{
        config_.api_url,
        config_.api_endpoint,
        config_.api_key,
        config_.model,
        config_.temperature,
        config_.n_predict,
        config_.n_ctx,
        config_.n_threads
    });

    auto result = http_adapter.generate(prompt, {});
    if (!result.success) {
        throw std::runtime_error("LLM generation failed: " + result.error);
    }
    return result.text;
}

bool LlamaAdapter::is_loaded() const {
    return loaded_;
}

}