#ifndef AGENTICDSL_LLM_LLM_TOOL_H
#define AGENTICDSL_LLM_LLM_TOOL_H

// 文件头注释
// 功能描述：LLM 工具接口（ILLMTool）与 LLMResult 数据结构
//          LLMParams 已迁移至 llm_config.h 中的统一 LLMConfig（Track 0.1 M1.3）
// 设计依据：track-01-cloud-llm.md M1.3
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-08

#include <string>
#include <nlohmann/json.hpp>

// LLMParams 已统一为 LLMConfig 的别名（见 llm_types.h），提供向后兼容
#include "llm_types.h"

namespace agenticdsl {

// LLMParams 已迁移至 llm_config.h 中的统一 LLMConfig，参见 llm_types.h 的 using 声明

struct LLMResult {
    bool success = false;
    std::string text;
    std::string error;
    int tokens_generated = 0;
};

class ILLMTool {
public:
    virtual ~ILLMTool() = default;

    virtual LLMResult generate(const std::string& prompt, const LLMParams& params = {}) = 0;
    virtual bool is_available() const = 0;
    virtual std::string name() const = 0;
};

} // namespace agenticdsl

#endif // AGENTICDSL_LLM_LLM_TOOL_H
