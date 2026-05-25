# CloudLLMAdapter API 设计

**ID**: API-001
**日期**: 2026-05-23
**状态**: 已批准
**关联**: BOOT-001, TEST-001, ROUTER-001, ADR-0001, ADR-0005

---

## 概述

CloudLLMAdapter 是云端 LLM（OpenAI/Claude）适配器，提供统一的接口访问不同的云端推理服务。

## 架构

```
                    ┌─────────────────────┐
                    │    LLMRouter        │
                    └──────────┬──────────┘
                               │
              ┌────────────────┼────────────────┐
              │                │                │
              ▼                ▼                ▼
    ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
    │ CloudLLMAdapter │ │ LocalLLMAdapter │ │ FutureAdapters  │
    │   (OpenAI/      │ │   (llama.cpp)   │ │  (Gemini, etc)  │
    │    Anthropic)   │ │                 │ │                 │
    └─────────────────┘ └─────────────────┘ └─────────────────┘
              │                │                │
              └────────────────┼────────────────┘
                               │
                    ┌──────────┴──────────┐
                    │   ILLMProvider      │
                    │   (统一接口)         │
                    └─────────────────────┘
```

## 接口设计

### ILLMProvider 基类

```cpp
// src/common/llm/illm_provider.h
class ILLMProvider {
public:
    virtual ~ILLMProvider() = default;

    // 生成文本（同步）
    virtual Result<std::string, LLMError> generate(
        const std::string& prompt,
        const LLMConfig& override_config = {}
    ) = 0;

    // 生成文本（流式）
    virtual Result<void, LLMError> generate_stream(
        const std::string& prompt,
        std::function<void(const std::string&)> on_token,
        const LLMConfig& override_config = {}
    ) = 0;

    // 获取提供者和模型信息
    virtual std::string provider() const = 0;
    virtual std::string model() const = 0;

    // 健康检查
    virtual bool is_available() const = 0;
};
```

### CloudLLMAdapter

```cpp
// src/common/llm/cloud_llm_adapter.h
class CloudLLMAdapter : public ILLMProvider {
public:
    struct Config {
        // 提供者配置
        std::string provider;              // "openai" / "anthropic"
        std::string api_key;
        std::string base_url;               // 可选，默认使用官方 API

        // 模型配置
        std::string model;                  // "gpt-4", "claude-3-opus"
        int max_tokens = 4096;
        float temperature = 0.7f;
        float top_p = 0.9f;
        int top_k = 40;
        float repeat_penalty = 1.1f;
        float frequency_penalty = 0.0f;
        float presence_penalty = 0.0f;

        // 请求配置
        std::vector<std::string> stop_tokens;
        float timeout_seconds = 60.0f;
        int max_retries = 3;

        // API 版本
        std::string api_version = "v1";    // OpenAI API 版本
    };

    CloudLLMAdapter() = default;
    explicit CloudLLMAdapter(const Config& config);
    ~CloudLLMAdapter() override;

    // ILLMProvider 接口
    Result<std::string, LLMError> generate(
        const std::string& prompt,
        const LLMConfig& override_config = {}
    ) override;

    Result<void, LLMError> generate_stream(
        const std::string& prompt,
        std::function<void(const std::string&)> on_token,
        const LLMConfig& override_config = {}
    ) override;

    std::string provider() const override { return config_.provider; }
    std::string model() const override { return config_.model; }
    bool is_available() const override;

    // 配置管理
    void initialize(const Config& config);
    void update_config(const Config& config);
    Config get_config() const { return config_; }

    // 统计信息
    struct Stats {
        int total_requests = 0;
        int successful_requests = 0;
        int failed_requests = 0;
        float total_latency_ms = 0;
        float avg_latency_ms = 0;
    };
    Stats get_stats() const;

private:
    Config config_;
    Stats stats_;

    // HTTP 客户端
    std::unique_ptr<httplib::HTTPClient> http_client_;

    // OpenAI API 调用
    Result<std::string, LLMError> call_openai(
        const std::string& prompt,
        const Config& override);

    Result<void, LLMError> call_openai_stream(
        const std::string& prompt,
        std::function<void(const std::string&)> on_token,
        const Config& override);

    // Anthropic API 调用
    Result<std::string, LLMError> call_anthropic(
        const std::string& prompt,
        const Config& override);

    Result<void, LLMError> call_anthropic_stream(
        const std::string& prompt,
        std::function<void(const std::string&)> on_token,
        const Config& override);

    // 请求构建
    std::string build_openai_request(const std::string& prompt, const Config& config);
    std::string build_anthropic_request(const std::string& prompt, const Config& config);

    // 响应解析
    Result<std::string, LLMError> parse_openai_response(const std::string& body);
    Result<std::string, LLMError> parse_anthropic_response(const std::string& body);
};
```

