// compliance_decorator.cpp
// 文件头注释
// 功能描述：ComplianceDecorator 实现 — emit compliance.log 事件
//          同步路径: prompt_hash + completion_hash (双 event)
//          流式路径: 累积 chunk 后单 event
//          payload 仅含 hash, 不含原始文本 (per ADR-0031 §决策 7)
// 设计依据：openspec/changes/phase5-illmprovider-call-chain-v2/specs/
//          illmprovider-decorator/spec.md (REQ-IPD-003)
// 作者：AgenticDSL Phase 5 ILLMProvider Call Chain V2
// 最后修改日期：2026-07-09

#include "compliance_decorator.h"

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "core/types/tool_result.h"

#include <chrono>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace agenticdsl {

namespace {

/// 获取当前时间戳 ISO 8601 字符串 (MVP 实现, 精确到秒)
std::string current_timestamp_iso() {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ",
                std::gmtime(&t));
  return std::string(buf);
}

/// 默认租户 ID (MVP 阶段固定, Phase 6+ 从请求上下文提取)
constexpr const char* kDefaultTenant = "tenant_default";

}  // namespace

// =====================================================================
// ComplianceDecorator 实现
// =====================================================================

ComplianceDecorator::ComplianceDecorator(
    std::unique_ptr<ILLMProvider> inner,
    std::shared_ptr<IInteractionBus> bus)
    : ILLMProviderDecorator(std::move(inner)), bus_(std::move(bus)) {}

Result<GenerationResult, LLMError> ComplianceDecorator::decorate_generate(
    const GenerationRequest& req,
    Result<GenerationResult, LLMError> inner_result) {
  if (bus_ == nullptr) {
    return inner_result;  // 无 bus 不 emit (防御性)
  }

  // === 1. prompt_hash 计算 + emit (调用前) ===
  const std::size_t prompt_hash =
      std::hash<std::string>{}(req.prompt);
  const std::string& model = req.params.model;
  const std::string timestamp = current_timestamp_iso();

  // payload 仅含 hash, 不含原始 prompt 文本 (ADR-0031 §决策 7)
  nlohmann::json prompt_payload = {
      {"prompt_hash", prompt_hash},
      {"model", model},
      {"tenant_id", kDefaultTenant},
      {"timestamp", timestamp}};
  // emit std::string 重载 (REQ-TR-005 兼容入口)
  bus_->emit(EventBuilder("compliance.log").args(prompt_payload).build());

  // === 2. completion_hash 计算 + emit (调用后, 仅成功时) ===
  if (inner_result.has_value()) {
    const auto& result = inner_result.value();
    const std::size_t completion_hash =
        std::hash<std::string>{}(result.text);
    nlohmann::json completion_payload = {
        {"completion_hash", completion_hash},
        {"prompt_hash", prompt_hash},
        {"model", model},
        {"timestamp", current_timestamp_iso()}};
    // payload 不含 result.text 原始 completion
    bus_->emit(EventBuilder("compliance.log").args(completion_payload).build());
  }

  // 装饰器不修改业务返回值 (REQ-IPD-002 §Scenario "同步 generate 计费")
  return inner_result;
}

std::unique_ptr<IGenerationStream>
ComplianceDecorator::decorate_generate_stream(
    const GenerationRequest& req,
    std::unique_ptr<IGenerationStream> inner_stream) {
  const std::size_t prompt_hash =
      std::hash<std::string>{}(req.prompt);
  return std::make_unique<ComplianceStream>(
      std::move(inner_stream), bus_, prompt_hash, req.params.model);
}

// =====================================================================
// ComplianceStream 实现
// =====================================================================

ComplianceDecorator::ComplianceStream::ComplianceStream(
    std::unique_ptr<IGenerationStream> inner,
    std::shared_ptr<IInteractionBus> bus,
    std::size_t prompt_hash,
    std::string model_name)
    : inner_(std::move(inner)),
      bus_(std::move(bus)),
      prompt_hash_(prompt_hash),
      model_name_(std::move(model_name)) {}

ComplianceDecorator::ComplianceStream::~ComplianceStream() {
  // 析构时若未 emit (例如流被提前销毁), 补一次 emit 保证审计完整
  if (!emitted_ && bus_ != nullptr) {
    emitted_ = true;
    const std::size_t completion_hash =
        std::hash<std::string>{}(accumulated_);
    nlohmann::json payload = {
        {"completion_hash", completion_hash},
        {"prompt_hash", prompt_hash_},
        {"model", model_name_},
        {"timestamp", current_timestamp_iso()}};
    bus_->emit(EventBuilder("compliance.log").args(payload).build());
  }
}

std::optional<std::string>
ComplianceDecorator::ComplianceStream::next(std::stop_token token) {
  auto chunk = inner_->next(token);
  if (chunk.has_value()) {
    // 累积 chunk 用于流结束计算 completion_hash
    accumulated_ += *chunk;
  } else if (!emitted_ && bus_ != nullptr) {
    // 流结束 (next 返回 nullopt), emit 单个 compliance.log event
    emitted_ = true;
    const std::size_t completion_hash =
        std::hash<std::string>{}(accumulated_);
    nlohmann::json payload = {
        {"completion_hash", completion_hash},
        {"prompt_hash", prompt_hash_},
        {"model", model_name_},
        {"timestamp", current_timestamp_iso()}};
    bus_->emit(EventBuilder("compliance.log").args(payload).build());
  }
  return chunk;
}

bool ComplianceDecorator::ComplianceStream::is_active() const {
  return inner_->is_active();
}

std::optional<LLMError>
ComplianceDecorator::ComplianceStream::error() const {
  return inner_->error();
}

}  // namespace agenticdsl
