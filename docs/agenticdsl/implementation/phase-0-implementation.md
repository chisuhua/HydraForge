# Phase 0 实施方案：云端 LLM 集成

**文档 ID**: IP-Phase0
**日期**: 2026-05-23
**状态**: ✅ 已完成（2026-06-08, phase-c1-migration）
**关联**: BOOT-001, IP-001, ADR-0001, ADR-0005
**预计工期**: 3-4 天（0.5 + 1 + 1 + 1）

> **2026-06-08 更新**：Phase 0 全部交付（Pre-Phase + Slice 00 + Track 0.1 + C1 迁移）。
> 本计划原 §2.4 / §3.2 等处描述的文件名与实际略有差异（详见文末"实际实现差异"小节）。
> 主要差异：
>
> | 原计划 | 实际 |
> |--------|------|
> | `cloud_llm_adapter.h/cpp` | `cloud_adapter.h/cpp` |
> | `llama_adapter.h` 修改继承新配置 | `LlamaAdapterProvider` 适配器（commit d38bc51），原 `LlamaAdapter` 保持同步签名 |
> | `llm_router.{h,cpp}` 路由器 | 移交 Phase 1（ADR-0034），M3.x 任务未实施 |
> | 单一阶段 "云端 LLM 集成" | 实际为 5 个原子 commit (d38bc51, 3f28020, 4312333, 5f21ea3, fe448a0) |

---

## 1. 概述

### 阶段 0 目标

建立云端 LLM 集成架构，实现以下目标：

1. **接口统一**：统一 `ILLMAdapter` 和 `ILLMProvider` 接口，合并 `LLMConfig` 和 `LLMParams`
2. **CloudLLMAdapter**：实现 OpenAI/Claude 兼容的云端 LLM 适配器
3. **LLMRouter**：实现推理路由器，支持云端/本地自动路由
4. **流式输出**：增强流式输出和错误处理能力

### 前提条件

- `DSLEngine` 可正常加载 `.agent.md` 工作流
- 网络可访问 OpenAI/Claude API
- 已配置 `llm_config.json` 或等效环境变量

### 时间估算

| Step | 任务 | 工期 |
|------|------|------|
| Step 1 | 接口统一 | 0.5 天 |
| Step 2 | CloudLLMAdapter 核心实现 | 1-2 天 |
| Step 3 | LLMRouter 实现 | 1 天 |
| Step 4 | 流式与错误处理增强 | 1 天 |

---

## 2. Step 1: 接口统一（0.5 天）

### 2.1 目标

标记 `ILLMAdapter` 为废弃（deprecated），统一配置结构。

### 2.2 标记 ILLMAdapter 为废弃

在 `src/common/llm/llm_adapter.h` 中使用 `[[deprecated]]` 标记：

```cpp
/**
 * @deprecated 请使用 ILLMProvider 和 IGenerationStream 接口（见 llm_types.h）
 */
class [[deprecated("Use ILLMProvider instead")]] ILLMAdapter {
public:
    virtual ~ILLMAdapter() = default;
    virtual LLMResult generate(const std::string& prompt, const LLMConfig& params = {}) = 0;
    virtual bool is_available() const = 0;
    virtual std::string name() const = 0;
};
```

### 2.3 统一配置结构

将 `LLMConfig`（llm_adapter.h）和 `LLMParams`（llm_tool.h）合并为统一的 `LLMConfig`：

```cpp
// src/common/llm/llm_config.h（新建）

#ifndef AGENTICDSL_LLM_LLM_CONFIG_H
#define AGENTICDSL_LLM_LLM_CONFIG_H

#include <string>
#include <optional>
#include <vector>

namespace agenticdsl {

/**
 * @brief 统一 LLM 配置结构
 * 
 * 整合原有 LLMConfig 和 LLMParams，兼容本地和云端配置
 */
struct LLMConfig {
    // === 连接配置 ===
    std::string provider = "openai";           // "openai" | "anthropic" | "local"
    std::string api_url = "http://localhost:8080";
    std::string api_endpoint = "/v1/chat/completions";
    std::string api_key;
    
    // === 模型配置 ===
    std::string model = "gpt-3.5-turbo";
    int n_ctx = 2048;
    
    // === 采样参数 ===
    float temperature = 0.7f;
    float top_p = 0.95f;
    int max_tokens = 512;
    std::vector<std::string> stop_tokens;
    
    // === 性能配置（本地 llama.cpp）===
    int n_threads = 4;
    float min_p = 0.05f;
    
    // === 云端专用配置 ===
    std::optional<std::string> organization;    // OpenAI 组织
    std::optional<std::string> project;        // OpenAI 项目
    std::optional<float> top_logprobs;         // OpenAI logprobs
    std::optional<std::string> response_format; // "json_object"
};

} // namespace agenticdsl

#endif
```