## API 请求/响应格式

### OpenAI Chat Completions

**请求**:
```json
POST /v1/chat/completions
{
  "model": "gpt-4",
  "messages": [
    {"role": "user", "content": "Hello"}
  ],
  "temperature": 0.7,
  "top_p": 0.9,
  "max_tokens": 4096,
  "stream": false
}
```

**响应**:
```json
{
  "id": "chatcmpl-xxx",
  "object": "chat.completion",
  "created": 1234567890,
  "model": "gpt-4",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "Hello! How can I help you?"
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 10,
    "completion_tokens": 20,
    "total_tokens": 30
  }
}
```

### Anthropic Messages

**请求**:
```json
POST /v1/messages
{
  "model": "claude-3-opus",
  "messages": [
    {"role": "user", "content": "Hello"}
  ],
  "temperature": 0.7,
  "max_tokens": 4096
}
```

**响应**:
```json
{
  "id": "msg_xxx",
  "type": "message",
  "role": "assistant",
  "content": [
    {
      "type": "text",
      "text": "Hello! How can I help you?"
    }
  ],
  "model": "claude-3-opus",
  "stop_reason": "end_turn",
  "usage": {
    "input_tokens": 10,
    "output_tokens": 20
  }
}
```

## 使用示例

### 基本使用

```cpp
// 初始化
CloudLLMAdapter::Config config;
config.provider = "openai";
config.api_key = std::getenv("OPENAI_API_KEY");  // 从环境变量读取
config.model = "gpt-4";
config.temperature = 0.7f;

CloudLLMAdapter adapter;
auto result = adapter.initialize(config);

// 生成
auto result = adapter.generate("What is the capital of France?");
if (result.is_ok()) {
    std::cout << result.value() << std::endl;
} else {
    std::cerr << "Error: " << result.error().message << std::endl;
}
```

### 流式生成

```cpp
adapter.generate_stream(
    "Write a story about a robot",
    [](const std::string& token) {
        std::cout << token << std::flush;
    }
);
```

### 配置覆盖

```cpp
// 使用默认配置生成
adapter.generate("Hello");

// 覆盖配置生成
LLMConfig override;
override.temperature = 0.5f;
override.max_tokens = 100;

adapter.generate("Hello", override);
```

## 错误处理

### LLMError 类型

```cpp
// src/common/llm/llm_error.h
class LLMError {
public:
    enum class Code {
        Success = 0,
        NetworkError,          // 网络错误
        Timeout,               // 请求超时
        AuthenticationError,  // API key 无效
        RateLimitError,        // 限流
        InvalidRequest,        // 请求格式错误
        ModelNotFound,         // 模型不存在
        ServerError,           // 服务器错误
        UnknownError
    };

    LLMError(Code code, const std::string& message)
        : code_(code), message_(message) {}

    Code code() const { return code_; }
    const std::string& message() const { return message_; }
    bool is_retryable() const;

    static LLMError network_error(const std::string& details);
    static LLMError timeout_error(const std::string& details);
    static LLMError auth_error(const std::string& details);
    static LLMError rate_limit_error(const std::string& details);

private:
    Code code_;
    std::string message_;
};
```

### 错误处理示例

```cpp
auto result = adapter.generate("Hello");
if (!result.is_ok()) {
    auto& error = result.error();
    switch (error.code()) {
        case LLMError::Code::AuthenticationError:
            // 处理认证错误
            break;
        case LLMError::Code::RateLimitError:
            // 处理限流，等待后重试
            std::this_thread::sleep_for(std::chrono::seconds(60));
            break;
        case LLMError::Code::NetworkError:
            // 网络错误，重试
            break;
        default:
            // 其他错误
            break;
    }
}
```

## 配置管理

### 配置文件格式

```json
{
  "llm": {
    "provider": "openai",
    "api_key_env": "OPENAI_API_KEY",
    "model": "gpt-4",
    "temperature": 0.7,
    "max_tokens": 4096,
    "timeout_seconds": 60,
    "max_retries": 3
  }
}
```

### 配置加载

```cpp
// 从配置文件加载
nlohmann::json config_json;
std::ifstream("config.json") >> config_json;

CloudLLMAdapter::Config config;
config.provider = config_json["provider"];
config.api_key = std::getenv(config_json["api_key_env"].get<std::string>().c_str());
config.model = config_json["model"];

// 从 JSON 直接加载
config = CloudLLMAdapter::Config::from_json(config_json["llm"]);
```

