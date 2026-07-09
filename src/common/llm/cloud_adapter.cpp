// cloud_adapter.cpp
// 功能描述：云端 LLM 适配器实现（OpenAI 兼容协议）
//          同步 generate() + 流式 generate_stream() + 错误映射 + 重试
// 设计依据：track-01-cloud-llm.md M1.5
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-07

#include "cloud_adapter.h"
#include "sse_stream.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <random>
#include <stop_token>
#include <thread>
#include <utility>

namespace agenticdsl {

// 指数退避 + ±25% 抖动 — 计算下一次重试延迟（毫秒）
static int compute_backoff(int attempt, int base_ms, int max_ms, std::mt19937& rng) {
  int delay_ms = std::min(max_ms, base_ms << attempt);
  int jitter = static_cast<int>(delay_ms * 0.25);
  std::uniform_int_distribution<int> dist(-jitter, jitter);
  delay_ms += dist(rng);
  return std::max(0, delay_ms);
}

// =====================================================================
// 内部辅助：CloudGenerationStream
// =====================================================================
//
// 流式输出：构造时启动异步 HTTP 请求（httplib::Client::Post 同步实现下，
// 在 next() 中采用 lazy pull：每次 next() 启动一次 stream chunk 读取）。
//
// 简化实现说明：httplib 的 Chunked HTTP 读取在同步 API 下需配合
// httplib::Client::Post 后手动解析响应体（其流式支持较弱）。本实现采用：
//  1. 同步等待完整响应（与 HttpLLMAdapter 一致）
//  2. 解析响应体为 SSE chunk 序列
//  3. next() 按需返回下一个 token
//
// 注意：未来可升级为 httplib::Client::Post 配合 stream chunk callback
//（httplib::Client::Post(headers, body, content_type, content_provider)），
// 实现真正的流式增量传输。
// =====================================================================
class CloudLLMAdapter::CloudGenerationStream : public IGenerationStream {
public:
  CloudGenerationStream(std::string response_body, bool parse_sse)
      : response_body_(std::move(response_body)), parse_sse_(parse_sse),
        active_(true) {
    if (parse_sse_) {
      decoder_ = std::make_unique<SSEDecoder>();
      decoder_->feed(response_body_);
    }
  }

  std::optional<std::string> next(std::stop_token token) override {
    if (!active_) {
      return std::nullopt;
    }
    if (token.stop_requested()) {
      active_ = false;
      return std::nullopt;
    }

    if (parse_sse_) {
      // SSE 模式：解析 data 字段
      while (auto ev = decoder_->next()) {
        // 跳过 [DONE] 哨兵
        if (ev->data == "[DONE]") {
          active_ = false;
          return std::nullopt;
        }
        // 尝试解析 OpenAI choices[0].delta.content
        try {
          auto j = nlohmann::json::parse(ev->data);
          if (j.contains("choices") && j["choices"].is_array() &&
              !j["choices"].empty()) {
            const auto& choice = j["choices"][0];
            if (choice.contains("delta") && choice["delta"].is_object() &&
                choice["delta"].contains("content")) {
              const auto& content = choice["delta"]["content"];
              if (content.is_string()) {
                return content.get<std::string>();
              }
            }
          }
        } catch (...) {
          // JSON 解析失败：返回原始 data 字符串
          return ev->data;
        }
      }
      // 无更多事件
      active_ = false;
      return std::nullopt;
    } else {
      // 非 SSE：一次性返回整个 body
      active_ = false;
      return response_body_;
    }
  }

