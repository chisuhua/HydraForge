#ifndef AGENTICDSL_LLM_LLM_ADAPTER_H
#define AGENTICDSL_LLM_LLM_ADAPTER_H

#include <string>
#include <memory>
#include <functional>

namespace agenticdsl {

struct LLMConfig {
    std::string api_url = "http://localhost:8080";
    std::string api_endpoint = "/v1/chat/completions";
    std::string api_key;
    std::string model = "gpt-3.5-turbo";
    float temperature = 0.7f;
    int max_tokens = 512;
    int n_ctx = 2048;
    int n_threads = 4;
};

struct LLMResult {
    bool success = false;
    std::string text;
    std::string error;
    int tokens_generated = 0;
};

class ILLMAdapter {
public:
    virtual ~ILLMAdapter() = default;
    virtual LLMResult generate(const std::string& prompt, const LLMConfig& params = {}) = 0;
    virtual bool is_available() const = 0;
    virtual std::string name() const = 0;
};

using LLMAdapterFactory = std::function<std::unique_ptr<ILLMAdapter>(const LLMConfig&)>;

} // namespace agenticdsl

#endif