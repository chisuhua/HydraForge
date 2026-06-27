// src/common/llm/stream_to_bus.cpp
// C2 Day 1-2 (2026-06-27, Sprint 12 P1)
#include "stream_to_bus.h"
#include "common/log/log.h"
#include "core/types/tool_result.h"
#include <stdexcept>

namespace agenticdsl {
namespace llm {

namespace {
const char* error_code_name(LLMError::Code c) {
    switch (c) {
        case LLMError::Code::NetworkError:        return "NetworkError";
        case LLMError::Code::RateLimited:         return "RateLimited";
        case LLMError::Code::AuthenticationError: return "AuthenticationError";
        case LLMError::Code::Cancelled:           return "Cancelled";
        case LLMError::Code::InvalidRequest:      return "InvalidRequest";
        case LLMError::Code::ServerError:         return "ServerError";
        case LLMError::Code::ContextOverflow:     return "ContextOverflow";
        case LLMError::Code::Unknown:             return "Unknown";
    }
    return "Unknown";
}

ToolResult make_token_payload(std::string_view request_id, const std::string& token) {
    nlohmann::json data;
    data["token"] = token;
    data["request_id"] = std::string(request_id);
    return ToolResult::success(data, {{"event", "llm.token"}});
}

ToolResult make_done_payload(std::string_view request_id,
                              const std::string& finish_reason,
                              size_t token_count,
                              const std::string& text) {
    nlohmann::json data;
    data["request_id"] = std::string(request_id);
    data["finish_reason"] = finish_reason;
    data["token_count"] = token_count;
    data["text"] = text;
    return ToolResult::success(data, {{"event", "llm.token.done"}});
}

ToolResult make_error_payload(std::string_view request_id,
                               const LLMError& err) {
    nlohmann::json data;
    data["request_id"] = std::string(request_id);
    data["code"] = static_cast<int>(err.code);
    data["code_name"] = error_code_name(err.code);
    data["message"] = err.message;
    nlohmann::json meta;
    meta["event"] = "llm.token.error";
    meta["request_id"] = std::string(request_id);
    meta["code"] = static_cast<int>(err.code);
    meta["code_name"] = error_code_name(err.code);
    if (err.retry_after.has_value()) {
        meta["retry_after_sec"] = err.retry_after->count();
    }
    return ToolResult::error(ErrorCode::Unknown, err.message, meta);
}
}  // namespace

GenerationResult run_stream_to_bus(
    IGenerationStream& stream,
    IInteractionBus& bus,
    std::stop_token token,
    std::string_view request_id)
{
    GenerationResult result;
    size_t token_count = 0;
    bool cancelled = false;

    while (stream.is_active()) {
        if (token.stop_requested()) {
            cancelled = true;
            break;
        }
        std::optional<std::string> chunk;
        try {
            chunk = stream.next(token);
        } catch (const std::exception& e) {
            LLMError err;
            err.code = LLMError::Code::Cancelled;
            err.message = std::string("stream_to_bus exception: ") + e.what();
            bus.emit(event_type::kLlmTokenError, make_error_payload(request_id, err));
            LOG_WARN("run_stream_to_bus: exception in stream.next, request_id=" << request_id);
            break;
        }
        if (!chunk.has_value()) {
            break;
        }
        result.text += *chunk;
        ++token_count;
        bus.emit(event_type::kLlmToken, make_token_payload(request_id, *chunk));
    }

    std::string finish_reason = cancelled ? finish_reason::kCancelled : finish_reason::kStop;
    auto stream_error = stream.error();
    if (stream_error.has_value()) {
        bus.emit(event_type::kLlmTokenError, make_error_payload(request_id, *stream_error));
        LOG_WARN("run_stream_to_bus: stream error, request_id=" << request_id
                 << " code=" << error_code_name(stream_error->code));
    } else {
        bus.emit(event_type::kLlmTokenDone,
                 make_done_payload(request_id, finish_reason, token_count, result.text));
    }
    result.finish_reason = finish_reason;
    result.completion_tokens = static_cast<int>(token_count);
    result.prompt_tokens = 0;
    return result;
}

}  // namespace llm
}  // namespace agenticdsl