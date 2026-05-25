// tests/test_llm_streaming.cpp
/**
 * @file test_llm_streaming.cpp
 * @brief TDD tests for ADR-0001 streaming LLM interface
 * @date 2026-05-14
 */

#include "catch_amalgamated.hpp"
#include "common/llm/llm_types.h"
#include <stop_token>
#include <chrono>

using namespace agenticdsl;

// ================================
// LLMError Tests (RED phase)
// ================================

TEST_CASE("LLMError code enum values", "[llm_streaming][llm_error]") {
  LLMError err;
  REQUIRE(static_cast<int>(err.code) >= 0);
}

TEST_CASE("LLMError default constructor", "[llm_streaming][llm_error]") {
  LLMError err;
  REQUIRE(err.code == LLMError::Code::Unknown);
  REQUIRE(err.message.empty());
  REQUIRE_FALSE(err.retry_after.has_value());
}

TEST_CASE("LLMError with message", "[llm_streaming][llm_error]") {
  LLMError err{LLMError::Code::NetworkError, "Connection failed"};
  REQUIRE(err.code == LLMError::Code::NetworkError);
  REQUIRE(err.message == "Connection failed");
}

TEST_CASE("LLMError with retry_after", "[llm_streaming][llm_error]") {
  LLMError err{LLMError::Code::RateLimited, "Rate limited", std::chrono::seconds(5)};
  REQUIRE(err.code == LLMError::Code::RateLimited);
  REQUIRE(err.retry_after.has_value());
  REQUIRE(err.retry_after.value().count() == 5);
}

TEST_CASE("LLMError retryable for NetworkError", "[llm_streaming][llm_error]") {
  LLMError err{LLMError::Code::NetworkError, "Connection failed"};
  REQUIRE(err.retryable() == true);
}

TEST_CASE("LLMError retryable for RateLimited", "[llm_streaming][llm_error]") {
  LLMError err{LLMError::Code::RateLimited, "Rate limited"};
  REQUIRE(err.retryable() == true);
}

TEST_CASE("LLMError retryable for ServerError", "[llm_streaming][llm_error]") {
  LLMError err{LLMError::Code::ServerError, "Server error"};
  REQUIRE(err.retryable() == true);
}

TEST_CASE("LLMError NOT retryable for AuthenticationError", "[llm_streaming][llm_error]") {
  LLMError err{LLMError::Code::AuthenticationError, "Auth failed"};
  REQUIRE(err.retryable() == false);
}

TEST_CASE("LLMError NOT retryable for InvalidRequest", "[llm_streaming][llm_error]") {
  LLMError err{LLMError::Code::InvalidRequest, "Bad request"};
  REQUIRE(err.retryable() == false);
}

TEST_CASE("LLMError NOT retryable for Cancelled", "[llm_streaming][llm_error]") {
  LLMError err{LLMError::Code::Cancelled, "User cancelled"};
  REQUIRE(err.retryable() == false);
}

// ================================
// IGenerationStream Tests (RED phase)
// ================================

TEST_CASE("IGenerationStream interface exists", "[llm_streaming][generation_stream]") {
  REQUIRE(sizeof(IGenerationStream) > 0);
}

TEST_CASE("IGenerationStream is abstract", "[llm_streaming][generation_stream]") {
  // Cannot instantiate abstract class directly
  REQUIRE_FALSE(std::is_default_constructible_v<IGenerationStream>);
}

TEST_CASE("MockGenerationStream basic pull", "[llm_streaming][generation_stream]") {
  class MockStream : public IGenerationStream {
  public:
    std::vector<std::string> tokens{"Hello", " world", "!"};
    size_t index = 0;
    bool active = true;

    std::optional<std::string> next(std::stop_token) override {
      if (index < tokens.size()) {
        return tokens[index++];
      }
      active = false;
      return std::nullopt;
    }

    bool is_active() const override {
      return active;
    }
  };

  MockStream stream;
  REQUIRE(stream.is_active() == true);

  auto tok1 = stream.next({});
  REQUIRE(tok1.has_value());
  REQUIRE(*tok1 == "Hello");

  auto tok2 = stream.next({});
  REQUIRE(tok2.has_value());
  REQUIRE(*tok2 == " world");

  auto tok3 = stream.next({});
  REQUIRE(tok3.has_value());
  REQUIRE(*tok3 == "!");

  auto tok4 = stream.next({});
  REQUIRE_FALSE(tok4.has_value());
  REQUIRE(stream.is_active() == false);
}

