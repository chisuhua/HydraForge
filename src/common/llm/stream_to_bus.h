// src/common/llm/stream_to_bus.h
// C2 Day 1-2 (2026-06-27, Sprint 12 P1, ADR-0030 V2 §决策 1)
//
// Bridge runner: 将 pull-based IGenerationStream 转为 IInteractionBus 事件流。
// Oracle 决议 (session ses_0f5541ebfffehKDxNVuYqB7bq4, 2026-06-27):
//   - IGenerationStream 是 pull-based (next() + is_active() + error()), 非 callback
//   - 保持 LLM provider 纯净 (4 个 provider 无需依赖 IInteractionBus)
//   - 一个 bridge 跨所有 provider 复用, 可 MockProvider + InMemoryBus 单测
//   - 未来 TUI 通过 bus.subscribe("llm.token", ...) 消费, 解耦彻底
#ifndef AGENTICDSL_COMMON_LLM_STREAM_TO_BUS_H
#define AGENTICDSL_COMMON_LLM_STREAM_TO_BUS_H

#include "common/llm/llm_types.h"  // IGenerationStream, LLMError
#include "agenticdsl/contract/iinteraction_bus.h"  // IInteractionBus
#include "core/types/common.h"  // GenerationResult
#include <stop_token>
#include <string>
#include <string_view>

namespace agenticdsl {
namespace llm {

/// 同步运行 pull-based IGenerationStream, 将 token/error/done 事件 emit 到 bus。
///
/// 行为:
///   1. 循环调用 stream.next(token) 拉取 token
///   2. 每个 token 通过 bus.emit("llm.token", ToolResult::success({...}, {...}))
///   3. 流结束后 emit "llm.token.done" (含 finish_reason + token 计数)
///   4. 出错 (stream.error() 或 next() 抛异常) emit "llm.token.error"
///   5. stop_token 触发后立即停止 pull-loop, emit "llm.token.done" (finish_reason="cancelled")
///
/// 返回: 聚合后的完整 GenerationResult (text + token 计数)。
/// 阻塞: 同步直到流结束或 stop_token 触发。
///
/// 不修改 LLM provider (LlamaAdapter/CloudAdapter/HttpAdapter/MockProvider) 任何代码。
/// provider 仅实现 IGenerationStream 接口, 本函数作为 transport adapter 桥接事件流。
GenerationResult run_stream_to_bus(
    IGenerationStream& stream,
    IInteractionBus& bus,
    std::stop_token token,
    std::string_view request_id);

/// 事件类型常量 (bus.subscribe() 时使用)
namespace event_type {
constexpr const char* kLlmToken       = "llm.token";
constexpr const char* kLlmTokenDone   = "llm.token.done";
constexpr const char* kLlmTokenError  = "llm.token.error";
}  // namespace event_type

/// finish_reason 字符串常量
namespace finish_reason {
constexpr const char* kStop        = "stop";
constexpr const char* kCancelled   = "cancelled";
}  // namespace finish_reason

}  // namespace llm
}  // namespace agenticdsl

#endif  // AGENTICDSL_COMMON_LLM_STREAM_TO_BUS_H