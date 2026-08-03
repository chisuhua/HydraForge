// src/common/llm/tracing_decorator.cpp
// 文件头注释
// 功能描述：TracingDecorator 实现 — 发射 llm.request / llm.response 生命周期事件
// 设计依据：openspec/changes/adr-0068-event-emission-contract/design.md Decision 3
// 作者：AgenticDSL Phase 6a
// 最后修改日期：2026-08-03
#include "tracing_decorator.h"

#include "agenticdsl/contract/event_builder.h"
#include "common/llm/llm_types.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace agenticdsl {

namespace {

std::string make_trace_id() {
  static std::atomic<std::uint64_t> counter{0};
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return "llm-trace-" + std::to_string(now) + "-" + std::to_string(counter++);
}

}  // namespace

TracingDecorator::TracingDecorator(std::unique_ptr<ILLMProvider> inner,
                                   std::shared_ptr<IInteractionBus> bus)
    : ILLMProviderDecorator(std::move(inner)), bus_(std::move(bus)) {}

std::string TracingDecorator::compute_prompt_hash(const std::string& prompt) {
  return std::to_string(std::hash<std::string>{}(prompt));
}

std::string TracingDecorator::make_trace_id() {
  // 调用静态 make_trace_id 在头文件私有段
  return ::agenticdsl::make_trace_id();
}

void TracingDecorator::emit_request(const GenerationRequest& req) {
  if (!bus_) return;
  trace_id_ = make_trace_id();
  nlohmann::json args = {
      {"model", req.params.model},
      {"max_tokens", req.params.max_tokens},
      {"prompt_hash", compute_prompt_hash(req.prompt)}};
  nlohmann::json meta = {{"trace_id", trace_id_}};
  bus_->emit(EventBuilder("llm.request").args(args).meta(meta).build());
}

void TracingDecorator::emit_response(
    const GenerationRequest& req,
    const Result<GenerationResult, LLMError>& inner_result,
    std::chrono::steady_clock::duration duration) {
  (void)req;
  if (!bus_) return;
  nlohmann::json args;
  nlohmann::json meta = {{"trace_id", trace_id_}};
  args["duration_ms"] =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  if (inner_result.has_value()) {
    const auto& r = inner_result.value();
    args["prompt_tokens"] = r.prompt_tokens;
    args["completion_tokens"] = r.completion_tokens;
    args["tokens"] = r.prompt_tokens + r.completion_tokens;
    args["ok"] = true;
  } else {
    const auto& err = inner_result.error();
    args["ok"] = false;
    args["error_code"] = static_cast<int>(err.code);
    args["error_message"] = err.message;
  }
  bus_->emit(EventBuilder("llm.response").args(args).meta(meta).build());
}

std::optional<LLMError> TracingDecorator::pre_check_generate(
    const GenerationRequest& req) {
  request_start_ = std::chrono::steady_clock::now();
  emit_request(req);
  return std::nullopt;
}

Result<GenerationResult, LLMError> TracingDecorator::decorate_generate(
    const GenerationRequest& req,
    Result<GenerationResult, LLMError> inner_result) {
  auto t1 = std::chrono::steady_clock::now();
  std::chrono::steady_clock::duration dur{0};
  if (request_start_) {
    dur = t1 - *request_start_;
  }
  emit_response(req, inner_result, dur);
  request_start_ = std::nullopt;
  return inner_result;
}

}  // namespace agenticdsl