TEST_CASE("MockGenerationStream stops on stop_token", "[llm_streaming][generation_stream]") {
  class StoppingStream : public IGenerationStream {
  public:
    std::vector<std::string> tokens{"a", "b", "c", "d", "e"};
    size_t index = 0;
    bool active = true;

    std::optional<std::string> next(std::stop_token token) override {
      if (token.stop_requested()) {
        active = false;
        return std::nullopt;
      }
      if (index < tokens.size()) {
        return tokens[index++];
      }
      return std::nullopt;
    }

    bool is_active() const override {
      return active;
    }
  };

  StoppingStream stream;
  std::stop_source src;

  // Get first token
  auto t1 = stream.next(src.get_token());
  REQUIRE(t1.has_value());

  // Request stop
  src.request_stop();

  // Next should return nullopt and deactivate
  auto t2 = stream.next(src.get_token());
  REQUIRE_FALSE(t2.has_value());
  REQUIRE(stream.is_active() == false);
}

// ================================
// ILLMProvider Tests (RED phase)
// ================================

TEST_CASE("ILLMProvider interface exists", "[llm_streaming][llm_provider]") {
  REQUIRE(sizeof(ILLMProvider) > 0);
}

TEST_CASE("ILLMProvider is abstract", "[llm_streaming][llm_provider]") {
  REQUIRE_FALSE(std::is_default_constructible_v<ILLMProvider>);
}

TEST_CASE("GenerationRequest default constructor", "[llm_streaming][llm_provider]") {
  GenerationRequest req;
  REQUIRE(req.prompt.empty());
  REQUIRE(req.params.temperature == 0.7f);
  REQUIRE(req.params.max_tokens == 512);
}

TEST_CASE("GenerationRequest with prompt", "[llm_streaming][llm_provider]") {
  GenerationRequest req{"Hello world"};
  REQUIRE(req.prompt == "Hello world");
}

TEST_CASE("GenerationResult default constructor", "[llm_streaming][llm_provider]") {
  GenerationResult result;
  REQUIRE(result.text.empty());
  REQUIRE(result.prompt_tokens == 0);
  REQUIRE(result.completion_tokens == 0);
  REQUIRE(result.finish_reason.empty());
}

TEST_CASE("MockProvider generate returns text", "[llm_streaming][llm_provider]") {
  class MockProvider : public ILLMProvider {
  public:
    std::expected<GenerationResult, LLMError>
        generate(const GenerationRequest& req, std::stop_token) override {
      return GenerationResult{"Mock response: " + req.prompt, 10, 5, "stop"};
    }

    std::unique_ptr<IGenerationStream>
        generate_stream(const GenerationRequest& req, std::stop_token) override {
      class final : public IGenerationStream {
        std::vector<std::string> toks = agenticdsl::split(req.prompt);
        size_t idx = 0;
        bool active = true;
      public:
        std::optional<std::string> next(std::stop_token) override {
          if (idx < toks.size()) return toks[idx++];
          active = false;
          return std::nullopt;
        }
        bool is_active() const override { return active; }
      };
      return std::make_unique<final>();
    }
  };

  MockProvider provider;
  auto result = provider.generate(GenerationRequest{"test"}, {});
  REQUIRE(result.has_value());
  REQUIRE(result->text == "Mock response: test");
}

TEST_CASE("MockProvider generate_stream returns tokens", "[llm_streaming][llm_provider]") {
  class MockProvider : public ILLMProvider {
  public:
    std::expected<GenerationResult, LLMError>
        generate(const GenerationRequest&, std::stop_token) override {
      return GenerationResult{"full text", 10, 10, "stop"};
    }

    std::unique_ptr<IGenerationStream>
        generate_stream(const GenerationRequest& req, std::stop_token) override {
      class final : public IGenerationStream {
        std::vector<std::string> toks = agenticdsl::split(req.prompt);
        size_t idx = 0;
        bool active = true;
      public:
        std::optional<std::string> next(std::stop_token) override {
          if (idx < toks.size()) return toks[idx++];
          active = false;
          return std::nullopt;
        }
        bool is_active() const override { return active; }
      };
      return std::make_unique<final>();
    }
  };

  MockProvider provider;
  auto stream = provider.generate_stream(GenerationRequest{"a b c"}, {});
  REQUIRE(stream->is_active() == true);

  auto t1 = stream->next({});
  REQUIRE(t1.has_value());
  REQUIRE(*t1 == "a");

  auto t2 = stream->next({});
  REQUIRE(t2.has_value());
  REQUIRE(*t2 == "b");

  auto t3 = stream->next({});
  REQUIRE(t3.has_value());
  REQUIRE(*t3 == "c");

  auto t4 = stream->next({});
  REQUIRE_FALSE(t4.has_value());
  REQUIRE(stream->is_active() == false);
}