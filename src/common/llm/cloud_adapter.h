#ifndef AGENTICDSL_LLM_CLOUD_ADAPTER_H
#define AGENTICDSL_LLM_CLOUD_ADAPTER_H

// 文件头注释
// 功能描述：云端 LLM 适配器（OpenAI 兼容协议 + Anthropic 作为 follow-up）
//          继承 ILLMProvider（流式、可取消），封装 httplib HTTP 调用与 SSE 解析
// 设计依据：track-01-cloud-llm.md M1.4、ADR-0001
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-07

#include "llm_config.h"
#include "llm_types.h"

#include <memory>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace agenticdsl {

/**
 * @brief 云端 LLM 适配器（统一 OpenAI 协议 + 后续扩展 Anthropic）
 *
 * 继承 ILLMProvider（流式、可取消），不复用旧版 ILLMAdapter。
 * 单类承载多 provider：OpenAI 协议占主流（DeepSeek/Qwen/月之暗面兼容），
 * 通过 config_.provider 字段在请求构造时分发 endpoint / header。
 *
 * 错误映射：
 * - 401 → AuthenticationError
 * - 429 → RateLimited（读取 Retry-After 头）
 * - 5xx → ServerError
 * - 4xx 其他 → InvalidRequest
 * - 网络失败 → NetworkError
 *
 * 重试：指数退避（500ms → 8s），±25% 抖动，最多 config_.max_retries 次。
 */
class CloudLLMAdapter : public ILLMProvider {
public:
  /**
   * @brief 构造时立即解析 API key（env > file > direct）
   * @param config LLM 统一配置
   */
  explicit CloudLLMAdapter(LLMConfig config);

  ~CloudLLMAdapter() override;

  // 禁止拷贝（持有 HTTP 客户端等资源）
  CloudLLMAdapter(const CloudLLMAdapter&) = delete;
  CloudLLMAdapter& operator=(const CloudLLMAdapter&) = delete;

  // === ILLMProvider 接口 ===
  Result<GenerationResult, LLMError>
      generate(const GenerationRequest& req, std::stop_token token) override;

  std::unique_ptr<IGenerationStream>
      generate_stream(const GenerationRequest& req,
                      std::stop_token token) override;

  // === 状态查询 ===
  /// API key 解析成功且 provider 已配置即为可用
  /// （非 override：ILLMProvider 基类暂未提供此查询，每个 adapter 自实现）
  bool is_available() const;

  /// 提供方名称（openai / anthropic / local）
  std::string provider_name() const { return config_.provider; }

  /// 模型名称
  std::string model_name() const { return config_.model; }

  // === 配置管理 ===
  /// 获取当前配置（只读）
  const LLMConfig& config() const { return config_; }

  /**
   * @brief 运行时更新配置（会重新解析 API key）
   * @param new_config 新配置
   */
  void update_config(const LLMConfig& new_config);

private:
  /**
   * @brief HTTP 响应封装（含网络错误情况）
   */
  struct HttpResponse {
    int status_code = 0;       ///< HTTP 状态码（0 表示无响应）
    std::string body;          ///< 响应体
    std::string error_message; ///< 网络错误时的错误信息
    bool network_error = false;///< 是否为网络失败（连接超时、解析失败等）
  };

  /**
   * @brief 发起 POST 请求
   * @param endpoint 端点路径（如 /v1/chat/completions）
   * @param json_body 请求体 JSON 字符串
   * @param stream 是否流式（设置 Accept: text/event-stream 头）
   * @param token 取消令牌
   * @return HTTP 响应（包含错误情况）
   */
  HttpResponse do_post(const std::string& endpoint,
                       const std::string& json_body,
                       bool stream,
                       std::stop_token token);

  /**
   * @brief 构造请求头（含 Authorization Bearer）
   * @param stream 是否流式
   * @return 头字段列表（key-value pair，避免在头文件中引入 httplib 类型）
   */
  std::vector<std::pair<std::string, std::string>>
      build_headers(bool stream) const;

  /**
   * @brief 构造 OpenAI 兼容请求体 JSON
   */
  std::string build_request_body(const GenerationRequest& req,
                                 bool stream) const;

  /**
   * @brief 映射 HTTP 状态码到 LLMError
   * @param retry_after_header 服务端 Retry-After 头值（空字符串表示无）
   */
  LLMError map_http_error(int status_code,
                          const std::string& body,
                          const std::string& retry_after_header) const;

  // === 内部状态 ===
  LLMConfig config_;            ///< 运行时配置
  std::string resolved_api_key_;///< 构造时解析的 API key

  // === 流实现（PIMPL forward）===
  class CloudGenerationStream;  // 实际实现在 .cpp 中
  friend class CloudGenerationStream;
};

} // namespace agenticdsl

#endif // AGENTICDSL_LLM_CLOUD_ADAPTER_H