### 2.4 涉及文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/common/llm/llm_adapter.h` | 修改 | 标记 `ILLMAdapter` 为 deprecated |
| `src/common/llm/llm_config.h` | 新建 | 统一配置结构 |
| `src/common/llm/llm_tool.h` | 删除 | 合并到 llm_config.h |
| `src/common/llm/llm_types.h` | 修改 | 导入统一配置，移除重复定义 |
| `src/common/llm/llama_adapter.h` | 修改 | 继承新配置结构 |
| `src/common/llm/http_adapter.h` | 修改 | 使用统一配置 |

### 2.5 代码示例

**旧接口（llm_adapter.h）**：
```cpp
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
```

**新接口（llm_config.h）**：
```cpp
struct LLMConfig {
    // 连接配置
    std::string provider = "openai";
    std::string api_url = "http://localhost:8080";
    std::string api_endpoint = "/v1/chat/completions";
    std::string api_key;
    
    // 模型配置
    std::string model = "gpt-3.5-turbo";
    int n_ctx = 2048;
    
    // 采样参数（统一）
    float temperature = 0.7f;
    float top_p = 0.95f;
    int max_tokens = 512;
    std::vector<std::string> stop_tokens;
    
    // 本地/云端专用
    int n_threads = 4;
    std::optional<std::string> organization;
};
```

---

## 3. Step 2: CloudLLMAdapter 核心实现（1-2 天）

### 3.1 功能要求列表

| 功能 | 描述 | 优先级 |
|------|------|--------|
| OpenAI 兼容调用 | 支持 `/v1/chat/completions` API | 必须 |
| Anthropic 兼容调用 | 支持 `/v1/messages` API | 必须 |
| 流式输出 | 支持 SSE 风格的流式响应 | 必须 |
| 错误重试 | NetworkError、RateLimited 自动重试 | 必须 |
| 配置验证 | 启动时验证 API key 和 endpoint | 必须 |
| 超时控制 | 可配置请求超时时间 | 应该 |

### 3.2 涉及文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/common/llm/cloud_llm_adapter.h` | 新建 | 云端 LLM 适配器接口 |
| `src/common/llm/openai_adapter.cpp/h` | 新建 | OpenAI API 实现 |
| `src/common/llm/anthropic_adapter.cpp/h` | 新建 | Anthropic API 实现 |
| `src/common/llm/http_client.cpp/h` | 新建 | HTTP 客户端封装 |
| `src/common/llm/CMakeLists.txt` | 修改 | 添加新目标 |

### 3.3 关键代码示例

**CloudLLMAdapter 接口定义**：