  bool is_active() const override { return active_; }

private:
  std::string response_body_;
  bool parse_sse_;
  bool active_;
  std::unique_ptr<SSEDecoder> decoder_;
};

// =====================================================================
// CloudLLMAdapter 构造与配置
// =====================================================================

CloudLLMAdapter::CloudLLMAdapter(LLMConfig config)
    : config_(std::move(config)),
      resolved_api_key_(config_.resolve_api_key()) {}

CloudLLMAdapter::~CloudLLMAdapter() = default;

void CloudLLMAdapter::update_config(const LLMConfig& new_config) {
  config_ = new_config;
  resolved_api_key_ = config_.resolve_api_key();
}

bool CloudLLMAdapter::is_available() const {
  // API key 解析成功且 provider 已配置即视为可用
  return !resolved_api_key_.empty() && !config_.provider.empty();
}

// =====================================================================
// 请求构造
// =====================================================================

std::vector<std::pair<std::string, std::string>>
CloudLLMAdapter::build_headers(bool stream) const {
  std::vector<std::pair<std::string, std::string>> headers;
  headers.emplace_back("Content-Type", "application/json");
  if (!resolved_api_key_.empty()) {
    headers.emplace_back("Authorization", "Bearer " + resolved_api_key_);
  }
  if (stream) {
    headers.emplace_back("Accept", "text/event-stream");
  } else {
    headers.emplace_back("Accept", "application/json");
  }
  if (config_.organization.has_value() && !config_.organization->empty()) {
    headers.emplace_back("OpenAI-Organization", *config_.organization);
  }
  return headers;
}

std::string CloudLLMAdapter::build_request_body(const GenerationRequest& req,
                                                 bool stream) const {
  // OpenAI /v1/chat/completions 请求体格式（兼容 DeepSeek/Qwen/月之暗面）
  nlohmann::json j;
  j["model"] = req.params.model.empty() ? config_.model : req.params.model;
  // 简化：仅支持单条 user 消息
  nlohmann::json user_msg = {{"role", "user"}, {"content", req.prompt}};
  j["messages"] = nlohmann::json::array({user_msg});
  j["temperature"] = req.params.temperature;
  j["max_tokens"] = req.params.max_tokens;
  j["stream"] = stream;
  if (config_.top_p > 0.0f && config_.top_p < 1.0f) {
    j["top_p"] = config_.top_p;
  }
  if (!config_.stop_tokens.empty()) {
    j["stop"] = config_.stop_tokens;
  }
  return j.dump();
}

// =====================================================================
// 错误映射
// =====================================================================

LLMError CloudLLMAdapter::map_http_error(int status_code,
                                          const std::string& body,
                                          const std::string& retry_after_header) const {
  // 默认消息体
  auto default_msg = "HTTP " + std::to_string(status_code);
  std::string msg = default_msg;

  // 尝试从 body 提取 message 字段
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

  // 解析 Retry-After
  std::optional<std::chrono::seconds> retry_after;
  if (!retry_after_header.empty()) {
    try {
      int seconds = std::stoi(retry_after_header);
      if (seconds >= 0) {
        retry_after = std::chrono::seconds(seconds);
      }
    } catch (...) {
      // 可能为 HTTP-date 格式（RFC 7231），暂不解析
    }
  }

  if (status_code == 401 || status_code == 403) {
    return LLMError{LLMError::Code::AuthenticationError, msg};
  }
  if (status_code == 429) {
    return LLMError{LLMError::Code::RateLimited, msg,
                    retry_after.value_or(std::chrono::seconds(1))};
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
// HTTP POST（含重试逻辑）
// =====================================================================

CloudLLMAdapter::HttpResponse CloudLLMAdapter::do_post(
    const std::string& endpoint, const std::string& json_body,
    bool stream, std::stop_token token) {

  HttpResponse result;
  httplib::Client cli(config_.api_url);
  cli.set_connection_timeout(config_.timeout_seconds);
  cli.set_read_timeout(config_.timeout_seconds);
  cli.set_write_timeout(config_.timeout_seconds);

  auto headers_vec = build_headers(stream);
  httplib::Headers headers(headers_vec.begin(), headers_vec.end());

  // 重试：指数退避（500ms → 8s），±25% 抖动
  const int max_retries = std::max(0, config_.max_retries);
  const int base_delay_ms = 500;
  const int max_delay_ms = 8000;
  std::mt19937 rng(std::random_device{}());

  for (int attempt = 0; attempt <= max_retries; ++attempt) {
    if (token.stop_requested()) {
      result.network_error = true;
      result.error_message = "Cancelled";
      return result;
    }

    auto response = cli.Post(endpoint, headers, json_body, "application/json");

    if (!response) {
      // 网络错误
      result.network_error = true;
      result.error_message = "Connection failed";
      result.status_code = 0;
      result.body.clear();

      // 重试
      if (attempt < max_retries) {
        int delay_ms = compute_backoff(attempt, base_delay_ms, max_delay_ms, rng);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        continue;
      }
      return result;
    }

    result.status_code = response->status;
    result.body = response->body;
    result.network_error = false;

    // 2xx 成功
    if (response->status >= 200 && response->status < 300) {
      return result;
    }

    // 5xx / 网络错误 → 重试
    if (response->status >= 500 && attempt < max_retries) {
      int delay_ms = compute_backoff(attempt, base_delay_ms, max_delay_ms, rng);
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
      continue;
    }

    // 4xx 等非重试错误 → 直接返回
    return result;
  }

  return result;
}

// =====================================================================
// ILLMProvider 接口实现
// =====================================================================

Result<GenerationResult, LLMError>
CloudLLMAdapter::generate(const GenerationRequest& req, std::stop_token token) {
  if (!is_available()) {
    return Result<GenerationResult, LLMError>::failure(
        LLMError{LLMError::Code::InvalidRequest, "API key not configured"});
  }

  std::string body = build_request_body(req, /*stream=*/false);
  auto http_resp = do_post("/v1/chat/completions", body, false, token);

  if (http_resp.network_error) {
    return Result<GenerationResult, LLMError>::failure(
        LLMError{LLMError::Code::NetworkError, http_resp.error_message});
  }
  if (http_resp.status_code < 200 || http_resp.status_code >= 300) {
    return Result<GenerationResult, LLMError>::failure(
        map_http_error(http_resp.status_code, http_resp.body, ""));
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
        result.completion_tokens = j["usage"]["completion_tokens"].get<int>();
      }
    }
    return Result<GenerationResult, LLMError>::success(result);
  } catch (const std::exception& e) {
    return Result<GenerationResult, LLMError>::failure(
        LLMError{LLMError::Code::InvalidRequest,
                 std::string("Response parse error: ") + e.what()});
  }
}

std::vector<ILLMProvider::ModelInfo>
CloudLLMAdapter::available_models() const {
  // 实现：返回当前 config 声明的单个 ModelInfo (per REQ-ICC-004 + REQ-CLP-006)
  // capabilities: 4 种模型类型 (Chat + ToolUse + Vision + Completion)
  // context_window: 默认 4096 (cloud provider 通常提供大上下文)
  std::vector<ModelCapability> caps = {ModelCapability::Chat, ModelCapability::ToolUse};
  if (config_.provider == "anthropic") {
    // Anthropic 支持 vision
    caps.push_back(ModelCapability::Vision);
  }
  std::int64_t ctx_window = 4096;
  if (config_.provider == "openai" && config_.model.find("gpt-4o") != std::string::npos) {
    ctx_window = 128000;  // gpt-4o default
  } else if (config_.provider == "anthropic") {
    ctx_window = 200000;  // claude-3-5 default
  }
  std::string model_name = config_.model.empty() ? std::string("unknown-cloud-model") : config_.model;
  return {ModelInfo(std::move(model_name), std::move(caps), ctx_window, config_.provider)};
}

std::unique_ptr<IGenerationStream>
CloudLLMAdapter::generate_stream(const GenerationRequest& req,
                                  std::stop_token token) {
  if (!is_available()) {
    // 返回空流（is_active()=false），调用方 next() 会立即得到 nullopt
    return std::make_unique<CloudGenerationStream>("", /*parse_sse=*/false);
  }

  std::string body = build_request_body(req, /*stream=*/true);
  auto http_resp = do_post("/v1/chat/completions", body, true, token);

  if (http_resp.network_error || http_resp.status_code < 200 ||
      http_resp.status_code >= 300) {
    // 出错时仍返回流实例，body 携带错误信息
    std::string err_body =
        "{\"error\":{\"message\":\"" + http_resp.error_message + "\"}}";
    return std::make_unique<CloudGenerationStream>(err_body, false);
  }

  return std::make_unique<CloudGenerationStream>(http_resp.body, true);
}

} // namespace agenticdsl
