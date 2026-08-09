#pragma once

// Phase B Step 5: Mock ILLMProvider implementation observing stop_token.
// Used by test_chat_session_cancellation E2E tests to verify end-to-end
// cancellation chain (ChatSession -> loop_agent -> ILLMProvider).

#include <chrono>
#include <stop_token>
#include <thread>
#include <utility>

#include "common/llm/llm_types.h"

namespace pdk_chat_demo {
namespace testing {

class MockBlockingProvider : public agenticdsl::ILLMProvider {
 public:
  explicit MockBlockingProvider(
      std::chrono::milliseconds max_block = std::chrono::seconds(5))
      : max_block_(max_block) {}

  agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError>
  generate(const agenticdsl::GenerationRequest& /*req*/,
           std::stop_token token) override {
    auto deadline = std::chrono::steady_clock::now() + max_block_;
    while (!token.stop_requested()) {
      if (std::chrono::steady_clock::now() >= deadline) {
        agenticdsl::GenerationResult result;
        result.text = "MockBlockingProvider: completed";
        return agenticdsl::Result<agenticdsl::GenerationResult,
                                  agenticdsl::LLMError>::success(
            std::move(result));
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return agenticdsl::Result<agenticdsl::GenerationResult,
                              agenticdsl::LLMError>::failure(
        agenticdsl::LLMError(agenticdsl::LLMError::Code::Cancelled,
                              "MockBlockingProvider cancelled by stop_token"));
  }

  std::unique_ptr<agenticdsl::IGenerationStream>
  generate_stream(const agenticdsl::GenerationRequest& req,
                  std::stop_token token) override {
    class SyncStream : public agenticdsl::IGenerationStream {
     public:
      SyncStream(agenticdsl::Result<agenticdsl::GenerationResult,
                                    agenticdsl::LLMError> r,
                 std::stop_token t)
          : result_(std::move(r)), token_(t) {}
      std::optional<std::string> next(std::stop_token /*token*/) override {
        if (emitted_) return std::nullopt;
        emitted_ = true;
        if (token_.stop_requested()) return std::nullopt;
        if (result_.has_value()) return result_.value().text;
        return std::nullopt;
      }
      bool is_active() const override { return !emitted_; }
      std::optional<agenticdsl::LLMError> error() const override {
        if (result_.has_value()) return std::nullopt;
        return result_.error();
      }
     private:
      agenticdsl::Result<agenticdsl::GenerationResult, agenticdsl::LLMError>
          result_;
      std::stop_token token_;
      bool emitted_ = false;
    };
    auto result = generate(req, token);
    return std::make_unique<SyncStream>(std::move(result), token);
  }

  std::vector<ModelInfo> available_models() const override {
    return {ModelInfo("mock-blocking-v1", {ModelCapability::Chat}, 4096,
                      "mock")};
  }

 private:
  std::chrono::milliseconds max_block_;
};

}  // namespace testing
}  // namespace pdk_chat_demo