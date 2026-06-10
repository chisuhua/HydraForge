// http_adapter.cpp
// 功能描述：本地 HTTP LLM 适配器实现（llama.cpp server / OpenAI 兼容 endpoint）
//          同步 generate() + 流式 generate_stream() + 错误映射（无重试、无认证）
// 设计依据：track-01-cloud-llm.md M1.3、ADR-0001
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-10

#include "http_adapter.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <stop_token>
#include <utility>

namespace agenticdsl {

// =====================================================================
// 内部流实现：同步拉取完整响应，按 8 字符 chunk 切分模拟流式
// =====================================================================
class HttpLLMAdapter::HttpGenerationStream : public IGenerationStream {
public:
    explicit HttpGenerationStream(std::string text)
        : text_(std::move(text)), pos_(0), active_(true) {}

    // 错误流构造：立即 inactive，通过 error() 报告
    explicit HttpGenerationStream(LLMError err)
        : error_(std::move(err)), pos_(0), active_(false) {}

    std::optional<std::string> next(std::stop_token token) override {
        if (!active_) {
            return std::nullopt;
        }
        if (token.stop_requested()) {
            active_ = false;
            return std::nullopt;
        }
        if (pos_ >= text_.size()) {
            active_ = false;
            return std::nullopt;
        }
        // 按 8 字符切分 chunk（与 LlamaAdapterProvider 一致）
        constexpr size_t chunk_size = 8;
        size_t remaining = text_.size() - pos_;
        size_t this_chunk = (remaining < chunk_size) ? remaining : chunk_size;
        std::string chunk = text_.substr(pos_, this_chunk);
        pos_ += this_chunk;
        return chunk;
    }

