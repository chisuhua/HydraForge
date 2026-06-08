#ifndef AGENTICDSL_LLM_LLAMA_ADAPTER_PROVIDER_H
#define AGENTICDSL_LLM_LLAMA_ADAPTER_PROVIDER_H

// 文件头注释
// 功能描述：把旧 LlamaAdapter（同步、不可流式）适配为新 ILLMProvider（流式 + 取消）
//          用于向后兼容已有代码路径，Phase 2 全部迁移完成后删除
// 设计依据：track-01-cloud-llm.md C₁.1、ADR-0001（ILLMProvider 流式接口）
// 作者：AgenticDSL Track C₁
// 最后修改日期：2026-06-08

#include "llm_types.h"
#include "llama_adapter.h"

#include <memory>

namespace agenticdsl {

/**
 * @brief 把旧 LlamaAdapter 适配为新 ILLMProvider 接口
 *
 * LlamaAdapter 使用旧 API（同步、throws on error），不能直接实现 ILLMProvider。
 * 本类在 ILLMProvider 接口上包装 LlamaAdapter 调用，把同步结果转换为流式 handle。
 *
 * 用于向后兼容已有代码路径；Phase 2 全部迁移到 ILLMProvider 后删除。
 */
class LlamaAdapterProvider : public ILLMProvider {
public:
    /**
     * @brief 构造（接管现有 LlamaAdapter 所有权）
     * @param adapter 已存在的 LlamaAdapter（必须非空）
     */
    explicit LlamaAdapterProvider(std::unique_ptr<LlamaAdapter> adapter);

    /**
     * @brief 构造（从 Config 创建新的 LlamaAdapter）
     * @param config LlamaAdapter 配置
     */
    explicit LlamaAdapterProvider(const LlamaAdapter::Config& config);

    ~LlamaAdapterProvider() override;

    // === ILLMProvider 接口 ===

    /**
     * @brief 同步生成（实现 ILLMProvider 接口）
     *
     * 内部调用 LlamaAdapter::generate()，捕获 std::exception 转换为 LLMError。
     *
     * @param req  生成请求
     * @param token 取消 token（默认空，即不可取消）
     * @return 成功时返回 GenerationResult，失败时返回 LLMError
     */
    Result<GenerationResult, LLMError>
        generate(const GenerationRequest& req, std::stop_token token) override;

    /**
     * @brief 流式生成（实现 ILLMProvider 接口）
     *
     * 返回一个 IGenerationStream handle，调用方通过 next() 拉取 chunk。
     * 本实现把 generate() 结果切分为 8 字符 chunk 模拟流式输出。
     *
     * @param req  生成请求
     * @param token 取消 token
     * @return stream handle（unique_ptr）
     */
    std::unique_ptr<IGenerationStream>
        generate_stream(const GenerationRequest& req, std::stop_token token) override;

    /// 获取底层 LlamaAdapter（用于测试/调试）
    LlamaAdapter* underlying() { return adapter_.get(); }

private:
    std::unique_ptr<LlamaAdapter> adapter_;

    // === 内部流实现 ===
    class AdapterGenerationStream;
};

} // namespace agenticdsl

#endif // AGENTICDSL_LLM_LLAMA_ADAPTER_PROVIDER_H