```cpp
// src/common/llm/cloud_llm_adapter.h

#ifndef AGENTICDSL_LLM_CLOUD_LLM_ADAPTER_H
#define AGENTICDSL_LLM_CLOUD_LLM_ADAPTER_H

#include "llm_config.h"
#include "llm_types.h"
#include <string>
#include <functional>
#include <stop_token>

namespace agenticdsl {

/**
 * @brief 云端 LLM 适配器接口
 * 
 * 支持 OpenAI 和 Anthropic 兼容 API
 */
class ICloudLLMAdapter {
public:
    virtual ~ICloudLLMAdapter() = default;
    
    /**
     * @brief 初始化适配器
     * @param config 配置信息
     * @return true 初始化成功
     */
    virtual bool initialize(const LLMConfig& config) = 0;
    
    /**
     * @brief 同步生成
     * @param req 生成请求
     * @param token 取消令牌
     * @return 生成结果
     */
    virtual Result<GenerationResult, LLMError>
        generate(const GenerationRequest& req, std::stop_token token) = 0;
    
    /**
     * @brief 流式生成
     * @param req 生成请求
     * @param token 取消令牌
     * @return 流式生成器
     */
    virtual std::unique_ptr<IGenerationStream>
        generate_stream(const GenerationRequest& req, std::stop_token token) = 0;
    
    /**
     * @brief 检查适配器是否可用
     */
    virtual bool is_available() const = 0;
    
    /**
     * @brief 获取提供商名称
     */
    virtual std::string provider_name() const = 0;
};

/**
 * @brief OpenAI 兼容适配器
 */
class OpenAIAdapter : public ICloudLLMAdapter {
public:
    bool initialize(const LLMConfig& config) override;
    Result<GenerationResult, LLMError>
        generate(const GenerationRequest& req, std::stop_token token) override;
    std::unique_ptr<IGenerationStream>
        generate_stream(const GenerationRequest& req, std::stop_token token) override;
    bool is_available() const override;
    std::string provider_name() const override { return "openai"; }

private:
    LLMConfig config_;
    std::string api_url_;
    std::string api_key_;
    
    Result<GenerationResult, LLMError> call_api(
        const GenerationRequest& req,
        std::stop_token token,
        bool stream = false);
};

/**
 * @brief Anthropic 兼容适配器
 */
class AnthropicAdapter : public ICloudLLMAdapter {
public:
    bool initialize(const LLMConfig& config) override;
    Result<GenerationResult, LLMError>
        generate(const GenerationRequest& req, std::stop_token token) override;
    std::unique_ptr<IGenerationStream>
        generate_stream(const GenerationRequest& req, std::stop_token token) override;
    bool is_available() const override;
    std::string provider_name() const override { return "anthropic"; }

private:
    LLMConfig config_;
    std::string api_url_;
    std::string api_key_;
    
    Result<GenerationResult, LLMError> call_api(
        const GenerationRequest& req,
        std::stop_token token,
        bool stream = false);
};

} // namespace agenticdsl

#endif
```

**OpenAI API 实现**：

```cpp
// src/common/llm/openai_adapter.cpp

#include "openai_adapter.h"
#include "http_client.h"
#include <nlohmann/json.hpp>

namespace agenticdsl {

bool OpenAIAdapter::initialize(const LLMConfig& config) {
    if (config.api_key.empty()) {
        return false;
    }
    
    config_ = config;
    api_url_ = config.api_url + config.api_endpoint;
    api_key_ = config.api_key;
    
    return true;
}

Result<GenerationResult, LLMError> OpenAIAdapter::generate(
    const GenerationRequest& req,
    std::stop_token token) {
    
    return call_api(req, token, false);
}

Result<GenerationResult, LLMError> OpenAIAdapter::call_api(
    const GenerationRequest& req,
    std::stop_token token,
    bool stream) {
    
    // 构建请求体
    nlohmann::json request_body = {
        {"model", req.params.model.empty() ? config_.model : req.params.model},
        {"messages", nlohmann::json::array({
            {{"role", "user"}, {"content", req.prompt}}
        })},
        {"temperature", req.params.temperature},
        {"max_tokens", req.params.max_tokens},
        {"top_p", req.params.top_p}
    };
    
    if (stream) {
        request_body["stream"] = true;
    }
    
    // 发送 HTTP 请求
    HttpClient::Request requst{
        .url = api_url_,
        .method = "POST",
        .headers = {
            {"Authorization", "Bearer " + api_key_},
            {"Content-Type", "application/json"}
        },
        .body = request_body.dump()
    };
    
    auto response = HttpClient::post(requst, std::chrono::seconds(60));
    
    if (!response) {
        return Result<GenerationResult, LLMError>::failure(
            LLMError(LLMError::Code::NetworkError, "Request failed"));
    }
    
    if (response->status != 200) {
        auto error_code = parse_error_code(response->body);
        return Result<GenerationResult, LLMError>::failure(
            LLMError(error_code, response->body));
    }
    
    // 解析响应
    auto json_resp = nlohmann::json::parse(response->body);
    
    GenerationResult result;
    result.text = json_resp.at("choices").at(0).at("message").at("content");
    result.completion_tokens = json_resp.value("usage", {}).value("completion_tokens", 0);
    result.prompt_tokens = json_resp.value("usage", {}).value("prompt_tokens", 0);
    result.finish_reason = json_resp.at("choices").at(0).value("finish_reason", "stop");
    
    return Result<GenerationResult, LLMError>::success(result);
}

std::unique_ptr<IGenerationStream> OpenAIAdapter::generate_stream(
    const GenerationRequest& req,
    std::stop_token token) {
    
    // 流式实现返回 SSE 解析器
    return std::make_unique<OpenAISSEStream>(shared_from_this(), req, token);
}

bool OpenAIAdapter::is_available() const {
    return !config_.api_key.empty();
}

} // namespace agenticdsl
```

