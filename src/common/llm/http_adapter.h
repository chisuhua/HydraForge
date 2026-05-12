#ifndef AGENTICDSL_LLM_HTTP_ADAPTER_H
#define AGENTICDSL_LLM_HTTP_ADAPTER_H

#include "llm_adapter.h"
#include <string>
#include <memory>

namespace agenticdsl {

class HttpLLMAdapter : public ILLMAdapter {
public:
    explicit HttpLLMAdapter(const LLMConfig& config);
    LLMResult generate(const std::string& prompt, const LLMConfig& params = {}) override;
    bool is_available() const override;
    std::string name() const override { return "http"; }

private:
    LLMConfig config_;
    LLMResult call_openai_compatible_api(const std::string& prompt, const LLMConfig& params);
};

} // namespace agenticdsl

#endif