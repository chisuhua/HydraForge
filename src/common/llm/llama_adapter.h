#ifndef AGENTICDSL_LLM_LLAMA_ADAPTER_H
#define AGENTICDSL_LLM_LLAMA_ADAPTER_H

#include <string>
#include <memory>
#include <vector>

namespace agenticdsl {

class [[deprecated("LlamaAdapter is deprecated; use pdk/llama_engine/ plugin, see ADR-0042 §2")]]
    LlamaAdapter {
public:
    struct Config {
        std::string api_url = "http://localhost:8080";
        std::string api_endpoint = "/v1/chat/completions";
        std::string api_key;
        std::string model = "gpt-3.5-turbo";
        int n_ctx = 2048;
        int n_threads = 4;
        float temperature = 0.7f;
        int n_predict = 512;
    };

    explicit LlamaAdapter(const Config& config);
    ~LlamaAdapter();

    std::string generate(const std::string& prompt);
    bool is_loaded() const;
    const Config& config() const { return config_; }

private:
    Config config_;
    bool loaded_ = false;
};

} // namespace agenticdsl

#endif