### 3.4 测试要求

| 测试用例 | 描述 | 预期结果 |
|---------|------|----------|
| `test_openai_generate` | 调用 OpenAI API 生成文本 | 返回非空文本 |
| `test_anthropic_generate` | 调用 Anthropic API 生成文本 | 返回非空文本 |
| `test_openai_stream` | 流式调用 OpenAI API | 逐步返回 token |
| `test_error_network` | 网络错误处理 | 返回 NetworkError |
| `test_error_auth` | 认证错误处理 | 返回 AuthenticationError |
| `test_retry_rate_limit` | 限流错误重试 | 等待后重试成功 |

---

## 4. Step 3: LLMRouter 实现（1 天）

### 4.1 功能要求列表

| 功能 | 描述 | 优先级 |
|------|------|--------|
| 路由决策 | 根据配置选择云端/本地 | 必须 |
| 故障转移 | 云端失败自动切换本地 | 应该 |
| 成本优化 | 低优先级任务使用本地 | 应该 |
| 统一接口 | 对上层提供统一 `ILLMProvider` 接口 | 必须 |

### 4.2 涉及文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/common/llm/llm_router.h` | 新建 | 路由器接口 |
| `src/common/llm/llm_router.cpp` | 新建 | 路由器实现 |
| `src/common/llm/CMakeLists.txt` | 修改 | 添加 router 目标 |

### 4.3 关键代码示例

**LLMRouter 接口**：

```cpp
// src/common/llm/llm_router.h

#ifndef AGENTICDSL_LLM_LLM_ROUTER_H
#define AGENTICDSL_LLM_LLM_ROUTER_H

#include "cloud_llm_adapter.h"
#include "llama_adapter.h"
#include "llm_types.h"
#include <memory>
#include <variant>

namespace agenticdsl {

/**
 * @brief LLM 后端类型
 */
enum class LLMBackend {
    CLOUD,  // 云端 LLM（高质量）
    LOCAL,  // 本地 llama.cpp（低质量）
    AUTO    // 自动选择
};

/**
 * @brief 路由决策结果
 */
struct RoutingDecision {
    LLMBackend backend;
    std::string provider;        // "openai", "anthropic", "local"
    std::string model;           // 实际使用的模型
    float estimated_quality;     // 0.0 ~ 1.0
};

/**
 * @brief LLM 路由器
 * 
 * 支持云端/本地自动路由，故障转移
 */
class LLMRouter : public ILLMProvider {
public:
    /**
     * @brief 构造函数
     * @param cloud_adapter 云端适配器
     * @param local_adapter 本地适配器（可选）
     */
    LLMRouter(std::unique_ptr<ICloudLLMAdapter> cloud_adapter,
              std::unique_ptr<LlamaAdapter> local_adapter = nullptr);
    
    // === ILLMProvider 接口 ===
    Result<GenerationResult, LLMError>
        generate(const GenerationRequest& req, std::stop_token token) override;
    
    std::unique_ptr<IGenerationStream>
        generate_stream(const GenerationRequest& req, std::stop_token token) override;
    
    // === 路由配置 ===
    void set_default_backend(LLMBackend backend);
    void set_quality_threshold(float threshold);  // 0.0 ~ 1.0
    
    // === 诊断 ===
    RoutingDecision get_decision(const GenerationRequest& req) const;
    LLMBackend get_active_backend() const;

private:
    std::unique_ptr<ICloudLLMAdapter> cloud_adapter_;
    std::unique_ptr<LlamaAdapter> local_adapter_;
    
    LLMBackend default_backend_ = LLMBackend::CLOUD;
    float quality_threshold_ = 0.5f;
    LLMBackend active_backend_ = LLMBackend::CLOUD;
    
    Result<GenerationResult, LLMError> route_and_generate(
        const GenerationRequest& req,
        std::stop_token token);
    
    Result<GenerationResult, LLMError> generate_with_fallback(
        const GenerationRequest& req,
        std::stop_token token,
        LLMBackend primary,
        LLMBackend fallback);
};

} // namespace agenticdsl

#endif
```

