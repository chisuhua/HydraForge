#ifndef AGENTICDSL_LLM_LLM_TYPES_H
#define AGENTICDSL_LLM_LLM_TYPES_H

// 文件头注释
// 功能描述：LLM 公共类型（LLMError / Result / IGenerationStream / ILLMProvider）
//          新版流式 LLM 接口（ADR-0001）入口头文件
//          Phase 1 新增: ModelCapability / ModelInfo / available_models() (Sprint 0)
// 设计依据：ADR-0001（ILLMProvider 流式接口）、track-01-cloud-llm.md M1.3
//          Phase 1 Sprint 0: Plugin Stub 验证 (K1 决策, ADR-0034 plugin-candidate)
// 作者：AgenticDSL Track 0.1 + Phase 1 Sprint 0
// 最后修改日期：2026-06-16

#include "llm_config.h"

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <stop_token>
#include <sstream>
#include <utility>
#include <cstdint>

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
  /// 流结束（is_active() == false）后调用，返回错误（若有）。
  /// 成功完成的流返回 nullopt。
  virtual std::optional<LLMError> error() const { return std::nullopt; }
};

// LLMParams 已迁移至 llm_config.h 中的统一 LLMConfig
// 为保持向后兼容，保留 LLMParams 作为别名
// 注意：LLMConfig 字段集是 LLMParams 的超集
using LLMParams = LLMConfig;

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

  // === Phase 1 Sprint 0 新增: Runtime 数据抽象 (非 Plugin 实现) ===
  // Phase 1 Plugin Stub 验证: ModelCapability enum + available_models() 函数
  // Plugin 端通过 available_models() 拿到 Runtime 暴露的模型列表,
  // 路由决策由 Plugin 内部逻辑完成 (见 Sprint 0 examples/phase1_model_router_plugin/)
  // 默认实现: 返回空 vector, MockLLMProvider/CloudAdapter 可选择 override

  /// 模型能力标签 (Phase 1 Sprint 0 新增, Runtime 数据模型)
  enum class ModelCapability {
    Chat,        // 对话生成
    Completion,  // 文本补全
    Embedding,   // 向量嵌入
    ToolUse,     // 工具调用
    Vision,      // 视觉输入
  };

  /// 模型元信息 (Runtime 暴露给 Plugin 的数据)
  struct ModelInfo {
    std::string name;                // 模型唯一标识 (e.g. "mock-llm-v1", "gpt-4")
    std::vector<ModelCapability> capabilities;  // 支持的能力
    std::int64_t context_window = 0;  // 上下文窗口大小 (tokens)
    std::string provider;             // 提供方 (e.g. "mock", "openai", "llama.cpp")

    ModelInfo() = default;
    ModelInfo(std::string n,
              std::vector<ModelCapability> caps,
              std::int64_t ctx = 0,
              std::string prov = "unknown")
        : name(std::move(n)),
          capabilities(std::move(caps)),
          context_window(ctx),
          provider(std::move(prov)) {}
  };

  /// 返回当前 provider 注册的所有模型
  /// REQ-ICC-004: pure virtual — 所有 ILLMProvider 子类 MUST override
  /// (Phase 5 ILLMProvider Call Chain V2, 2026-07-09)
  virtual std::vector<ModelInfo> available_models() const = 0;
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

} // namespace agenticdsl

#endif