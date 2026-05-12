#include "http_adapter.h"
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <httplib.h>

namespace agenticdsl {

HttpLLMAdapter::HttpLLMAdapter(const LLMConfig& config) : config_(config) {
}

LLMResult HttpLLMAdapter::generate(const std::string& prompt, const LLMConfig& params) {
    return call_openai_compatible_api(prompt, params);
}

bool HttpLLMAdapter::is_available() const {
    return true;
}

LLMResult HttpLLMAdapter::call_openai_compatible_api(const std::string& prompt, const LLMConfig& params) {
    LLMResult result;

    httplib::Client cli(config_.api_url);
    std::string endpoint = config_.api_endpoint.empty() ? "/v1/chat/completions" : config_.api_endpoint;
    std::string model = params.model.empty() ? config_.model : params.model;
    float temperature = params.temperature > 0 ? params.temperature : config_.temperature;
    int max_tokens = params.max_tokens > 0 ? params.max_tokens : config_.max_tokens;

    nlohmann::json request_body = {
        {"model", model},
        {"messages", {{{"role", "user"}, {"content", prompt}}}},
        {"temperature", temperature},
        {"max_tokens", max_tokens}
    };

    auto response = cli.Post(endpoint.c_str(),
        request_body.dump(),
        "application/json");

    if (!response || response->status != 200) {
        result.error = response ? "HTTP " + std::to_string(response->status) : "Connection failed";
        return result;
    }

    try {
        auto response_json = nlohmann::json::parse(response->body);
        result.text = response_json["choices"][0]["message"]["content"];
        result.success = true;
        result.tokens_generated = response_json.value("usage", nlohmann::json{}).value("completion_tokens", 0);
    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }

    return result;
}

}