**LLMRouter 实现**：

```cpp
// src/common/llm/llm_router.cpp

#include "llm_router.h"

namespace agenticdsl {

LLMRouter::LLMRouter(std::unique_ptr<ICloudLLMAdapter> cloud_adapter,
                     std::unique_ptr<LlamaAdapter> local_adapter)
    : cloud_adapter_(std::move(cloud_adapter)),
      local_adapter_(std::move(local_adapter)) {
}

Result<GenerationResult, LLMError> LLMRouter::generate(
    const GenerationRequest& req,
    std::stop_token token) {
    
    return route_and_generate(req, token);
}

Result<GenerationResult, LLMError> LLMRouter::route_and_generate(
    const GenerationRequest& req,
    std::stop_token token) {
    
    auto decision = get_decision(req);
    
    switch (decision.backend) {
        case LLMBackend::CLOUD:
            if (cloud_adapter_ && cloud_adapter_->is_available()) {
                active_backend_ = LLMBackend::CLOUD;
                return cloud_adapter_->generate(req, token);
            }
            // 故障转移到本地
            if (local_adapter_) {
                active_backend_ = LLMBackend::LOCAL;
                return generate_local_fallback(req, token);
            }
            return Result<GenerationResult, LLMError>::failure(
                LLMError(LLMError::Code::NetworkError, "No available backend"));
            
        case LLMBackend::LOCAL:
            if (local_adapter_) {
                active_backend_ = LLMBackend::LOCAL;
                return generate_local_fallback(req, token);
            }
            // 本地不可用，尝试云端
            if (cloud_adapter_ && cloud_adapter_->is_available()) {
                active_backend_ = LLMBackend::CLOUD;
                return cloud_adapter_->generate(req, token);
            }
            return Result<GenerationResult, LLMError>::failure(
                LLMError(LLMError::Code::NetworkError, "No available backend"));
            
        case LLMBackend::AUTO:
            // 根据质量阈值自动选择
            if (decision.estimated_quality >= quality_threshold_) {
                return generate_with_fallback(req, token, 
                    LLMBackend::CLOUD, LLMBackend::LOCAL);
            } else {
                return generate_with_fallback(req, token,
                    LLMBackend::LOCAL, LLMBackend::CLOUD);
            }
    }
    
    return Result<GenerationResult, LLMError>::failure(
        LLMError(LLMError::Code::Unknown, "Invalid routing decision"));
}

RoutingDecision LLMRouter::get_decision(const GenerationRequest& req) const {
    RoutingDecision decision;
    
    if (default_backend_ != LLMBackend::AUTO) {
        decision.backend = default_backend_;
        decision.provider = default_backend_ == LLMBackend::CLOUD 
            ? cloud_adapter_->provider_name() 
            : "local";
        decision.model = req.params.model.empty() ? "default" : req.params.model;
        decision.estimated_quality = default_backend_ == LLMBackend::CLOUD ? 0.9f : 0.4f;
        return decision;
    }
    
    // AUTO 模式：根据请求特征判断
    // TODO: 实现更复杂的路由策略
    decision.backend = LLMBackend::CLOUD;
    decision.provider = "openai";
    decision.model = req.params.model.empty() ? "gpt-3.5-turbo" : req.params.model;
    decision.estimated_quality = 0.7f;
    
    return decision;
}

} // namespace agenticdsl
```

### 4.4 测试要求

