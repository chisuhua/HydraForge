// 文件头注释
// 功能描述：本地 HTTP LLM 适配器（与 llama.cpp server / OpenAI 兼容 endpoint 通信）
//          继承 ILLMProvider（流式、可取消），无认证头、无重试（本地场景）
// 设计依据：track-01-cloud-llm.md M1.3、ADR-0001
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-10

#ifndef AGENTICDSL_LLM_HTTP_ADAPTER_H
#define AGENTICDSL_LLM_HTTP_ADAPTER_H

#include "llm_types.h"
#include "llm_config.h"

#include <memory>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace agenticdsl {

/**
 * @brief 本地 HTTP LLM 适配器（llama.cpp server / OpenAI 兼容 endpoint）
 *
 * 与 CloudLLMAdapter 的区别：
 * - 无 Authorization 头（本地服务通常不需要认证）
 * - 无重试（本地网络稳定，失败应直接报错）
 * - 端点路径从 config_.api_url 解析（保持与 LlamaAdapter::Config 兼容）
 *
 * 错误映射：
 * - 401/403 → AuthenticationError
 * - 429 → RateLimited
 * - 5xx → ServerError
 * - 4xx 其他 → InvalidRequest
 * - 网络失败 → NetworkError
 */
class HttpLLMAdapter : public ILLMProvider {
public:
  /**
   * @brief 构造时记录配置
   * @param config 统一 LLM 配置（api_url 指向本地 endpoint）
   */
  explicit HttpLLMAdapter(LLMConfig config);

  ~HttpLLMAdapter() override;

  // 禁止拷贝（持有 HTTP 客户端等资源）
  HttpLLMAdapter(const HttpLLMAdapter&) = delete;
  HttpLLMAdapter& operator=(const HttpLLMAdapter&) = delete;

  // === ILLMProvider 接口 ===

  /**
   * @brief 同步生成
   * @param req  生成请求
   * @param token 取消 token（默认空，即不可取消）
   * @return 成功时返回 GenerationResult，失败时返回 LLMError
   */
  Result<GenerationResult, LLMError>
      generate(const GenerationRequest& req, std::stop_token token) override;

  /**
   * @brief 流式生成（同步拉取完整响应后按 8 字符 chunk 切分）
   *
   * httplib 同步 API 不支持真正的增量流传输；本实现采用 lazy chunk 切分。
   *
   * @param req  生成请求
   * @param token 取消 token
   * @return stream handle
   */
  std::unique_ptr<IGenerationStream>
      generate_stream(const GenerationRequest& req,
                      std::stop_token token) override;

  // === 状态查询 ===

  /// 本地 HTTP 服务通常假设可用，配置非空即视为可用
  bool is_available() const;

  /// 提供方名称
  std::string provider_name() const { return config_.provider; }

  /// 获取当前配置（只读）
  const LLMConfig& config() const { return config_; }

  // === REQ-ICC-004: available_models() override ===
  std::vector<ModelInfo> available_models() const override;

private:
  /**
   * @brief 发起 POST 请求
   * @param endpoint 端点路径（如 /v1/chat/completions）
   * @param json_body 请求体 JSON 字符串
   * @param token 取消令牌
   * @return HTTP 响应（包含错误情况）
   */
  struct HttpResponse {
    int status_code = 0;       ///< HTTP 状态码（0 表示无响应）
    std::string body;          ///< 响应体
    std::string error_message; ///< 网络错误时的错误信息
    bool network_error = false;///< 是否为网络失败
  };

  HttpResponse do_post(const std::string& endpoint,
                       const std::string& json_body,
                       std::stop_token token);

  /**
   * @brief 构造请求头（无认证）
   */
  std::vector<std::pair<std::string, std::string>> build_headers() const;

  /**
   * @brief 构造 OpenAI 兼容请求体 JSON
   */
  std::string build_request_body(const GenerationRequest& req) const;

  /**
   * @brief 映射 HTTP 状态码到 LLMError
   */
  LLMError map_http_error(int status_code,
                          const std::string& body) const;

  LLMConfig config_;

  // === 流实现（PIMPL forward）===
  class HttpGenerationStream;
  friend class HttpGenerationStream;
};

} // namespace agenticdsl

#endif // AGENTICDSL_LLM_HTTP_ADAPTER_H
