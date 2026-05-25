#ifndef AGENTICDSL_LLM_LLM_TYPES_H
#define AGENTICDSL_LLM_LLM_TYPES_H

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <stop_token>
#include <sstream>
#include <utility>

namespace agenticdsl {

struct LLMError {
  enum class Code {
    NetworkError,
    RateLimited,
    AuthenticationError,
    Cancelled,
    InvalidRequest,
    ServerError,
    ContextOverflow,
    Unknown
  };

  Code code = Code::Unknown;
  std::string message;
  std::optional<std::chrono::seconds> retry_after;

  LLMError() = default;
  LLMError(Code c, std::string msg) : code(c), message(std::move(msg)) {}
  LLMError(Code c, std::string msg, std::chrono::seconds retry)
      : code(c), message(std::move(msg)), retry_after(retry) {}

  bool retryable() const {
    return code == Code::NetworkError || code == Code::RateLimited ||
           code == Code::ServerError;
  }
};

class IGenerationStream {
public:
  virtual ~IGenerationStream() = default;
  virtual std::optional<std::string> next(std::stop_token token) = 0;
  virtual bool is_active() const = 0;
};

struct LLMParams {
  float temperature = 0.7f;
  int max_tokens = 512;
  float top_p = 0.95f;
  int n_ctx = 2048;
  int n_threads = 4;
  std::string model;
};

struct GenerationRequest {
  std::string prompt;
  LLMParams params;

  GenerationRequest() = default;
  explicit GenerationRequest(std::string p) : prompt(std::move(p)) {}
};

struct GenerationResult {
  std::string text;
  int prompt_tokens = 0;
  int completion_tokens = 0;
  std::string finish_reason;
};

template <typename T, typename E>
class Result {
public:
  bool has_value() const { return has_val_; }
  T& value() { return val_; }
  const T& value() const { return val_; }
  E& error() { return err_; }
  const E& error() const { return err_; }

  static Result success(T v) {
    Result r;
    r.has_val_ = true;
    r.val_ = std::move(v);
    return r;
  }
  static Result failure(E e) {
    Result r;
    r.has_val_ = false;
    r.err_ = std::move(e);
    return r;
  }

private:
  Result() = default;
  bool has_val_ = false;
  T val_;
  E err_;
};

class ILLMProvider {
public:
  virtual ~ILLMProvider() = default;

  virtual Result<GenerationResult, LLMError>
      generate(const GenerationRequest& req, std::stop_token token) = 0;

  virtual std::unique_ptr<IGenerationStream>
      generate_stream(const GenerationRequest& req, std::stop_token token) = 0;
};

inline std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> result;
  std::istringstream iss(s);
  std::string word;
  while (iss >> word) {
    result.push_back(word);
  }
  return result;
}

} // namespace

#endif