| 测试用例 | 描述 | 预期结果 |
|---------|------|----------|
| `test_router_cloud_only` | 仅配置云端 | 使用云端适配器 |
| `test_router_local_only` | 仅配置本地 | 使用本地适配器 |
| `test_router_fallback` | 云端失败后切换本地 | 自动切换成功 |
| `test_router_auto_quality` | AUTO 模式质量路由 | 根据阈值选择 |
| `test_router_concurrent` | 并发请求 | 正确路由 |

---

## 5. Step 4: 流式与错误处理增强（1 天）

### 5.1 流式输出

#### 5.1.1 SSE 解析器实现

```cpp
// src/common/llm/sse_stream.h

#ifndef AGENTICDSL_LLM_SSE_STREAM_H
#define AGENTICDSL_LLM_SSE_STREAM_H

#include "llm_types.h"
#include <string>
#include <optional>
#include <functional>

namespace agenticdsl {

/**
 * @brief SSE 流式解析器
 * 
 * 解析 Server-Sent Events 格式的流式响应
 */
class SSEStream : public IGenerationStream {
public:
    using TokenCallback = std::function<void(const std::string&)>;
    
    SSEStream(TokenCallback callback);
    
    std::optional<std::string> next(std::stop_token token) override;
    bool is_active() const override;
    
    void feed(const std::string& data);
    void finish();

private:
    TokenCallback callback_;
    bool active_ = true;
    std::string buffer_;
    
    void parse_buffer();
    std::optional<std::string> extract_event(const std::string& line);
};

} // namespace agenticdsl

#endif
```

```cpp
// src/common/llm/sse_stream.cpp

#include "sse_stream.h"
#include <sstream>

namespace agenticdsl {

SSEStream::SSEStream(TokenCallback callback) : callback_(std::move(callback)) {}

std::optional<std::string> SSEStream::next(std::stop_token token) {
    while (active_ && !token.stop_requested()) {
        // 从缓冲区提取事件
        auto event = extract_event(buffer_);
        if (event) {
            return event;
        }
        
        // 等待更多数据（实际实现需要线程协作）
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return std::nullopt;
}

void SSEStream::feed(const std::string& data) {
    buffer_ += data;
    parse_buffer();
}

void SSEStream::parse_buffer() {
    // 解析 SSE 格式：
    // data: {"choices": [{"delta": {"content": "Hello"}}]}
    // data: {"choices": [{"delta": {"content": " world"}}]}
    // data: [DONE]
    
    size_t pos = 0;
    while ((pos = buffer_.find("data: ", pos)) != std::string::npos) {
        size_t end = buffer_.find('\n', pos);
        if (end == std::string::npos) break;
        
        std::string line = buffer_.substr(pos + 6, end - pos - 6);
        if (line == "[DONE]") {
            finish();
            break;
        }
        
        try {
            auto json = nlohmann::json::parse(line);
            if (json.contains("choices")) {
                auto content = json.at("choices").at(0).at("delta").value("content", "");
                if (!content.empty()) {
                    callback_(content);
                }
            }
        } catch (...) {
            // 忽略解析错误
        }
        
        pos = end;
    }
}

std::optional<std::string> SSEStream::extract_event(const std::string& line) {
    if (line.rfind("data: ", 0) == 0) {
        return line.substr(6);
    }
    return std::nullopt;
}

bool SSEStream::is_active() const {
    return active_;
}

void SSEStream::finish() {
    active_ = false;
}

} // namespace agenticdsl
```

### 5.2 重试策略