## 安全性

### API Key 管理

- **禁止**硬编码 API key
- **必须**从环境变量读取
- **建议**使用密钥管理服务（如 AWS Secrets Manager）

```cpp
// 推荐方式
config.api_key = std::getenv("OPENAI_API_KEY");
if (config.api_key.empty()) {
    return LLMError::auth_error("OPENAI_API_KEY not set");
}

// 不推荐
config.api_key = "sk-xxx";  // 禁止
```

### HTTPS 强制

```cpp
// 确保使用 HTTPS
if (!starts_with(config.base_url, "https://")) {
    return LLMError::invalid_request("base_url must use HTTPS");
}
```

## 性能考虑

### 连接复用

```cpp
// HTTP 客户端应该保持连接复用
class CloudLLMAdapter {
private:
    std::unique_ptr<httplib::HTTPClient> http_client_;

    void ensure_connection() {
        if (!http_client_ || !http_client_->is_valid()) {
            http_client_ = std::make_unique<httplib::HTTPClient>(
                config_.base_url,
                config_.timeout_seconds
            );
        }
    }
};
```

### 重试策略

```cpp
Result<std::string, LLMError> CloudLLMAdapter::generate_with_retry(
    const std::string& prompt,
    const Config& config) {

    for (int attempt = 0; attempt < config.max_retries; ++attempt) {
        auto result = generate(prompt, config);
        if (result.is_ok()) {
            return result;
        }

        auto& error = result.error();
        if (!error.is_retryable()) {
            return result;
        }

        // 指数退避
        std::this_thread::sleep_for(
            std::chrono::milliseconds(100 * (1 << attempt))
        );
    }

    return LLMError::unknown_error("Max retries exceeded");
}
```

## 测试策略

### 单元测试

```cpp
TEST_CASE("CloudLLMAdapter - 初始化") {
    CloudLLMAdapter adapter;
    CloudLLMAdapter::Config config;
    config.provider = "openai";
    config.api_key = "test-key";
    config.model = "gpt-4";

    auto result = adapter.initialize(config);
    REQUIRE(result.is_ok());
}

TEST_CASE("CloudLLMAdapter - OpenAI 请求构建") {
    CloudLLMAdapter adapter;
    auto body = adapter.build_openai_request("Hello", config);

    auto json = nlohmann::json::parse(body);
    REQUIRE(json["model"] == "gpt-4");
    REQUIRE(json["messages"][0]["content"] == "Hello");
}

TEST_CASE("CloudLLMAdapter - 响应解析") {
    CloudLLMAdapter adapter;
    std::string response = R"({
        "choices": [{"message": {"content": "Hi"}}]
    })";

    auto result = adapter.parse_openai_response(response);
    REQUIRE(result.is_ok());
    REQUIRE(result.value() == "Hi");
}
```

### Mock 测试

```cpp
TEST_CASE("CloudLLMAdapter - Mock 服务器", "[mock]") {
    MockHttpServer server;
    server.set_response(R"({
        "choices": [{"message": {"content": "Mock response"}}]
    })");
    server.start(18080);
    SCOPE_EXIT { server.stop(); };

    CloudLLMAdapter adapter;
    adapter.initialize({
        .provider = "openai",
        .base_url = "http://localhost:18080",
        .api_key = "test",
        .model = "gpt-4"
    });

    auto result = adapter.generate("test");
    REQUIRE(result.is_ok());
    REQUIRE(result.value() == "Mock response");
}
```

## 验证标准

- [ ] 支持 OpenAI Chat Completions API
- [ ] 支持 Anthropic Messages API
- [ ] 流式输出正常工作
- [ ] 错误处理完善（网络、超时、认证、限流）
- [ ] API key 从环境变量读取
- [ ] 配置可序列化为 JSON
- [ ] 单元测试覆盖率 > 80%
- [ ] Mock 测试覆盖主要场景

## 关联文档

| 文档 | 关系 |
|------|------|
| [BOOT-001: 自举实施路径](../implementation/self-bootstrapping-path.md) | CloudLLMAdapter 是阶段 0 核心组件 |
| [ROUTER-001: 推理路由器](../architecture/inference-router.md) | CloudLLMAdapter 被 LLMRouter 调用 |
| [TEST-001: 测试策略](test-strategy.md) | CloudLLMAdapter 需要完整的测试覆盖 |
| [LLM Types](../llm/llm_types.md) | 使用统一的 LLM 类型系统 |