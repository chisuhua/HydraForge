// src/common/llm/tracing_decorator.h
// 文件头注释
// 功能描述：TracingDecorator — ILLMProvider 装饰器
//          在 generate() 调用前后发射 llm.request / llm.response 生命周期事件
//          ADR-0068 §决策 3: 5 个幻影主题中 2 个由本装饰器完成
// 设计依据：openspec/changes/adr-0068-event-emission-contract/design.md Decision 3
//           + AgenticDSL_V2_Contract §illmprovider-decorator REQ-IPD-001
// 作者：AgenticDSL Phase 6a
// 最后修改日期：2026-08-03
#ifndef AGENTICDSL_LLM_TRACING_DECORATOR_H
#define AGENTICDSL_LLM_TRACING_DECORATOR_H

#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/types/capture_mode.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace agenticdsl {

/**
 * @brief 追踪装饰器 (ADR-0068 §决策 3)
 *
 * 部署位置: 装饰器链最外层 (与 CostTrackingDecorator 并列, 事件追踪优先)
 * 行为:
 *  - pre_check_generate: 发射 llm.request 事件 + 记录开始时间
 *  - decorate_generate: 发射 llm.response 事件 (含 ok flag, error_code, tokens, duration_ms)
 *  - 无 bus 时静默 no-op (允许 unit test 在无 bus 场景使用)
 *  - 线程安全: trace_id_ 与 request_start_ 仅在同一线程相邻两个钩子中使用
 */
class TracingDecorator : public ILLMProviderDecorator {
 public:
  TracingDecorator(std::unique_ptr<ILLMProvider> inner,
                   std::shared_ptr<IInteractionBus> bus);

  // ADR-0080 v1.1 D10: opt-in Distillation Capture
  // 开启后 emit_request 携带 prompt_text + available_tools_schema + system_prompt_source
  // emit_response 携带 response_text (默认关闭, 行为不变)
  void set_capture_mode(CaptureMode mode) { capture_mode_ = mode; }
  CaptureMode capture_mode() const { return capture_mode_; }

 protected:
  std::optional<LLMError> pre_check_generate(
      const GenerationRequest& req) override;

  Result<GenerationResult, LLMError> decorate_generate(
      const GenerationRequest& req,
      Result<GenerationResult, LLMError> inner_result) override;

 private:
  std::shared_ptr<IInteractionBus> bus_;
  std::string trace_id_;
  std::optional<std::chrono::steady_clock::time_point> request_start_;
  // ADR-0080 v1.1 D10: opt-in Distillation Capture（默认 Off, 行为不变）
  CaptureMode capture_mode_ = CaptureMode::Off;

  static std::string compute_prompt_hash(const std::string& prompt);
  static std::string make_trace_id();

  void emit_request(const GenerationRequest& req);
  void emit_response(const GenerationRequest& req,
                     const Result<GenerationResult, LLMError>& inner_result,
                     std::chrono::steady_clock::duration duration);
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_LLM_TRACING_DECORATOR_H