```cpp
// src/common/llm/retry_policy.h

#ifndef AGENTICDSL_LLM_RETRY_POLICY_H
#define AGENTICDSL_LLM_RETRY_POLICY_H

#include "llm_types.h"
#include <chrono>
#include <functional>

namespace agenticdsl {

/**
 * @brief 重试策略配置
 */
struct RetryPolicy {
    int max_retries = 3;
    std::chrono::milliseconds initial_delay = std::chrono::seconds(1);
    float backoff_multiplier = 2.0f;
    float jitter = 0.1f;
    
    std::chrono::milliseconds get_delay(int attempt) const {
        auto delay = std::chrono::milliseconds(
            static_cast<long>(initial_delay.count() * std::pow(backoff_multiplier, attempt)));
        
        // 添加 jitter
        int jitter_range = static_cast<int>(delay.count() * jitter);
        int jitter_value = (rand() % (jitter_range * 2 + 1)) - jitter_range;
        
        return std::chrono::milliseconds(delay.count() + jitter_value);
    }
};

/**
 * @brief 带重试的生成器包装
 */
class RetryableGenerator {
public:
    RetryableGenerator(std::unique_ptr<ICloudLLMAdapter> adapter,
                      RetryPolicy policy = RetryPolicy());
    
    Result<GenerationResult, LLMError> generate_with_retry(
        const GenerationRequest& req,
        std::stop_token token);
    
private:
    std::unique_ptr<ICloudLLMAdapter> adapter_;
    RetryPolicy policy_;
};

} // namespace agenticdsl

#endif
```

```cpp
// src/common/llm/retry_policy.cpp

#include "retry_policy.h"
#include <thread>
#include <random>

namespace agenticdsl {

RetryableGenerator::RetryableGenerator(std::unique_ptr<ICloudLLMAdapter> adapter,
                                       RetryPolicy policy)
    : adapter_(std::move(adapter)), policy_(policy) {}

Result<GenerationResult, LLMError> RetryableGenerator::generate_with_retry(
    const GenerationRequest& req,
    std::stop_token token) {
    
    int attempt = 0;
    
    while (attempt < policy_.max_retries) {
        auto result = adapter_->generate(req, token);
        
        if (result.has_value()) {
            return result;
        }
        
        const auto& error = result.error();
        
        // 不可重试的错误直接返回
        if (!error.retryable()) {
            return result;
        }
        
        attempt++;
        
        if (attempt >= policy_.max_retries) {
            return result;
        }
        
        // 等待后重试
        auto delay = policy_.get_delay(attempt);
        std::this_thread::sleep_for(delay);
    }
    
    return Result<GenerationResult, LLMError>::failure(
        LLMError(LLMError::Code::Unknown, "Max retries exceeded"));
}

} // namespace agenticdsl
```

### 5.3 错误分类

```cpp
// src/common/llm/llm_error.cpp

#include "llm_error.h"

namespace agenticdsl {

LLMErrorClassification classify_error(int http_status, const std::string& body) {
    switch (http_status) {
        case 400:
            return LLMErrorClassification{
                LLMError::Code::InvalidRequest,
                "Invalid request parameters",
                false
            };
            
        case 401:
        case 403:
            return LLMErrorClassification{
                LLMError::Code::AuthenticationError,
                "Authentication failed",
                false
            };
            
        case 429:
            // 解析 Retry-After 头
            return LLMErrorClassification{
                LLMError::Code::RateLimited,
                "Rate limit exceeded",
                true,
                std::chrono::seconds(60)
            };
            
        case 500:
        case 502:
        case 503:
            return LLMErrorClassification{
                LLMError::Code::ServerError,
                "Server error",
                true,
                std::chrono::seconds(30)
            };
            
        default:
            return LLMErrorClassification{
                LLMError::Code::Unknown,
                "Unknown error",
                false
            };
    }
}

} // namespace agenticdsl
```

### 5.4 涉及文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/common/llm/sse_stream.h` | 新建 | SSE 解析器接口 |
| `src/common/llm/sse_stream.cpp` | 新建 | SSE 解析器实现 |
| `src/common/llm/retry_policy.h` | 新建 | 重试策略接口 |
| `src/common/llm/retry_policy.cpp` | 新建 | 重试策略实现 |
| `src/common/llm/llm_error.h` | 新建 | 错误分类接口 |
| `src/common/llm/llm_error.cpp` | 新建 | 错误分类实现 |
| `src/common/llm/CMakeLists.txt` | 修改 | 添加新目标 |

---

## 6. 文件变更清单

### 6.1 新建文件