    bool is_active() const override { return active_; }
    std::optional<LLMError> error() const override { return error_; }

private:
    std::string text_;
    std::optional<LLMError> error_;
    size_t pos_;
    bool active_;
};

// =====================================================================
// 构造与配置
// =====================================================================

HttpLLMAdapter::HttpLLMAdapter(LLMConfig config) : config_(std::move(config)) {}

HttpLLMAdapter::~HttpLLMAdapter() = default;

bool HttpLLMAdapter::is_available() const {
    // 本地场景：api_url 非空即视为可用（实际可用性由运行时 HTTP 决定）
    return !config_.api_url.empty();
}

// =====================================================================
// 请求构造
// =====================================================================

std::vector<std::pair<std::string, std::string>>
HttpLLMAdapter::build_headers() const {
    std::vector<std::pair<std::string, std::string>> headers;
    headers.emplace_back("Content-Type", "application/json");
    headers.emplace_back("Accept", "application/json");
    return headers;
}

std::string HttpLLMAdapter::build_request_body(const GenerationRequest& req) const {
    std::string model = req.params.model.empty() ? config_.model : req.params.model;
    float temperature = req.params.temperature > 0
                            ? req.params.temperature
                            : config_.temperature;
    int max_tokens = req.params.max_tokens > 0
                         ? req.params.max_tokens
                         : config_.max_tokens;

    nlohmann::json j;
    j["model"] = model;
    j["messages"] = nlohmann::json::array(
        {{{"role", "user"}, {"content", req.prompt}}});
    j["temperature"] = temperature;
    j["max_tokens"] = max_tokens;
    return j.dump();
}

// =====================================================================
// 错误映射
// =====================================================================

LLMError HttpLLMAdapter::map_http_error(int status_code,
                                        const std::string& body) const {
    std::string msg = "HTTP " + std::to_string(status_code);

    // 尝试从 body 提取 message 字段（OpenAI 错误格式）
    try {
        if (!body.empty()) {
            auto j = nlohmann::json::parse(body);
            if (j.contains("error") && j["error"].is_object() &&
                j["error"].contains("message")) {
                msg = j["error"]["message"].get<std::string>();
            } else if (j.contains("message")) {
                msg = j["message"].get<std::string>();
            }
        }
    } catch (...) {
        // body 不是 JSON：保留默认消息
    }

    if (status_code == 401 || status_code == 403) {
        return LLMError{LLMError::Code::AuthenticationError, msg};
    }
    if (status_code == 429) {
        return LLMError{LLMError::Code::RateLimited, msg,
                        std::chrono::seconds(1)};
    }
    if (status_code >= 500 && status_code < 600) {
        return LLMError{LLMError::Code::ServerError, msg};
    }
    if (status_code >= 400 && status_code < 500) {
        return LLMError{LLMError::Code::InvalidRequest, msg};
    }
    return LLMError{LLMError::Code::Unknown, msg};
}

// =====================================================================
// HTTP POST（无重试）
// =====================================================================

HttpLLMAdapter::HttpResponse HttpLLMAdapter::do_post(
    const std::string& endpoint, const std::string& json_body,
    std::stop_token token) {

    HttpResponse result;
    httplib::Client cli(config_.api_url);
    cli.set_connection_timeout(config_.timeout_seconds);
    cli.set_read_timeout(config_.timeout_seconds);
    cli.set_write_timeout(config_.timeout_seconds);

    auto headers_vec = build_headers();
    httplib::Headers headers(headers_vec.begin(), headers_vec.end());

    if (token.stop_requested()) {
        result.network_error = true;
        result.error_message = "Cancelled";
        return result;
    }

    auto response = cli.Post(endpoint, headers, json_body, "application/json");

    if (!response) {
        result.network_error = true;
        result.error_message = "Connection failed";
        return result;
    }

    result.status_code = response->status;
    result.body = response->body;
    result.network_error = false;
    return result;
}

// =====================================================================
// ILLMProvider 接口实现
// =====================================================================

Result<GenerationResult, LLMError>
HttpLLMAdapter::generate(const GenerationRequest& req, std::stop_token token) {
    if (!is_available()) {
        return Result<GenerationResult, LLMError>::failure(
            LLMError{LLMError::Code::InvalidRequest, "api_url not configured"});
    }

    std::string body = build_request_body(req);
    std::string endpoint = config_.api_endpoint.empty()
                               ? "/v1/chat/completions"
                               : config_.api_endpoint;
    auto http_resp = do_post(endpoint, body, token);

    if (http_resp.network_error) {
        return Result<GenerationResult, LLMError>::failure(
            LLMError{LLMError::Code::NetworkError, http_resp.error_message});
    }
    if (http_resp.status_code < 200 || http_resp.status_code >= 300) {
        return Result<GenerationResult, LLMError>::failure(
            map_http_error(http_resp.status_code, http_resp.body));
    }

    // 解析 OpenAI 响应
    try {
        auto j = nlohmann::json::parse(http_resp.body);
        GenerationResult result;
        if (j.contains("choices") && j["choices"].is_array() &&
            !j["choices"].empty()) {
            const auto& choice = j["choices"][0];
            if (choice.contains("message") && choice["message"].is_object() &&
                choice["message"].contains("content")) {
                result.text = choice["message"]["content"].get<std::string>();
            }
            if (choice.contains("finish_reason")) {
                result.finish_reason = choice["finish_reason"].get<std::string>();
            }
        }
        if (j.contains("usage") && j["usage"].is_object()) {
            if (j["usage"].contains("prompt_tokens")) {
                result.prompt_tokens = j["usage"]["prompt_tokens"].get<int>();
            }
            if (j["usage"].contains("completion_tokens")) {
                result.completion_tokens =
                    j["usage"]["completion_tokens"].get<int>();
            }
        }
        return Result<GenerationResult, LLMError>::success(result);
    } catch (const std::exception& e) {
        return Result<GenerationResult, LLMError>::failure(
            LLMError{LLMError::Code::InvalidRequest,
                     std::string("Response parse error: ") + e.what()});
    }
}

std::unique_ptr<IGenerationStream>
HttpLLMAdapter::generate_stream(const GenerationRequest& req,
                                std::stop_token token) {
    if (!is_available()) {
        LLMError err{LLMError::Code::InvalidRequest, "api_url not configured"};
        return std::make_unique<HttpGenerationStream>(std::move(err));
    }

    std::string body = build_request_body(req);
    std::string endpoint = config_.api_endpoint.empty()
                               ? "/v1/chat/completions"
                               : config_.api_endpoint;
    auto http_resp = do_post(endpoint, body, token);

    if (http_resp.network_error) {
        LLMError err{LLMError::Code::NetworkError, http_resp.error_message};
        return std::make_unique<HttpGenerationStream>(std::move(err));
    }
    if (http_resp.status_code < 200 || http_resp.status_code >= 300) {
        LLMError err = map_http_error(http_resp.status_code, http_resp.body);
        return std::make_unique<HttpGenerationStream>(std::move(err));
    }

    // 成功：提取文本，构造流
    try {
        auto j = nlohmann::json::parse(http_resp.body);
        std::string text;
        if (j.contains("choices") && j["choices"].is_array() &&
            !j["choices"].empty()) {
            const auto& choice = j["choices"][0];
            if (choice.contains("message") && choice["message"].is_object() &&
                choice["message"].contains("content")) {
                text = choice["message"]["content"].get<std::string>();
            }
        }
        return std::make_unique<HttpGenerationStream>(std::move(text));
    } catch (const std::exception& e) {
        LLMError err{LLMError::Code::InvalidRequest,
                     std::string("Response parse error: ") + e.what()};
        return std::make_unique<HttpGenerationStream>(std::move(err));
    }
}

} // namespace agenticdsl
