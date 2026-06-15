# ADR-0001: ILLMProvider 流式接口设计

## 状态

**✅ Approved** (2026-05-28)

## 背景

HydraForge Phase 1 需要支持多后端 LLM（OpenAI SSE、Anthropic SSE、llama-server HTTP），TUI 需要流式显示 token 实现打字机效果，同时需要支持 Ctrl+C 取消执行。现有的 `ILLMAdapter` 接口（同步 `generate()`，阻塞 `httplib::Client::Post`）无法满足需求。

---

## 决策

### 1. 接口风格：双方法 + Stream Handle 拉取模式

```cpp
// ============================================================
// 核心类型定义
// ============================================================

// 生成结果（同步模式用）
struct GenerationResult {
    std::string text;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    std::string finish_reason;
};

// 生成请求
struct GenerationRequest {
    std::string prompt;
    LLMParams params;  // temperature, max_tokens, top_p 等
};

// 结构化错误（替代 bool success + string error 反模式）
struct LLMError {
    enum class Code {
        NetworkError,       // 网络中断，可重试
        RateLimited,        // 限流，带 retry_after
        AuthenticationError, // 认证失败，不重试
        Cancelled,          // 用户取消
        InvalidRequest,     // 参数错误
        ServerError,        // 服务端错误，可重试
        ContextOverflow,    // 上下文超限
        Unknown
    };
    Code code;
    std::string message;
    std::optional<std::chrono::seconds> retry_after;

    bool retryable() const {
        return code == Code::RateLimited
            || code == Code::ServerError
            || code == Code::NetworkError;
    }
};

// ============================================================
// 流式接口（核心创新）
// ============================================================

// 流式生成器：调用方通过 next() 拉取 token，非回调推送
class IGenerationStream {
public:
    virtual ~IGenerationStream() = default;

    // 阻塞直到下一个 token 可用
    // - 返回 std::string: 有效的 token
    // - 返回 std::nullopt: 生成结束
    // - 抛出 LLMError: 生成过程出错
    // - 若 token.stop_requested() 为 true，应立即返回 std::nullopt
    virtual std::optional<std::string> next(std::stop_token token) = 0;

    virtual bool is_active() const = 0;

    // 未来可扩展（无需破坏现有实现）：
    // virtual std::optional<UsageStats> usage_stats();
    // virtual std::string finish_reason() const;
};

// ============================================================
// Provider 接口
// ============================================================

class ILLMProvider {
public:
    virtual ~ILLMProvider() = default;

    // 同步模式（简单场景用）
    virtual std::expected<GenerationResult, LLMError>
        generate(const GenerationRequest& req,
                 std::stop_token token = {}) = 0;

    // 流式模式（返回 stream handle，调用方拉取）
    virtual std::unique_ptr<IGenerationStream>
        generate_stream(const GenerationRequest& req,
                        std::stop_token token = {}) = 0;

    virtual bool is_available() const = 0;
    virtual std::string name() const = 0;

    // ===== ADR-0034 新增：模型发现能力（默认实现，非破坏性）=====

    // 返回该 Provider 支持的所有模型能力描述
    // 默认返回空列表——不影响现有实现
    virtual std::vector<ModelCapability> available_models() const {
        return {};
    }

    // 检查指定模型是否可用
    // 默认返回 true——向后兼容
    virtual bool is_model_available(const std::string& model_id) const {
        (void)model_id;
        return true;
    }
};
```

### 2. 取消机制：`std::stop_token`

```cpp
// 使用示例
std::stop_source stop_src;
std::stop_token token = stop_src.get_token();

// 在另一个线程取消
stop_src.request_stop();

// Provider 端检查
std::optional<std::string> next(std::stop_token token) override {
    if (token.stop_requested()) {
        // 清理资源，关闭连接
        close_connection();
        return std::nullopt;
    }
    // 继续读取下一个 token...
}
```

**设计原则**：
- `std::stop_token` 是 C++20 标准协作取消机制
- 即使后端不支持真正的请求取消（如 Anthropic SSE），也可在 `next()` 中检测停止请求后丢弃后续数据
- 多个 Agent 的取消可以合并到同一个 `stop_source`（如"取消所有正在执行的任务"）

### 3. 错误处理：结构化 `LLMError` + `std::expected`

```cpp
// 同步模式使用 std::expected
std::expected<GenerationResult, LLMError> generate(...);

// 流式模式：next() 抛出或返回 unexpected
std::optional<std::string> next(std::stop_token token) override {
    if (error_occurred) {
        throw LLMError{LLMError::Code::NetworkError, "connection reset"};
        // 或者
        return std::unexpected(LLMError{LLMError::Code::NetworkError, ...});
    }
}

// 调用方重试逻辑示例
auto result = provider.generate(req, token);
if (!result && result.error().retryable()) {
    std::this_thread::sleep_for(result.error().retry_after.value());
    goto retry;
}
```

**`LLMError::Code` 设计理由**：
- `RateLimited` 携带 `retry_after` 用于指数退避
- `AuthenticationError` 直接失败，不重试
- `ContextOverflow` 可触发上下文压缩策略（Phase 2）
- `Cancelled` 区分用户主动取消 vs 错误取消

### 4. SSE 解析要求

所有 HTTP 流式适配器必须实现状态机解析：