| 文件 | 说明 |
|------|------|
| `src/common/llm/llm_config.h` | 统一配置结构 |
| `src/common/llm/cloud_llm_adapter.h` | 云端 LLM 适配器接口 |
| `src/common/llm/openai_adapter.h` | OpenAI 适配器 |
| `src/common/llm/openai_adapter.cpp` | OpenAI 适配器实现 |
| `src/common/llm/anthropic_adapter.h` | Anthropic 适配器 |
| `src/common/llm/anthropic_adapter.cpp` | Anthropic 适配器实现 |
| `src/common/llm/http_client.h` | HTTP 客户端封装 |
| `src/common/llm/http_client.cpp` | HTTP 客户端实现 |
| `src/common/llm/llm_router.h` | LLM 路由器 |
| `src/common/llm/llm_router.cpp` | LLM 路由器实现 |
| `src/common/llm/sse_stream.h` | SSE 流式解析器 |
| `src/common/llm/sse_stream.cpp` | SSE 流式解析器实现 |
| `src/common/llm/retry_policy.h` | 重试策略 |
| `src/common/llm/retry_policy.cpp` | 重试策略实现 |
| `src/common/llm/llm_error.h` | 错误分类 |
| `src/common/llm/llm_error.cpp` | 错误分类实现 |

### 6.2 修改文件

| 文件 | 说明 |
|------|------|
| `src/common/llm/llm_adapter.h` | 标记 `ILLMAdapter` 为 deprecated |
| `src/common/llm/llm_types.h` | 导入统一配置，移除重复定义 |
| `src/common/llm/llama_adapter.h` | 使用新配置结构 |
| `src/common/llm/llama_adapter.cpp` | 适配新接口 |
| `src/common/llm/http_adapter.h` | 使用统一配置 |
| `src/common/llm/CMakeLists.txt` | 添加新目标 |

### 6.3 删除文件

| 文件 | 说明 |
|------|------|
| `src/common/llm/llm_tool.h` | 已合并到 llm_config.h |

---

## 7. 验证标准

- [ ] `ILLMAdapter` 接口标记为 `[[deprecated]]`
- [ ] `LLMConfig` 包含 provider、model、temperature、max_tokens 等统一字段
- [ ] `OpenAIAdapter::generate()` 可成功调用 OpenAI API 并返回结果
- [ ] `AnthropicAdapter::generate()` 可成功调用 Anthropic API 并返回结果
- [ ] `LLMRouter`可根据配置选择云端/本地后端
- [ ] 云端失败时自动切换到本地后端（故障转移）
- [ ] 流式输出逐步返回 token（非等待全部完成）
- [ ] NetworkError、RateLimited 错误自动重试
- [ ] 认证错误立即返回，不重试
- [ ] 编译通过，无警告
- [ ] 单元测试覆盖所有新代码
- [ ] 集成测试验证端到端流程

---

## 8. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| API 密钥泄露 | 高 | 使用环境变量，不硬编码 |
| 网络不稳定 | 中 | 实现指数退避重试 |
| API 定价超预期 | 中 | 添加用量监控和告警 |
| 流式响应解析失败 | 中 | 优雅处理，fallback 到非流式 |
| 本地 llama.cpp 版本不兼容 | 低 | 检测版本并记录日志 |
| 并发请求超过限制 | 中 | 实现请求队列和限流 |

---

## 9. 下一步行动

### 立即开始

1. **创建 `llm_config.h`**
   - 定义统一的 `LLMConfig` 结构
   - 包含 provider、model、采样参数等

2. **修改 `CMakeLists.txt`**
   - 添加新目标：`cloud_llm_adapter`、`llm_router`
   - 确保依赖关系正确

3. **实现 `http_client.h/cpp`**
   - 提供基础的 HTTP POST/GET 能力
   - 支持自定义 header 和超时

4. **实现 `openai_adapter.h/cpp`**
   - 继承 `ICloudLLMAdapter`
   - 实现 `/v1/chat/completions` 调用
   - 解析 JSON 响应

5. **实现 `llm_router.h/cpp`**
   - 组合 `ICloudLLMAdapter` 和 `LlamaAdapter`
   - 实现路由逻辑

### 后续步骤（阶段 1）

- 集成 `SessionRegistry` 实现多会话支持
- 实现 `ModuleState` 的 json scope nesting
- 添加 DSL 工具注册

---

**文档版本**: 1.0.0
**创建日期**: 2026-05-23
**作者**: AgenticDSL Team
