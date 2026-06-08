// src/common/llm/llama_adapter_provider.cpp
// 功能描述：LlamaAdapterProvider 实现
//          把旧 LlamaAdapter 适配为新 ILLMProvider 接口
//          把同步 throw-on-error 调用转换为 Result<T,E> 风格
// 设计依据：track-01-cloud-llm.md C₁.1、ADR-0001
// 作者：AgenticDSL Track C₁
// 最后修改日期：2026-06-08

#include "llama_adapter_provider.h"

#include <stdexcept>
#include <stop_token>
#include <utility>

namespace agenticdsl {

// =====================================================================
// 内部流实现：把字符串切分为多个 chunk 进行流式返回
// =====================================================================

class LlamaAdapterProvider::AdapterGenerationStream : public IGenerationStream {
public:
    explicit AdapterGenerationStream(std::string text)
        : text_(std::move(text)), pos_(0), active_(true) {}

    std::optional<std::string> next(std::stop_token token) override {
        if (!active_) {
            return std::nullopt;
        }
        if (token.stop_requested()) {
            active_ = false;
            return std::nullopt;
        }
        if (pos_ >= text_.size()) {
            active_ = false;
            return std::nullopt;
        }
        // 按 8 字符切分 chunk（模拟流式输出）
        constexpr size_t chunk_size = 8;
        size_t remaining = text_.size() - pos_;
        size_t this_chunk = (remaining < chunk_size) ? remaining : chunk_size;
        std::string chunk = text_.substr(pos_, this_chunk);
        pos_ += this_chunk;
        return chunk;
    }

    bool is_active() const override { return active_; }

private:
    std::string text_;
    size_t pos_;
    bool active_;
};

// =====================================================================
// LlamaAdapterProvider 实现
// =====================================================================

LlamaAdapterProvider::LlamaAdapterProvider(std::unique_ptr<LlamaAdapter> adapter)
    : adapter_(std::move(adapter)) {
    if (!adapter_) {
        throw std::invalid_argument("LlamaAdapterProvider: adapter must not be null");
    }
}

LlamaAdapterProvider::LlamaAdapterProvider(const LlamaAdapter::Config& config)
    : adapter_(std::make_unique<LlamaAdapter>(config)) {}

LlamaAdapterProvider::~LlamaAdapterProvider() = default;

Result<GenerationResult, LLMError>
LlamaAdapterProvider::generate(const GenerationRequest& req, std::stop_token token) {
    // 检查取消（调用前）
    if (token.stop_requested()) {
        return Result<GenerationResult, LLMError>::failure(
            LLMError{LLMError::Code::Cancelled, "Cancelled before generate"});
    }

    try {
        // 调用底层 LlamaAdapter
        std::string text = adapter_->generate(req.prompt);

        // 检查取消（generate 是阻塞调用，无法真正中断）
        if (token.stop_requested()) {
            return Result<GenerationResult, LLMError>::failure(
                LLMError{LLMError::Code::Cancelled, "Cancelled during generate"});
        }

        GenerationResult result;
        result.text = std::move(text);
        result.finish_reason = "stop";
        return Result<GenerationResult, LLMError>::success(std::move(result));
    } catch (const std::exception& e) {
        // 转换异常为 LLMError
        LLMError err{LLMError::Code::ServerError, e.what()};
        return Result<GenerationResult, LLMError>::failure(std::move(err));
    }
}

std::unique_ptr<IGenerationStream>
LlamaAdapterProvider::generate_stream(const GenerationRequest& req, std::stop_token token) {
    // 取消时返回空流
    if (token.stop_requested()) {
        return std::make_unique<AdapterGenerationStream>(std::string{});
    }

    try {
        // 同步调用底层 adapter
        std::string text = adapter_->generate(req.prompt);
        return std::make_unique<AdapterGenerationStream>(std::move(text));
    } catch (const std::exception& e) {
        // 错误时返回包含错误信息的单 chunk 流
        std::string error_msg = std::string("ERROR: ") + e.what();
        return std::make_unique<AdapterGenerationStream>(std::move(error_msg));
    }
}

} // namespace agenticdsl