```cpp
// SSE 解析状态机（示意）
class SSETokenParser {
    std::string buffer_;
public:
    // 按 \n 分割行，累积多行 data: 字段，空白行触发 emit
    std::vector<std::string> parse(const std::string& chunk) {
        std::vector<std::string> tokens;
        std::istringstream stream(chunk);
        std::string line;
        while (std::getline(stream, line)) {
            if (line == "\r" || line.empty()) {
                if (!buffer_.empty()) {
                    tokens.push_back(extract_content(buffer_));
                    buffer_.clear();
                }
            } else if (line.starts_with("data: ")) {
                buffer_ += line.substr(6);  // 累积跨行 data
            }
        }
        return tokens;
    }
};
```

**注意**：
- `data:` 字段可能跨多行，需要累积后提取
- 空行（`\n\n`）表示一个事件结束，触发 emit
- `data: [DONE]` 表示流结束

---

## 实现要求

### 每个适配器必须实现

| 方法 | 要求 |
|------|------|
| `generate()` | 同步阻塞，完整响应后返回 |
| `generate_stream()` | 返回 `unique_ptr<IGenerationStream>` |
| `IGenerationStream::next()` | 线程安全，可被多个线程调用（每个 stream 自己独立） |
| 取消响应 | 检测 `stop_requested()` 后尽可能清理资源 |

### 编译要求

- C++20 编译器
- `std::expected` 为 C++23，若编译器不支持，用 `tl::expected` 或自定义 `Result<T, E>` 替代
- 所有 `LLMError` 构造必须使用具名构造函数，禁止裸字符串构造

---

## 权衡

### 为什么不用回调（push 模式）？

| 回调模式 | Stream Handle 模式 |
|---------|-------------------|
| Provider 在自己的线程调用 `on_token` | Provider 把 token 写入内部队列 |
| 调用方需要处理线程转发（PostEvent 等） | 调用方在任意线程调用 `next()` 拉取 |
| 新增方法（如 `usage_stats()`）需要改签名 | 新增方法只需扩展 `IGenerationStream` 接口 |
| 取消只能通过检查标志 | `next(token)` 可直接感知取消 |

### 为什么不用 `std::future`？

- C++ `future` 对 I/O 密集场景过重（需要线程池/executor）
- 与 SSE push 模型语义冲突（流式数据不是"一次性结果"）
- `std::expected` + `std::stop_token` 组合足够表达所有场景

---

## 未来扩展能力（不破坏当前设计）

以下能力可在不破坏 `ILLMProvider` 接口的情况下添加：

1. **`IGenerationStream::usage_stats()`** — 返回 token 使用统计
2. **`IGenerationStream::finish_reason()`** — 返回结束原因（stop/complete）
3. **`IGenerationStream::metadata()`** — 返回原始响应头/元数据
4. **Streaming embeddings** — 新增 `IEmbeddingStream` 接口

---

## 影响范围

| 组件 | 变更 |
|------|------|
| `src/common/llm/illm_provider.h` | 新增接口文件 |
| `src/common/llm/llama_adapter.h` | 重构实现 `ILLMProvider` |
| `src/common/llm/openai_adapter.h/cpp` | 新增，实现 SSE 解析 |
| `src/common/llm/anthropic_adapter.h/cpp` | 新增，实现 SSE 解析 |
| `src/common/llm/llm_provider_factory.h/cpp` | 工厂模式不变 |
| `src/modules/executor/node_executor.h` | 调整调用方式（从裸指针到 shared_ptr）|
| `src/core/engine.h/cpp` | 可能需要传递 `stop_token` |

---

## 替代方案

### 替代 1：单方法 + 空回调（被否决）

```cpp
virtual LLMResult generate(const std::string& prompt, const LLMParams& params,
                          std::function<void(const std::string&)> on_token = {},
                          std::stop_token token = {});
```

**否决理由**：语义隐晦（空回调 = 同步是隐式约定），长期维护歧义。

### 替代 2：async future 模式（被否决）

**否决理由**：future 对 I/O 过重，SSE push 模型不匹配。

### 替代 3：回调 + 无界队列（被否决）

**否决理由**：无背压，高频 token 时内存爆炸，无取消语义。

---

## 结论

采用 Stream Handle 拉取模式 + `std::stop_token` + 结构化错误是长期最优解：

- **可扩展**：新能力通过扩展 `IGenerationStream` 添加，不破坏已有实现
- **可测试**：Stream 可 mock，取消语义清晰
- **可组合**：多 Agent 取消可合并，错误分类支持重试策略
- **无架构债务**：不是短期便利，是 5 年以上演化路径

---

*文档版本: v1.1*
*最后更新: 2026-05-28*

---

## 附录：ADR-0034 模型路由扩展

### 变更说明

ADR-0034 在 `ILLMProvider` 上新增了两个**默认实现方法**，用于支持模型路由：

| 方法 | 用途 | 默认行为 |
|------|------|----------|
| `available_models()` | 查询 Provider 支持的所有模型 | 返回空列表 `{}` |
| `is_model_available(model_id)` | 检查指定模型是否可用 | 返回 `true` |

### 兼容性保证

- **现有实现无需修改**：默认实现不破坏已有代码
- **新实现可覆盖**：多模型 Provider（如 HTTP 后端）可覆盖返回实际支持的模型列表
- **非侵入式**：不影响 `generate()` / `generate_stream()` 的现有调用方

### 与 ADR-0034 的关系

```
ADR-0034 (ModelRouter)
    │
    ├── 使用 ILLMProvider::available_models() 获取候选模型
    ├── 使用 ILLMProvider::is_model_available() 验证可用性
    └── 通过 ModelRegistry 聚合多 Provider 的模型信息
```

### 参考

- [ADR-0034: IModelRouter 模型路由接口](plugin/adr-0034-model-router.md)