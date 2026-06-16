#ifndef AGENTICDSL_LLM_MOCK_PROVIDER_H
#define AGENTICDSL_LLM_MOCK_PROVIDER_H

// 文件头注释
// 功能描述：离线测试用 Mock LLM Provider
//          支持队列模式 / 固定响应模式 / 错误注入 / 延迟模拟
//          用于单元测试与 CI 无网络环境
// 设计依据：track-01-cloud-llm.md M1.8
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-07

#include "llm_types.h"

#include <chrono>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace agenticdsl {

/**
 * @brief Mock LLM Provider（测试桩）
 *
 * 支持的响应模式：
 *  1. 队列模式（enqueue_response）：每次 generate() 消耗一个预设响应
 *  2. 固定模式（set_fixed_response）：所有 generate() 返回相同响应
 *  3. 错误注入（set_simulate_error）：返回指定 LLMError
 *  4. 延迟模拟（set_simulate_delay）：模拟网络延迟
 *  5. 流式 token（set_stream_tokens）：流式 generate_stream 返回的 token 列表
 *
 * 线程安全：单线程使用
 */
class MockLLMProvider : public ILLMProvider {
public:
  MockLLMProvider() = default;
  ~MockLLMProvider() override = default;

  MockLLMProvider(const MockLLMProvider&) = delete;
  MockLLMProvider& operator=(const MockLLMProvider&) = delete;

  // === 响应配置 ===

  /**
   * @brief 入队一个字符串响应（自动包装为 GenerationResult）
   * @param content 响应文本
   * @note 队列耗尽后回退到 fixed_response_（若已设置）
   */
  void enqueue_response(const std::string& content);

  /**
   * @brief 入队一个完整 GenerationResult
   */
  void enqueue_response(GenerationResult result);

  /**
   * @brief 设置固定响应（所有未匹配的 generate() 返回此响应）
   */
  void set_fixed_response(const std::string& content);

  /**
   * @brief 设置固定响应（完整 GenerationResult）
   */
  void set_fixed_response(GenerationResult result);

  /**
   * @brief 设置流式 token 列表
   * @param tokens 流式 generate_stream() 依次返回的 token
   */
  void set_stream_tokens(std::vector<std::string> tokens);

  // === 行为模拟 ===

  /**
   * @brief 模拟错误
   * @param code 错误码
   * @param message 错误消息
   */
  void set_simulate_error(LLMError::Code code,
                          const std::string& message = "");

  /**
   * @brief 模拟网络延迟
   * @param delay 延迟时长
   */
  void set_simulate_delay(std::chrono::milliseconds delay);

  /**
   * @brief 清空所有配置（重置为初始状态）
   */
  void reset();

  // === ILLMProvider 接口 ===

  Result<GenerationResult, LLMError>
      generate(const GenerationRequest& req, std::stop_token token) override;

  std::unique_ptr<IGenerationStream>
      generate_stream(const GenerationRequest& req,
                      std::stop_token token) override;

  // === Phase 1 Sprint 0 新增 (K1 Plugin Stub 验证) ===
  /// MockLLMProvider 默认注册 1 个 mock 模型
  /// Plugin 端通过 available_models() 拿到此模型做路由决策
  std::vector<ModelInfo> available_models() const override;

  // === 测试断言辅助 ===

  /// 获取所有调用历史（按调用顺序）
  const std::vector<GenerationRequest>& call_history() const { return history_; }

  /// 获取总调用次数
  int call_count() const { return static_cast<int>(history_.size()); }

  /// 清空调用历史
  void clear_history() { history_.clear(); }

private:
  /// 返回下一个响应（优先队列，其次 fixed_response_）
  GenerationResult next_response();

  // === 内部流实现 ===
  class MockGenerationStream;

  std::queue<GenerationResult> response_queue_;
  std::optional<GenerationResult> fixed_response_;
  std::vector<std::string> stream_tokens_;
  std::vector<GenerationRequest> history_;

  std::optional<LLMError> simulated_error_;
  std::chrono::milliseconds delay_{0};
};

} // namespace agenticdsl

#endif // AGENTICDSL_LLM_MOCK_PROVIDER_H
