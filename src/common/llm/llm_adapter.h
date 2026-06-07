#ifndef AGENTICDSL_LLM_LLM_ADAPTER_H
#define AGENTICDSL_LLM_LLM_ADAPTER_H

// 文件头注释
// 功能描述：旧版 LLM 适配器接口（同步、不可流式、不可取消）
//          已被 ILLMProvider (llm_types.h) 取代，本文件仅作过渡期兼容保留
// 设计依据：track-01-cloud-llm.md M1.2
// 作者：AgenticDSL Track 0.1
// 最后修改日期：2026-06-07

#include <string>
#include <memory>
#include <functional>

namespace agenticdsl {

// === 旧版配置结构保留 ===
// 注意：field 集合已在新版 LLMConfig (llm_config.h) 中合并扩展
// 本结构仅在旧版代码路径中保留使用（HttpLLMAdapter / LlamaAdapter）
// 新代码请使用 agenticdsl::LLMConfig
struct LLMConfig {
    std::string api_url = "http://localhost:8080";
    std::string api_endpoint = "/v1/chat/completions";
    std::string api_key;
    std::string model = "gpt-3.5-turbo";
    float temperature = 0.7f;
    int max_tokens = 512;
    int n_ctx = 2048;
    int n_threads = 4;
};

struct LLMResult {
    bool success = false;
    std::string text;
    std::string error;
    int tokens_generated = 0;
};

/**
 * @brief 旧版 LLM 适配器接口（已被 ILLMProvider 取代）
 *
 * 旧接口同步、不可流式、不支持取消，限制太多。
 * 新实现请继承 agenticdsl::ILLMProvider（见 llm_types.h）。
 * 本类保留至 Phase 2 全部迁移完成后删除。
 */
class [[deprecated("Use ILLMProvider (llm_types.h) instead")]] ILLMAdapter {
public:
    virtual ~ILLMAdapter() = default;
    virtual LLMResult generate(const std::string& prompt, const LLMConfig& params = {}) = 0;
    virtual bool is_available() const = 0;
    virtual std::string name() const = 0;
};

using LLMAdapterFactory = std::function<std::unique_ptr<ILLMAdapter>(const LLMConfig&)>;

} // namespace agenticdsl

#endif