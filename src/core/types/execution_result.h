// src/core/types/execution_result.h
// 功能描述：统一 ExecutionResult 类型（P9 error-taxonomy-execution-boundary）
//          含 ErrorCode 字段 + is_retryable() 成员函数
//          替代 budget.h 中的重复定义（保留字段名兼容性）
// 设计依据：openspec/changes/error-taxonomy-execution-boundary (P9)
// 作者：HydraForge Sprint 22 P9 ship
// 最后修改日期：2026-08-20

#pragma once

#include "core/types/context.h"        // Context (using nlohmann::json)
#include "core/types/tool_result.h"    // ErrorCode enum

#include <optional>
#include <string>

namespace agenticdsl {

struct ExecutionResult {
    bool success;
    std::string message;               // 错误信息或成功信息
    Context final_context;             // 执行结束时的上下文
    std::optional<std::string> paused_at; // set if paused at llm_call
    std::optional<ErrorCode> error_code; // P9: 错误码（nullopt = 未指定）

    // 判断是否可重试（P9 error taxonomy）
    // 按 openspec/changes/error-taxonomy-execution-boundary/proposal.md 分类表
    bool is_retryable() const {
        if (!error_code.has_value()) {
            // 未指定 error_code → 兼容旧行为：success=true 不重试，success=false 可重试
            return !success;
        }
        switch (error_code.value()) {
            case ErrorCode::Retry:
            case ErrorCode::Timeout:
            case ErrorCode::ResourceExhausted:
            case ErrorCode::MaxStepsExceeded:
            case ErrorCode::Crash:
            case ErrorCode::BudgetExhausted:
                return true;
            default:
                return false;
        }
    }
};

}  // namespace agenticdsl